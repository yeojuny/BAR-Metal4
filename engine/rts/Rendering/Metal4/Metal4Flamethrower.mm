/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Flamethrower.h"
#include "NativeFlamethrowerShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <bit>
#include <cstring>
#include <map>
#include <tuple>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 cameraViewProjection,cameraViewInverse;
	simd_float4 timeInfo,windVelocity;
	std::array<float,12> defines{};
	simd_uint4 options;
};
static_assert(sizeof(Uniform)==224);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,std::size_t size)
{
	Require(size && size<=lease.Device().maxBufferLength,"Original Flamethrower buffer exceeds actual device capacity");
	auto result=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original Flamethrower immutable GPU buffer"); lease.AddAllocation(result); return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch (value) {
		case ScreenBlendEquation::Add:return MTLBlendOperationAdd;
		case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;
		case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;
	}
	throw std::runtime_error("Unknown original Flamethrower blend equation");
}
std::uint64_t PipelineBits(const ScreenBlendState& blend,const std::array<bool,4>& mask)
{
	return uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|
		uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|
		uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
}
}
struct PreparedFlamethrower::Impl {
	struct Draw {
		std::uint64_t sequence=0;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		std::uint32_t indexCount=0;
		std::uint32_t instanceCount=0;
		effectstate::Raster raster;
		NativeWorldDrawState state;
	};
	std::vector<Draw> draws;
	std::shared_ptr<const NativeFlamethrowerFrame> owner;
};
PreparedFlamethrower::PreparedFlamethrower():impl(std::make_unique<Impl>()) {}
PreparedFlamethrower::~PreparedFlamethrower()=default;
std::size_t PreparedFlamethrower::DrawCount() const { return impl->draws.size(); }
std::uint64_t PreparedFlamethrower::SequenceAt(std::size_t index) const { return impl->draws.at(index).sequence; }
std::uint32_t PreparedFlamethrower::InstanceCountAt(std::size_t index) const { return impl->draws.at(index).instanceCount; }
void PreparedFlamethrower::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const
{
	const auto& draw=impl->draws.at(index); effectstate::RasterState(encoder,draw.state,draw.raster);
	const auto cull=draw.state.cullEnabled?draw.state.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setRenderPipelineState:draw.pipeline]; [encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.indexCount indexType:MTLIndexTypeUInt16
		indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length instanceCount:draw.instanceCount];
}
struct FlamethrowerGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,std::uint64_t,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct CachedBuffer { std::weak_ptr<const NativeBufferSnapshot> owner; __strong id<MTLBuffer> buffer=nil; };
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source,std::size_t bytes)
	{
		Require(bytes && bytes<=source->bytes.size(),"Original Flamethrower GPU read exceeds immutable buffer");
		auto& entry=buffers[source.get()];
		if (entry.owner.expired() || entry.buffer.length<bytes) { auto next=Upload(lease,source->bytes.data(),bytes); entry={source,next}; }
		else lease.AddAllocation(entry.buffer);
		return entry.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,bool coverage=false)
	{
		const Key key{color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(blend,mask),coverage};
		if (auto it=pipelines.find(key);it!=pipelines.end()) return it->second;
		const auto start=Clock::now(); NSError* error=nil;
		if (!library) {
			auto descriptor=[MTL4LibraryDescriptor new]; descriptor.name=@"Original game31657 complete FlamethrowerGL4 program";
			descriptor.source=String(std::string(flamethrower::SHADER));
			auto options=[MTLCompileOptions new]; options.mathMode=MTLMathModeSafe; options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise; descriptor.options=options;
			library=lease.NewLibrary(descriptor,&error);
			Require(library!=nil,"Cannot compile original Flamethrower native program: "+Error(error));
		}
		auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new]; vertex.library=fragment.library=library;
		vertex.name=@"FlamethrowerVertexMain"; fragment.name=@"FlamethrowerFragmentMain";
		descriptor.vertexFunctionDescriptor=vertex; descriptor.fragmentFunctionDescriptor=fragment;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle; descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto attachment=descriptor.colorAttachments[0]; attachment.pixelFormat=color;
		attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor); attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha); attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		attachment.rgbBlendOperation=Equation(blend.colorEquation); attachment.alphaBlendOperation=Equation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new]; options.shaderValidation=ShaderValidationMode(validation); descriptor.options=options;
		auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile original Flamethrower actual render pipeline: "+Error(error));
		pipelines.emplace(key,pipeline); ms+=Milliseconds(start); return pipeline;
	}
};
FlamethrowerGPU::FlamethrowerGPU():impl(std::make_unique<Impl>()) {}
FlamethrowerGPU::~FlamethrowerGPU()=default;
void FlamethrowerGPU::ClearResources() { impl->buffers.clear(); }
void FlamethrowerGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
	unsigned samples,bool validation,double& milliseconds)
{
	ScreenBlendState blend; blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::One;
	impl->Pipeline(lease,color,depth,stencil,samples,validation,blend,{true,true,true,true},milliseconds);
}
std::unique_ptr<PreparedFlamethrower> FlamethrowerGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeFlamethrowerFrame> frame,
	const WorldCamera& originalCamera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
	ScreenGPU& images,const PreparedScreenFrame* producers,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
	unsigned samples,bool validation,double& milliseconds)
{
	if (!frame || frame->draws.empty()) return {};
	const auto values=ReadNativeFlamethrowerUniforms(frame->uniforms);
	Require(depth==MTLPixelFormatDepth32Float || depth==MTLPixelFormatDepth32Float_Stencil8,"Original Flamethrower requires actual scene depth");
	Initialize(lease,color,depth,stencil,samples,validation,milliseconds); lease.Retain(impl->library);
	std::erase_if(impl->buffers,[](const auto& entry) { return entry.second.owner.expired(); });
	auto result=std::unique_ptr<PreparedFlamethrower>(new PreparedFlamethrower); result->impl->owner=frame;
	for (const auto& draw:frame->draws) {
		ValidateNativeFlamethrowerDraw(draw);
		if (!draw.instanceCount || (draw.world.cullEnabled && draw.world.cull==NativeWorldCull::FrontAndBack)) continue;
		Require(draw.world.frontPolygonMode==NativePolygonMode::Fill && draw.world.backPolygonMode==NativePolygonMode::Fill,"Original Flamethrower non-fill raster consumer is unavailable");
		Require(!draw.world.inheritedClipPlane,"Original Flamethrower original DrawWorld cannot inherit a water clip plane");
		WorldCamera camera=originalCamera;
		if (draw.surface.viewport) { const auto& v=*draw.surface.viewport; camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)}; }
		auto scissor=draw.surface.scissor;
		if (scissor && !scissor->windowCoordinates) { scissor->x+=originalCamera.viewport[0]; scissor->y+=originalCamera.viewport[1]; scissor->windowCoordinates=true; }
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,scissor); if (!raster) continue;
		auto vertices=impl->Buffer(lease,draw.vertices,6*16);
		auto indices=impl->Buffer(lease,draw.indices,std::size_t(draw.indexCount)*2);
		auto instances=impl->Buffer(lease,draw.instances,std::size_t(draw.instanceCount)*64);
		Uniform uniform{}; std::memcpy(&uniform.cameraViewProjection,values.cameraViewProjection.data(),64);
		std::memcpy(&uniform.cameraViewInverse,values.cameraViewInverse.data(),64); std::memcpy(&uniform.timeInfo,values.timeInfo.data(),16);
		std::memcpy(&uniform.windVelocity,draw.windVelocity.data(),12); uniform.defines=draw.defines;
		uniform.options=simd_make_uint4(values.clipDepth==ClipDepthConvention::MinusOneToOne,draw.surface.alphaTest.enabled,
			unsigned(draw.surface.alphaTest.compare),std::bit_cast<std::uint32_t>(draw.surface.alphaTest.reference));
		auto constants=Upload(lease,&uniform,sizeof(uniform));
		auto descriptor=[MTL4ArgumentTableDescriptor new]; descriptor.maxBufferBindCount=3; descriptor.maxTextureBindCount=2; descriptor.maxSamplerStateBindCount=2; descriptor.initializeBindings=YES;
		NSError* error=nil; auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
		Require(arguments!=nil,"Cannot allocate original Flamethrower native arguments: "+Error(error)); lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0]; [arguments setAddress:instances.gpuAddress atIndex:1]; [arguments setAddress:constants.gpuAddress atIndex:2];
		for (unsigned n=0;n<2;++n) {
			const auto image=images.PrepareImage(lease,draw.textures[n],draw.samplers[n],producers);
			[arguments setTexture:image.texture.gpuResourceID atIndex:n]; [arguments setSamplerState:image.sampler.gpuResourceID atIndex:n];
		}
		auto pipeline=impl->Pipeline(lease,color,depth,stencil,samples,validation,draw.surface.blend,draw.world.colorWrite,milliseconds,draw.world.alphaToCoverage); lease.Retain(pipeline);
		PreparedFlamethrower::Impl::Draw packet; packet.sequence=draw.sequence; packet.pipeline=pipeline;
		packet.indices=indices;packet.indexCount=draw.indexCount;
		packet.depth=effectstate::Depth(lease,draw.world,stencil); packet.arguments=arguments; packet.instanceCount=draw.instanceCount;
		packet.raster=*raster; packet.state=draw.world; result->impl->draws.push_back(std::move(packet));
	}
	return result;
}
}
