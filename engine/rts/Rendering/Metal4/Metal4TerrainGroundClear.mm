/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainGroundClear.h"
#include "NativeSampleShadingShaders.h"
#include "Metal4CUSSource.h"
#include "Metal4ScreenScissor.h"
#include "Rendering/Backend/NativeInitialAttachmentClear182.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <atomic>

namespace rendering::metal4 {
namespace {
constexpr const char* ClearShader=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ClearUniform { float4 color; float4 depth; };
struct ClearVertex { float4 position [[position]]; };
vertex ClearVertex TerrainGroundClearVertex(uint index [[vertex_id]]) {
 const float2 triangle[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(triangle[index],0,1)};
}
struct ClearFragment { float4 color [[color(0)]]; float depth [[depth(any)]]; };
struct ClearMRTFragment {
 float4 c0 [[color(0)]];
 float4 c1 [[color(1)]];
 float4 c2 [[color(2)]];
 float4 c3 [[color(3)]];
 float4 c4 [[color(4)]];
float depth [[depth(any)]];};
fragment ClearMRTFragment TerrainMapClearFragment(constant ClearUniform& value [[buffer(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 return {value.color,value.color,value.color,value.color,value.color,value.depth.x};
}
struct ClearCombinedFragment {
 float4 c0 [[color(0)]];float4 c1 [[color(1)]];float4 c2 [[color(2)]];
 float4 c3 [[color(3)]];float4 c4 [[color(4)]];float4 c5 [[color(5)]];float depth [[depth(any)]];
};
fragment ClearCombinedFragment TerrainCombinedClearFragment(constant ClearUniform& value [[buffer(0)]]) {
 return {value.color,value.color,value.color,value.color,value.color,value.color,value.depth.x};
}
fragment ClearFragment TerrainGroundClearFragment(constant ClearUniform& value [[buffer(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 return {value.color,value.depth.x};
}
)MSL";
// One bounded immutable program cache per preparation thread. Device/compiler
// changes discard cache ownership; RuntimeLease and prepared draws independently
// retain all programs submitted to the GPU, including poisoned requests.
struct ClearProgramCache {
	__strong id<MTLDevice> device=nil;
	__strong id<MTL4Compiler> compiler=nil;
	__strong id<MTLLibrary> library=nil;
	std::map<std::array<uint64_t,12>,id<MTLRenderPipelineState>> pipelines;
};
struct ClearUniform {std::array<float,4> color,depth;};
static_assert(sizeof(ClearUniform)==32);

std::optional<MTLScissorRect> ClearScissor(const NativeWorldAttachmentClear& source,const CUSSourceTarget& target)
{
	// Original glClear ignores viewport. Clamp in the complete logical target,
	// convert bottom-left to Metal top-left, then map to actual attachment pixels.
	const auto logical=ClampScreenScissor(source.scissor,target.logicalWidth,target.logicalHeight);
	if(!logical)return std::nullopt;
	const auto floorScale=[](uint64_t value,uint32_t physical,uint32_t logicalExtent){return value*physical/logicalExtent;};
	const auto ceilScale=[](uint64_t value,uint32_t physical,uint32_t logicalExtent){return (value*physical+logicalExtent-1)/logicalExtent;};
	const auto x=floorScale(logical->x,target.width,target.logicalWidth);
	const auto y=floorScale(logical->y,target.height,target.logicalHeight);
	const auto right=ceilScale(uint64_t(logical->x)+logical->width,target.width,target.logicalWidth);
	const auto bottom=ceilScale(uint64_t(logical->y)+logical->height,target.height,target.logicalHeight);
	if(right<=x||bottom<=y)return std::nullopt;
	Require(right<=target.width&&bottom<=target.height,"Original water ground clear scissor exceeds its actual attachment");
	return MTLScissorRect{NSUInteger(x),NSUInteger(y),NSUInteger(right-x),NSUInteger(bottom-y)};
}
id<MTLRenderPipelineState> ClearPipeline(RuntimeLease& lease,id<MTLLibrary> library,unsigned mask,
	const CUSSourceTarget& target,bool validation,double& milliseconds,unsigned combinedMask)
{
	const auto start=Clock::now();
	auto descriptor=[MTL4RenderPipelineDescriptor new];
	auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"TerrainGroundClearVertex";
	auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=target.combinedColor!=MTLPixelFormatInvalid?@"TerrainCombinedClearFragment":target.colors[1]!=MTLPixelFormatInvalid?@"TerrainMapClearFragment":@"TerrainGroundClearFragment";
	descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
	descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
	descriptor.rasterSampleCount=target.samples;descriptor.rasterizationEnabled=YES;descriptor.maxVertexAmplificationCount=1;
	descriptor.alphaToCoverageState=MTL4AlphaToCoverageStateDisabled;
	descriptor.alphaToOneState=MTL4AlphaToOneStateDisabled;
	descriptor.colorAttachmentMappingState=MTL4LogicalToPhysicalColorAttachmentMappingStateIdentity;
	for(unsigned attachment=0;attachment<target.colors.size();++attachment)if(target.colors[attachment]!=MTLPixelFormatInvalid){
	auto color=descriptor.colorAttachments[attachment];color.pixelFormat=target.colors[attachment];
	color.blendingState=MTL4BlendStateDisabled;
	color.writeMask=MTLColorWriteMask((mask&1?MTLColorWriteMaskRed:0)|(mask&2?MTLColorWriteMaskGreen:0)|
		(mask&4?MTLColorWriteMaskBlue:0)|(mask&8?MTLColorWriteMaskAlpha:0));
 }
	if(target.combinedColor!=MTLPixelFormatInvalid){auto color=descriptor.colorAttachments[5];color.pixelFormat=target.combinedColor;color.blendingState=MTL4BlendStateDisabled;
		color.writeMask=MTLColorWriteMask((combinedMask&1?MTLColorWriteMaskRed:0)|(combinedMask&2?MTLColorWriteMaskGreen:0)|(combinedMask&4?MTLColorWriteMaskBlue:0)|(combinedMask&8?MTLColorWriteMaskAlpha:0));}
	// Metal4 render PSOs do not have the legacy depth/stencil pixel-format fields.
	// Actual depth/stencil attachment format/sample agreement is the target contract.
	auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);
	descriptor.options=options;NSError* error=nil;
	auto pipeline=lease.NewRenderPipeline(descriptor,&error,false);
	Require(pipeline!=nil,"Cannot compile original water ground clear pipeline: "+Error(error));
	milliseconds+=Milliseconds(start);lease.Retain(pipeline);return pipeline;
}
id<MTLDepthStencilState> ClearDepth(RuntimeLease& lease,bool depthWrite,bool stencilWrite,uint32_t stencilMask)
{
	auto descriptor=[MTLDepthStencilDescriptor new];descriptor.depthCompareFunction=MTLCompareFunctionAlways;
	descriptor.depthWriteEnabled=depthWrite;
 if(stencilWrite) {
  auto stencil=[MTLStencilDescriptor new];stencil.stencilCompareFunction=MTLCompareFunctionAlways;
  stencil.stencilFailureOperation=stencil.depthFailureOperation=stencil.depthStencilPassOperation=MTLStencilOperationReplace;
  stencil.readMask=~0u;stencil.writeMask=stencilMask;
  descriptor.frontFaceStencil=stencil;descriptor.backFaceStencil=stencil;
 }
	auto state=[lease.Device()newDepthStencilStateWithDescriptor:descriptor];
	Require(state!=nil,"Cannot allocate original water ground clear depth/stencil state");lease.Retain(state);return state;
}
} // namespace

struct PreparedTerrainGroundClear::Impl {
	struct Draw {
		uint64_t sequence=0;
  uint32_t stencilReference=0;
		std::optional<MTLScissorRect> scissor;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> uniform=nil;
		std::optional<NativeInitialAttachmentClear182> attachmentClear182;
		NativeWorldDrawStage stage=NativeWorldDrawStage::Ground;
		bool usedAttachmentClear182=false;
		uint64_t sourceDrawFrame182=0;
		int64_t simulationFrame182=0;
	};
	std::vector<Draw> draws;
	__strong id<MTLLibrary> library=nil;
	MTLViewport viewport{};
	CUSSourceTarget target;
	size_t next=0;
};
PreparedTerrainGroundClear::PreparedTerrainGroundClear():impl(std::make_unique<Impl>()){}
PreparedTerrainGroundClear::~PreparedTerrainGroundClear()=default;
size_t PreparedTerrainGroundClear::DrawCount()const{return impl->draws.size();}
uint64_t PreparedTerrainGroundClear::SequenceAt(size_t index)const
{
	Require(index<impl->draws.size(),"Original water ground clear sequence index is out of range");return impl->draws[index].sequence;
}
bool PreparedTerrainGroundClear::TryInitialAttachmentClear182(MTL4RenderPassDescriptor* pass,
	uint64_t sourceDrawFrame,int64_t simulationFrame)
{
	Require(impl->next==0&&!impl->draws.empty()&&!impl->draws.front().usedAttachmentClear182,
		"Initial attachment clear must precede its first original source event exactly once");
	auto& draw=impl->draws.front();const auto& target=impl->target;
	if(!pass||!draw.attachmentClear182||!draw.pipeline||target.samples!=1||
		pass.defaultRasterSampleCount!=target.samples||pass.renderTargetWidth!=target.width||pass.renderTargetHeight!=target.height)return false;
	const auto exact=[&](MTLRenderPassAttachmentDescriptor* a,MTLPixelFormat format){
		if(format==MTLPixelFormatInvalid)return !a.texture&&!a.resolveTexture;
		return a.texture&&a.texture.pixelFormat==format&&a.texture.width==target.width&&a.texture.height==target.height&&
			a.texture.sampleCount==1&&!a.level&&!a.depthPlane&&!a.resolveTexture&&
			a.storeAction==MTLStoreActionStore;
	};
	for(unsigned n=0;n<5;++n){
		const auto format=target.colors[n];
		if(format!=MTLPixelFormatInvalid&&format!=MTLPixelFormatRGBA8Unorm&&format!=MTLPixelFormatBGRA8Unorm)return false;
		if(!exact(pass.colorAttachments[n],format))return false;
	}
	if(!exact(pass.colorAttachments[5],target.combinedColor)||
		!exact(pass.colorAttachments[6],MTLPixelFormatInvalid)||!exact(pass.colorAttachments[7],MTLPixelFormatInvalid)||
		!exact(pass.depthAttachment,target.depth)||!exact(pass.stencilAttachment,target.stencil))return false;
	// No descriptor mutation until every target/format gate has passed. Existing
	// callers prove these are their actual named attachments and first event.
	const auto& clear=*draw.attachmentClear182;
	for(unsigned n=0;n<6;++n)if(clear.colors&(1u<<n)){
		auto a=pass.colorAttachments[n];a.loadAction=MTLLoadActionClear;
		a.clearColor=MTLClearColorMake(clear.color[0],clear.color[1],clear.color[2],clear.color[3]);
	}
	if(clear.depth){pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.clearDepth=clear.depthValue;}
	if(clear.stencil){pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.clearStencil=clear.stencilValue;}
	draw.usedAttachmentClear182=true;draw.sourceDrawFrame182=sourceDrawFrame;draw.simulationFrame182=simulationFrame;
	return true;
}
void PreparedTerrainGroundClear::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder!=nil&&index==impl->next&&index<impl->draws.size(),
		"Original water ground clear must consume each source ordinal exactly once in order");
	const auto& draw=impl->draws[index];
	// Consume even a no-op event, without emitting an invalid zero-size scissor.
	++impl->next;
	if(draw.pipeline==nil)return;
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	[encoder setViewport:impl->viewport];[encoder setScissorRect:*draw.scissor];
	[encoder setVertexAmplificationCount:1 viewMappings:nullptr];
	// Original glClear must not filter writes by stored depth. Metal encoders
	// start with depth bounds disabled, and no native producer enables them.
	// Preserve that default: even a 0..1 reset requires Apple10 depth-bounds
	// support and traps on Apple9 (including M4). ClearDepth preserves writes.
	[encoder setDepthClipMode:MTLDepthClipModeClip];
	[encoder setCullMode:MTLCullModeNone];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
 [encoder setStencilFrontReferenceValue:draw.stencilReference backReferenceValue:draw.stencilReference];
	[encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	if(draw.usedAttachmentClear182){
#ifdef RECOIL_METAL4_LIVE_SCENE
		// One receipt per family, only in positive gameplay. No per-frame timers,
		// file writes, additional submission or GPU synchronization.
		static std::atomic<unsigned> reported{0};
		const unsigned family=impl->target.colors[1]!=MTLPixelFormatInvalid?0u:
			draw.stage==NativeWorldDrawStage::Ground?1u:draw.stage==NativeWorldDrawStage::TerrainReflection?2u:
			draw.stage==NativeWorldDrawStage::WaterReflection?3u:4u;
		if(draw.simulationFrame182>0&&!(reported.load(std::memory_order_relaxed)&(1u<<family))&&
			!(reported.fetch_or(1u<<family,std::memory_order_relaxed)&(1u<<family))){const auto& clear=*draw.attachmentClear182;
			LOG("[NativeClear182] family=%u source_frame=%llu simulation_frame=%lld sequence=%llu width=%u height=%u color_mask=%u depth=%u stencil=%u omitted_clear_draws=1 original_ordinal_consumed=1 state_preserved=1 encoded=1 gpu_time=unknown",
				family,(unsigned long long)draw.sourceDrawFrame182,(long long)draw.simulationFrame182,
				(unsigned long long)draw.sequence,impl->target.width,impl->target.height,clear.colors,unsigned(clear.depth),unsigned(clear.stencil));
		}
#endif
		return;
	}
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
}

std::shared_ptr<PreparedTerrainGroundClear> PrepareTerrainGroundClears(RuntimeLease& lease,
	std::span<const NativeWorldAttachmentClear> source,const CUSSourceTarget& target,bool validation,double& milliseconds)
{
	Require(target.width&&target.height&&target.logicalWidth&&target.logicalHeight&&
		target.width<=lease.Capabilities().maxTextureDimension2D&&target.height<=lease.Capabilities().maxTextureDimension2D,
		"Original water ground clear requires actual nonzero physical/logical attachment extents");
	Require((target.samples>=1&&target.samples<=32&&[lease.Device() supportsTextureSampleCount:target.samples])&&target.colors[0]!=MTLPixelFormatInvalid&&
		target.depth==MTLPixelFormatDepth32Float&&(target.stencil==MTLPixelFormatInvalid||target.stencil==MTLPixelFormatStencil8),
		"Original ground clear requires its actual single-color/depth32/stencil target");
	const bool mrt=target.colors[1]!=MTLPixelFormatInvalid;
	Require(target.combinedColor==MTLPixelFormatInvalid||(mrt&&target.combinedColor==MTLPixelFormatRGBA8Unorm),"Optional combined clear requires the actual sixth Map RGBA8 attachment");
 Require(!mrt||(target.samples==1&&target.stencil==MTLPixelFormatInvalid&&std::all_of(target.colors.begin(),target.colors.end(),[](auto f){return f==MTLPixelFormatRGBA8Unorm;})),
  "Original map clear requires the actual five RGBA8 plus D32 MRT target");
	auto result=std::shared_ptr<PreparedTerrainGroundClear>(new PreparedTerrainGroundClear);
	result->impl->viewport={0,0,double(target.width),double(target.height),0,1};
	result->impl->target=target;
	thread_local ClearProgramCache programs;
	if(programs.device!=lease.Device() || programs.compiler!=lease.Compiler()) {
		programs={};programs.device=lease.Device();programs.compiler=lease.Compiler();
	}
	uint64_t previous=0;
	for(const auto& clear:source){
		Require(clear.sequence>previous,"Original water ground clear source sequence is zero, duplicated or unordered");previous=clear.sequence;
		Require((clear.stage==NativeWorldDrawStage::WaterReflection||clear.stage==NativeWorldDrawStage::WaterRefraction||
   clear.stage==NativeWorldDrawStage::Ground||clear.stage==NativeWorldDrawStage::TerrainReflection||
   clear.stage==NativeWorldDrawStage::ModelDeferred)&&
			clear.stage==source.front().stage,"Original water ground clear span crosses actual water attachment stages");
		Require(clear.stage!=NativeWorldDrawStage::ModelDeferred||
			(mrt&&clear.mask==static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth)),
			"Original model clear requires its actual five-color/D32 MRT and color/depth mask");
		const auto bits=unsigned(clear.mask);Require(!(bits&~unsigned(ScreenAttachmentAll)),"Original water ground clear has unknown attachment bits");
		// Preserve finite clear colors until conversion to each attachment format.
		// Depth remains source-clamped; do not reinterpret it via camera.
		for(auto channel:clear.values.color)Require(std::isfinite(channel),"Original water ground clear color is nonfinite");
		Require(std::isfinite(clear.values.depth)&&clear.values.depth>=0&&clear.values.depth<=1,"Original water ground clear depth was not source-clamped");
		unsigned colorMask=0;if(bits&ScreenAttachmentColor)for(unsigned i=0;i<4;++i)if(clear.colorWrite[i])colorMask|=1u<<i;
		unsigned combinedMask=0;if(target.combinedColor!=MTLPixelFormatInvalid&&(bits&ScreenAttachmentColor))for(unsigned i=0;i<4;++i)if(clear.combinedColorWrite[i])combinedMask|=1u<<i;
		const std::array<uint64_t,12> pipelineKey{
			uint64_t(target.colors[0]),uint64_t(target.colors[1]),uint64_t(target.colors[2]),uint64_t(target.colors[3]),uint64_t(target.colors[4]),
			uint64_t(target.combinedColor),uint64_t(target.depth),uint64_t(target.stencil),target.samples,colorMask,combinedMask,uint64_t(validation)};
		const bool depthWrite=(bits&ScreenAttachmentDepth)&&clear.depthWrite;
  const bool stencilWrite=(bits&ScreenAttachmentStencil)&&target.stencil!=MTLPixelFormatInvalid&&(clear.frontStencilWriteMask&255u);
		PreparedTerrainGroundClear::Impl::Draw draw;draw.sequence=clear.sequence;draw.scissor=ClearScissor(clear,target);draw.stencilReference=clear.values.stencil;
		draw.stage=clear.stage;
		draw.attachmentClear182=PlanNativeInitialAttachmentClear182(clear,target.logicalWidth,target.logicalHeight,
			mrt?5u:1u,target.combinedColor!=MTLPixelFormatInvalid,target.stencil!=MTLPixelFormatInvalid);
		// A missing attachment has no effect. The original front stencil mask
  // applies to glClear; consume its ordinal even when all writes are masked.
		if(draw.scissor&&(colorMask||combinedMask||depthWrite||stencilWrite)){
			if(programs.library==nil){
				const auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(ClearShader);
				descriptor.name=@"Original ordered water ground attachment clears";
				auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;descriptor.options=options;NSError* error=nil;
				programs.library=lease.NewLibrary(descriptor,&error);
				Require(programs.library!=nil,"Cannot compile original water ground clear library: "+Error(error));
				milliseconds+=Milliseconds(start);
			}
			result->impl->library=programs.library;lease.Retain(result->impl->library);
			auto found=programs.pipelines.find(pipelineKey);
			if(found==programs.pipelines.end()) {
				auto pipeline=ClearPipeline(lease,result->impl->library,colorMask,target,validation,milliseconds,combinedMask);
				if(programs.pipelines.size()>=256)programs.pipelines.erase(programs.pipelines.begin());
				found=programs.pipelines.emplace(pipelineKey,pipeline).first;
			}
			draw.pipeline=found->second;lease.Retain(draw.pipeline);draw.depth=ClearDepth(lease,depthWrite,stencilWrite,clear.frontStencilWriteMask);
			const ClearUniform uniform{clear.values.color,{clear.values.depth,0,0,0}};
			draw.uniform=[lease.Device()newBufferWithBytes:&uniform length:sizeof(uniform) options:MTLResourceStorageModeShared];
			Require(draw.uniform!=nil,"Cannot allocate original water ground clear uniform");lease.AddAllocation(draw.uniform);
			auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=1;
			descriptor.maxTextureBindCount=0;descriptor.maxSamplerStateBindCount=0;descriptor.initializeBindings=YES;NSError* error=nil;
			draw.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];
			if(draw.arguments==nil)Require(false,"Cannot allocate original water ground clear argument table: "+Error(error));lease.Retain(draw.arguments);
			[draw.arguments setAddress:draw.uniform.gpuAddress atIndex:0];
		}
		result->impl->draws.push_back(std::move(draw));
	}
	return result;
}
} // namespace rendering::metal4
