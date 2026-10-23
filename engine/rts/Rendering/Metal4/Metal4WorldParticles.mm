/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldParticles.h"
#include "NativeSampleShadingShaders.h"
#include <tuple>
#include <map>
#include "NativeWorldNanoShaders.h"
#include "NativeShadowColorShaders.h"
#include "Metal4MiniMapNative.h"
#include "Rendering/Backend/NativeLuaNanoRecorder.h"
#include "Rendering/Backend/NativeWorldParticlePass.h"
#include "Rendering/Backend/NativeParticleAtlasSource.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
// Same-revision ProjFXVertProg/ProjFXFragProg expressions and VA_TYPE_PROJ
// fields. Atlas rows/UV/page/animation remain the original CPU output.
const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ParticleVertex {packed_float3 position,uvw;packed_float4 uvInfo;packed_float3 animation;uchar4 color;};
struct Uniform {float4x4 viewProjection,view,projection;float4 clipPlane,camera,fogColor,fogParameters;uint4 options;};
struct Varying {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float4 uv [[centroid_perspective]];float page [[RECOIL_SAMPLE_PERSPECTIVE]],blend [[RECOIL_SAMPLE_PERSPECTIVE]],fog [[RECOIL_SAMPLE_PERSPECTIVE]],viewZ [[RECOIL_SAMPLE_PERSPECTIVE]];float2 screenUV [[RECOIL_SAMPLE_NOPERSPECTIVE]];float clipDistance [[clip_distance]];};
vertex Varying ParticleVertexMain(uint id [[vertex_id]],device const ParticleVertex* vertices [[buffer(0)]],constant Uniform& u [[buffer(1)]]) {
 const ParticleVertex v=vertices[id];Varying o;const float3 ap=float3(v.animation),uvw=float3(v.uvw),world=float3(v.position);
 float progress=fract(ap.z),maximum=ap.x*ap.y-1;progress*=maximum;float i0=floor(progress),i1=i0+1;o.blend=fract(progress);
 const float4 uvInfo=float4(v.uvInfo);if(maximum>1){const float2 diff=uvw.xy-uvInfo.xy;o.uv=diff.xyxy+float4(floor(fmod(i0,ap.x)),floor(i0/ap.x),floor(fmod(i1,ap.x)),floor(i1/ap.x))*uvInfo.zwzw;o.uv/=ap.xyxy;o.uv+=uvInfo.xyxy;}else o.uv=uvw.xyxy;
 o.page=uvw.z;o.color=float4(v.color)/255.f;float distance=length(world-u.camera.xyz);o.fog=clamp((u.fogParameters.y-distance)/(u.fogParameters.y-u.fogParameters.x),0.f,1.f);
 const float4 view=u.view*float4(world,1);o.viewZ=view.z;o.position=u.projection*view;o.screenUV=(o.position.xy/o.position.w)*.5f+.5f;
 if(u.options.x==1)o.position.z=(o.position.z+o.position.w)*.5f;o.clipDistance=dot(float4(world,1),u.clipPlane);return o;
}
fragment float4 ParticleFragmentMain(Varying in [[stage_in]],constant Uniform& u [[buffer(1)]],texture2d_array<float> atlas [[texture(0)]]
#if PARTICLE_HAS_DEPTH
 ,depth2d<float> depth [[texture(1)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 constexpr sampler source(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
 const float4 c0=atlas.sample(source,in.uv.xy,uint(in.page)),c1=atlas.sample(source,in.uv.zw,uint(in.page));float4 result=mix(c0,c1,in.blend)*in.color;
 result.rgb=mix(result.rgb,u.fogColor.rgb*result.a,1-in.fog);
 #if PARTICLE_HAS_DEPTH
 if(u.options.y!=0){constexpr sampler nearest(coord::normalized,address::clamp_to_edge,filter::nearest);float d=depth.sample(nearest,float2(in.screenUV.x,1-in.screenUV.y));if(u.options.x==1)d=d*2-1;
  const float viewDepth=-u.projection[3][2]/(u.projection[2][2]+d);float smoothness;
  if(u.fogParameters.z>0){smoothness=smoothstep(0.f,u.fogParameters.z,in.viewZ-viewDepth);result*=pow(smoothness,.6f);}else{smoothness=smoothstep(u.fogParameters.z,0.f,in.viewZ-viewDepth);result*=pow(smoothness,8.f);}}
 #endif
 if(!(result.a>u.fogParameters.w))discard_fragment();return result;
}
)MSL";
struct Uniform {
	simd_float4x4 viewProjection,view,projection;
	simd_float4 clipPlane,camera,fogColor,fogParameters;
	simd_uint4 options;
};
static_assert(sizeof(Uniform)==272);
struct Atlas {
	std::shared_ptr<const NativeWorldParticleAtlas> owner;
	__strong id<MTLTexture> texture=nil;
	__strong id<MTLTexture> image2D=nil;
	bool initialized=false;
	uint64_t encodedSubmission=0;
};
void EncodeAtlasMipmaps(std::span<const std::shared_ptr<Atlas>> atlases,id<MTL4CommandBuffer> command,uint64_t submission)
{
	if(atlases.empty())return;
	Require(command&&submission,"Original atlas mip producer lacks its actual common command/submission");
	bool pending=false;
	for(const auto& atlas:atlases)if(!atlas->initialized){
		Require(!atlas->encodedSubmission||atlas->encodedSubmission==submission,"Original atlas mips belong to an unretired different submission");
		pending|=atlas->encodedSubmission==0;
	}
	if(!pending)return;
	auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode original particle atlas mip generation");
	for(const auto& atlas:atlases)if(!atlas->initialized&&!atlas->encodedSubmission){[encoder generateMipmapsForTexture:atlas->texture];atlas->encodedSubmission=submission;}
	[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
}
void CompleteAtlasMipmaps(std::span<const std::shared_ptr<Atlas>> atlases,uint64_t submission)
{
	for(const auto& atlas:atlases){Require(atlas->initialized||(submission&&atlas->encodedSubmission==submission),"Cannot publish atlas mips before their actual producer retires");atlas->initialized=true;atlas->encodedSubmission=0;}
}
struct RetainedAtlasImage {
	std::shared_ptr<const NativeCUSTextureObject> object;
	std::shared_ptr<Atlas> image;
};
struct ShadowColorImage {
	__strong id<MTLTexture> target=nil,view=nil;
	bool colored=true;
};
struct ShadowColorCache {std::shared_ptr<ShadowColorImage> completed;};
struct ShadowColorUniform {simd_float4x4 view,projection;simd_uint4 options;};
static_assert(sizeof(ShadowColorUniform)==144);
struct LuaNanoUniform {
	simd_float4x4 cameraViewProjection,cameraViewInverse;
	simd_float4 mapSize,timeInfo;
	std::array<float,24> parameters{};
	simd_uint4 options;
};
static_assert(sizeof(LuaNanoUniform)==272);
struct EngineNanoUniform {
	simd_float4x4 cameraViewProjection;
	simd_float4 cameraPosition,cameraRight,cameraUp,clipPlane;
	std::array<simd_float4,6> planes;
	std::array<float,20> parameters{};
	simd_uint4 options;
};
static_assert(sizeof(EngineNanoUniform)==320);
simd_float4x4 Matrix(const std::array<float,16>& values) {simd_float4x4 result;std::memcpy(&result,values.data(),sizeof(result));return result;}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t length)
{
	Require(length&&length<=lease.Device().maxBufferLength,"Original particle GPU buffer exceeds device bounds");
	auto buffer=[lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];Require(buffer!=nil,"Cannot allocate original particle GPU buffer");lease.AddAllocation(buffer);return buffer;
}
id<MTLLibrary> Library(RuntimeLease& lease,const char* source,const char* label)
{
	auto d=[MTL4LibraryDescriptor new];d.source=String(source);d.name=String(label);
	auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;d.options=options;NSError* error=nil;
	auto library=lease.NewLibrary(d,&error);Require(library!=nil,std::string("Cannot compile original nano MSL: ")+Error(error));return library;
}
id<MTLRenderPipelineState> NanoPipeline(RuntimeLease& lease,id<MTLLibrary> library,const char* vertexName,const char* fragmentName,MTLPixelFormat colorFormat,bool validation,unsigned outputSamples)
{
	auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=String(vertexName);auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=String(fragmentName);
	auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=outputSamples;d.vertexFunctionDescriptor=vertex;d.fragmentFunctionDescriptor=fragment;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
	auto attachment=d.colorAttachments[0];attachment.pixelFormat=colorFormat;attachment.blendingState=MTL4BlendStateEnabled;
	attachment.sourceRGBBlendFactor=attachment.sourceAlphaBlendFactor=MTLBlendFactorOne;attachment.destinationRGBBlendFactor=attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
	auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;NSError* error=nil;
	auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original nano Metal4 pipeline: "+Error(error));return pipeline;
}
std::shared_ptr<Atlas> Upload(RuntimeLease& lease,std::shared_ptr<const NativeWorldParticleAtlas> owner)
{
	Require(owner&&!owner->pages.empty()&&owner->pages.size()<=16,"Original particle atlas exceeds its original 16-page allocator contract");
	Require(owner->pages.front().size()==1,"Original particle packed atlas requires exactly one CPU base level");
	const auto& base=owner->pages.front().front();const auto maximum=lease.Capabilities().maxTextureDimension2D;
	Require(base.width&&base.height&&base.width<=maximum&&base.height<=maximum,"Original particle packed atlas exceeds actual device extent");
	uint32_t levels=1;for(uint32_t side=std::max(base.width,base.height);side>1;side>>=1)++levels;
	Require(owner->requestedMipLevels>=1&&owner->requestedMipLevels<=levels,"Original particle requested mip count exceeds image extent");
	for(const auto& page:owner->pages) Require(page.size()==1&&page.front().width==base.width&&page.front().height==base.height&&page.front().rgba.size()==size_t(base.width)*base.height*4,"Original particle packed page extent/bytes mismatch");
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:base.width height:base.height mipmapped:owner->generateMipmaps];
	d.textureType=MTLTextureType2DArray;d.arrayLength=owner->pages.size();d.mipmapLevelCount=owner->requestedMipLevels;d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
	auto texture=[lease.Device() newTextureWithDescriptor:d];Require(texture!=nil,"Cannot allocate exact original packed particle atlas");texture.label=String(owner->sourceName);
	for(size_t page=0;page<owner->pages.size();++page) [texture replaceRegion:MTLRegionMake2D(0,0,base.width,base.height) mipmapLevel:0 slice:page withBytes:owner->pages[page].front().rgba.data() bytesPerRow:size_t(base.width)*4 bytesPerImage:size_t(base.width)*base.height*4];
	auto result=std::make_shared<Atlas>();result->owner=std::move(owner);result->texture=texture;result->initialized=!result->owner->generateMipmaps;
	if(result->owner->pages.size()==1){result->image2D=[texture newTextureViewWithPixelFormat:texture.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,texture.mipmapLevelCount) slices:NSMakeRange(0,1)];Require(result->image2D!=nil,"Cannot expose the original single-page atlas as its actual2D view");}
	return result;
}
}
struct PreparedWorldParticles::Impl {
	struct Draw {
		NativeWorldParticleRange range;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		MTLIndexType indexType=MTLIndexTypeUInt32;
		NSUInteger indexStride=4,vertexCount=0,instanceCount=1;
		bool stencil=false;
		uint64_t sequence=0;
	};
	std::vector<Draw> draws;
	std::vector<std::shared_ptr<Atlas>> pendingMipmaps;
	uint64_t submission=0;
	std::shared_ptr<PreparedMiniMap> infoProducer;
	__strong MTL4RenderPassDescriptor* clearInfo=nil;
	__strong id<MTLTexture> normalInfo=nil;
	std::shared_ptr<bool> normalInfoInitialized;
};
PreparedWorldParticles::PreparedWorldParticles():impl(std::make_unique<Impl>()){}
PreparedWorldParticles::~PreparedWorldParticles()=default;
void PreparedWorldParticles::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	if(impl->infoProducer)impl->infoProducer->EncodeDependencies(command);
	if(impl->clearInfo!=nil) {
		auto clear=[command renderCommandEncoderWithDescriptor:impl->clearInfo];Require(clear!=nil,"Cannot encode original empty-mode $info clear");
		[clear endEncoding];auto mip=[command computeCommandEncoder];Require(mip!=nil,"Cannot generate original empty-mode $info mips");
		[mip generateMipmapsForTexture:impl->normalInfo];[mip barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[mip endEncoding];
	}
	EncodeAtlasMipmaps(impl->pendingMipmaps,command,impl->submission);
}
void PreparedWorldParticles::DidComplete() const {CompleteAtlasMipmaps(impl->pendingMipmaps,impl->submission);if(impl->infoProducer)impl->infoProducer->DidComplete();if(impl->normalInfoInitialized)*impl->normalInfoInitialized=true;}
uint32_t PreparedWorldParticles::DrawCount(NativeWorldParticleStage stage) const {return std::count_if(impl->draws.begin(),impl->draws.end(),[stage](const auto& draw){return draw.range.stage==stage;});}
std::size_t PreparedWorldParticles::DrawCount() const {return impl->draws.size();}
std::uint64_t PreparedWorldParticles::SequenceAt(std::size_t index) const {return impl->draws.at(index).sequence;}
void PreparedWorldParticles::Encode(NativeWorldParticleStage stage,id<MTL4RenderCommandEncoder> encoder,id<MTLDepthStencilState> inheritedAlphaDepth) const
{
	for(std::size_t n=0;n<impl->draws.size();++n)if(impl->draws[n].range.stage==stage)EncodeDraw(n,encoder,inheritedAlphaDepth);
}
void PreparedWorldParticles::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder,id<MTLDepthStencilState> inheritedAlphaDepth) const
{
	const auto& draw=impl->draws.at(index);
	for(const auto& atlas:impl->pendingMipmaps)Require(atlas->initialized||atlas->encodedSubmission==impl->submission,"Original particles sampled before their actual atlas mip producer");
	Require(!inheritedAlphaDepth||draw.range.stage==NativeWorldParticleStage::AlphaAll||draw.range.stage==NativeWorldParticleStage::AlphaBelowWater||draw.range.stage==NativeWorldParticleStage::AlphaAboveWater||draw.range.stage==NativeWorldParticleStage::NanoAll||draw.range.stage==NativeWorldParticleStage::NanoBelowWater||draw.range.stage==NativeWorldParticleStage::NanoAboveWater,"Inherited callback depth applies only to original engine alpha/nano stages");
		[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:inheritedAlphaDepth&&draw.range.depthTest?inheritedAlphaDepth:draw.depth];[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setCullMode:MTLCullModeNone];
		[encoder setDepthBias:draw.range.groundAtlas?-1000:0 slopeScale:draw.range.groundAtlas?-20:0 clamp:0];
		[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		if(draw.stencil)[encoder setStencilReferenceValue:0x40];
		if(draw.indices!=nil){const NSUInteger offset=NSUInteger(draw.range.firstIndex)*draw.indexStride;
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.range.indexCount indexType:draw.indexType indexBuffer:draw.indices.gpuAddress+offset indexBufferLength:draw.indices.length-offset instanceCount:draw.instanceCount];}
		else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.vertexCount instanceCount:draw.instanceCount];
	// Original ground-flash polygon offset is disabled before subsequent stages.
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
}
struct PreparedShadowColor::Impl {
	std::shared_ptr<const NativeShadowColorFrame> source;
	std::shared_ptr<ShadowColorCache> cache;
	std::shared_ptr<ShadowColorImage> image;
	std::shared_ptr<Atlas> atlas;
	__strong id<MTLTexture> depth=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depthState=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong id<MTLBuffer> indices=nil;
	bool clear=false;
	uint64_t submission=0;
	mutable bool encoded=false;
};
PreparedShadowColor::PreparedShadowColor():impl(std::make_unique<Impl>()){}
PreparedShadowColor::~PreparedShadowColor()=default;
id<MTLTexture> PreparedShadowColor::Color()const{return impl->image->view;}
id<MTLTexture> PreparedShadowColor::RawColor()const{return impl->image->target;}
uint32_t PreparedShadowColor::SourceDrawFrame()const{return impl->source->sourceDrawFrame;}
uint32_t PreparedShadowColor::DrawCount()const{return impl->source->redraw&&!impl->source->indices.empty()?1:0;}
void PreparedShadowColor::EncodeDependencies(id<MTL4CommandBuffer> command)const
{
	if(impl->atlas){const std::array atlases{impl->atlas};EncodeAtlasMipmaps(atlases,command,impl->submission);}
}
void PreparedShadowColor::Encode(id<MTL4CommandBuffer> command,const std::function<void(id<MTL4RenderCommandEncoder>)>& afterParticles)const
{
	Require(!impl->encoded,"Original shadow color producer encoded twice");
	if(!impl->source->redraw&&!impl->clear){impl->encoded=true;return;}
	Require(!impl->atlas||impl->atlas->initialized||impl->atlas->encodedSubmission==impl->submission,"Original shadow color sampled unproduced particle atlas mips");
	auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=pass.renderTargetHeight=impl->source->size;
	pass.colorAttachments[0].texture=impl->image->target;pass.colorAttachments[0].loadAction=impl->clear?MTLLoadActionClear:MTLLoadActionLoad;
	pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(1,1,1,0);
	pass.depthAttachment.texture=impl->depth;pass.depthAttachment.loadAction=MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;
	auto e=[command renderCommandEncoderWithDescriptor:pass];Require(e!=nil,"Cannot encode original transparent shadow color pass");
	[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[e setViewport:MTLViewport{0,0,double(impl->source->size),double(impl->source->size),0,1}];
	[e setScissorRect:MTLScissorRect{0,0,impl->source->size,impl->source->size}];
	[e setDepthClipMode:MTLDepthClipModeClip];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setCullMode:MTLCullModeBack];[e setDepthBias:0 slopeScale:0 clamp:0];
	if(DrawCount()){
		[e setRenderPipelineState:impl->pipeline];[e setDepthStencilState:impl->depthState];[e setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:impl->source->indices.size() indexType:MTLIndexTypeUInt32 indexBuffer:impl->indices.gpuAddress indexBufferLength:impl->indices.length];
	}
	if(impl->source->redraw&&afterParticles)afterParticles(e);
	[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];impl->encoded=true;
}
void PreparedShadowColor::DidComplete()const
{
	Require(impl->encoded,"Cannot publish unencoded original shadow color output");
	if(impl->atlas){const std::array atlases{impl->atlas};CompleteAtlasMipmaps(atlases,impl->submission);}
	impl->cache->completed=impl->image;
}
struct WorldParticlesGPU::Impl {
	std::unordered_map<const NativeWorldParticleAtlas*,std::shared_ptr<Atlas>> atlases;
	std::vector<std::shared_ptr<Atlas>> sourceAtlases;
	uint64_t sourceSubmission=0;
	std::shared_ptr<Atlas> GetAtlas(RuntimeLease& lease,std::shared_ptr<const NativeWorldParticleAtlas> owner){
		Require(owner!=nullptr,"Original atlas cache lost its immutable publication");
		// Evict only versions no source object/frame retains. Prepared owners and
		// the actual runtime lease still keep any retired version's GPU storage.
		for(auto it=atlases.begin();it!=atlases.end();)if(it->second->owner.use_count()==1&&!it->second->encodedSubmission)it=atlases.erase(it);else ++it;
		const auto found=atlases.find(owner.get());
		auto cached=found==atlases.end()?Upload(lease,owner):found->second;
		if(found==atlases.end())atlases.emplace(owner.get(),cached);
		Require(cached->texture.device==lease.Device(),"Original atlas cache belongs to another actual device");
		lease.AddAllocation(cached->texture);if(cached->image2D)lease.AddAllocation(cached->image2D);return cached;
	}
	std::shared_ptr<ShadowColorCache> shadowColor=std::make_shared<ShadowColorCache>();
	__strong id<MTLLibrary> shadowColorLibrary=nil;
	__strong id<MTLRenderPipelineState> shadowColorRGBPipeline=nil,shadowColorGrayPipeline=nil;
	bool shadowColorValidation=false;
	struct ParticleProgram {
		__strong id<MTLLibrary> library=nil;
		__strong id<MTLRenderPipelineState> alpha=nil,groundPipeline=nil;
		MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
	};
	std::map<std::tuple<bool,MTLPixelFormat,bool,unsigned>,ParticleProgram> particlePrograms; // Actual depth presence and output count are part of PSO identity.
	__strong id<MTLLibrary> luaLibrary=nil,engineLibrary=nil;
	__strong id<MTLRenderPipelineState> luaPipeline=nil,enginePipeline=nil;
	__strong id<MTLDepthStencilState> luaDepth=nil,luaGhostDepth=nil,luaNoStencilDepth=nil,luaGhostNoStencilDepth=nil;
	MTLPixelFormat nanoFormat=MTLPixelFormatInvalid;bool nanoValidation=false;
	using NanoPrograms=std::tuple<id<MTLLibrary>,id<MTLLibrary>,id<MTLRenderPipelineState>,id<MTLRenderPipelineState>,id<MTLDepthStencilState>,id<MTLDepthStencilState>,id<MTLDepthStencilState>,id<MTLDepthStencilState>>;
	std::map<std::tuple<MTLPixelFormat,bool,unsigned>,NanoPrograms> nanoPrograms;
	std::unique_ptr<MiniMapGPU> infoGPU=std::make_unique<MiniMapGPU>();
	__strong id<MTLTexture> normalInfo=nil;
	std::shared_ptr<bool> normalInfoInitialized;
	bool lastInfoNormal=true;
};
WorldParticlesGPU::WorldParticlesGPU():impl(std::make_unique<Impl>()){}
WorldParticlesGPU::~WorldParticlesGPU(){if(impl->luaPipeline!=nil)SetNativeLuaNanoConsumerAvailable(false);if(impl->enginePipeline!=nil)SetNativeEngineNanoConsumerAvailable(false);}
void WorldParticlesGPU::ClearResources()
{
	// Preserve the actual initialized family across scene reload; the next
	// original gadget Initialize must observe its already connected consumer.
	for(const auto& [source,atlas]:impl->atlases)Require(!atlas->encodedSubmission||atlas->initialized,"Original atlas resources still have unretired work");
	impl->atlases.clear();impl->sourceAtlases.clear();impl->sourceSubmission=0;impl->shadowColor=std::make_shared<ShadowColorCache>();impl->normalInfo=nil;impl->normalInfoInitialized.reset();
	impl->infoGPU=std::make_unique<MiniMapGPU>();
	impl->lastInfoNormal=true;
}
void WorldParticlesGPU::InitializeLuaNano(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& shaderMilliseconds,unsigned outputSamples)
{
	const auto key=std::make_tuple(colorFormat,validation,lease.SampleShadingKey(outputSamples));
	if(auto found=impl->nanoPrograms.find(key);found!=impl->nanoPrograms.end()) {
		std::tie(impl->luaLibrary,impl->engineLibrary,impl->luaPipeline,impl->enginePipeline,impl->luaDepth,impl->luaGhostDepth,impl->luaNoStencilDepth,impl->luaGhostNoStencilDepth)=found->second;
		impl->nanoFormat=colorFormat;impl->nanoValidation=validation;return;
	}
	const auto start=Clock::now();
	auto lua=Library(lease,nano::LUA_SHADER,"Original test30770 Lua nano NoGS cube/octa/halo");
	auto engine=Library(lease,nano::ENGINE_SHADER,"Original engine analytic nano NoGS cube/halo");
	auto luaPipeline=NanoPipeline(lease,lua,"LuaNanoVertexMain","LuaNanoFragmentMain",colorFormat,validation,outputSamples);
	auto enginePipeline=NanoPipeline(lease,engine,"EngineNanoVertexMain","EngineNanoFragmentMain",colorFormat,validation,outputSamples);
	const auto depthState=[&](bool ghost,bool hasStencil) {
		auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=ghost?MTLCompareFunctionAlways:MTLCompareFunctionLessEqual;d.depthWriteEnabled=NO;
		if(hasStencil){
		 auto stencil=[MTLStencilDescriptor new];stencil.stencilCompareFunction=ghost?MTLCompareFunctionEqual:MTLCompareFunctionNotEqual;
		 stencil.readMask=0x40;stencil.writeMask=0;stencil.stencilFailureOperation=stencil.depthFailureOperation=stencil.depthStencilPassOperation=MTLStencilOperationKeep;
		 d.frontFaceStencil=stencil;d.backFaceStencil=stencil;
		}
		auto state=[lease.Device() newDepthStencilStateWithDescriptor:d];
		Require(state!=nil,"Cannot initialize original Lua nano paired depth/stencil state");return state;
	};
	auto depth=depthState(false,true),ghostDepth=depthState(true,true);
	auto noStencilDepth=depthState(false,false),ghostNoStencilDepth=depthState(true,false);
	impl->luaLibrary=lua;impl->engineLibrary=engine;impl->luaPipeline=luaPipeline;impl->enginePipeline=enginePipeline;impl->nanoFormat=colorFormat;impl->nanoValidation=validation;
	impl->luaDepth=depth;impl->luaGhostDepth=ghostDepth;
	impl->luaNoStencilDepth=noStencilDepth;impl->luaGhostNoStencilDepth=ghostNoStencilDepth;
	impl->nanoPrograms.emplace(key,Impl::NanoPrograms{lua,engine,luaPipeline,enginePipeline,depth,ghostDepth,noStencilDepth,ghostNoStencilDepth});
	shaderMilliseconds+=Milliseconds(start);
	SetNativeLuaNanoConsumerAvailable(true);SetNativeEngineNanoConsumerAvailable(true);
}
std::unique_ptr<PreparedWorldParticles> WorldParticlesGPU::Prepare(RuntimeLease& lease,uint64_t submission,const NativeWorldParticleFrame& frame,const WorldCamera& viewCamera,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,id<MTLTexture> terrainDepth,bool validation,double& shaderMilliseconds,uint32_t disabledAlphaClipStages,unsigned outputSamples)
{
	if(frame.ranges.empty()&&frame.engineNano.empty())return {};
	Require(submission,"Original world particles lack the actual common submission");
	Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original world particles require actual depth attachment");
	Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original world particles require actual camera clip depth");
	ValidateNativeWorldParticlePass(frame,viewCamera);
	Require(!frame.softenRequested||terrainDepth!=nil,"Original soft particles require the real terrain-only depth copy");
	for(const auto& range:frame.ranges) {
		Require(range.indexCount%3==0&&range.firstIndex<=frame.indices.size()&&range.indexCount<=frame.indices.size()-range.firstIndex,"Original particle indexed range outside captured geometry");
		const auto owner=range.groundAtlas?frame.groundAtlas:frame.atlas;Require(owner!=nullptr,"Original world particle has no real packed atlas image");
		for(size_t n=range.firstIndex;n<size_t(range.firstIndex)+range.indexCount;++n) {Require(frame.indices[n]<frame.vertices.size(),"Original particle index outside captured vertices");const auto& v=frame.vertices[frame.indices[n]];
			for(float x:v.position)Require(std::isfinite(x),"Original particle nonfinite position");for(float x:v.uvw)Require(std::isfinite(x),"Original particle nonfinite UV/page");
			Require(v.uvw[2]>=0&&std::floor(v.uvw[2])==v.uvw[2]&&v.uvw[2]<owner->pages.size(),"Original particle uses invalid original atlas page");}
	}
	const bool hasDepth=terrainDepth!=nil;
	auto& particleProgram=impl->particlePrograms[std::make_tuple(hasDepth,colorFormat,validation,lease.SampleShadingKey(outputSamples))];
	if(particleProgram.alpha==nil||particleProgram.format!=colorFormat||particleProgram.validation!=validation) {
		const auto start=Clock::now();NSError* error=nil;auto ld=[MTL4LibraryDescriptor new];ld.source=String(std::string("#define PARTICLE_HAS_DEPTH ")+(hasDepth?"1\n":"0\n")+SHADER);ld.name=@"Original engine projectile atlas/animation/fog/soft-depth shaders";
		auto library=lease.NewLibrary(ld,&error);Require(library!=nil,"Cannot compile original particle MSL: "+Error(error));
		auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"ParticleVertexMain";auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=@"ParticleFragmentMain";
		const auto pipeline=[&](bool ground) {auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=outputSamples;d.vertexFunctionDescriptor=vertex;d.fragmentFunctionDescriptor=fragment;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
			auto attachment=d.colorAttachments[0];attachment.pixelFormat=colorFormat;attachment.blendingState=MTL4BlendStateEnabled;
			attachment.sourceRGBBlendFactor=attachment.sourceAlphaBlendFactor=ground?MTLBlendFactorSourceAlpha:MTLBlendFactorOne;attachment.destinationRGBBlendFactor=attachment.destinationAlphaBlendFactor=ground?MTLBlendFactorOne:MTLBlendFactorOneMinusSourceAlpha;
			auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
			auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original particle Metal4 pipeline: "+Error(error));return p;};
		auto alpha=pipeline(false),ground=pipeline(true);particleProgram.library=library;particleProgram.alpha=alpha;particleProgram.groundPipeline=ground;particleProgram.format=colorFormat;particleProgram.validation=validation;shaderMilliseconds+=Milliseconds(start);
	}
	lease.Retain(particleProgram.library);lease.Retain(particleProgram.alpha);lease.Retain(particleProgram.groundPipeline);
	auto result=std::unique_ptr<PreparedWorldParticles>(new PreparedWorldParticles);result->impl->submission=submission;
	const auto atlas=[&](bool ground) {auto cached=impl->GetAtlas(lease,ground?frame.groundAtlas:frame.atlas);
		if(!cached->initialized&&std::find(result->impl->pendingMipmaps.begin(),result->impl->pendingMipmaps.end(),cached)==result->impl->pendingMipmaps.end())result->impl->pendingMipmaps.push_back(cached);return cached;};
	id<MTLBuffer> vertices=frame.ranges.empty()?nil:Buffer(lease,frame.vertices.data(),frame.vertices.size()*sizeof(NativeWorldParticleVertex));
	id<MTLBuffer> indices=frame.ranges.empty()?nil:Buffer(lease,frame.indices.data(),frame.indices.size()*sizeof(uint32_t));
	if(terrainDepth!=nil)lease.AddAllocation(terrainDepth);
	for(const auto& range:frame.ranges) {
		auto image=atlas(range.groundAtlas);Uniform u{};u.view=Matrix(viewCamera.view);u.projection=Matrix(viewCamera.projection);u.viewProjection=simd_mul(u.projection,u.view);
		u.clipPlane=simd_make_float4(0,range.stage==NativeWorldParticleStage::AlphaBelowWater?-1.f:(range.stage==NativeWorldParticleStage::AlphaAboveWater?1.f:0.f),0,range.stage==NativeWorldParticleStage::AlphaBelowWater||range.stage==NativeWorldParticleStage::AlphaAboveWater?0.f:1.f);
		if(disabledAlphaClipStages&(1u<<unsigned(range.stage)))u.clipPlane=simd_make_float4(0,0,0,1);
		u.camera=simd_make_float4(frame.fogCameraPosition[0],frame.fogCameraPosition[1],frame.fogCameraPosition[2],0);u.fogColor=simd_make_float4(frame.fogColor[0],frame.fogColor[1],frame.fogColor[2],0);
		u.fogParameters=simd_make_float4(frame.fogDistances[0],frame.fogDistances[1],range.softenThreshold,range.alphaThreshold);u.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne?1:0,frame.softenRequested?1:0,0,0);
		for(const auto* matrix:{viewCamera.view.data(),viewCamera.projection.data()})for(unsigned n=0;n<16;++n)Require(std::isfinite(matrix[n]),"Original particle nonfinite camera matrix");
		auto uniform=Buffer(lease,&u,sizeof(u));auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=2;ad.maxTextureBindCount=2;ad.initializeBindings=YES;NSError* error=nil;
		auto arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(arguments!=nil,"Cannot allocate original particle argument table: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0];[arguments setAddress:uniform.gpuAddress atIndex:1];[arguments setTexture:image->texture.gpuResourceID atIndex:0];if(terrainDepth!=nil)[arguments setTexture:terrainDepth.gpuResourceID atIndex:1];
		auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=range.depthTest?MTLCompareFunctionLessEqual:MTLCompareFunctionAlways;dd.depthWriteEnabled=range.depthWrite;
		auto depth=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(depth!=nil,"Cannot allocate original particle depth state");lease.Retain(depth);
		result->impl->draws.push_back({range,range.groundAtlas?particleProgram.groundPipeline:particleProgram.alpha,depth,arguments,indices});
	}
	if(!frame.engineNano.empty()) {
		Require(frame.engineNanoTemplate.size()==42,"Original engine nano requires its actual 42-vertex template");
		for(const auto& v:frame.engineNano){for(float x:v.startPosition)Require(std::isfinite(x),"Nonfinite engine nano start position");for(float x:v.velocity)Require(std::isfinite(x),"Nonfinite engine nano velocity");for(float x:v.frames)Require(std::isfinite(x),"Nonfinite engine nano frame state");}
		InitializeLuaNano(lease,colorFormat,validation,shaderMilliseconds,outputSamples);lease.Retain(impl->engineLibrary);lease.Retain(impl->enginePipeline);
		auto mesh=Buffer(lease,frame.engineNanoTemplate.data(),frame.engineNanoTemplate.size()*sizeof(NativeEngineNanoTemplateVertex));
		auto instances=Buffer(lease,frame.engineNano.data(),frame.engineNano.size()*sizeof(NativeEngineNanoInstance));
		const auto prepare=[&](NativeWorldParticleStage stage,float clipY) {
			EngineNanoUniform u{};u.cameraViewProjection=Matrix(frame.cameraViewProjection);
			u.cameraPosition=simd_make_float4(frame.cameraPosition[0],frame.cameraPosition[1],frame.cameraPosition[2],0);
			u.cameraRight=simd_make_float4(frame.cameraRight[0],frame.cameraRight[1],frame.cameraRight[2],0);u.cameraUp=simd_make_float4(frame.cameraUp[0],frame.cameraUp[1],frame.cameraUp[2],0);
			u.clipPlane=simd_make_float4(0,clipY,0,clipY==0?1.f:0.f);
			for(unsigned n=0;n<6;++n)std::memcpy(&u.planes[n],frame.frustumPlanes[n].data(),sizeof(simd_float4));
			static_assert(sizeof(NativeEngineNanoAppearance)==19*sizeof(float));std::memcpy(u.parameters.data(),&frame.engineNanoAppearance,sizeof(NativeEngineNanoAppearance));u.parameters[19]=frame.animationFrame;
			for(float value:u.parameters)Require(std::isfinite(value),"Nonfinite original engine nano appearance");u.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne?1:0,0,0,0);
			auto uniform=Buffer(lease,&u,sizeof(u));auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=3;ad.initializeBindings=YES;NSError* error=nil;
			auto arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(arguments!=nil,"Cannot allocate original engine nano arguments: "+Error(error));lease.Retain(arguments);
			[arguments setAddress:mesh.gpuAddress atIndex:0];[arguments setAddress:instances.gpuAddress atIndex:1];[arguments setAddress:uniform.gpuAddress atIndex:2];
			auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;auto depth=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(depth!=nil,"Cannot allocate original engine nano depth state");lease.Retain(depth);
			PreparedWorldParticles::Impl::Draw draw;draw.range.stage=stage;draw.pipeline=impl->enginePipeline;draw.depth=depth;draw.arguments=arguments;draw.vertexCount=42;draw.instanceCount=frame.engineNano.size();result->impl->draws.push_back(std::move(draw));
		};
		if(frame.drawReflection)prepare(NativeWorldParticleStage::NanoAboveWater,1);
		else if(frame.drawRefraction)prepare(NativeWorldParticleStage::NanoBelowWater,-1);
		else if(frame.visibleWater){prepare(NativeWorldParticleStage::NanoBelowWater,-1);prepare(NativeWorldParticleStage::NanoAboveWater,1);}else prepare(NativeWorldParticleStage::NanoAll,0);
	}
	return result;
}

std::unique_ptr<PreparedWorldParticles> WorldParticlesGPU::PrepareLuaNano(RuntimeLease& lease,
	const NativeLuaNanoFrame& frame,const WorldCamera& viewCamera,float animationFrame,
	MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
	bool validation,double& shaderMilliseconds,unsigned outputSamples)
{
	if(frame.draws.empty())return {};
	Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original Lua nano requires the real scene depth attachment");
	Require(stencilFormat==MTLPixelFormatInvalid||stencilFormat==MTLPixelFormatStencil8||stencilFormat==MTLPixelFormatDepth32Float_Stencil8,"Original Lua nano stencil target format is unsupported");
	const bool hasStencil=stencilFormat!=MTLPixelFormatInvalid;
	Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported&&std::isfinite(animationFrame),"Original Lua nano requires actual finite camera/time");
	for(const auto& draw:frame.draws)ValidateNativeLuaNanoDraw(draw);
	for(float value:frame.cameraViewInverse)Require(std::isfinite(value),"Original Lua nano nonfinite cameraViewInv source");
	for(float value:frame.cameraViewProjection)Require(std::isfinite(value),"Original Lua nano nonfinite cameraViewProj source");
	Require(frame.mapSquares[0]&&frame.mapSquares[1]&&frame.mapSquares[2]>=frame.mapSquares[0]&&frame.mapSquares[3]>=frame.mapSquares[1],"Original Lua nano mapSize UBO source invalid");
	InitializeLuaNano(lease,colorFormat,validation,shaderMilliseconds,outputSamples);lease.Retain(impl->luaLibrary);lease.Retain(impl->luaPipeline);
	auto result=std::unique_ptr<PreparedWorldParticles>(new PreparedWorldParticles);id<MTLTexture> info=nil;
	if(frame.infoIsNormal) {
		// Original CInfoTextureCombiner(empty mode) clears its actual padded
		// RGB10_A2 image to .5, then generates mips. This is its real default
		// producer, not a white LOS substitute or the minimap shading texture.
		const auto width=frame.mapSquares[2],height=frame.mapSquares[3],limit=lease.Capabilities().maxTextureDimension2D;
		Require(width<=limit&&height<=limit,"Original Lua nano $info exceeds actual device bounds");
		if(impl->normalInfo==nil||impl->normalInfo.width!=width||impl->normalInfo.height!=height) {
			auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGB10A2Unorm width:width height:height mipmapped:YES];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
			impl->normalInfo=[lease.Device() newTextureWithDescriptor:d];Require(impl->normalInfo!=nil,"Cannot allocate original empty-mode $info image");impl->normalInfoInitialized=std::make_shared<bool>(false);
		}
		info=impl->normalInfo;result->impl->normalInfo=info;
		if(!*impl->normalInfoInitialized) {
			auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=info;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(.5,.5,.5,1);
			pass.renderTargetWidth=width;pass.renderTargetHeight=height;result->impl->clearInfo=pass;result->impl->normalInfoInitialized=impl->normalInfoInitialized;
		}
	} else {
		Require(frame.infoBackground&&frame.infoBackground->info,"Original Lua nano requested $info mode has no real native producer");
		if(impl->lastInfoNormal)impl->infoGPU=std::make_unique<MiniMapGPU>();
		result->impl->infoProducer=impl->infoGPU->Prepare(lease,frame.infoBackground,validation,shaderMilliseconds);info=result->impl->infoProducer->Info();
	}
	impl->lastInfoNormal=frame.infoIsNormal;
	Require(info!=nil,"Original Lua nano has no original $info texture");lease.AddAllocation(info);
	for(const auto& draw:frame.draws) {
		if(draw.instanceCount==0)continue;
		auto mesh=Buffer(lease,draw.vertexSlots->bytes.data(),draw.vertexSlots->bytes.size());auto instances=Buffer(lease,draw.instances->bytes.data(),draw.instances->bytes.size());auto indices=Buffer(lease,draw.indices->bytes.data(),draw.indices->bytes.size());
		LuaNanoUniform u{};u.cameraViewProjection=Matrix(frame.cameraViewProjection);u.cameraViewInverse=Matrix(frame.cameraViewInverse);
		u.mapSize=simd_make_float4(frame.mapSquares[0]*8.f,frame.mapSquares[1]*8.f,frame.mapSquares[2]*8.f,frame.mapSquares[3]*8.f);u.timeInfo=simd_make_float4(animationFrame,0,0,0);
		for(size_t n=0;n<nano::PARAMETER_NAMES.size();++n){const auto it=draw.uniforms.find(std::string(nano::PARAMETER_NAMES[n]));if(it!=draw.uniforms.end())u.parameters[n]=it->second;}
		u.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne?1:0,draw.shape,draw.rejectFutureSpawns?1u:0u,0);
		auto uniform=Buffer(lease,&u,sizeof(u));auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=3;ad.maxTextureBindCount=1;ad.initializeBindings=YES;NSError* error=nil;
		auto arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(arguments!=nil,"Cannot allocate original Lua nano arguments: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:mesh.gpuAddress atIndex:0];[arguments setAddress:instances.gpuAddress atIndex:1];[arguments setAddress:uniform.gpuAddress atIndex:2];[arguments setTexture:info.gpuResourceID atIndex:0];
		// Preserve both original passes; absent stencil makes either test pass.
		auto depth=hasStencil?(draw.ghostStencilPass?impl->luaGhostDepth:impl->luaDepth):
		 (draw.ghostStencilPass?impl->luaGhostNoStencilDepth:impl->luaNoStencilDepth);lease.Retain(depth);
		PreparedWorldParticles::Impl::Draw output;output.range.stage=NativeWorldParticleStage::LuaNanoLaterWorld;output.range.indexCount=draw.indices->layout.elements;output.pipeline=impl->luaPipeline;output.depth=depth;output.arguments=arguments;output.indices=indices;
		output.indexType=draw.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32;output.indexStride=draw.indices->layout.stride;output.instanceCount=draw.instanceCount;output.stencil=hasStencil;output.sequence=draw.sequence;result->impl->draws.push_back(std::move(output));
	}
	return result;
}
}

namespace rendering::metal4 {
std::unique_ptr<PreparedShadowColor> WorldParticlesGPU::PrepareShadowColor(RuntimeLease& lease,uint64_t submission,
	std::shared_ptr<const NativeShadowColorFrame> source,id<MTLTexture> depth,bool validation,double& shaderMs,id<MTLTexture> actualNamedColor)
{
	Require(submission,"Original shadow color lacks the actual common submission");
	Require(source&&source->size&&source->size<=lease.Capabilities().maxTextureDimension2D,"Original shadow color extent exceeds actual device bounds");
	Require(depth!=nil&&depth.width==source->size&&depth.height==source->size&&(depth.pixelFormat==MTLPixelFormatDepth32Float||depth.pixelFormat==MTLPixelFormatDepth32Float_Stencil8),"Original shadow color requires matching real opaque shadow depth");
	for(const auto& matrix:{source->view,source->projection})for(float value:matrix)Require(std::isfinite(value),"Nonfinite original shadow drawing matrix");
	Require(source->indices.size()%3==0,"Original shadow transparent geometry is not triangles");
	for(uint32_t index:source->indices){Require(index<source->vertices.size(),"Original shadow transparent index exceeds captured geometry");const auto& v=source->vertices[index];
		for(float x:v.position)Require(std::isfinite(x),"Nonfinite original shadow position");for(float x:v.uvw)Require(std::isfinite(x),"Nonfinite original shadow UV/page");
		for(float x:v.uvInfo)Require(std::isfinite(x),"Nonfinite original shadow atlas bounds");for(float x:v.animation)Require(std::isfinite(x),"Nonfinite original shadow animation");
		Require(source->atlas&&v.uvw[2]>=0&&std::floor(v.uvw[2])==v.uvw[2]&&v.uvw[2]<source->atlas->pages.size(),"Original shadow particle atlas page outside real source");
		Require(v.animation[0]>0&&v.animation[1]>0,"Original shadow particle animation cells must be positive");
	}
	auto result=std::unique_ptr<PreparedShadowColor>(new PreparedShadowColor);result->impl->submission=submission;result->impl->source=source;result->impl->depth=depth;result->impl->cache=impl->shadowColor;lease.AddAllocation(depth);
	auto image=impl->shadowColor->completed;
	if(actualNamedColor!=nil){
		Require(actualNamedColor.textureType==MTLTextureType2D&&actualNamedColor.sampleCount==1&&actualNamedColor.width==source->size&&actualNamedColor.height==source->size&&actualNamedColor.pixelFormat==(source->colored?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR8Unorm),"Actual named shadow color does not match the original producer");
		if(!image||image->target!=actualNamedColor){
			image=std::make_shared<ShadowColorImage>();image->colored=source->colored;image->target=actualNamedColor;
			const MTLTextureSwizzleChannels channels=source->colored?MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleBlue,MTLTextureSwizzleOne}:MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleOne};
			image->view=[image->target newTextureViewWithPixelFormat:image->target.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1) swizzle:channels];
			Require(image->view!=nil,"Cannot create actual named shadow color sampling view");
		}
		// Initial named clears are real source dependencies. A dark-sun frame
		// retains its original content and must not be cleared here merely
		// because this renderer cache has not seen that logical object yet.
	}else if(!image||image->target.width!=source->size||image->colored!=source->colored){
		image=std::make_shared<ShadowColorImage>();image->colored=source->colored;
		auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:source->colored?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR8Unorm width:source->size height:source->size mipmapped:NO];
		d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget|MTLTextureUsagePixelFormatView;
		image->target=[lease.Device() newTextureWithDescriptor:d];Require(image->target!=nil,"Cannot allocate original RGB8/R8 shadow color storage");
		const MTLTextureSwizzleChannels channels=source->colored?MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleBlue,MTLTextureSwizzleOne}:MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleOne};
		image->view=[image->target newTextureViewWithPixelFormat:image->target.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1) swizzle:channels];
		Require(image->view!=nil,"Cannot create original RGB1/RRR1 shadow color view");result->impl->clear=true;
	}
	result->impl->image=image;result->impl->clear|=source->redraw;lease.AddAllocation(image->target);lease.AddAllocation(image->view);
	if(!result->DrawCount())return result;
	if(impl->shadowColorLibrary==nil||impl->shadowColorValidation!=validation){
		const auto start=Clock::now();auto library=Library(lease,NATIVE_SHADOW_COLOR_SHADER,"Original transparent projectile shadow color");
		const auto pipeline=[&](MTLPixelFormat format){auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=@"ShadowColorVertex";auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=@"ShadowColorFragment";
			auto d=[MTL4RenderPipelineDescriptor new];d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
			auto a=d.colorAttachments[0];a.pixelFormat=format;a.blendingState=MTL4BlendStateEnabled;a.writeMask=MTLColorWriteMaskRed|MTLColorWriteMaskGreen|MTLColorWriteMaskBlue;
			a.sourceRGBBlendFactor=MTLBlendFactorZero;a.destinationRGBBlendFactor=MTLBlendFactorSourceColor;
			auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;NSError* error=nil;
			auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original transparent shadow pipeline: "+Error(error));return p;};
		auto rgb=pipeline(MTLPixelFormatRGBA8Unorm),gray=pipeline(MTLPixelFormatR8Unorm);
		impl->shadowColorLibrary=library;impl->shadowColorRGBPipeline=rgb;impl->shadowColorGrayPipeline=gray;impl->shadowColorValidation=validation;shaderMs+=Milliseconds(start);
	}
	result->impl->atlas=impl->GetAtlas(lease,source->atlas);lease.Retain(impl->shadowColorLibrary);
	result->impl->pipeline=source->colored?impl->shadowColorRGBPipeline:impl->shadowColorGrayPipeline;lease.Retain(result->impl->pipeline);
	auto vertices=Buffer(lease,source->vertices.data(),source->vertices.size()*sizeof(NativeWorldParticleVertex));result->impl->indices=Buffer(lease,source->indices.data(),source->indices.size()*sizeof(uint32_t));
	ShadowColorUniform uniform{Matrix(source->view),Matrix(source->projection),simd_make_uint4(source->zeroToOneDepth,source->colored,0,0)};auto buffer=Buffer(lease,&uniform,sizeof(uniform));
	auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=2;ad.maxTextureBindCount=1;ad.initializeBindings=YES;NSError* error=nil;
	auto args=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(args!=nil,"Cannot allocate original shadow color arguments: "+Error(error));lease.Retain(args);
	[args setAddress:vertices.gpuAddress atIndex:0];[args setAddress:buffer.gpuAddress atIndex:1];[args setTexture:result->impl->atlas->texture.gpuResourceID atIndex:0];result->impl->arguments=args;
	auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;
	result->impl->depthState=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(result->impl->depthState!=nil,"Cannot allocate original transparent-shadow depth read-only state");lease.Retain(result->impl->depthState);
	return result;
}
CUSSourceImage WorldParticlesGPU::AtlasImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,id<MTLSamplerState> sampler,uint64_t submission)
{
	Require(submission&&sampler&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Original atlas image lacks its actual binding/sampler/submission");
	const auto& object=*binding.object;auto backing=std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(object.backing);
	Require(backing&&backing->atlas&&backing->content==NativeTextureContent::ProducerVersion&&object.storage==NativeTextureStorageKnowledge::Published&&backing->storageGeneration==object.identity,"Original atlas image lacks its actual immutable published storage");
	const auto& pages=backing->atlas->pages;const auto target=pages.size()>1?NativeCUSTextureTarget::Image2DArray:NativeCUSTextureTarget::Image2D;
	Require(object.target==target&&object.allocation.depth==pages.size()&&object.allocation.levels==backing->atlas->requestedMipLevels&&object.allocation.samples==1,"Original atlas logical target/allocation differs from its retained pages");
	const auto& sampling=object.sampling;
	Require(sampling.baseLevel==0&&sampling.maxLevel==int(object.allocation.levels)-1&&sampling.minLod==-1000&&sampling.maxLod==1000&&sampling.lodBias==0&&sampling.compareMode==0,"Original atlas changed LOD/comparison needs its actual consumer");
	auto atlas=impl->GetAtlas(lease,backing->atlas);
	Require(object.allocation.width==atlas->texture.width&&object.allocation.height==atlas->texture.height,"Original atlas image extent differs from its published allocation");
	Require(impl->sourceSubmission==submission,"Original atlas image was resolved outside its actual common preparation scope");
	if(std::find(impl->sourceAtlases.begin(),impl->sourceAtlases.end(),atlas)==impl->sourceAtlases.end())impl->sourceAtlases.push_back(atlas);
	CUSSourceImage image;image.texture=target==NativeCUSTextureTarget::Image2D?atlas->image2D:atlas->texture;image.sampler=sampler;
	image.owner=std::make_shared<RetainedAtlasImage>(RetainedAtlasImage{binding.object,atlas});lease.Retain(sampler);
	image.encodeDependencies=[atlas,submission](id<MTL4CommandBuffer> command){const std::array atlases{atlas};EncodeAtlasMipmaps(atlases,command,submission);};
	image.requireReadable=[atlas,submission]{Require(atlas->initialized||atlas->encodedSubmission==submission,"Original atlas image sampled before its actual same-submission mip producer");};
	image.didComplete=[atlas,submission]{const std::array atlases{atlas};CompleteAtlasMipmaps(atlases,submission);};
	return image;
}
void WorldParticlesGPU::PrepareAtlasImages(uint64_t submission)
{
	Require(submission,"Original atlas image preparation lacks its actual common submission");
	if(impl->sourceSubmission==submission)return;
	for(const auto& old:impl->sourceAtlases)Require(old->initialized||!old->encodedSubmission,"Original named atlas image still belongs to unretired work");
	impl->sourceAtlases.clear();impl->sourceSubmission=submission;
}
void WorldParticlesGPU::EncodeAtlasImages(id<MTL4CommandBuffer> command,uint64_t submission)
{
	if(impl->sourceAtlases.empty())return;
	Require(impl->sourceSubmission==submission,"Original atlas image producer has another common submission");EncodeAtlasMipmaps(impl->sourceAtlases,command,submission);
}
void WorldParticlesGPU::CompleteAtlasImages(uint64_t submission)
{
	if(impl->sourceAtlases.empty())return;
	Require(impl->sourceSubmission==submission,"Original atlas image retirement has another common submission");CompleteAtlasMipmaps(impl->sourceAtlases,submission);impl->sourceAtlases.clear();impl->sourceSubmission=0;
}
}
