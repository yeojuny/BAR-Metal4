/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CommandsNative.h"
#include "NativeCommandsShaders.h"
#include "Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>
namespace rendering::metal4 {
namespace {
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,size_t bytes){Require(data && bytes && bytes<=lease.Device().maxBufferLength,"Original Commands FX buffer exceeds device capacity");auto b=[lease.Device()newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate immutable Commands FX buffer");lease.AddAllocation(b);return b;}
uint64_t Bits(const ScreenBlendState& b,const std::array<bool,4>& m){return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;}
MTLBlendOperation Equation(ScreenBlendEquation e){switch(e){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown Commands FX blend equation");}
}
struct PreparedCommands::Impl {
	struct Draw{uint64_t sequence;__strong id<MTLRenderPipelineState> pipeline;__strong id<MTLDepthStencilState> depth;__strong id<MTL4ArgumentTable> arguments;effectstate::Raster raster;NativeWorldDrawState state;PreparedStartBoxImage image;uint32_t first,count,instances;};
	std::vector<Draw> draws;std::shared_ptr<const NativeCommandsFrame> source;
};
PreparedCommands::PreparedCommands():impl(std::make_unique<Impl>()){}PreparedCommands::~PreparedCommands()=default;
size_t PreparedCommands::DrawCount()const{return impl->draws.size();}uint64_t PreparedCommands::SequenceAt(size_t index)const{return impl->draws.at(index).sequence;}
void PreparedCommands::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder)const{
	const auto& d=impl->draws.at(index);if(d.image.requireReadable)d.image.requireReadable();effectstate::RasterState(encoder,d.state,d.raster);
	const auto c=d.state.cullEnabled?d.state.cull:NativeWorldCull::None;[encoder setCullMode:c==NativeWorldCull::Back?MTLCullModeBack:c==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeNone];
	[encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];[encoder setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:d.first vertexCount:d.count instanceCount:d.instances];
}
struct CommandsGPU::Impl {
	std::map<StartBoxImageKind,id<MTLLibrary>> libraries;
	using Key=std::tuple<StartBoxImageKind,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct Cache{std::weak_ptr<const NativeBufferSnapshot> original;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,Cache> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source){auto& entry=buffers[source.get()];if(entry.original.lock()!=source)entry={source,Upload(lease,source->bytes.data(),source->bytes.size())};lease.AddAllocation(entry.buffer);return entry.buffer;}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,StartBoxImageKind kind,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,bool coverage=false){
		const Key key{kind,color,depth,stencil,lease.SampleShadingKey(samples),validation,Bits(blend,mask),coverage};if(auto i=pipelines.find(key);i!=pipelines.end()){lease.Retain(i->second);return i->second;}
		const auto start=Clock::now();NSError* error=nil;auto& library=libraries[kind];if(!library){auto d=[MTL4LibraryDescriptor new];d.name=@"Exact30770 Commands FX complete original quad VS/FS";d.source=String("#define COMMANDS_IMAGE "+std::to_string(unsigned(kind))+"\n"+std::string(NativeCommandsShaderSource));auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original Commands FX shader: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=@"OriginalCommandsVertex";f.name=@"OriginalCommandsFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original Commands FX pipeline: "+Error(error));pipelines.emplace(key,pipeline);lease.Retain(library);lease.Retain(pipeline);ms+=Milliseconds(start);return pipeline;
	}
};
CommandsGPU::CommandsGPU():impl(std::make_unique<Impl>()){}CommandsGPU::~CommandsGPU()=default;
void CommandsGPU::ClearResources(){impl->buffers.clear();}
void CommandsGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms){for(auto kind:{StartBoxImageKind::Empty,StartBoxImageKind::Color,StartBoxImageKind::Depth})impl->Pipeline(lease,kind,color,depth,stencil,samples,validation,ScreenBlendState{},std::array<bool,4>{true,true,true,true},ms);}
std::unique_ptr<PreparedCommands> CommandsGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeCommandsFrame> frame,const StartBoxImageResolver& resolver,
	const WorldCamera& camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms){
	if(!frame || frame->draws.empty())return{};auto result=std::unique_ptr<PreparedCommands>(new PreparedCommands);result->impl->source=frame;std::erase_if(impl->buffers,[](const auto& p){return p.second.original.expired();});
	for(const auto& draw:frame->draws){ValidateNativeCommandsDraw(draw);Require(draw.sourceDrawFrame==frame->sourceDrawFrame,"Original Commands FX draw belongs to another frame");if(!draw.instanceCount || (draw.raster.cullEnabled && draw.raster.cull==NativeWorldCull::FrontAndBack))continue;
		Require(!draw.raster.inheritedClipPlane && draw.raster.frontPolygonMode==NativePolygonMode::Fill && draw.raster.backPolygonMode==NativePolygonMode::Fill,"Original Commands FX PreUnit raster differs");
		auto actual=camera;if(draw.surface.viewport){const auto& v=*draw.surface.viewport;actual.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}auto scissor=draw.surface.scissor;if(scissor && !scissor->windowCoordinates){scissor->x+=camera.viewport[0];scissor->y+=camera.viewport[1];scissor->windowCoordinates=true;}auto raster=effectstate::Viewport(actual,width,height,logicalWidth,logicalHeight,scissor);if(!raster)continue;
		auto u=ReadNativeCommandsUniforms(draw);PreparedStartBoxImage image;StartBoxImageKind kind=StartBoxImageKind::Empty;
		if(draw.program.useTexture){kind=ClassifyNativeStartBoxImage(draw.image);if(kind!=StartBoxImageKind::Empty){Require(bool(resolver),"Original Commands FX needs its shared sampled-image owner");image=resolver(lease,draw.image,NativePostprocessConsumer{NativePostprocessStage::PreUnit,draw.sequence,0});
			Require(image.kind==kind && image.original==draw.image.object && image.texture && image.sampler && bool(image.storageOwner),"Original Commands FX resolver returned another image/version");Require(image.original->sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original Commands FX image requires its actual swizzle view");u.options[1]=image.flipV;lease.AddAllocation(image.texture);lease.Retain(image.sampler);}}
		auto vertices=impl->Buffer(lease,draw.vertices),instances=impl->Buffer(lease,draw.instances),uniform=Upload(lease,&u,sizeof(u));auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=3;d.maxTextureBindCount=d.maxSamplerStateBindCount=1;d.initializeBindings=YES;NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(arguments!=nil,"Cannot allocate original Commands FX arguments: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0];[arguments setAddress:instances.gpuAddress+size_t(draw.firstInstance)*48 atIndex:1];[arguments setAddress:uniform.gpuAddress atIndex:2];if(image.texture){[arguments setTexture:image.texture.gpuResourceID atIndex:0];[arguments setSamplerState:image.sampler.gpuResourceID atIndex:0];}
		auto pipeline=impl->Pipeline(lease,kind,color,depth,stencil,samples,validation,draw.surface.blend,draw.raster.colorWrite,ms,draw.raster.alphaToCoverage);auto ds=effectstate::Depth(lease,draw.raster,stencil);result->impl->draws.push_back({draw.sequence,pipeline,ds,arguments,*raster,draw.raster,std::move(image),draw.firstVertex,draw.vertexCount,draw.instanceCount});
	}return result;
}
}
