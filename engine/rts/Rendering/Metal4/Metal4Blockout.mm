/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Blockout.h"
#include "Metal4WorldBCDecode.h"
#include "NativeBlockoutShaders.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
namespace rendering::metal4 {
namespace {
struct Vertex{std::array<float,3> position,normal,tangent,bitangent;std::array<float,2> uv;uint32_t piece;};
static_assert(sizeof(Vertex)==60);
struct Uniform{simd_float4x4 viewProjection;simd_float4 positionYaw,parameters,sunDirection,ambient,diffuse;simd_uint4 options;};
static_assert(sizeof(Uniform)==160);
struct Mesh{std::shared_ptr<const WorldModel> owner;__strong id<MTLBuffer> vertices=nil,indices=nil;NSUInteger count=0;};
struct Image{std::shared_ptr<const WorldTexture> owner;__strong id<MTLTexture> texture=nil;bool initialized=false;};
struct SourceSampler{NativeTextureSampling source;__strong id<MTLSamplerState> sampler=nil;};
struct TargetStorage{__strong id<MTLTexture> color=nil,depthProbe=nil;};
MTLSamplerAddressMode Address(unsigned value) {
	switch(value) {
		case 0x2901:return MTLSamplerAddressModeRepeat;
		case 0x8370:return MTLSamplerAddressModeMirrorRepeat;
		case 0x812f:return MTLSamplerAddressModeClampToEdge;
		case 0x812d:return MTLSamplerAddressModeClampToBorderColor;
	}
	throw std::runtime_error("Original Blockout texture wrap requires its actual coordinate consumer");
}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* data,size_t size) {
	Require(size&&size<=lease.Device().maxBufferLength,"Original Blockout GPU buffer extent invalid");auto result=[lease.Device()newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];Require(result!=nil,"Cannot allocate original Blueprint GPU buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value) {
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown original Blueprint blend equation");
}
MTLPixelFormat Format(WorldTextureFormat value) {
	switch(value){case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;}throw std::runtime_error("Unknown original Blueprint image format");
}
std::shared_ptr<Image> Upload(RuntimeLease& lease,std::shared_ptr<const WorldTexture> owner) {
	Require(owner&&!owner->mips.empty()&&owner->mips.size()<=16,"Original Blockout image mip count invalid");
	// Preserve the original material owner and sampler. Only generated-mip
	// transport expands BC1/2/3; embedded BC levels remain byte-identical.
	const auto decoded=DecodeWorldBCBaseForMipmaps(*owner,lease.Capabilities().maxTextureDimension2D);
	const auto storageFormat=decoded?WorldTextureFormat::RGBA8Unorm:owner->format;
	const bool compressed=storageFormat!=WorldTextureFormat::RGBA8Unorm&&storageFormat!=WorldTextureFormat::BGRA8Unorm;
	Require(!compressed||lease.Device().supportsBCTextureCompression,"Original Blockout BC image unsupported on actual device");
	auto width=owner->mips[0].width,height=owner->mips[0].height;Require(width&&height&&width<=lease.Capabilities().maxTextureDimension2D&&height<=lease.Capabilities().maxTextureDimension2D,"Original Blockout image exceeds actual texture dimensions");
	unsigned levels=1;for(auto extent=std::max(width,height);extent>1;extent>>=1)++levels;Require(owner->mips.size()<=levels,"Original Blockout image has excess mip levels");
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(decoded && owner->format == WorldTextureFormat::BC7RGBASRGB ? MTLPixelFormatRGBA8Unorm_sRGB : Format(storageFormat)) width:width height:height mipmapped:YES];descriptor.mipmapLevelCount=owner->generateMipmaps?levels:owner->mips.size();descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageShaderRead;
	auto image=std::make_shared<Image>();image->texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(image->texture!=nil,"Cannot allocate original Blueprint material image");image->texture.label=String(owner->sourcePath);
	for(size_t level=0;level<owner->mips.size();++level) {
		const auto& mip=decoded?*decoded:owner->mips[level];const size_t rows=compressed?(size_t(height)+3)/4:height;
		const size_t rowBytes=compressed?((size_t(width)+3)/4)*((storageFormat==WorldTextureFormat::BC1RGBA||storageFormat==WorldTextureFormat::BC4Unorm)?8:16):size_t(width)*4;
		Require(mip.width==width&&mip.height==height&&mip.bytesPerRow>=rowBytes&&mip.bytesPerRow<=128*1024&&mip.bytes.size()>=mip.bytesPerRow*rows,"Original Blockout image mip bytes differ");
		[image->texture replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:level withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];width=std::max(1u,width/2);height=std::max(1u,height/2);
	}
	image->initialized=!owner->generateMipmaps||image->texture.mipmapLevelCount==1;image->owner=std::move(owner);return image;
}

std::shared_ptr<Mesh> Geometry(RuntimeLease& lease,std::shared_ptr<const WorldModel> owner){
	std::vector<Vertex> vertices;std::vector<uint32_t> indices;
	for(const auto& piece:owner->pieces){
		Require(vertices.size()+piece.vertices.size()<=UINT32_MAX,"Blockout original geometry index overflow");
		auto base=uint32_t(vertices.size());for(const auto& v:piece.vertices)vertices.push_back({v.position,v.normal,v.sTangent,v.tTangent,v.uv[0],uint32_t(v.boneIds[0]&255)});
		for(auto index:piece.indices){Require(index<piece.vertices.size(),"Blockout source piece index invalid");indices.push_back(base+index);}
	}
	auto out=std::make_shared<Mesh>();out->owner=std::move(owner);out->count=indices.size();
	if(out->count){out->vertices=Buffer(lease,vertices.data(),vertices.size()*sizeof(Vertex));out->indices=Buffer(lease,indices.data(),indices.size()*sizeof(uint32_t));}
	return out;
}
}
struct PreparedBlockoutFrame::Impl{
	struct Draw{
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		std::shared_ptr<Mesh> mesh;
		NativeWorldDrawState world;effectstate::Raster raster;
	};
	struct Packet{
		uint64_t sequence=0;std::vector<Draw> draws;
		std::shared_ptr<const NativeBlockoutTarget> target;
		std::shared_ptr<const NativePostprocessVersion> output;
		__strong id<MTLTexture> depth=nil;
		float clear=1;std::shared_ptr<bool> encoded=std::make_shared<bool>(false);
	};
	std::shared_ptr<const NativeBlockoutFrame> owner;
	std::vector<Packet> packets;
	std::vector<std::shared_ptr<Image>> images,pending;
};
PreparedBlockoutFrame::PreparedBlockoutFrame():impl(std::make_unique<Impl>()){}
PreparedBlockoutFrame::~PreparedBlockoutFrame()=default;
size_t PreparedBlockoutFrame::DrawCount()const{return impl->packets.size();}
uint64_t PreparedBlockoutFrame::SequenceAt(size_t n)const{return impl->packets.at(n).sequence;}
bool PreparedBlockoutFrame::IsDepthPass(size_t n)const{return bool(impl->packets.at(n).target);}
std::shared_ptr<const NativePostprocessVersion> PreparedBlockoutFrame::OutputAt(size_t n)const{return impl->packets.at(n).output;}
void PreparedBlockoutFrame::EncodeDependencies(id<MTL4CommandBuffer> command)const{
	if(impl->pending.empty())return;
	auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode actual Blockout model mip producers");
	for(const auto& image:impl->pending)[encoder generateMipmapsForTexture:image->texture];
	[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
}
namespace{
template<class Draw>void EncodeBlockoutDraw(const Draw& draw,id<MTL4RenderCommandEncoder> encoder){
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	effectstate::RasterState(encoder,draw.world,draw.raster);
	[encoder setDepthClipMode:MTLDepthClipModeClamp]; // Actual original gl.DepthClamp(true).
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.mesh->count indexType:MTLIndexTypeUInt32 indexBuffer:draw.mesh->indices.gpuAddress indexBufferLength:draw.mesh->indices.length];
}
}
void PreparedBlockoutFrame::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder)const{
	const auto& packet=impl->packets.at(n);Require(!packet.target&&!*packet.encoded,"Blockout main draw encoded in wrong target/twice");
	for(const auto& draw:packet.draws)EncodeBlockoutDraw(draw,encoder);*packet.encoded=true;
}
void PreparedBlockoutFrame::EncodeDepthPass(size_t n,id<MTL4CommandBuffer> command)const{
	const auto& packet=impl->packets.at(n);Require(packet.target&&packet.output&&!*packet.encoded,"Blockout private depth producer encoded twice/without target");
	auto storage=std::static_pointer_cast<const TargetStorage>(packet.target->storage);
	auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=storage->color;
	pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
	pass.depthAttachment.texture=packet.depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.clearDepth=packet.clear;pass.depthAttachment.storeAction=MTLStoreActionStore;
	pass.renderTargetWidth=packet.target->width;pass.renderTargetHeight=packet.target->height;
	auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot encode actual Blockout R8+D24 redraw");
	for(const auto& draw:packet.draws)EncodeBlockoutDraw(draw,encoder);
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];*packet.encoded=true;
}
void PreparedBlockoutFrame::DidComplete()const{
	for(const auto& packet:impl->packets)Require(*packet.encoded,"Cannot retire unencoded Blockout source geometry/depth");
	for(const auto& image:impl->pending)image->initialized=true;
}
struct BlockoutGPU::Impl{
	__strong id<MTLLibrary> library=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
	NormalImageProvider normals;
	std::map<std::shared_ptr<const WorldModel>,std::shared_ptr<Mesh>,std::owner_less<std::shared_ptr<const WorldModel>>> meshes;
	std::map<std::shared_ptr<const WorldTexture>,std::shared_ptr<Image>,std::owner_less<std::shared_ptr<const WorldTexture>>> images;
	std::vector<SourceSampler> sourceSamplers;
	std::map<std::tuple<bool,bool,unsigned,uint64_t,unsigned,bool>,id<MTLRenderPipelineState>> pipelines;
	id<MTLSamplerState> Sampler(RuntimeLease& lease,const NativeCUSTextureObject& object) {
		const auto& p=object.sampling;
		Require(p.compareMode==0&&p.baseLevel==0&&p.maxLevel>=int(object.allocation.levels)-1&&p.lodBias==0,
			"Original Blockout model sampler comparison/base/max/bias requires its actual consumer");
		Require(std::isfinite(p.minLod)&&std::isfinite(p.maxLod)&&p.maxLod>=0&&p.minLod<=p.maxLod&&
			p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,
			"Original Blockout model sampler LOD/anisotropy differs");
		Require(p.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},
			"Original Blockout model texture swizzle requires its actual consumer");
		Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{1,1,1,1},
			"Original Blockout model texture custom border requires its actual consumer");
		Require((p.minFilter==0x2600||p.minFilter==0x2601||(p.minFilter>=0x2700&&p.minFilter<=0x2703))&&
			(p.magFilter==0x2600||p.magFilter==0x2601),"Unknown original Blueprint model texture filter");
		for(const auto& cached:sourceSamplers)if(cached.source==p){lease.Retain(cached.sampler);return cached.sampler;}
		auto descriptor=[MTLSamplerDescriptor new];
		descriptor.minFilter=(p.minFilter==0x2600||p.minFilter==0x2700||p.minFilter==0x2702)?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
		descriptor.magFilter=p.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
		descriptor.mipFilter=(p.minFilter==0x2600||p.minFilter==0x2601)?MTLSamplerMipFilterNotMipmapped:
			(p.minFilter==0x2700||p.minFilter==0x2701)?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
		descriptor.sAddressMode=Address(p.wrapS);descriptor.tAddressMode=Address(p.wrapT);
		descriptor.borderColor=p.borderColor==std::array<float,4>{1,1,1,1}?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorTransparentBlack;
		descriptor.lodMinClamp=std::max(0.f,p.minLod);descriptor.lodMaxClamp=p.maxLod;
		descriptor.maxAnisotropy=NSUInteger(p.anisotropy);descriptor.supportArgumentBuffers=YES;
		auto result=[lease.Device()newSamplerStateWithDescriptor:descriptor];Require(result!=nil,"Cannot allocate original Blueprint source sampler");
		sourceSamplers.push_back({p,result});lease.Retain(result);return result;
	}

	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,bool target,bool hasNormal,MTLPixelFormat color,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,unsigned samples,bool coverage){
		const uint64_t bits=uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
		auto key=std::tuple{target,hasNormal,unsigned(color),bits,lease.SampleShadingKey(samples),coverage};if(auto it=pipelines.find(key);it!=pipelines.end()){lease.Retain(it->second);return it->second;}
		auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=@"BlockoutVertex";auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=target?@"BlockoutDepthFragment":@"BlockoutFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto constants=[MTLFunctionConstantValues new];
		[constants setConstantValue:&hasNormal type:MTLDataTypeBool atIndex:0];
		auto specialized=[MTL4SpecializedFunctionDescriptor new];
		specialized.functionDescriptor=f;specialized.constantValues=constants;
		d.fragmentFunctionDescriptor=specialized;
		auto attachment=d.colorAttachments[0];attachment.pixelFormat=color;attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);attachment.rgbBlendOperation=Equation(blend.colorEquation);attachment.alphaBlendOperation=Equation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		NSError* error=nil;auto result=lease.NewRenderPipeline(d,&error);Require(result!=nil,"Original Blockout pipeline failed: "+Error(error));pipelines.emplace(key,result);lease.Retain(result);ms+=Milliseconds(start);return result;
	}
};
BlockoutGPU::BlockoutGPU():impl(std::make_unique<Impl>()){}
BlockoutGPU::~BlockoutGPU(){SetNativeBlockoutConsumerAvailable(false);}
void BlockoutGPU::SetNormalImageProvider(NormalImageProvider provider){impl->normals=std::move(provider);}
void BlockoutGPU::ClearResources(){impl->meshes.clear();impl->images.clear();impl->sourceSamplers.clear();}
void BlockoutGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,bool validate,double& ms,unsigned samples){
	if(impl->library&&impl->format==color&&impl->validation==validate)return;
	SetNativeBlockoutConsumerAvailable(false);auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(NativeBlockoutShaderSource);descriptor.name=@"Original31657 Map Blockout feature models";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;descriptor.options=options;NSError* error=nil;
	auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Original Map Blockout shader failed: "+Error(error));impl->library=library;impl->format=color;impl->validation=validate;impl->pipelines.clear();ms+=Milliseconds(start);
	ScreenBlendState blend;const std::array<bool,4> mask{true,true,true,true};
	for(bool hasNormal:{false,true}){impl->Pipeline(lease,false,hasNormal,color,blend,mask,ms,samples,false);impl->Pipeline(lease,true,hasNormal,MTLPixelFormatR8Unorm,blend,mask,ms,1,false);}
	SetNativeBlockoutConsumerAvailable(true);
}
std::shared_ptr<const NativeBlockoutTarget> BlockoutGPU::AllocateTarget(RuntimeLease& lease,uint32_t w,uint32_t h){
	Require(IsNativeBlockoutConsumerAvailable()&&w&&h&&w<=lease.Capabilities().maxTextureDimension2D&&h<=lease.Capabilities().maxTextureDimension2D,"Original Blockout FBO extent exceeds actual device");
	auto storage=std::make_shared<TargetStorage>();
	auto color=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:w height:h mipmapped:NO];color.storageMode=MTLStorageModePrivate;color.usage=MTLTextureUsageRenderTarget;
	auto depth=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];depth.storageMode=MTLStorageModePrivate;depth.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
	storage->color=[lease.Device()newTextureWithDescriptor:color];storage->depthProbe=[lease.Device()newTextureWithDescriptor:depth];
	if(!storage->color||!storage->depthProbe)return {};
	return std::make_shared<NativeBlockoutTarget>(NativeBlockoutTarget{w,h,std::move(storage)});
}
std::unique_ptr<PreparedBlockoutFrame> BlockoutGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeBlockoutFrame> frame,std::span<const BlockoutDepthImage> depthImages,const WorldCamera& camera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat color,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	Require(frame&&impl->normals,"Blockout source frame/normal-image owner absent");Initialize(lease,color,validation,ms,samples);
	auto result=std::unique_ptr<PreparedBlockoutFrame>(new PreparedBlockoutFrame());result->impl->owner=frame;
	auto append=[&](PreparedBlockoutFrame::Impl::Packet& packet,const NativeBlockoutDraw& source,bool target){
		ValidateNativeBlockoutDraw(source);WorldCamera rasterCamera=camera;
		if(target)rasterCamera.viewport={0,0,int32_t(packet.target->width),int32_t(packet.target->height)};
		else if(source.surface.viewport){const auto& p=*source.surface.viewport;rasterCamera.viewport={p.x,p.y,int32_t(p.width),int32_t(p.height)};}
		const unsigned w=target?packet.target->width:width,h=target?packet.target->height:height;
		auto raster=effectstate::Viewport(rasterCamera,w,h,target?w:logicalWidth,target?h:logicalHeight,source.surface.scissor);if(!raster)return;
		std::array<std::shared_ptr<Image>,2> material;std::array<__strong id<MTLSamplerState>,2> samplers{};
		for(unsigned n=0;n<2;++n){auto pixels=NativeS3OTexturePixels(*source.textures,n);auto& image=impl->images[pixels];if(!image)image=Upload(lease,pixels);material[n]=image;samplers[n]=impl->Sampler(lease,*source.textures->objects[n]);lease.AddAllocation(image->texture);result->impl->images.push_back(image);if(!image->initialized&&std::find(result->impl->pending.begin(),result->impl->pending.end(),image)==result->impl->pending.end())result->impl->pending.push_back(image);}
		PreparedScreenImage normal;if(source.hasNormal){normal=impl->normals(lease,source.normal,source.normalSampler);Require(normal.texture&&normal.sampler,"Blockout normal source image/sampler absent");lease.AddAllocation(normal.texture);lease.Retain(normal.sampler);}
		for(const auto& item:source.models){
			auto& mesh=impl->meshes[item.model];if(!mesh)mesh=Geometry(lease,item.model);if(!mesh->count)continue;lease.AddAllocation(mesh->vertices);lease.AddAllocation(mesh->indices);
			Uniform u{};std::memcpy(&u.viewProjection,source.viewProjection.data(),64);std::memcpy(&u.positionYaw,item.positionYaw.data(),16);std::memcpy(&u.parameters,item.parameters.data(),16);
			std::memcpy(&u.sunDirection,source.uniforms->params->bytes.data()+7*16,16);std::memcpy(&u.ambient,source.uniforms->params->bytes.data()+8*16,16);std::memcpy(&u.diffuse,source.uniforms->params->bytes.data()+10*16,16);
			u.options=simd_make_uint4(source.clipDepth==ClipDepthConvention::MinusOneToOne,material[0]->owner->flipV,material[1]->owner->flipV,(source.hasNormal?256u:0u)|(source.normal&&source.normal->flipV?1u:0u));
			auto transforms=Buffer(lease,item.transforms.data(),item.transforms.size()*sizeof(NativeBlueprintTransform));auto uniforms=Buffer(lease,&u,sizeof(u));
			auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;descriptor.maxTextureBindCount=3;descriptor.maxSamplerStateBindCount=3;descriptor.initializeBindings=YES;NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Blockout argument allocation failed: "+Error(error));lease.Retain(arguments);
			[arguments setAddress:mesh->vertices.gpuAddress atIndex:0];[arguments setAddress:transforms.gpuAddress atIndex:1];[arguments setAddress:uniforms.gpuAddress atIndex:2];
			for(unsigned n=0;n<2;++n){[arguments setTexture:material[n]->texture.gpuResourceID atIndex:n];[arguments setSamplerState:samplers[n].gpuResourceID atIndex:n];}
			if(source.hasNormal){[arguments setTexture:normal.texture.gpuResourceID atIndex:2];[arguments setSamplerState:normal.sampler.gpuResourceID atIndex:2];}
			auto pipeline=impl->Pipeline(lease,target,source.hasNormal,target?MTLPixelFormatR8Unorm:color,source.surface.blend,source.world.colorWrite,ms,target?1:samples,source.world.alphaToCoverage);
			packet.draws.push_back({pipeline,effectstate::Depth(lease,source.world,target?MTLPixelFormatInvalid:stencil),arguments,mesh,source.world,*raster});
		}
	};
	for(const auto& draw:frame->draws){PreparedBlockoutFrame::Impl::Packet packet;packet.sequence=draw.sequence;append(packet,draw,false);if(!packet.draws.empty())result->impl->packets.push_back(std::move(packet));}
	for(const auto& source:frame->depthPasses){
		ValidateNativeBlockoutDepthPass(source);auto found=std::find_if(depthImages.begin(),depthImages.end(),[&](const auto& image){return image.version==source.output;});
		Require(found!=depthImages.end()&&found->depth&&found->depth.pixelFormat==MTLPixelFormatDepth32Float&&found->depth.width==source.target->width&&found->depth.height==source.target->height,"Blockout actual shared depth output storage differs");
		PreparedBlockoutFrame::Impl::Packet packet;packet.sequence=source.sequence;packet.target=source.target;packet.output=source.output;packet.depth=found->depth;packet.clear=source.clearDepth;
		auto storage=std::static_pointer_cast<const TargetStorage>(source.target->storage);Require(storage&&storage->color.device==lease.Device(),"Blockout FBO belongs to another actual device");lease.AddAllocation(storage->color);lease.AddAllocation(found->depth);
		for(const auto& draw:source.draws)append(packet,draw,true);result->impl->packets.push_back(std::move(packet));
	}
	std::sort(result->impl->packets.begin(),result->impl->packets.end(),[](const auto& x,const auto& y){return x.sequence<y.sequence;});return result;
}
}
