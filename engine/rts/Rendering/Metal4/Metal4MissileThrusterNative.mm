/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4MissileThrusterNative.h"
#include <map>
#include <tuple>
#include "NativeMissileThrusterShaders.h"
#include "Metal4WorldViewport.h"
#include "Rendering/Backend/NativeMissileThrusterRecorder.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 cameraViewProjection,cameraViewInverse;
	simd_float4 timeInfo;
	std::array<float,12> defines{};
	simd_uint4 options;
};
static_assert(sizeof(Uniform)==208);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,std::size_t size)
{
	Require(size && size<=lease.Device().maxBufferLength,"Original MissileThruster buffer exceeds actual device capacity");
	auto result=[lease.Device() newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original MissileThruster immutable GPU storage"); lease.AddAllocation(result); return result;
}
simd_float4x4 Matrix(const std::array<float,16>& values)
{
	simd_float4x4 result; std::memcpy(&result,values.data(),sizeof(result)); return result;
}
}
struct PreparedMissileThruster::Impl {
	struct Draw {
		std::uint64_t sequence=0;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		std::uint32_t instanceCount=0,vertexCount=6;
		MTLIndexType indexType=MTLIndexTypeUInt16;
		bool indexed=false;
	};
	std::vector<Draw> draws;
	MTLViewport viewport{}; MTLScissorRect scissor{};
};
PreparedMissileThruster::PreparedMissileThruster():impl(std::make_unique<Impl>()) {}
PreparedMissileThruster::~PreparedMissileThruster()=default;
std::size_t PreparedMissileThruster::DrawCount() const { return impl->draws.size(); }
std::uint64_t PreparedMissileThruster::SequenceAt(std::size_t index) const { return impl->draws.at(index).sequence; }
void PreparedMissileThruster::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const
{
	const auto& draw=impl->draws.at(index);
	[encoder setViewport:impl->viewport]; [encoder setScissorRect:impl->scissor];
	[encoder setRenderPipelineState:draw.pipeline]; [encoder setDepthStencilState:draw.depth];
	[encoder setDepthClipMode:MTLDepthClipModeClip]; [encoder setCullMode:MTLCullModeNone];
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise]; [encoder setTriangleFillMode:MTLTriangleFillModeFill];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	if (draw.indexed) [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.vertexCount indexType:draw.indexType indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length instanceCount:draw.instanceCount];
	else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.vertexCount instanceCount:draw.instanceCount];
}
struct MissileThrusterGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	std::array<id<MTLRenderPipelineState>,7> pipelines{};
	__strong id<MTLDepthStencilState> depth=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid;
	bool validation=false;
 unsigned samples=1;
 using Programs=std::tuple<id<MTLLibrary>,decltype(pipelines),id<MTLDepthStencilState>>;
 std::map<std::tuple<MTLPixelFormat,bool,unsigned>,Programs> variants;

	struct CachedBuffer { std::weak_ptr<const NativeBufferSnapshot> owner; __strong id<MTLBuffer> buffer=nil; };
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source,std::size_t usedBytes)
	{
		Require(usedBytes && usedBytes<=source->bytes.size(),"Original MissileThruster GPU read prefix exceeds immutable source");
		auto& entry=buffers[source.get()];
		if (entry.owner.expired() || entry.buffer.length<usedBytes) { auto next=Upload(lease,source->bytes.data(),usedBytes); entry={source,next}; }
		else lease.AddAllocation(entry.buffer);
		return entry.buffer;
	}
};
MissileThrusterGPU::MissileThrusterGPU():impl(std::make_unique<Impl>()) {}
MissileThrusterGPU::~MissileThrusterGPU() { if (impl->library!=nil) SetNativeMissileThrusterConsumerAvailable(false); }
void MissileThrusterGPU::ClearResources() { impl->buffers.clear(); }
void MissileThrusterGPU::Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& milliseconds,unsigned samples)
{
	const auto key=std::make_tuple(colorFormat,validation,lease.SampleShadingKey(samples));
 if(auto found=impl->variants.find(key);found!=impl->variants.end()){
  std::tie(impl->library,impl->pipelines,impl->depth)=found->second;
  impl->format=colorFormat;impl->validation=validation;impl->samples=samples;return;
 }
	const auto start=Clock::now();
	auto descriptor=[MTL4LibraryDescriptor new]; descriptor.name=@"Original source-paired MissileThrusterGL4 legacy and31657 VS/FS pairs"; descriptor.source=String(missileThruster::SHADER);
	auto options=[MTLCompileOptions new]; options.mathMode=MTLMathModeSafe; options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise; descriptor.options=options;
	NSError* error=nil; auto library=lease.NewLibrary(descriptor,&error);
	Require(library!=nil,"Cannot compile original MissileThruster native program: "+Error(error));
 std::array<id<MTLRenderPipelineState>,7> pipelines{};
 constexpr std::array<const char*,7> vertexNames{"MissileThrusterFlameVertex","MissileThrusterCrossFlameVertex","MissileThrusterCrossSectionVertex","MissileThrusterGlowVertex","MissileThrusterFlame31657Vertex","MissileThrusterCrossSection31657Vertex","MissileThrusterGlow31657Vertex"};
 constexpr std::array<const char*,7> fragmentNames{"MissileThrusterFlameFragment","MissileThrusterFlameFragment","MissileThrusterCrossSectionFragment","MissileThrusterGlowFragment","MissileThrusterFlameFragment","MissileThrusterCrossSectionFragment","MissileThrusterGlowFragment"};
 for(unsigned i=0;i<7;++i){
  auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=String(vertexNames[i]);
  auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=String(fragmentNames[i]);
  auto pipelineDescriptor=[MTL4RenderPipelineDescriptor new];pipelineDescriptor.rasterSampleCount=samples;pipelineDescriptor.vertexFunctionDescriptor=vertex;pipelineDescriptor.fragmentFunctionDescriptor=fragment;pipelineDescriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
  auto color=pipelineDescriptor.colorAttachments[0];color.pixelFormat=colorFormat;color.blendingState=MTL4BlendStateEnabled;color.sourceRGBBlendFactor=color.sourceAlphaBlendFactor=MTLBlendFactorOne;color.destinationRGBBlendFactor=color.destinationAlphaBlendFactor=MTLBlendFactorOne;
  auto validationOptions=[MTL4PipelineOptions new];validationOptions.shaderValidation=ShaderValidationMode(validation);pipelineDescriptor.options=validationOptions;
  pipelines[i]=lease.NewRenderPipeline(pipelineDescriptor,&error);Require(pipelines[i]!=nil,"Cannot compile actual original MissileThruster additive PSO: "+Error(error));
 }
	auto depthDescriptor=[MTLDepthStencilDescriptor new]; depthDescriptor.depthCompareFunction=MTLCompareFunctionLessEqual; depthDescriptor.depthWriteEnabled=NO;
	auto depth=[lease.Device() newDepthStencilStateWithDescriptor:depthDescriptor]; Require(depth!=nil,"Cannot create original MissileThruster depth state");
	// Publish this cache atomically after every actual shader/state creation.
	// Consumer availability remains the root's paired-hook publication.
	impl->library=library; impl->pipelines=pipelines; impl->depth=depth; impl->format=colorFormat; impl->validation=validation;impl->samples=samples;impl->variants.emplace(key,Impl::Programs{impl->library,impl->pipelines,impl->depth});
	milliseconds+=Milliseconds(start);
}
std::unique_ptr<PreparedMissileThruster> MissileThrusterGPU::Prepare(RuntimeLease& lease,const NativeMissileThrusterFrame& frame,
	const WorldCamera& viewCamera,std::uint32_t width,std::uint32_t height,
	std::uint32_t logicalWidth,std::uint32_t logicalHeight,ScreenGPU& images,const PreparedScreenFrame* producers,
	MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,bool validation,double& milliseconds,unsigned samples)
{
	if (frame.draws.empty()) return {};
	if(viewCamera.viewport[2]==0 || viewCamera.viewport[3]==0)return {};
	auto mapped=MapWorldViewport(viewCamera.viewport,logicalWidth,logicalHeight,width,height);if(!mapped)return {};
	Require(depthFormat==MTLPixelFormatDepth32Float || depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original MissileThruster needs actual scene depth");
	Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original MissileThruster camera clip convention unavailable");
	for (const auto* matrix:{&frame.cameraViewProjection,&frame.cameraViewInverse}) for (float value:*matrix) Require(std::isfinite(value),"Original MissileThruster camera is nonfinite");
	for (float value:frame.timeInfo) Require(std::isfinite(value),"Original MissileThruster frame interpolation is nonfinite");
	for (const auto& draw:frame.draws) ValidateNativeMissileThrusterDraw(draw);
	Initialize(lease,colorFormat,validation,milliseconds,samples); lease.Retain(impl->library); for(auto pipeline:impl->pipelines)lease.Retain(pipeline); lease.Retain(impl->depth);
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});
	auto result=std::unique_ptr<PreparedMissileThruster>(new PreparedMissileThruster);
	result->impl->viewport={mapped->viewport[0],mapped->viewport[1],mapped->viewport[2],mapped->viewport[3],0,1};
	result->impl->scissor={mapped->scissor[0],mapped->scissor[1],mapped->scissor[2],mapped->scissor[3]};
	ScreenSampler sampler; sampler.filter=ScreenFilter::Linear; sampler.wrapS=sampler.wrapT=ScreenWrap::Repeat; sampler.mipFilter=ScreenMipFilter::Linear;
	for (const auto& draw:frame.draws) {
		if (!draw.instanceCount) continue;
		auto vertices=impl->Buffer(lease,draw.vertices,draw.vertexCount*16);
		auto instances=impl->Buffer(lease,draw.instances,std::size_t(draw.instanceCount)*draw.instances->layout.stride);
		// The original helper can bind an index object yet issue DrawArrays.
		// Retain its CPU contract but upload only storage actually read by this draw.
		auto indices=draw.indexed?impl->Buffer(lease,draw.indices,draw.vertexCount*draw.indices->layout.stride):nil;
		Uniform uniform{}; uniform.cameraViewProjection=Matrix(frame.cameraViewProjection); uniform.cameraViewInverse=Matrix(frame.cameraViewInverse);
		std::memcpy(&uniform.timeInfo,frame.timeInfo.data(),16); uniform.defines=draw.defines;
		uniform.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne,0,0,0);
		auto constants=Upload(lease,&uniform,sizeof(uniform));
		auto descriptor=[MTL4ArgumentTableDescriptor new]; descriptor.maxBufferBindCount=3; descriptor.maxTextureBindCount=1; descriptor.maxSamplerStateBindCount=1; descriptor.initializeBindings=YES;
		NSError* error=nil; auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error]; Require(arguments!=nil,"Cannot allocate original MissileThruster native arguments: "+Error(error)); lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0]; [arguments setAddress:instances.gpuAddress atIndex:1]; [arguments setAddress:constants.gpuAddress atIndex:2];
  const auto image=images.PrepareImage(lease,draw.texture,sampler,producers);[arguments setTexture:image.texture.gpuResourceID atIndex:0];[arguments setSamplerState:image.sampler.gpuResourceID atIndex:0];
		result->impl->draws.push_back({draw.sequence,impl->pipelines.at(unsigned(draw.program)),impl->depth,arguments,indices,draw.instanceCount,draw.vertexCount,draw.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32,draw.indexed});
	}
	return result;
}
}
