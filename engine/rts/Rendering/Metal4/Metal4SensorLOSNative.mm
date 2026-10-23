/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4SensorLOSNative.h"
#include "NativeSensorLOSShaders.h"
#include "Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>
namespace rendering::metal4 {
namespace {
struct alignas(16) CircleUniform {
	std::array<float,16> cameraViewProjection{},cameraViewInverse{},cameraViewProjectionInverse{};
	std::array<float,4> timeInfo{},viewGeometry{},mapSize{},rangeColor{},viewport{},fragmentToLogical{},values{},parameters{};
	std::array<uint32_t,4> options{},counts{};
};
static_assert(sizeof(CircleUniform)==352);
id<MTLBuffer> UploadCircle(RuntimeLease& lease,const void* bytes,size_t size)
{
	Require(bytes&&size&&size<=lease.Device().maxBufferLength,"Current original sensor buffer exceeds actual device bounds");
	auto buffer=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate current original sensor immutable bytes");lease.AddAllocation(buffer);return buffer;
}
MTLBlendOperation CircleEquation(ScreenBlendEquation equation)
{
	switch(equation){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::logic_error("Unknown original sensor blend equation");
}
uint64_t CircleBlendBits(const ScreenBlendState& blend,const std::array<bool,4>& mask)
{
	return uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|
		uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|
		uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
}
}
struct PreparedSensorLOSDraw::Impl {
	NativeSensorLOSDraw source;
	PreparedStartBoxImage height,mask;
	NativeWorldDrawState state;
	effectstate::Raster raster;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	uint32_t vertices=0,instances=0;
};
PreparedSensorLOSDraw::PreparedSensorLOSDraw():impl(std::make_unique<Impl>()){}
PreparedSensorLOSDraw::~PreparedSensorLOSDraw()=default;
bool PreparedSensorLOSDraw::Empty()const{return !impl->vertices||!impl->instances;}
void PreparedSensorLOSDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(Empty())return;
	Require(encoder!=nil,"Current sensor has no actual world encoder");
	if(impl->height.requireReadable)impl->height.requireReadable();
 if(impl->mask.requireReadable)impl->mask.requireReadable();
	effectstate::RasterState(encoder,impl->state,impl->raster);
	// Original GL lines are independent of polygon culling; these triangles
	// implement their existing diamond-exit/wide-line coverage only.
	[encoder setCullMode:MTLCullModeNone];[encoder setDepthStencilState:impl->depth];
	[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:impl->vertices instanceCount:impl->instances];
}
struct SensorLOSGPU::Impl {
	using Key=std::tuple<MTLPixelFormat,unsigned,bool,bool,uint64_t,bool>;
	__strong id<MTLLibrary> library=nil;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct Buffer {std::weak_ptr<const NativeBufferSnapshot> source;__strong id<MTLBuffer> value=nil;};
	std::unordered_map<const NativeBufferSnapshot*,Buffer> buffers;
	id<MTLBuffer> Snapshot(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		Require(bool(source),"Current sensor buffer lacks its actual immutable owner");
		auto& entry=buffers[source.get()];
		if(entry.source.lock()!=source)entry={source,UploadCircle(lease,source->bytes.data(),source->bytes.size())};
		lease.AddAllocation(entry.value);return entry.value;
	}
	void Library(RuntimeLease& lease,double& milliseconds)
	{
		if(library){lease.Retain(library);return;}
		const auto start=Clock::now();NSError* error=nil;
		auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Original31657 LOS R8 fan and thinned outline";
		descriptor.source=String(NativeSensorLOSShaderSource);
		auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
		options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
		auto compiled=lease.NewLibrary(descriptor,&error);
		Require(compiled!=nil,"Cannot compile current original sensor source family: "+Error(error));
		Require([compiled newFunctionWithName:@"OriginalSensorLOSVertex"]!=nil&&
			[compiled newFunctionWithName:@"OriginalSensorLOSFragment"]!=nil&&[compiled newFunctionWithName:@"OriginalSensorLOSFanVertex"]!=nil&&[compiled newFunctionWithName:@"OriginalSensorLOSFanFragment"]!=nil,
			"Current sensor source family lacks its actual native entrypoints");
		library=compiled;lease.Retain(library);milliseconds+=Milliseconds(start);
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeSensorLOSDraw& draw,
		MTLPixelFormat color,unsigned samples,bool validation,double& milliseconds)
	{
		Library(lease,milliseconds);const auto& blend=draw.surface.blend;const auto& mask=draw.raster.colorWrite;
		const Key key{color,lease.SampleShadingKey(samples),validation,draw.program.defines->stencilPass,CircleBlendBits(blend,mask),draw.raster.alphaToCoverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()){lease.Retain(found->second);return found->second;}
		const auto start=Clock::now();NSError* error=nil;auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];
		vertex.library=fragment.library=library;vertex.name=draw.program.defines->stencilPass?@"OriginalSensorLOSFanVertex":@"OriginalSensorLOSVertex";fragment.name=draw.program.defines->stencilPass?@"OriginalSensorLOSFanFragment":samples>1?@"OriginalSensorLOSFragmentMSAA":@"OriginalSensorLOSFragment";
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;
		attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		attachment.rgbBlendOperation=CircleEquation(blend.colorEquation);attachment.alphaBlendOperation=CircleEquation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile current original sensor target pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);lease.Retain(pipeline);milliseconds+=Milliseconds(start);return pipeline;
	}
};
SensorLOSGPU::SensorLOSGPU():impl(std::make_shared<Impl>()){}
SensorLOSGPU::~SensorLOSGPU(){SetNativeSensorLOSConsumerLifetime({});}
void SensorLOSGPU::Initialize(RuntimeLease& lease,double& milliseconds){impl->Library(lease,milliseconds);SetNativeSensorLOSConsumerLifetime(impl);}
std::shared_ptr<const void> SensorLOSGPU::CompiledFamilyLifetime()const
{
	if(!impl->library)return {};
	return impl;
}
void SensorLOSGPU::ClearResources(){impl->buffers.clear();impl->pipelines.clear();}
std::unique_ptr<PreparedSensorLOSDraw> SensorLOSGPU::Prepare(RuntimeLease& lease,const NativeSensorLOSDraw& draw,
	const StartBoxImageResolver& resolver,const WorldCamera& camera,uint32_t renderWidth,uint32_t renderHeight,
	uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
	unsigned samples,bool validation,double& milliseconds,std::optional<NativePostprocessConsumer> consumer)
{
	ValidateNativeSensorLOSDraw(draw);
	Require(impl->library&&draw.consumerLifetime.get()==impl.get(),"Current sensor draw belongs to another or uncompiled original family/context");
	auto result=std::unique_ptr<PreparedSensorLOSDraw>(new PreparedSensorLOSDraw);
	if(!draw.count||!draw.instanceCount)return result;
	Require(renderWidth&&renderHeight&&logicalWidth&&logicalHeight&&samples&&[lease.Device() supportsTextureSampleCount:samples],"Current original sensor requires actual supported target dimensions/count");
	auto actualCamera=camera;
	if(draw.surface.viewport){const auto& v=*draw.surface.viewport;actualCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
	auto raster=effectstate::Viewport(actualCamera,renderWidth,renderHeight,logicalWidth,logicalHeight,draw.surface.scissor);
	if(!raster)return result;
	auto& out=*result->impl;out.source=draw;out.state=draw.raster;out.raster=*raster;
	if(depth==MTLPixelFormatInvalid){out.state.depthTest=false;out.state.depthWrite=false;}
	if(stencil==MTLPixelFormatInvalid)out.state.stencilTest=false;
	Require(bool(resolver),"Current original sensor lacks the shared actual map texture resolver");
	out.height=resolver(lease,draw.heightImage,consumer);const auto& original=*draw.heightImage.object;
	Require(out.height.kind==StartBoxImageKind::Color&&out.height.original==draw.heightImage.object&&out.height.texture&&out.height.sampler&&out.height.storageOwner,
		"Current sensor height resolver substituted its original image owner");
	Require(out.height.texture.pixelFormat==MTLPixelFormatR32Float&&out.height.texture.textureType==MTLTextureType2D&&
		out.height.texture.width==original.allocation.width&&out.height.texture.height==original.allocation.height&&out.height.texture.mipmapLevelCount==original.allocation.levels,
		"Current sensor height extent/storage differs from actual engine height source");
	Require(original.sampling.swizzle[0]==0x1903&&out.height.texture.swizzle.red==MTLTextureSwizzleRed,"Current sensor original height red channel differs");
	lease.AddAllocation(out.height.texture);lease.Retain(out.height.sampler);
 if(draw.program.defines->stencilPass){
  Require(color==MTLPixelFormatR8Unorm&&depth==MTLPixelFormatInvalid&&stencil==MTLPixelFormatInvalid,"LOS fan requires original color-only R8 target");
  Require(renderWidth==uint32_t(draw.program.defines->viewWidth/4)&&renderHeight==uint32_t(draw.program.defines->viewHeight/4),"LOS fan requires actual quarter-resolution target");
 }else{
  out.mask=resolver(lease,draw.stencilImage,consumer);const auto& s=draw.stencilImage.object->sampling;
  Require(out.mask.original==draw.stencilImage.object&&out.mask.texture&&out.mask.sampler&&out.mask.storageOwner&&out.mask.requireReadable&&out.mask.texture.pixelFormat==MTLPixelFormatR8Unorm,"LOS mask resolver lost actual generated R8 owner");
  Require(out.mask.texture.width==uint32_t(draw.program.defines->viewWidth/4)&&out.mask.texture.height==uint32_t(draw.program.defines->viewHeight/4)&&s.minFilter==0x2600&&s.magFilter==0x2601&&s.wrapS==0x812f&&s.wrapT==0x812f&&s.swizzle[0]==0x1903,"LOS mask dimensions/original nearest-linear sampling differ");
  lease.AddAllocation(out.mask.texture);lease.Retain(out.mask.sampler);
 }

	const auto source=ReadNativeSensorLOSUniforms(draw);const auto& defines=*draw.program.defines;
	CircleUniform uniform;uniform.cameraViewProjection=source.cameraViewProjection;uniform.cameraViewInverse=source.cameraViewInverse;
	uniform.cameraViewProjectionInverse=source.cameraViewProjectionInverse;uniform.timeInfo=source.timeInfo;
	uniform.viewGeometry=source.viewGeometry;uniform.mapSize=source.mapSize;uniform.rangeColor=draw.program.rangeColor;
	const auto& viewport=out.raster.viewport;
	uniform.viewport={float(viewport.originX),float(viewport.originY),float(viewport.width),float(viewport.height)};
	uniform.fragmentToLogical={float(logicalWidth)/renderWidth,float(logicalHeight),float(logicalHeight)/renderHeight,0};
	uniform.values={defines.viewWidth,defines.viewHeight,draw.program.teamColorMix,draw.program.lineWidthPx};
	uniform.parameters={draw.raster.lineWidth,draw.surface.alphaTest.reference,float(out.mask.flipV),0};
	uniform.options={uint32_t(!(source.renderCaps&1)),uint32_t(out.height.flipV),uint32_t(defines.stencilPass),
		uint32_t(draw.surface.alphaTest.compare)|(uint32_t(draw.surface.alphaTest.enabled)<<8)};
	uniform.counts={draw.count,draw.first,draw.firstInstance,0};
	const auto positions=ReadNativeSensorLOSInstances(draw);
	std::erase_if(impl->buffers,[](const auto& value){return value.second.source.expired();});
	std::array<id<MTLBuffer>,5> buffers{impl->Snapshot(lease,draw.vertices),impl->Snapshot(lease,draw.instances),
		UploadCircle(lease,positions.positions.data(),positions.positions.size()*16),UploadCircle(lease,&uniform,sizeof(uniform)),
		UploadCircle(lease,positions.colors.data(),positions.colors.size()*16)};
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=5;descriptor.maxTextureBindCount=2;descriptor.maxSamplerStateBindCount=2;descriptor.initializeBindings=YES;
	NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];
	Require(out.arguments!=nil,"Cannot allocate current original sensor argument table");lease.Retain(out.arguments);
	for(unsigned i=0;i<buffers.size();++i)[out.arguments setAddress:buffers[i].gpuAddress atIndex:i];
	[out.arguments setTexture:out.height.texture.gpuResourceID atIndex:0];[out.arguments setSamplerState:out.height.sampler.gpuResourceID atIndex:0];
	if(out.mask.texture){[out.arguments setTexture:out.mask.texture.gpuResourceID atIndex:1];[out.arguments setSamplerState:out.mask.sampler.gpuResourceID atIndex:1];}
 out.pipeline=impl->Pipeline(lease,draw,color,samples,validation,milliseconds);out.depth=effectstate::Depth(lease,out.state,stencil);
	Require(uint64_t(draw.count)*6<=UINT32_MAX,"Current original sensor line invocation count overflow");out.vertices=defines.stencilPass?(draw.count-2)*3:draw.count*6;out.instances=draw.instanceCount;
	return result;
}
}
