/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainGroundClear.h"
#include "NativeSampleShadingShaders.h"
#include "Metal4CUSSource.h"
#include "Metal4ScreenScissor.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

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
	};
	std::vector<Draw> draws;
	__strong id<MTLLibrary> library=nil;
	MTLViewport viewport{};
	size_t next=0;
};
PreparedTerrainGroundClear::PreparedTerrainGroundClear():impl(std::make_unique<Impl>()){}
PreparedTerrainGroundClear::~PreparedTerrainGroundClear()=default;
size_t PreparedTerrainGroundClear::DrawCount()const{return impl->draws.size();}
uint64_t PreparedTerrainGroundClear::SequenceAt(size_t index)const
{
	Require(index<impl->draws.size(),"Original water ground clear sequence index is out of range");return impl->draws[index].sequence;
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
	std::array<__strong id<MTLRenderPipelineState>,256> pipelines{};
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
		// NativeUIRecorder::ClearAttachments already clamps glClearColor/Depth.
		// Preserve those recorded values, rather than reinterpret depth via camera.
		for(auto channel:clear.values.color)Require(std::isfinite(channel)&&channel>=0&&channel<=1,"Original water ground clear color was not source-clamped");
		Require(std::isfinite(clear.values.depth)&&clear.values.depth>=0&&clear.values.depth<=1,"Original water ground clear depth was not source-clamped");
		unsigned colorMask=0;if(bits&ScreenAttachmentColor)for(unsigned i=0;i<4;++i)if(clear.colorWrite[i])colorMask|=1u<<i;
		unsigned combinedMask=0;if(target.combinedColor!=MTLPixelFormatInvalid&&(bits&ScreenAttachmentColor))for(unsigned i=0;i<4;++i)if(clear.combinedColorWrite[i])combinedMask|=1u<<i;
		const unsigned pipelineKey=colorMask|(combinedMask<<4);
		const bool depthWrite=(bits&ScreenAttachmentDepth)&&clear.depthWrite;
  const bool stencilWrite=(bits&ScreenAttachmentStencil)&&target.stencil!=MTLPixelFormatInvalid&&(clear.frontStencilWriteMask&255u);
		PreparedTerrainGroundClear::Impl::Draw draw;draw.sequence=clear.sequence;draw.scissor=ClearScissor(clear,target);draw.stencilReference=clear.values.stencil;
		// A missing attachment has no effect. The original front stencil mask
  // applies to glClear; consume its ordinal even when all writes are masked.
		if(draw.scissor&&(colorMask||combinedMask||depthWrite||stencilWrite)){
			if(result->impl->library==nil){
				const auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(ClearShader);
				descriptor.name=@"Original ordered water ground attachment clears";
				auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;descriptor.options=options;NSError* error=nil;
				result->impl->library=lease.NewLibrary(descriptor,&error);
				Require(result->impl->library!=nil,"Cannot compile original water ground clear library: "+Error(error));
				milliseconds+=Milliseconds(start);lease.Retain(result->impl->library);
			}
			if(pipelines[pipelineKey]==nil)pipelines[pipelineKey]=ClearPipeline(lease,result->impl->library,colorMask,target,validation,milliseconds,combinedMask);
			draw.pipeline=pipelines[pipelineKey];draw.depth=ClearDepth(lease,depthWrite,stencilWrite,clear.frontStencilWriteMask);
			const ClearUniform uniform{clear.values.color,{clear.values.depth,0,0,0}};
			draw.uniform=[lease.Device()newBufferWithBytes:&uniform length:sizeof(uniform) options:MTLResourceStorageModeShared];
			Require(draw.uniform!=nil,"Cannot allocate original water ground clear uniform");lease.AddAllocation(draw.uniform);
			auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=1;
			descriptor.maxTextureBindCount=0;descriptor.maxSamplerStateBindCount=0;descriptor.initializeBindings=YES;NSError* error=nil;
			draw.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];
			Require(draw.arguments!=nil,"Cannot allocate original water ground clear argument table: "+Error(error));lease.Retain(draw.arguments);
			[draw.arguments setAddress:draw.uniform.gpuAddress atIndex:0];
		}
		result->impl->draws.push_back(std::move(draw));
	}
	return result;
}
} // namespace rendering::metal4
