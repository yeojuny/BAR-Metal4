/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4GameDecal.h"
#include "NativeGameDecalShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <cstring>
#include <map>
#include <tuple>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 viewProjection,viewInverse,shadowView;
	simd_float4 time,mapSize,sunDirection,sunAmbient,sunDiffuse;
	simd_float4 waterAbsorb,waterMinimum,waterBase;
	simd_uint4 options;
	simd_float4 alpha;
};
static_assert(sizeof(Uniform)==352);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,std::size_t size)
{
	Require(size && size<=lease.Device().maxBufferLength,"Original game decal buffer exceeds actual device capacity");
	auto buffer=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate original game decal immutable buffer");lease.AddAllocation(buffer);return buffer;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Unknown original game decal blend equation");
}
MTLSamplerAddressMode Address(ScreenWrap value)
{
	switch(value){case ScreenWrap::ClampEdge:return MTLSamplerAddressModeClampToEdge;case ScreenWrap::TransparentBorder:return MTLSamplerAddressModeClampToBorderColor;case ScreenWrap::Repeat:return MTLSamplerAddressModeRepeat;case ScreenWrap::LegacyClamp:break;}
	throw std::runtime_error("Original game decal scene image does not use GL_CLAMP");
}
std::uint64_t PipelineBits(const ScreenBlendState& blend,const std::array<bool,4>& mask)
{
	return uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|
		uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|
		uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
}
}
struct PreparedGameDecal::Impl {
	struct Draw {
		std::uint64_t sequence=0;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		MTLIndexType indexType=MTLIndexTypeUInt32;
		std::uint32_t count=0,first=0,instances=0;
		std::size_t indexStride=0;
		effectstate::Raster raster;
		NativeWorldDrawState state;
	};
	std::vector<Draw> draws;
	std::shared_ptr<const NativeCUSUniformPublication> originalUniforms;
};
PreparedGameDecal::PreparedGameDecal():impl(std::make_unique<Impl>()){}
PreparedGameDecal::~PreparedGameDecal()=default;
std::size_t PreparedGameDecal::DrawCount()const{return impl->draws.size();}
std::uint64_t PreparedGameDecal::SequenceAt(std::size_t index)const{return impl->draws.at(index).sequence;}
void PreparedGameDecal::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder)const
{
	const auto& draw=impl->draws.at(index);effectstate::RasterState(encoder,draw.state,draw.raster);
	const auto cull=draw.state.cullEnabled?draw.state.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	if(draw.indices){
		const auto offset=std::size_t(draw.first)*draw.indexStride;
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:draw.indexType indexBuffer:draw.indices.gpuAddress+offset indexBufferLength:draw.indices.length-offset instanceCount:draw.instances];
	}else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:96 instanceCount:draw.instances];
}
struct GameDecalGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	using Key=std::tuple<unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,std::uint64_t,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct CachedBuffer{std::weak_ptr<const NativeBufferSnapshot> owner;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	struct CachedSampler{ScreenSampler source;bool shadow=false;__strong id<MTLSamplerState> sampler=nil;};
	std::vector<CachedSampler> samplers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source,std::size_t used)
	{
		Require(used && used<=source->bytes.size(),"Original game decal GPU read exceeds immutable buffer");
		auto& entry=buffers[source.get()];
		if(entry.owner.expired() || entry.buffer.length<used){auto next=Upload(lease,source->bytes.data(),used);entry={source,next};}
		else lease.AddAllocation(entry.buffer);
		return entry.buffer;
	}
	id<MTLSamplerState> Sampler(RuntimeLease& lease,const ScreenSampler& source,bool shadow)
	{
		ValidateScreenSampler(source);
		for(const auto& entry:samplers)if(entry.source==source && entry.shadow==shadow){lease.Retain(entry.sampler);return entry.sampler;}
		auto d=[MTLSamplerDescriptor new];d.minFilter=source.filter==ScreenFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
		d.magFilter=source.magnificationFilter.value_or(source.filter)==ScreenFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
		d.mipFilter=source.mipFilter==ScreenMipFilter::None?MTLSamplerMipFilterNotMipmapped:source.mipFilter==ScreenMipFilter::Nearest?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
		d.sAddressMode=Address(source.wrapS);d.tAddressMode=Address(source.wrapT);d.maxAnisotropy=source.anisotropy;
		d.borderColor=source.whiteBorder?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorTransparentBlack;d.supportArgumentBuffers=YES;
		if(shadow)d.compareFunction=MTLCompareFunctionLessEqual;
		auto sampler=[lease.Device()newSamplerStateWithDescriptor:d];Require(sampler!=nil,"Cannot create original game decal scene sampler");
		samplers.push_back({source,shadow,sampler});lease.Retain(sampler);return sampler;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,NativeGameDecalProgram program,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,bool coverage=false)
	{
		const Key key{unsigned(program),color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(blend,mask),coverage};if(auto it=pipelines.find(key);it!=pipelines.end())return it->second;
		const auto start=Clock::now();NSError* error=nil;
		if(!library){
			auto d=[MTL4LibraryDescriptor new];d.name=@"Original test30770 game Decals GL4 complete program family";d.source=String(NativeGameDecalShaderSource);
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;
			library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original game decal program family: "+Error(error));
		}
		auto d=[MTL4RenderPipelineDescriptor new];auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		vertex.name=program==NativeGameDecalProgram::Geometry?@"GameDecalGeometryVertex":@"GameDecalMeshVertex";fragment.name=@"GameDecalFragment";
		d.vertexFunctionDescriptor=vertex;d.fragmentFunctionDescriptor=fragment;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);
		a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original game decal actual render pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);ms+=Milliseconds(start);return pipeline;
	}
};
GameDecalGPU::GameDecalGPU():impl(std::make_unique<Impl>()){}
GameDecalGPU::~GameDecalGPU()=default;
void GameDecalGPU::ClearResources(){impl->buffers.clear();impl->samplers.clear();}
void GameDecalGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds)
{
	ScreenBlendState blend;
	for(auto program:{NativeGameDecalProgram::Geometry,NativeGameDecalProgram::LargeMesh})
		impl->Pipeline(lease,program,color,depth,stencil,samples,validation,blend,{true,true,true,true},milliseconds);
}
std::unique_ptr<PreparedGameDecal> GameDecalGPU::Prepare(RuntimeLease& lease,const NativeGameDecalFrame& frame,const GameDecalSceneImages& scene,
	const WorldCamera& originalCamera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,ScreenGPU& sharedImages,const PreparedScreenFrame* producers,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds)
{
	if(frame.draws.empty())return {};
	const auto uniforms=ReadNativeGameDecalUniforms(frame.uniforms);
	Require(scene.sourceDrawFrame==frame.uniforms.sourceDrawFrame,"Original game decal scene images differ from actual UBO draw");
	Require(depth==MTLPixelFormatDepth32Float || depth==MTLPixelFormatDepth32Float_Stencil8,"Original game decal draw requires actual terrain/main depth");
	Require(scene.infoOwner && scene.requestedInfo && scene.infoOwner->MatchesFrame(*scene.requestedInfo) && scene.images[2].texture==scene.infoOwner->Info(),"Original game decal current-info owner/generation differs");
	Require(scene.images[0].texture.pixelFormat==MTLPixelFormatR32Float && scene.images[4].texture.pixelFormat==MTLPixelFormatRG16Float,"Original game decal height/normal producer format differs");
	Require(scene.images[3].texture.pixelFormat==MTLPixelFormatDepth32Float,"Original game decal real shadow depth is missing");
	for(const auto& image:scene.images){Require(image.texture!=nil,"Original game decal engine image is missing");lease.AddAllocation(image.texture);}
	Initialize(lease,color,depth,stencil,samples,validation,milliseconds);lease.Retain(impl->library);
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});
	auto result=std::unique_ptr<PreparedGameDecal>(new PreparedGameDecal);
	result->impl->originalUniforms=frame.uniforms.original;
	for(const auto& draw:frame.draws){
		ValidateNativeGameDecalDraw(draw);if(!draw.count || !draw.instanceCount || (draw.world.cullEnabled && draw.world.cull==NativeWorldCull::FrontAndBack))continue;
		Require(draw.world.frontPolygonMode==NativePolygonMode::Fill && draw.world.backPolygonMode==NativePolygonMode::Fill,"Original game decal inherited non-fill polygon mode requires its raster consumer");
		Require(!draw.world.inheritedClipPlane,"Original game decal main stage cannot inherit water user-plane state");
		WorldCamera rasterCamera=originalCamera;if(draw.surface.viewport){const auto& v=*draw.surface.viewport;rasterCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto scissor=draw.surface.scissor;
		if(scissor && !scissor->windowCoordinates){scissor->x+=originalCamera.viewport[0];scissor->y+=originalCamera.viewport[1];scissor->windowCoordinates=true;}
		auto raster=effectstate::Viewport(rasterCamera,width,height,logicalWidth,logicalHeight,scissor);if(!raster)continue;
		const bool small=draw.program==NativeGameDecalProgram::Geometry;
		auto vertices=impl->Buffer(lease,draw.vertices,small?std::size_t(draw.first+draw.count)*80:draw.vertices->bytes.size());
		auto instances=small?nil:impl->Buffer(lease,draw.instances,std::size_t(draw.firstInstance+draw.instanceCount)*80);
		auto indices=small?nil:impl->Buffer(lease,draw.indices,std::size_t(draw.first+draw.count)*draw.indices->layout.stride);
		Uniform u{};std::memcpy(&u.viewProjection,uniforms.cameraViewProjection.data(),64);std::memcpy(&u.viewInverse,uniforms.cameraViewInverse.data(),64);std::memcpy(&u.shadowView,uniforms.shadowView.data(),64);
		std::memcpy(&u.time,uniforms.timeInfo.data(),16);std::memcpy(&u.mapSize,uniforms.mapSize.data(),16);std::memcpy(&u.sunDirection,uniforms.sunDirection.data(),16);std::memcpy(&u.sunAmbient,uniforms.sunAmbient.data(),16);std::memcpy(&u.sunDiffuse,uniforms.sunDiffuse.data(),16);
		std::memcpy(&u.waterAbsorb,draw.water.absorb.data(),12);std::memcpy(&u.waterMinimum,draw.water.minimum.data(),12);std::memcpy(&u.waterBase,draw.water.base.data(),12);
		unsigned flips=0;for(unsigned n=0;n<5;++n)flips|=unsigned(scene.images[n].flipV)<<n;for(unsigned n=0;n<2;++n)flips|=unsigned(draw.atlases[n].image->flipV)<<(n+5);
		u.options=simd_make_uint4(uniforms.clipDepth==ClipDepthConvention::MinusOneToOne,flips,small?draw.first:draw.firstInstance,(unsigned(draw.surface.alphaTest.enabled)<<8)|unsigned(draw.surface.alphaTest.compare));u.alpha.x=draw.surface.alphaTest.reference;
		auto constants=Upload(lease,&u,sizeof(u));auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;descriptor.maxTextureBindCount=7;descriptor.maxSamplerStateBindCount=7;descriptor.initializeBindings=YES;
		NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Cannot allocate original game decal argument table: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0];if(instances)[arguments setAddress:instances.gpuAddress atIndex:1];[arguments setAddress:constants.gpuAddress atIndex:2];
		for(unsigned n=0;n<5;++n){auto sampler=impl->Sampler(lease,scene.images[n].sampler,n==3);[arguments setTexture:scene.images[n].texture.gpuResourceID atIndex:n];[arguments setSamplerState:sampler.gpuResourceID atIndex:n];}
		for(unsigned n=0;n<2;++n){auto image=sharedImages.PrepareImage(lease,draw.atlases[n].image,draw.atlases[n].sampler,producers);[arguments setTexture:image.texture.gpuResourceID atIndex:n+5];[arguments setSamplerState:image.sampler.gpuResourceID atIndex:n+5];}
		auto pipeline=impl->Pipeline(lease,draw.program,color,depth,stencil,samples,validation,draw.surface.blend,draw.world.colorWrite,milliseconds,draw.world.alphaToCoverage);lease.Retain(pipeline);
		PreparedGameDecal::Impl::Draw packet;packet.sequence=draw.sequence;packet.pipeline=pipeline;packet.depth=effectstate::Depth(lease,draw.world,stencil);packet.arguments=arguments;packet.indices=indices;packet.count=draw.count;packet.first=draw.first;packet.instances=small?draw.count:draw.instanceCount;
		if(!small){packet.indexStride=draw.indices->layout.stride;packet.indexType=packet.indexStride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32;}
		packet.raster=*raster;packet.state=draw.world;result->impl->draws.push_back(std::move(packet));
	}
	return result;
}
}
