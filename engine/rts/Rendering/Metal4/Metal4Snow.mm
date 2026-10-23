/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Snow.h"
#include "NativeSnowShaders.h"
#include "Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <cstring>
#include <map>
#include <tuple>
namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 modelView,projection;
	simd_float4 snowColor,speed,camPos,params;
	simd_float4 viewport,target;
	simd_uint4 options;
};
static_assert(sizeof(Uniform)==240);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,std::size_t length)
{
	Require(length&&length<=lease.Device().maxBufferLength,"Original Snow buffer exceeds actual device capacity");
	auto buffer=[lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate original Snow immutable points/uniforms");lease.AddAllocation(buffer);return buffer;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value) {
		case ScreenBlendEquation::Add:return MTLBlendOperationAdd;
		case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;
		case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;
	}
	throw std::runtime_error("Unknown original Snow blend equation");
}
std::uint64_t Bits(const ScreenBlendState& b,const std::array<bool,4>& mask)
{
	return std::uint64_t(b.enabled)|std::uint64_t(b.sourceColor)<<1|std::uint64_t(b.destinationColor)<<5|
		std::uint64_t(b.sourceAlpha)<<9|std::uint64_t(b.destinationAlpha)<<13|
		std::uint64_t(mask[0])<<17|std::uint64_t(mask[1])<<18|std::uint64_t(mask[2])<<19|std::uint64_t(mask[3])<<20|
		std::uint64_t(b.colorEquation)<<21|std::uint64_t(b.alphaEquation)<<24;
}
}
struct PreparedSnow::Impl {
	struct Draw {
		std::uint64_t sequence;
		__strong id<MTLRenderPipelineState> pipeline;
		__strong id<MTLDepthStencilState> depth;
		__strong id<MTL4ArgumentTable> arguments;
		effectstate::Raster raster;
		NativeWorldDrawState state;
		std::uint32_t count;
	};
	std::vector<Draw> draws;
	std::shared_ptr<const NativeSnowFrame> owner;
};
PreparedSnow::PreparedSnow():impl(std::make_unique<Impl>()) {}
PreparedSnow::~PreparedSnow()=default;
std::size_t PreparedSnow::DrawCount() const { return impl->draws.size(); }
std::uint64_t PreparedSnow::SequenceAt(std::size_t n) const { return impl->draws.at(n).sequence; }
void PreparedSnow::EncodeDraw(std::size_t n,id<MTL4RenderCommandEncoder> encoder) const
{
	const auto& draw=impl->draws.at(n);
	effectstate::RasterState(encoder,draw.state,draw.raster);
	// Polygon culling/mode/offset do not affect the original GL_POINTS.
	[encoder setCullMode:MTLCullModeNone];[encoder setDepthBias:0 slopeScale:0 clamp:0];
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.count*6];
}
struct SnowGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,std::uint64_t,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& milliseconds,bool coverage=false)
	{
		const Key key{color,depth,stencil,lease.SampleShadingKey(samples),validation,Bits(blend,mask),coverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()) return found->second;
		const auto start=Clock::now();NSError* error=nil;
		if(!library) {
			auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 original Snow point-footprint VS/FS";
			descriptor.source=String(std::string(snow::SHADER));auto options=[MTLCompileOptions new];
			options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
			library=lease.NewLibrary(descriptor,&error);
			Require(library!=nil,"Cannot compile original Snow program: "+Error(error));
		}
		auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];
		vertex.library=fragment.library=library;vertex.name=@"SnowVertexMain";fragment.name=@"SnowFragmentMain";
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;
		attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		attachment.rgbBlendOperation=Equation(blend.colorEquation);attachment.alphaBlendOperation=Equation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile original Snow actual pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);milliseconds+=Milliseconds(start);return pipeline;
	}
};
SnowGPU::SnowGPU():impl(std::make_unique<Impl>()) {}
SnowGPU::~SnowGPU()=default;
void SnowGPU::ClearResources() { impl->pipelines.clear();impl->library=nil; }
void SnowGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds)
{
	ScreenBlendState blend;blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::One;
	impl->Pipeline(lease,color,depth,stencil,samples,validation,blend,{true,true,true,true},milliseconds);
}
std::unique_ptr<PreparedSnow> SnowGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeSnowFrame> frame,const WorldCamera& original,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,ScreenGPU& images,const PreparedScreenFrame* producers,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds)
{
	if(!frame||frame->draws.empty()) return {};
	const auto clip=ReadNativeSnowClipDepth(*frame);
	auto result=std::unique_ptr<PreparedSnow>(new PreparedSnow);result->impl->owner=frame;
	for(const auto& draw:frame->draws) {
		ValidateNativeSnowDraw(draw);if(draw.points->empty()) continue;
		Require(!draw.world.inheritedClipPlane,"Original Snow DrawWorld must not inherit a planar water clip plane");
		auto camera=original;
		if(draw.surface.viewport) {const auto& v=*draw.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto scissor=draw.surface.scissor;
		if(scissor&&!scissor->windowCoordinates) {scissor->x+=original.viewport[0];scissor->y+=original.viewport[1];scissor->windowCoordinates=true;}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,std::nullopt);if(!raster) continue;
		const auto sourceViewport=raster->viewport;
		// Expanded source points retain their footprint beyond source viewport
		// edges. Only explicit original scissor and attachment bounds clip it.
		raster->viewport={0,0,double(width),double(height),0,1};
		raster->scissor={0,0,width,height};
		if(scissor) {
			const double sx=double(width)/logicalWidth,sy=double(height)/logicalHeight;
			const auto x0=NSUInteger(std::clamp(std::floor(double(scissor->x)*sx),0.,double(width)));
			const auto x1=NSUInteger(std::clamp(std::ceil((double(scissor->x)+scissor->width)*sx),0.,double(width)));
			const auto y0=NSUInteger(std::clamp(std::floor((double(logicalHeight)-scissor->y-scissor->height)*sy),0.,double(height)));
			const auto y1=NSUInteger(std::clamp(std::ceil((double(logicalHeight)-scissor->y)*sy),0.,double(height)));
			if(x1<=x0||y1<=y0) continue;
			raster->scissor={x0,y0,x1-x0,y1-y0};
		}
		const auto& source=draw.uniforms;Uniform uniform{};
		uniform.viewport=simd_make_float4(sourceViewport.originX,sourceViewport.originY,sourceViewport.width,sourceViewport.height);
		uniform.target=simd_make_float4(width,height,0,0);
		std::memcpy(&uniform.modelView,draw.modelView.data(),64);std::memcpy(&uniform.projection,draw.projection.data(),64);
		std::memcpy(&uniform.snowColor,source.snowColor.data(),16);
		uniform.speed=simd_make_float4(source.speed[0],source.speed[1],source.speed[2],0);
		uniform.camPos=simd_make_float4(source.camPos[0],source.camPos[1],source.camPos[2],0);
		uniform.params=simd_make_float4(source.time,source.scale,source.sizeMult,draw.surface.alphaTest.reference);
		uniform.options=simd_make_uint4(clip==ClipDepthConvention::MinusOneToOne,draw.surface.alphaTest.enabled,unsigned(draw.surface.alphaTest.compare),draw.texture->flipV!=!draw.pointState.upperLeft);
		const auto image=images.PrepareImage(lease,draw.texture,draw.sampler,producers,NativePostprocessConsumer{NativePostprocessStage::World,draw.sequence,0});
		Require(image.texture!=nil&&image.sampler!=nil,"Original Snow sampler0 did not resolve to its actual GPU image");
		auto points=Upload(lease,draw.points->data(),draw.points->size()*sizeof(std::array<float,4>));
		auto uniforms=Upload(lease,&uniform,sizeof(uniform));
		auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=1;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;
		NSError* error=nil;auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
		Require(arguments!=nil,"Cannot allocate original Snow argument table: "+Error(error));
		[arguments setAddress:points.gpuAddress atIndex:0];[arguments setAddress:uniforms.gpuAddress atIndex:1];
		[arguments setTexture:image.texture.gpuResourceID atIndex:0];[arguments setSamplerState:image.sampler.gpuResourceID atIndex:0];
		lease.Retain(arguments);lease.Retain(image.texture);lease.Retain(image.sampler);
		auto pipeline=impl->Pipeline(lease,color,depth,stencil,samples,validation,draw.surface.blend,draw.world.colorWrite,milliseconds,draw.world.alphaToCoverage);
		lease.Retain(impl->library);lease.Retain(pipeline);
		result->impl->draws.push_back({draw.sequence,pipeline,effectstate::Depth(lease,draw.world,stencil),arguments,*raster,draw.world,std::uint32_t(draw.points->size())});
	}
	return result;
}
}
