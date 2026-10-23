/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4BuildSquareNative.h"
#include "NativeBuildSquareShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>
namespace rendering::metal4 {
namespace {
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,size_t size)
{
	Require(data&&size&&size<=lease.Device().maxBufferLength,"Original BuildSquare buffer extent invalid");auto b=[lease.Device()newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original BuildSquare buffer");lease.AddAllocation(b);return b;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::logic_error("Unknown BuildSquare blend equation");
}
uint64_t PipelineBits(const ScreenBlendState& b,const std::array<bool,4>& m)
{
	return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;
}
}
struct PreparedBuildSquareDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	NativeBuildSquareDraw source;PreparedStartBoxImage height;
	NativeWorldDrawState rasterState;effectstate::Raster raster;
	uint32_t count=0,instances=0;
};
PreparedBuildSquareDraw::PreparedBuildSquareDraw():impl(std::make_unique<Impl>()){}
PreparedBuildSquareDraw::~PreparedBuildSquareDraw()=default;
bool PreparedBuildSquareDraw::Empty()const{return !impl->count||!impl->instances;}
void PreparedBuildSquareDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(Empty())return;if(impl->height.requireReadable)impl->height.requireReadable();effectstate::RasterState(encoder,impl->rasterState,impl->raster);
	const auto cull=impl->rasterState.cullEnabled?impl->rasterState.cull:NativeWorldCull::None;[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setDepthStencilState:impl->depth];[encoder setRenderPipelineState:impl->pipeline];[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:impl->count instanceCount:impl->instances];
}
struct BuildSquareGPU::Impl {
	using Key=std::tuple<bool,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	__strong id<MTLLibrary> library=nil;std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,Cached> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& s){auto& entry=buffers[s.get()];if(entry.source.lock()!=s)entry={s,Upload(lease,s->bytes.data(),s->bytes.size())};lease.AddAllocation(entry.buffer);return entry.buffer;}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeBuildSquareDraw& draw,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms)
	{
		const Key key{bool(draw.program.isMiniMap),color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(draw.surface.blend,draw.raster.colorWrite),draw.raster.alphaToCoverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()){lease.Retain(found->second);return found->second;}
		const auto start=Clock::now();NSError* error=nil;
		if(!library){auto d=[MTL4LibraryDescriptor new];d.name=@"Exact31657 BuildSquare cells, merged blocks and queued boundaries";d.source=String(NativeBuildSquareShaderSource);auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original BuildSquare MSL: "+Error(error));}
		lease.Retain(library);auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=draw.program.isMiniMap?@"OriginalBuildSquareMiniMap":@"OriginalBuildSquareWorld";f.name=@"OriginalBuildSquareFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		const auto& b=draw.surface.blend;const auto& m=draw.raster.colorWrite;auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);a.writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));
		auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;auto result=lease.NewRenderPipeline(d,&error);Require(result!=nil,"Cannot compile original BuildSquare pipeline: "+Error(error));pipelines.emplace(key,result);lease.Retain(result);ms+=Milliseconds(start);return result;
	}
};
BuildSquareGPU::BuildSquareGPU():impl(std::make_unique<Impl>()){}
BuildSquareGPU::~BuildSquareGPU()=default;
void BuildSquareGPU::ClearResources(){impl->buffers.clear();}
std::unique_ptr<PreparedBuildSquareDraw> BuildSquareGPU::Prepare(RuntimeLease& lease,const NativeBuildSquareDraw& draw,const StartBoxImageResolver& resolver,MTLViewport viewport,MTLScissorRect scissor,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms,std::optional<NativePostprocessConsumer> consumer)
{
	ValidateNativeBuildSquareDraw(draw);auto result=std::unique_ptr<PreparedBuildSquareDraw>(new PreparedBuildSquareDraw);
	if(!draw.instanceCount||!scissor.width||!scissor.height||(draw.raster.cullEnabled&&draw.raster.cull==NativeWorldCull::FrontAndBack))return result;
	Require(viewport.width>0&&viewport.height>0&&viewport.znear==0&&viewport.zfar==1,"Original BuildSquare raster depth range differs");
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.source.expired();});auto& out=*result->impl;out.source=draw;out.rasterState=draw.raster;out.raster={viewport,scissor};
	auto u=ReadNativeBuildSquareUniforms(draw);
	if(!draw.program.isMiniMap){
		Require(bool(resolver),"Original BuildSquare needs actual heightmap owner");auto image=resolver(lease,draw.height,consumer);
		Require(image.kind==StartBoxImageKind::Color&&image.original==draw.height.object&&image.texture&&image.sampler&&bool(image.storageOwner),"Original BuildSquare height image/version/sampler owner differs");const auto& a=image.original->allocation;
		Require(image.texture.textureType==MTLTextureType2D&&image.texture.pixelFormat==MTLPixelFormatR32Float&&image.texture.width==a.width&&image.texture.height==a.height&&image.texture.mipmapLevelCount==a.levels&&(image.texture.usage&MTLTextureUsageShaderRead),"Original BuildSquare requires real readable R32F height image");
		Require(image.original->sampling.swizzle[0]==0x1903&&image.texture.swizzle.red==MTLTextureSwizzleRed,"Original BuildSquare height red channel differs");out.height=std::move(image);u.options[3]=unsigned(out.height.flipV);
	}
	if(depth==MTLPixelFormatInvalid){out.rasterState.depthTest=false;out.rasterState.depthWrite=false;}if(stencil==MTLPixelFormatInvalid)out.rasterState.stencilTest=false;
	auto vertices=impl->Buffer(lease,draw.vertices),instances=impl->Buffer(lease,draw.instances),uniforms=Upload(lease,&u,sizeof(u));
	auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=3;d.maxTextureBindCount=draw.program.isMiniMap?0:1;d.maxSamplerStateBindCount=draw.program.isMiniMap?0:1;d.initializeBindings=YES;NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(out.arguments!=nil,"Cannot allocate original BuildSquare arguments: "+Error(error));lease.Retain(out.arguments);
	[out.arguments setAddress:vertices.gpuAddress atIndex:0];[out.arguments setAddress:instances.gpuAddress+size_t(draw.firstInstance)*72 atIndex:1];[out.arguments setAddress:uniforms.gpuAddress atIndex:2];
	if(out.height.texture){lease.AddAllocation(out.height.texture);lease.Retain(out.height.sampler);[out.arguments setTexture:out.height.texture.gpuResourceID atIndex:0];[out.arguments setSamplerState:out.height.sampler.gpuResourceID atIndex:0];}
	out.pipeline=impl->Pipeline(lease,draw,color,depth,stencil,samples,validation,ms);out.depth=effectstate::Depth(lease,out.rasterState,stencil);out.count=draw.vertexCount;out.instances=draw.instanceCount;return result;
}
}
