/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4FireSmokeNative.h"
#include <map>
#include <tuple>
#include "NativeFireSmokeShaders.h"
#include "Metal4WorldViewport.h"
#include "Rendering/Backend/NativeFireSmokeRecorder.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 cameraViewProjection,cameraViewInverse;
	simd_float4 timeInfo,windVelocity,highlightPass;
	std::array<float,8> defines{};
	simd_uint4 options;
};
static_assert(sizeof(Uniform)==224);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,std::size_t size)
{
	Require(size && size<=lease.Device().maxBufferLength,"Original FireSmoke buffer exceeds actual device capacity");
	auto result=[lease.Device() newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original FireSmoke immutable GPU storage"); lease.AddAllocation(result); return result;
}
simd_float4x4 Matrix(const std::array<float,16>& values)
{
	simd_float4x4 result; std::memcpy(&result,values.data(),sizeof(result)); return result;
}
}
struct PreparedFireSmoke::Impl {
	struct Draw {
		std::uint64_t sequence=0;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		std::uint32_t instanceCount=0;
		MTLIndexType indexType=MTLIndexTypeUInt16;
		bool indexed=false;
	};
	std::vector<Draw> draws;
	MTLViewport viewport{}; MTLScissorRect scissor{};
};
PreparedFireSmoke::PreparedFireSmoke():impl(std::make_unique<Impl>()) {}
PreparedFireSmoke::~PreparedFireSmoke()=default;
std::size_t PreparedFireSmoke::DrawCount() const { return impl->draws.size(); }
std::uint64_t PreparedFireSmoke::SequenceAt(std::size_t index) const { return impl->draws.at(index).sequence; }
void PreparedFireSmoke::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const
{
	const auto& draw=impl->draws.at(index);
	[encoder setViewport:impl->viewport]; [encoder setScissorRect:impl->scissor];
	[encoder setRenderPipelineState:draw.pipeline]; [encoder setDepthStencilState:draw.depth];
	[encoder setDepthClipMode:MTLDepthClipModeClip]; [encoder setCullMode:MTLCullModeNone];
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise]; [encoder setTriangleFillMode:MTLTriangleFillModeFill];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	if (draw.indexed) [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:draw.indexType indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length instanceCount:draw.instanceCount];
	else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:draw.instanceCount];
}
struct FireSmokeGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid;
	bool validation=false;
 unsigned samples=1;
 using Programs=std::tuple<id<MTLLibrary>,decltype(pipeline),id<MTLDepthStencilState>>;
 std::map<std::tuple<MTLPixelFormat,bool,unsigned>,Programs> variants;

	struct CachedBuffer { std::weak_ptr<const NativeBufferSnapshot> owner; __strong id<MTLBuffer> buffer=nil; };
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source,std::size_t usedBytes)
	{
		Require(usedBytes && usedBytes<=source->bytes.size(),"Original FireSmoke GPU read prefix exceeds immutable source");
		auto& entry=buffers[source.get()];
		if (entry.owner.expired() || entry.buffer.length<usedBytes) { auto next=Upload(lease,source->bytes.data(),usedBytes); entry={source,next}; }
		else lease.AddAllocation(entry.buffer);
		return entry.buffer;
	}
};
FireSmokeGPU::FireSmokeGPU():impl(std::make_unique<Impl>()) {}
FireSmokeGPU::~FireSmokeGPU() { if (impl->library!=nil) SetNativeFireSmokeConsumerAvailable(false); }
void FireSmokeGPU::ClearResources() { impl->buffers.clear(); }
void FireSmokeGPU::Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& milliseconds,unsigned samples)
{
	const auto key=std::make_tuple(colorFormat,validation,lease.SampleShadingKey(samples));
 if(auto found=impl->variants.find(key);found!=impl->variants.end()){
  std::tie(impl->library,impl->pipeline,impl->depth)=found->second;
  impl->format=colorFormat;impl->validation=validation;impl->samples=samples;return;
 }
	const auto start=Clock::now();
	auto descriptor=[MTL4LibraryDescriptor new]; descriptor.name=@"Original test30770 FireSmokeGL4 VS/FS"; descriptor.source=String(fireSmoke::SHADER);
	auto options=[MTLCompileOptions new]; options.mathMode=MTLMathModeSafe; options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise; descriptor.options=options;
	NSError* error=nil; auto library=lease.NewLibrary(descriptor,&error);
	Require(library!=nil,"Cannot compile original FireSmoke native program: "+Error(error));
	auto vertex=[MTL4LibraryFunctionDescriptor new]; vertex.library=library; vertex.name=@"FireSmokeVertexMain";
	auto fragment=[MTL4LibraryFunctionDescriptor new]; fragment.library=library; fragment.name=@"FireSmokeFragmentMain";
	auto pipelineDescriptor=[MTL4RenderPipelineDescriptor new];pipelineDescriptor.rasterSampleCount=samples; pipelineDescriptor.vertexFunctionDescriptor=vertex; pipelineDescriptor.fragmentFunctionDescriptor=fragment; pipelineDescriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
	auto color=pipelineDescriptor.colorAttachments[0]; color.pixelFormat=colorFormat; color.blendingState=MTL4BlendStateEnabled;
	color.sourceRGBBlendFactor=color.sourceAlphaBlendFactor=MTLBlendFactorOne;
	color.destinationRGBBlendFactor=color.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
	auto validationOptions=[MTL4PipelineOptions new]; validationOptions.shaderValidation=ShaderValidationMode(validation); pipelineDescriptor.options=validationOptions;
	auto pipeline=lease.NewRenderPipeline(pipelineDescriptor,&error);
	Require(pipeline!=nil,"Cannot compile original FireSmoke real premultiplied PSO: "+Error(error));
	auto depthDescriptor=[MTLDepthStencilDescriptor new]; depthDescriptor.depthCompareFunction=MTLCompareFunctionLessEqual; depthDescriptor.depthWriteEnabled=NO;
	auto depth=[lease.Device() newDepthStencilStateWithDescriptor:depthDescriptor]; Require(depth!=nil,"Cannot create original FireSmoke depth state");
	// Publish this cache atomically after every actual shader/state creation.
	// Consumer availability remains the root's paired-hook publication.
	impl->library=library; impl->pipeline=pipeline; impl->depth=depth; impl->format=colorFormat; impl->validation=validation;impl->samples=samples;impl->variants.emplace(key,Impl::Programs{impl->library,impl->pipeline,impl->depth});
	milliseconds+=Milliseconds(start);
}
std::unique_ptr<PreparedFireSmoke> FireSmokeGPU::Prepare(RuntimeLease& lease,const NativeFireSmokeFrame& frame,
	const WorldCamera& viewCamera,std::uint32_t width,std::uint32_t height,
	std::uint32_t logicalWidth,std::uint32_t logicalHeight,ScreenGPU& images,const PreparedScreenFrame* producers,
	MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,bool validation,double& milliseconds,unsigned samples)
{
	if (frame.draws.empty()) return {};
	if(viewCamera.viewport[2]==0 || viewCamera.viewport[3]==0)return {};
	auto mapped=MapWorldViewport(viewCamera.viewport,logicalWidth,logicalHeight,width,height);if(!mapped)return {};
	Require(depthFormat==MTLPixelFormatDepth32Float || depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original FireSmoke needs actual scene depth");
	Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original FireSmoke camera clip convention unavailable");
	for (const auto* matrix:{&frame.cameraViewProjection,&frame.cameraViewInverse}) for (float value:*matrix) Require(std::isfinite(value),"Original FireSmoke camera is nonfinite");
	for (float value:frame.timeInfo) Require(std::isfinite(value),"Original FireSmoke frame interpolation is nonfinite");
	for (const auto& draw:frame.draws) ValidateNativeFireSmokeDraw(draw);
	Initialize(lease,colorFormat,validation,milliseconds,samples); lease.Retain(impl->library); lease.Retain(impl->pipeline); lease.Retain(impl->depth);
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});
	auto result=std::unique_ptr<PreparedFireSmoke>(new PreparedFireSmoke);
	result->impl->viewport={mapped->viewport[0],mapped->viewport[1],mapped->viewport[2],mapped->viewport[3],0,1};
	result->impl->scissor={mapped->scissor[0],mapped->scissor[1],mapped->scissor[2],mapped->scissor[3]};
	ScreenSampler sampler; sampler.filter=ScreenFilter::Linear; sampler.wrapS=sampler.wrapT=ScreenWrap::Repeat; sampler.mipFilter=ScreenMipFilter::Linear;
	for (const auto& draw:frame.draws) {
		if (!draw.instanceCount) continue;
		auto vertices=impl->Buffer(lease,draw.vertices,6*16);
		auto instances=impl->Buffer(lease,draw.instances,std::size_t(draw.instanceCount)*64);
		// The original helper can bind an index object yet issue DrawArrays.
		// Retain its CPU contract but upload only storage actually read by this draw.
		auto indices=draw.indexed?impl->Buffer(lease,draw.indices,6*draw.indices->layout.stride):nil;
		Uniform uniform{}; uniform.cameraViewProjection=Matrix(frame.cameraViewProjection); uniform.cameraViewInverse=Matrix(frame.cameraViewInverse);
		std::memcpy(&uniform.timeInfo,frame.timeInfo.data(),16); std::memcpy(&uniform.windVelocity,draw.windVelocity.data(),12); std::memcpy(&uniform.highlightPass,draw.highlightPass.data(),16); uniform.defines=draw.defines;
		uniform.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne,0,0,0);
		auto constants=Upload(lease,&uniform,sizeof(uniform));
		auto descriptor=[MTL4ArgumentTableDescriptor new]; descriptor.maxBufferBindCount=3; descriptor.maxTextureBindCount=2; descriptor.maxSamplerStateBindCount=1; descriptor.initializeBindings=YES;
		NSError* error=nil; auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error]; Require(arguments!=nil,"Cannot allocate original FireSmoke native arguments: "+Error(error)); lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0]; [arguments setAddress:instances.gpuAddress atIndex:1]; [arguments setAddress:constants.gpuAddress atIndex:2];
		for (unsigned n=0;n<2;++n) { const auto image=images.PrepareImage(lease,draw.textures[n],sampler,producers); [arguments setTexture:image.texture.gpuResourceID atIndex:n]; if (!n) [arguments setSamplerState:image.sampler.gpuResourceID atIndex:0]; }
		result->impl->draws.push_back({draw.sequence,impl->pipeline,impl->depth,arguments,indices,draw.instanceCount,draw.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32,draw.indexed});
	}
	return result;
}
}
