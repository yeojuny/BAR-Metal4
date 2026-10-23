/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Blueprint.h"
#include "Metal4WorldBCDecode.h"
#include "NativeBlueprintShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
namespace rendering::metal4 {
namespace {
struct ModelVertex {std::array<float,3> position;std::array<float,2> uv;uint32_t bone;};
static_assert(sizeof(ModelVertex)==24);
struct ModelUniform {simd_float4x4 viewProjection;simd_float4 positionRotation,parameters,team,cameraIcon,time;simd_uint4 options;simd_float4 alpha,pitchRoll,tint;};
static_assert(sizeof(ModelUniform)==208);
struct RasterUniform {simd_uint4 options;simd_float4 alpha;};
struct Mesh {std::shared_ptr<const WorldModel> owner;__strong id<MTLBuffer> vertices=nil,indices=nil;NSUInteger count=0;};
struct Image {std::shared_ptr<const WorldTexture> owner;__strong id<MTLTexture> texture=nil;bool initialized=false;};
struct SourceSampler {NativeTextureSampling source;__strong id<MTLSamplerState> sampler=nil;};
MTLSamplerAddressMode Address(unsigned value) {
	switch(value) {
		case 0x2901:return MTLSamplerAddressModeRepeat;
		case 0x8370:return MTLSamplerAddressModeMirrorRepeat;
		case 0x812f:return MTLSamplerAddressModeClampToEdge;
		case 0x812d:return MTLSamplerAddressModeClampToBorderColor;
	}
	throw std::runtime_error("Original Blueprint texture wrap requires its actual coordinate consumer");
}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* data,size_t size) {
	Require(size&&size<=lease.Device().maxBufferLength,"Original Blueprint GPU buffer extent invalid");auto result=[lease.Device()newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];Require(result!=nil,"Cannot allocate original Blueprint GPU buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value) {
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown original Blueprint blend equation");
}
MTLPixelFormat Format(WorldTextureFormat value) {
	switch(value){case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;}throw std::runtime_error("Unknown original Blueprint image format");
}
std::shared_ptr<Image> Upload(RuntimeLease& lease,std::shared_ptr<const WorldTexture> owner) {
	Require(owner&&!owner->mips.empty()&&owner->mips.size()<=16,"Original Blueprint image mip count invalid");
	auto width=owner->mips[0].width,height=owner->mips[0].height;Require(width&&height&&width<=lease.Capabilities().maxTextureDimension2D&&height<=lease.Capabilities().maxTextureDimension2D,"Original Blueprint image exceeds actual texture dimensions");
	unsigned levels=1;for(auto extent=std::max(width,height);extent>1;extent>>=1)++levels;Require(owner->mips.size()<=levels,"Original Blueprint image has excess mip levels");
	const auto decoded=DecodeWorldBCBaseForMipmaps(*owner,lease.Capabilities().maxTextureDimension2D);
	const auto storageFormat=decoded?WorldTextureFormat::RGBA8Unorm:owner->format;
	const bool compressed=storageFormat!=WorldTextureFormat::RGBA8Unorm&&storageFormat!=WorldTextureFormat::BGRA8Unorm;
	Require(!compressed||lease.Device().supportsBCTextureCompression,"Original Blueprint BC image unsupported on actual device");
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(decoded && owner->format == WorldTextureFormat::BC7RGBASRGB ? MTLPixelFormatRGBA8Unorm_sRGB : Format(storageFormat)) width:width height:height mipmapped:YES];descriptor.mipmapLevelCount=owner->generateMipmaps?levels:owner->mips.size();descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageShaderRead;
	auto image=std::make_shared<Image>();image->texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(image->texture!=nil,"Cannot allocate original Blueprint material image");image->texture.label=String(owner->sourcePath);
	for(size_t level=0;level<owner->mips.size();++level) {
		const auto& mip=decoded?*decoded:owner->mips[level];const size_t rows=compressed?(size_t(height)+3)/4:height;
		const size_t rowBytes=compressed?((size_t(width)+3)/4)*((storageFormat==WorldTextureFormat::BC1RGBA||storageFormat==WorldTextureFormat::BC4Unorm)?8:16):size_t(width)*4;
		Require(mip.width==width&&mip.height==height&&mip.bytesPerRow>=rowBytes&&mip.bytesPerRow<=128*1024&&mip.bytes.size()>=mip.bytesPerRow*rows,"Original Blueprint image mip bytes differ");
		[image->texture replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:level withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];width=std::max(1u,width/2);height=std::max(1u,height/2);
	}
	// Upload scratch may now retire; owner, generated-level dependency and
	// completion stay attached to the original immutable source and GPU image.
	image->initialized=!owner->generateMipmaps||image->texture.mipmapLevelCount==1;image->owner=std::move(owner);return image;
}
std::shared_ptr<Mesh> Upload(RuntimeLease& lease,std::shared_ptr<const WorldModel> owner) {
	std::vector<ModelVertex> vertices;std::vector<uint32_t> indices;
	for(const auto& piece:owner->pieces) {
		Require(vertices.size()+piece.vertices.size()<=UINT32_MAX,"Original Blueprint combined model index overflow");const auto base=uint32_t(vertices.size());
		for(const auto& vertex:piece.vertices)vertices.push_back({vertex.position,vertex.uv[0],uint32_t(vertex.boneIds[0]&255)});
		for(auto index:piece.indices)indices.push_back(base+index);
	}
	auto mesh=std::make_shared<Mesh>();mesh->owner=std::move(owner);mesh->count=indices.size();
	if(mesh->count){mesh->vertices=Buffer(lease,vertices.data(),vertices.size()*sizeof(ModelVertex));mesh->indices=Buffer(lease,indices.data(),indices.size()*sizeof(uint32_t));}
	return mesh;
}
template<class T> T Read(const NativeBufferSnapshot& buffer,size_t row,size_t offset) {T value{};std::memcpy(&value,buffer.bytes.data()+row*buffer.layout.stride+offset,sizeof(T));return value;}
NativeWorldRasterMesh Outline(const NativeBlueprintOutlineDraw& draw,const effectstate::Raster& raster,uint32_t width,uint32_t height,unsigned samples) {
	NativeWorldRasterMesh result;auto surface=draw.surface;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};
	const auto& m=draw.view.viewProjection;const std::array<double,4> viewport{raster.viewport.originX,raster.viewport.originY,raster.viewport.width,raster.viewport.height};
	for(size_t row=draw.firstInstance;row<size_t(draw.firstInstance)+draw.instanceCount;++row) {
		const auto position=Read<std::array<float,3>>(*draw.instances,row,0);const auto dimensions=Read<std::array<float,2>>(*draw.instances,row,12);const auto color=Read<std::array<float,4>>(*draw.instances,row,20);
		const float y=NativeBlueprintHeightLOD0(*draw.heights,NativeBlueprintHeightUV({position[0],position[2]},draw.mapWorldSize));std::array<NativeWorldDrawVertex,5> corners;
		for(unsigned n=0;n<5;++n) {
			const auto local=Read<std::array<float,2>>(*draw.vertices,n,0);const std::array<float,4> world{position[0]+local[0]*dimensions[0],y,position[2]+local[1]*dimensions[1],1};auto& vertex=corners[n];vertex={};vertex.color=color;
			for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)vertex.position[r]+=m[c*4+r]*world[c];
			if(draw.view.clipDepth==ClipDepthConvention::ZeroToOne)vertex.position[2]=2*vertex.position[2]-vertex.position[3];
		}
		auto command=MakeNativeWorldDrawCommand(draw.stage,NativeDrawPrimitive::LineStrip,corners,surface,draw.world);auto mesh=BuildNativeWorldDrawGeometry(command,viewport,width,height,samples);
		result.fullViewport=result.fullViewport||mesh.fullViewport;
		Require(result.vertices.size()+mesh.vertices.size()<=UINT32_MAX,"Original Blueprint line geometry overflow");const auto base=uint32_t(result.vertices.size());result.vertices.insert(result.vertices.end(),mesh.vertices.begin(),mesh.vertices.end());for(auto index:mesh.indices)result.indices.push_back(base+index);
	}
	return result;
}
} // namespace
struct PreparedBlueprintFrame::Impl {
	struct Draw {
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
		NativeWorldDrawState state;effectstate::Raster raster;NSUInteger count=0;bool clear=false,model=false;
	};
	struct Packet {
		uint64_t sequence;NativeWorldDrawStage stage;std::vector<Draw> draws;
		// Keep both the source object versions and their uploaded images alive
		// through retirement, independently of later bind/reload/cache changes.
		std::shared_ptr<const NativeS3OTexturePair> modelTextures{};
		std::array<std::shared_ptr<Image>,2> images{};
	};
	std::vector<Packet> packets;std::vector<std::shared_ptr<Image>> pending;
};
PreparedBlueprintFrame::PreparedBlueprintFrame():impl(std::make_unique<Impl>()){}
PreparedBlueprintFrame::~PreparedBlueprintFrame()=default;
size_t PreparedBlueprintFrame::DrawCount()const{return impl->packets.size();}
uint64_t PreparedBlueprintFrame::SequenceAt(size_t n)const{return impl->packets.at(n).sequence;}
NativeWorldDrawStage PreparedBlueprintFrame::StageAt(size_t n)const{return impl->packets.at(n).stage;}
void PreparedBlueprintFrame::EncodeDependencies(id<MTL4CommandBuffer> command)const {
	if(impl->pending.empty())return;auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode original Blueprint material mips");
	for(const auto& image:impl->pending)[encoder generateMipmapsForTexture:image->texture];[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
}
void PreparedBlueprintFrame::DidComplete()const{for(const auto& image:impl->pending)image->initialized=true;}
void PreparedBlueprintFrame::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder)const {
	for(const auto& draw:impl->packets.at(n).draws) {
		[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];effectstate::RasterState(encoder,draw.state,draw.raster);
		if(draw.model)[encoder setCullMode:MTLCullModeBack];
		[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		if(draw.clear)[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		else [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];
	}
}
struct BlueprintGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	std::map<std::tuple<unsigned,uint64_t,unsigned,bool>,id<MTLRenderPipelineState>> pipelines;
	std::map<std::shared_ptr<const WorldModel>,std::shared_ptr<Mesh>,std::owner_less<std::shared_ptr<const WorldModel>>> meshes;
	std::map<std::shared_ptr<const WorldTexture>,std::shared_ptr<Image>,std::owner_less<std::shared_ptr<const WorldTexture>>> images;
	std::array<__strong id<MTLSamplerState>,2> samplers{};
	std::vector<SourceSampler> sourceSamplers;
	MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
	id<MTLSamplerState> Sampler(RuntimeLease& lease,const NativeCUSTextureObject& object) {
		const auto& p=object.sampling;
		Require(p.compareMode==0&&p.baseLevel==0&&p.maxLevel>=int(object.allocation.levels)-1&&p.lodBias==0,
			"Original Blueprint model sampler comparison/base/max/bias requires its actual consumer");
		Require(std::isfinite(p.minLod)&&std::isfinite(p.maxLod)&&p.maxLod>=0&&p.minLod<=p.maxLod&&
			p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,
			"Original Blueprint model sampler LOD/anisotropy differs");
		Require(p.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},
			"Original Blueprint model texture swizzle requires its actual consumer");
		Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{1,1,1,1},
			"Original Blueprint model texture custom border requires its actual consumer");
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
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,unsigned kind,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,unsigned samples,bool coverage) {
		const uint64_t state=uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
		const auto key=std::tuple{kind,state,lease.SampleShadingKey(samples),coverage};if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		const auto start=Clock::now();constexpr const char* names[]{"BlueprintModel","BlueprintRaster","BlueprintStencilClear","BlueprintFeature"};auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=String(std::string(names[kind])+"Vertex");auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=String(std::string(names[kind])+"Fragment");d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto a=d.colorAttachments[0];a.pixelFormat=format;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);
		a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		NSError* error=nil;auto result=lease.NewRenderPipeline(d,&error);Require(result!=nil,"Cannot compile original Blueprint pipeline: "+Error(error));pipelines.emplace(key,result);ms+=Milliseconds(start);return result;
	}
	id<MTL4ArgumentTable> Arguments(RuntimeLease& lease,unsigned buffers,unsigned imagesCount,unsigned samplerCount) {
		auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=imagesCount;d.maxSamplerStateBindCount=samplerCount;d.initializeBindings=YES;NSError* error=nil;auto result=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(result!=nil,"Cannot allocate original Blueprint argument table: "+Error(error));lease.Retain(result);return result;
	}
};
BlueprintGPU::BlueprintGPU():impl(std::make_unique<Impl>()){}
BlueprintGPU::~BlueprintGPU(){if(impl->library!=nil)SetNativeBlueprintConsumerAvailable(false);}
void BlueprintGPU::ClearResources(){impl->meshes.clear();impl->images.clear();impl->sourceSamplers.clear();}
void BlueprintGPU::Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& milliseconds,unsigned samples) {
	if(impl->library!=nil&&impl->format==colorFormat&&impl->validation==validation)return;
	SetNativeBlueprintConsumerAvailable(false);const auto start=Clock::now();auto d=[MTL4LibraryDescriptor new];d.source=String(NativeBlueprintShaderSource);d.name=@"Original exact30770 Blueprint models, outlines and stencil clear";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;d.options=options;NSError* error=nil;
	auto library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original Blueprint Metal library: "+Error(error));
	impl->library=library;impl->format=colorFormat;impl->validation=validation;impl->pipelines.clear();milliseconds+=Milliseconds(start);
	ScreenBlendState source;impl->Pipeline(lease,0,source,{true,true,true,true},milliseconds,samples,false);impl->Pipeline(lease,1,source,{true,true,true,true},milliseconds,samples,false);impl->Pipeline(lease,3,source,{true,true,true,true},milliseconds,samples,false);source.enabled=false;impl->Pipeline(lease,2,source,{false,false,false,false},milliseconds,samples,false);
	for(unsigned mip=0;mip<2;++mip){auto sampler=[MTLSamplerDescriptor new];sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;sampler.mipFilter=mip?MTLSamplerMipFilterLinear:MTLSamplerMipFilterNotMipmapped;sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeRepeat;sampler.supportArgumentBuffers=YES;impl->samplers[mip]=[lease.Device()newSamplerStateWithDescriptor:sampler];Require(impl->samplers[mip]!=nil,"Cannot allocate original Blueprint material sampler");}
	SetNativeBlueprintConsumerAvailable(true);
}
std::unique_ptr<PreparedBlueprintFrame> BlueprintGPU::Prepare(RuntimeLease& lease,const NativeBlueprintFrame& frame,const WorldCamera& viewCamera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& milliseconds,unsigned samples) {
	Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original Blueprint needs actual shared world depth");Require(stencilFormat==MTLPixelFormatStencil8||stencilFormat==MTLPixelFormatDepth32Float_Stencil8,"Original Blueprint needs actual shared stencil attachment");
	for(const auto& draw:frame.outlines)ValidateNativeBlueprintOutline(draw);for(const auto& draw:frame.models)ValidateNativeBlueprintModel(draw);for(const auto& clear:frame.clears)ValidateNativeWorldStencilClear(clear);
	Initialize(lease,colorFormat,validation,milliseconds,samples);lease.Retain(impl->library);for(auto sampler:impl->samplers)lease.Retain(sampler);
	auto result=std::unique_ptr<PreparedBlueprintFrame>(new PreparedBlueprintFrame);
	for(const auto& source:frame.models) {
		auto raster=effectstate::Viewport(viewCamera,width,height,logicalWidth,logicalHeight,source.surface.scissor);if(!raster)continue;
		PreparedBlueprintFrame::Impl::Packet packet{source.sequence,source.stage,{}};
		packet.modelTextures=source.modelTextures;
		std::array<std::shared_ptr<Image>,2> images;
		std::array<__strong id<MTLSamplerState>,2> imageSamplers{};
		for(unsigned n=0;n<2;++n) {
			const auto pixels=source.modelTextures?NativeS3OTexturePixels(*source.modelTextures,n):source.textures[n];
			auto& image=impl->images[pixels];if(!image)image=Upload(lease,pixels);
			images[n]=image;lease.AddAllocation(image->texture);
			imageSamplers[n]=source.modelTextures?impl->Sampler(lease,*source.modelTextures->objects[n]):impl->samplers[pixels->mipmappedSampling];
			if(!image->initialized&&std::find(result->impl->pending.begin(),result->impl->pending.end(),image)==result->impl->pending.end())result->impl->pending.push_back(image);
		}
		packet.images=images;
		for(const auto& item:source.submissions) {
			auto& mesh=impl->meshes[item.model];if(!mesh)mesh=Upload(lease,item.model);if(!mesh->count)continue;lease.AddAllocation(mesh->vertices);lease.AddAllocation(mesh->indices);
			ModelUniform uniform{};std::memcpy(&uniform.viewProjection,source.view.viewProjection.data(),64);uniform.positionRotation=Read<simd_float4>(*source.instances,item.instanceIndex,0);uniform.parameters=Read<simd_float4>(*source.instances,item.instanceIndex,16);std::memcpy(&uniform.team,item.selectedTeamColor.data(),16);
   if(source.program==NativeBlueprintProgram::FeatureShape){uniform.pitchRoll=Read<simd_float4>(*source.instances,item.instanceIndex,16);uniform.parameters=Read<simd_float4>(*source.instances,item.instanceIndex,32);uniform.tint=Read<simd_float4>(*source.instances,item.instanceIndex,48);}
			uniform.cameraIcon=simd_make_float4(source.view.cameraPosition[0],source.view.cameraPosition[1],source.view.cameraPosition[2],source.iconDistance);std::memcpy(&uniform.time,source.view.timeInfo.data(),16);
			uniform.options=simd_make_uint4(source.view.clipDepth==ClipDepthConvention::MinusOneToOne,images[0]->owner->flipV,images[1]->owner->flipV,(unsigned(source.surface.alphaTest.enabled)<<8)|unsigned(source.surface.alphaTest.compare));uniform.alpha=simd_make_float4(source.surface.alphaTest.reference,0,0,0);
			auto transform=Buffer(lease,item.transforms.data(),item.transforms.size()*sizeof(NativeBlueprintTransform));auto uniforms=Buffer(lease,&uniform,sizeof(uniform));auto args=impl->Arguments(lease,3,2,2);[args setAddress:mesh->vertices.gpuAddress atIndex:0];[args setAddress:transform.gpuAddress atIndex:1];[args setAddress:uniforms.gpuAddress atIndex:2];
			for(unsigned n=0;n<2;++n){[args setTexture:images[n]->texture.gpuResourceID atIndex:n];[args setSamplerState:imageSamplers[n].gpuResourceID atIndex:n];}
			auto pipeline=impl->Pipeline(lease,source.program==NativeBlueprintProgram::FeatureShape?3u:0u,source.surface.blend,source.world.colorWrite,milliseconds,samples,source.world.alphaToCoverage);lease.Retain(pipeline);
			packet.draws.push_back({pipeline,effectstate::Depth(lease,source.world,stencilFormat),args,mesh->indices,source.world,*raster,mesh->count,false,true});
		}
		if(!packet.draws.empty())result->impl->packets.push_back(std::move(packet));
	}
	for(const auto& source:frame.outlines) {
		auto raster=effectstate::Viewport(viewCamera,width,height,logicalWidth,logicalHeight,source.surface.scissor);if(!raster)continue;auto mesh=Outline(source,*raster,width,height,samples);if(mesh.indices.empty())continue;if(mesh.fullViewport)raster->viewport={0,0,double(width),double(height),0,1};
		auto vertices=Buffer(lease,mesh.vertices.data(),mesh.vertices.size()*sizeof(NativeWorldDrawVertex));auto indices=Buffer(lease,mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));
		RasterUniform uniform{simd_make_uint4(source.surface.alphaTest.enabled,unsigned(source.surface.alphaTest.compare),0,0),simd_make_float4(source.surface.alphaTest.reference,0,0,0)};auto uniforms=Buffer(lease,&uniform,sizeof(uniform));auto args=impl->Arguments(lease,2,0,0);[args setAddress:vertices.gpuAddress atIndex:0];[args setAddress:uniforms.gpuAddress atIndex:1];auto pipeline=impl->Pipeline(lease,1,source.surface.blend,source.world.colorWrite,milliseconds,samples,source.world.alphaToCoverage);lease.Retain(pipeline);
		auto state=source.world;state.cullEnabled=false;state.polygonOffset=false;
		PreparedBlueprintFrame::Impl::Draw draw{pipeline,effectstate::Depth(lease,state,stencilFormat),args,indices,state,*raster,mesh.indices.size(),false,false};result->impl->packets.push_back({source.sequence,source.stage,{std::move(draw)}});
	}
	for(const auto& source:frame.clears) {
		WorldCamera wholeFramebuffer;wholeFramebuffer.viewport={0,0,int32_t(logicalWidth),int32_t(logicalHeight)};auto raster=effectstate::Viewport(wholeFramebuffer,width,height,logicalWidth,logicalHeight,source.scissor);if(!raster)continue;
		NativeWorldDrawState state;state.stencilTest=true;state.depthTest=false;state.depthWrite=false;state.colorWrite={false,false,false,false};
		for(auto* face:{&state.frontStencil,&state.backStencil}){face->compare=ScreenAlphaCompare::Always;face->reference=source.value;face->writeMask=source.writeMask;face->readMask=0xff;face->pass=NativeWorldStencilOp::Replace;}
		ScreenBlendState blend;blend.enabled=false;auto pipeline=impl->Pipeline(lease,2,blend,state.colorWrite,milliseconds,samples,false);lease.Retain(pipeline);auto args=impl->Arguments(lease,0,0,0);
		PreparedBlueprintFrame::Impl::Draw draw{pipeline,effectstate::Depth(lease,state,stencilFormat),args,nil,state,*raster,3,true,false};result->impl->packets.push_back({source.sequence,source.stage,{std::move(draw)}});
	}
	std::sort(result->impl->packets.begin(),result->impl->packets.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
	for(size_t n=1;n<result->impl->packets.size();++n)Require(result->impl->packets[n-1].sequence<result->impl->packets[n].sequence,"Original Blueprint world sequence duplicated");
	return result;
}
} // namespace rendering::metal4
