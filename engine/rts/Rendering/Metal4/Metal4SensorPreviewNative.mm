/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4SensorPreviewNative.h"
#include "NativeSensorPreviewShaders.h"
#include "NativeJammerPreviewShaders.h"
#include "Metal4ScreenNative.h"
#include "Metal4NativeEffectState.h"
#include <bit>
#include <cstring>
#include <map>
#include <regex>
#include <tuple>
#include <unordered_map>
namespace rendering::metal4 {
namespace {
id<MTLBuffer> PreviewUpload(RuntimeLease& lease,const void* bytes,size_t size)
{
	Require(bytes&&size&&size<=lease.Device().maxBufferLength,"Original Preview buffer exceeds actual device bounds");
	auto value=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(value!=nil,"Cannot allocate actual original Preview buffer");lease.AddAllocation(value);return value;
}
bool PreviewDepth(id<MTLTexture> texture)
{
	return texture&&(texture.pixelFormat==MTLPixelFormatDepth32Float||texture.pixelFormat==MTLPixelFormatDepth32Float_Stencil8||texture.pixelFormat==MTLPixelFormatDepth16Unorm);
}
MTLBlendOperation PreviewEquation(ScreenBlendEquation equation)
{
	switch(equation){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::logic_error("Unknown original Preview blend equation");
}
uint64_t PreviewBlendBits(const ScreenBlendState& blend,const std::array<bool,4>& mask)
{
	return uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|
		uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|
		uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
}
const char* PreviewFragmentName(NativeSensorPreviewProgramKind kind)
{
	using K=NativeSensorPreviewProgramKind;
	switch(kind){case K::Mip:return "OriginalPreviewMipFragment";case K::Coverage:return "OriginalPreviewCoverageFragment";case K::Smooth:return "OriginalPreviewSmoothFragment";case K::Sheet:return "OriginalPreviewSheetFragment";case K::Minimap:return "OriginalPreviewMinimapFragment";case K::JammerDisc:return "OriginalPreviewJammerDiscFragment";case K::JammerSheet:return "OriginalPreviewJammerSheetFragment";}
	throw std::logic_error("Unknown original Preview program kind");
}
}
struct PreparedSensorPreviewCommand::Impl {
	NativeSensorPreviewCommand source;
	SensorPreviewTarget target;
	std::vector<PreparedSensorPreviewImage> images;
	NativeWorldDrawState state;
	effectstate::Raster raster;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong MTL4RenderPassDescriptor* pass=nil;
	std::unique_ptr<PreparedScreenFrame> clear;
	uint32_t first=0,count=0;
	mutable bool encoded=false;
};
PreparedSensorPreviewCommand::PreparedSensorPreviewCommand():impl(std::make_unique<Impl>()){}
PreparedSensorPreviewCommand::~PreparedSensorPreviewCommand()=default;
bool PreparedSensorPreviewCommand::Empty()const{return !impl->count&&!impl->clear;}
bool PreparedSensorPreviewCommand::IsTarget()const{return bool(impl->source.output);}
bool PreparedSensorPreviewCommand::Encoded()const{return impl->encoded;}
std::shared_ptr<const NativePostprocessVersion> PreparedSensorPreviewCommand::OutputVersion()const{return impl->source.output;}
void PreparedSensorPreviewCommand::EncodeDependencies(id<MTL4CommandBuffer> command)const
{
	for(const auto& image:impl->images)if(image.encodeDependencies)image.encodeDependencies(command);
}
void PreparedSensorPreviewCommand::DidComplete()const
{
	for(const auto& image:impl->images)if(image.didComplete)image.didComplete();
}
void PreparedSensorPreviewCommand::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	Require(encoder!=nil,"Original Preview lacks an actual attachment encoder");
	if(impl->clear){impl->clear->Encode(encoder);return;}
	if(!impl->count)return;
	for(const auto& image:impl->images)if(image.requireReadable)image.requireReadable();
	effectstate::RasterState(encoder,impl->state,impl->raster);
	[encoder setCullMode:MTLCullModeNone];[encoder setDepthStencilState:impl->depth];
	[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:impl->first vertexCount:impl->count];
}
void PreparedSensorPreviewCommand::EncodeTarget(id<MTL4CommandBuffer> command)const
{
	Require(command!=nil&&IsTarget()&&impl->pass&&!impl->encoded,"Original Preview target is absent or was encoded twice");
	EncodeDependencies(command);
	if(impl->target.previous){
		if(impl->target.requirePreviousReadable)impl->target.requirePreviousReadable();
		auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot preserve actual Preview target previous generation");
		[copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:impl->target.previous toTexture:impl->target.output];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	auto encoder=[command renderCommandEncoderWithDescriptor:impl->pass];Require(encoder!=nil,"Cannot open actual original Preview float target");
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	Encode(encoder);
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder endEncoding];impl->encoded=true;
}
struct SensorPreviewGPU::Impl {
	struct Libraries {std::array<id<MTLLibrary>,4> variants{};bool modelDepth=false;};
	struct Program {NativeSensorPreviewProgramSource source;std::shared_ptr<const Libraries> libraries;};
	std::map<std::string,std::weak_ptr<const Libraries>> libraries;
	std::unordered_map<const void*,std::weak_ptr<const Program>> programs;
	using Key=std::tuple<const Libraries*,NativeSensorPreviewProgramKind,unsigned,unsigned,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	struct Pipeline {std::weak_ptr<const Libraries> owner;__strong id<MTLRenderPipelineState> value=nil;};
	std::map<Key,Pipeline> pipelines;
	struct Buffer {std::weak_ptr<const void> owner;__strong id<MTLBuffer> value=nil;};
	std::unordered_map<const void*,Buffer> buffers;
	template<class T>id<MTLBuffer> Cached(RuntimeLease& lease,std::shared_ptr<const T> owner,const void* bytes,size_t size)
	{
		Require(bool(owner),"Original Preview bytes lost their immutable owner");auto& entry=buffers[owner.get()];
		if(entry.owner.lock().get()!=owner.get())entry={owner,PreviewUpload(lease,bytes,size)};
		Require(entry.value.length==size,"Original Preview immutable byte extent changed");lease.AddAllocation(entry.value);return entry.value;
	}
	std::shared_ptr<const Libraries> LibrariesFor(RuntimeLease& lease,const NativeSensorPreviewProgramSource& source,double& milliseconds)
	{
		const auto key=source.defines;
		if(auto found=libraries.find(key);found!=libraries.end())if(auto retained=found->second.lock())return retained;
		auto out=std::make_shared<Libraries>();out->modelDepth=source.modelDepthTest;
		// A native image may be a real depth2d or a typed color backing. Compile
		// both exact sampled-type transports; select from the ACTUAL bound images.
		// They execute the same original source depth comparisons in either case.
		const auto defines=std::regex_replace(source.defines,std::regex("\\bvec3\\b"),"float3");
		const unsigned count=source.modelDepthTest?4:2;
		for(unsigned variant=0;variant<count;++variant){
			const auto start=Clock::now();NSError* error=nil;
			const auto prefix=defines+"\n#define PREVIEW_MAP_DEPTH_IS_DEPTH "+std::to_string(variant&1u)+
				"\n#define PREVIEW_MODEL_DEPTH_IS_DEPTH "+std::to_string((variant>>1)&1u)+"\n";
			auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=source.jammer?@"Original31657 Jammer four programs":@"Original31657 Preview five programs";
			descriptor.source=String(prefix+(source.jammer?NativeJammerPreviewShaderSource:NativeSensorPreviewShaderSource));
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
			options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
			auto library=lease.NewLibrary(descriptor,&error);
			Require(library!=nil,"Cannot compile actual original Preview source: "+Error(error));
			using K=NativeSensorPreviewProgramKind;
			const auto kinds=source.jammer?std::vector<K>{K::JammerDisc,K::Smooth,K::JammerSheet,K::Minimap}:std::vector<K>{K::Mip,K::Coverage,K::Smooth,K::Sheet,K::Minimap};
			for(auto kind:kinds)
				Require([library newFunctionWithName:String(PreviewFragmentName(kind))]!=nil,"Original Preview compiled fragment entrypoint missing");
			Require([library newFunctionWithName:@"OriginalPreviewPassVertex"]!=nil&&[library newFunctionWithName:@"OriginalPreviewMinimapVertex"]!=nil,
				"Original Preview compiled vertex entrypoint missing");
			out->variants[variant]=library;lease.Retain(library);milliseconds+=Milliseconds(start);
		}
		libraries[key]=out;return out;
	}
	id<MTLRenderPipelineState> PipelineFor(RuntimeLease& lease,const Program& program,unsigned variant,const NativeSensorPreviewDraw& draw,
		const SensorPreviewTarget& target,bool validation,double& milliseconds)
	{
		const auto& blend=draw.surface.blend;const auto& mask=draw.raster.colorWrite;
		// Presence comes from the same immutable source predicates that resolve
		// actual resources below. Runtime uniforms alone cannot make a declared
		// Metal entrypoint argument optional at API validation time.
		const unsigned bindings=NativeSensorPreviewRequiredTextureMask(draw.program)|
			(unsigned(NativeSensorPreviewRequiresStorage(draw.program))<<7);
		const Key key{program.libraries.get(),program.source.kind,variant,bindings,target.color,lease.SampleShadingKey(target.samples),validation,PreviewBlendBits(blend,mask),draw.raster.alphaToCoverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()&&!found->second.owner.expired()){lease.Retain(found->second.value);return found->second.value;}
		const auto start=Clock::now();NSError* error=nil;auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];
		vertex.library=fragment.library=program.libraries->variants[variant];
		vertex.name=program.source.kind==NativeSensorPreviewProgramKind::Minimap?@"OriginalPreviewMinimapVertex":@"OriginalPreviewPassVertex";
		fragment.name=String(PreviewFragmentName(program.source.kind));descriptor.vertexFunctionDescriptor=vertex;
		auto constants=[MTLFunctionConstantValues new];
		for(NSUInteger i=0;i<8;++i){const bool present=(bindings&(1u<<i))!=0;[constants setConstantValue:&present type:MTLDataTypeBool atIndex:i];}
		auto specialized=[MTL4SpecializedFunctionDescriptor new];
		specialized.functionDescriptor=fragment;specialized.constantValues=constants;
		descriptor.fragmentFunctionDescriptor=specialized;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=target.samples;descriptor.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=target.color;
		attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		attachment.rgbBlendOperation=PreviewEquation(blend.colorEquation);attachment.alphaBlendOperation=PreviewEquation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile actual original Preview attachment pipeline: "+Error(error));
		pipelines[key]={program.libraries,pipeline};lease.Retain(pipeline);milliseconds+=Milliseconds(start);return pipeline;
	}
};
SensorPreviewGPU::SensorPreviewGPU():impl(std::make_unique<Impl>()){}
SensorPreviewGPU::~SensorPreviewGPU()=default;
void SensorPreviewGPU::ClearResources(){impl->buffers.clear();impl->pipelines.clear();std::erase_if(impl->programs,[](const auto& x){return x.second.expired();});std::erase_if(impl->libraries,[](const auto& x){return x.second.expired();});}
std::shared_ptr<const NativeSensorPreviewProgramConsumer> SensorPreviewGPU::CompileOriginalStages(RuntimeLease& lease,const NativeSensorPreviewProgramSource& source,double& milliseconds)
{
	ValidateNativeSensorPreviewProgramSource(source);
	auto program=std::make_shared<Impl::Program>();program->source=source;program->libraries=impl->LibrariesFor(lease,source,milliseconds);
	auto consumer=std::make_shared<NativeSensorPreviewProgramConsumer>();consumer->pipelineLifetime=program;consumer->source=source;
	impl->programs[program.get()]=program;return consumer;
}
std::unique_ptr<PreparedSensorPreviewCommand> SensorPreviewGPU::Prepare(RuntimeLease& lease,const NativeSensorPreviewCommand& command,
	const SensorPreviewTarget& target,const SensorPreviewImageResolver& resolver,ScreenGPU& screen,bool validation,double& milliseconds,NativePostprocessConsumer consumer)
{
	Require(target.width&&target.height&&target.logicalWidth&&target.logicalHeight&&(target.samples>=1&&target.samples<=32&&[lease.Device() supportsTextureSampleCount:target.samples])&&target.color!=MTLPixelFormatInvalid,
		"Original Preview requires its actual supported target dimensions/format/sample count");
	auto prepared=std::unique_ptr<PreparedSensorPreviewCommand>(new PreparedSensorPreviewCommand);auto& out=*prepared->impl;out.source=command;out.target=target;
	if(command.output){
		Require(command.output==target.outputVersion&&command.previous==target.previousVersion&&target.output&&command.output->image&&command.output->generation,
			"Original Preview actual target version identities differ");
		const auto& image=*command.output->image;
		Require(image.renderToTexture&&target.width==image.width&&target.height==image.height&&target.output.width==image.width&&target.output.height==image.height&&
			target.output.pixelFormat==target.color&&(target.color==MTLPixelFormatR16Float||target.color==MTLPixelFormatR32Float||target.color==MTLPixelFormatR8Unorm)&&target.output.sampleCount==1&&target.samples==1&&
			(target.output.usage&MTLTextureUsageRenderTarget)&&target.depth==MTLPixelFormatInvalid&&target.stencil==MTLPixelFormatInvalid,
			"Original Preview target is not its actual original scalar color attachment");
		Require(image.format==(target.color==MTLPixelFormatR8Unorm?NativePostprocessFormat::R8Unorm:target.color==MTLPixelFormatR16Float?NativePostprocessFormat::R16Float:NativePostprocessFormat::R32Float),
			"Original Preview scalar color storage differs from its actual declaration");
		lease.AddAllocation(target.output);
		if(command.previous){Require(target.previous&&command.previous->image==command.output->image&&command.previous->generation<command.output->generation&&
			target.previous!=target.output&&target.previous.width==target.width&&target.previous.height==target.height&&target.previous.pixelFormat==target.color,
			"Original Preview partial update lacks its exact previous generation");lease.AddAllocation(target.previous);}
		else Require(target.previous==nil,"Original Preview invented previous target contents");
		if(command.previous)Require(bool(target.requirePreviousReadable),"Original Preview previous target lacks its actual producer-readability proof");
		out.pass=[MTL4RenderPassDescriptor new];out.pass.colorAttachments[0].texture=target.output;
		out.pass.colorAttachments[0].loadAction=target.previous?MTLLoadActionLoad:MTLLoadActionDontCare;
		out.pass.colorAttachments[0].storeAction=MTLStoreActionStore;out.pass.renderTargetWidth=target.width;out.pass.renderTargetHeight=target.height;
	}else Require(!target.output&&!target.previous&&!target.outputVersion&&!target.previousVersion,"Final original Preview draw was given a private replacement target");
	if(command.kind==NativeSensorPreviewCommandKind::Clear){
		Require(bool(command.output)&&!command.draw,"Original Preview clear requires its actual FBO command");
		out.clear=screen.PrepareColorAttachmentClear(lease,command.clearColor,target.width,target.height,command.surface.scissor,command.raster.colorWrite,target.color,validation,milliseconds);
		return prepared;
	}
	Require(command.draw&&command.kind==NativeSensorPreviewCommandKind::Draw,"Unknown actual original Preview command");
	const auto& draw=*command.draw;ValidateNativeSensorPreviewDraw(draw);
	Require(draw.sequence==command.sequence&&draw.sourceDrawFrame==command.sourceDrawFrame&&draw.callin==command.callin,"Original Preview command/draw ordinal differs");
	const auto found=impl->programs.find(draw.program.consumer->pipelineLifetime.get());
	Require(found!=impl->programs.end(),"Original Preview program belongs to another native context");const auto program=found->second.lock();
	Require(program&&program.get()==draw.program.consumer->pipelineLifetime.get()&&program->source.vertex==draw.program.source->vertex&&program->source.fragment==draw.program.source->fragment,
		"Original Preview actual compiled program owner expired or changed");
	WorldCamera camera;camera.viewport=target.cameraViewport;
	if(draw.surface.viewport){const auto& v=*draw.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
	auto raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,draw.surface.scissor);
	if(!raster)return prepared;
	out.raster=*raster;out.state=draw.raster;
	if(target.depth==MTLPixelFormatInvalid){out.state.depthTest=false;out.state.depthWrite=false;}
	if(target.stencil==MTLPixelFormatInvalid)out.state.stencilTest=false;
	const auto mask=NativeSensorPreviewRequiredTextureMask(draw.program),contents=NativeSensorPreviewRequiredContentsMask(draw.program);
	auto uniform=draw.program.uniforms;uniform.imageOptions={};
	uniform.imageOptions[2]=uint32_t(draw.surface.alphaTest.compare)|(uint32_t(draw.surface.alphaTest.enabled)<<8);
	uniform.imageOptions[3]=std::bit_cast<uint32_t>(draw.surface.alphaTest.reference);
	uniform.fragmentTransform={float(target.logicalWidth)/target.width,-float(target.logicalHeight)/target.height,0,float(target.logicalHeight)};
	Require(bool(draw.original),"Original Preview vertex clip control lacks actual engine uniforms");ValidateNativeCUSUniformPublication(*draw.original);
	uint32_t caps=0;std::memcpy(&caps,draw.original->params->bytes.data()+12,4);uniform.imageOptions[1]=caps&1u;
	std::array<id<MTLTexture>,7> textures{};std::array<id<MTLSamplerState>,7> samplers{};
	for(unsigned i=0;i<7;++i)if(mask&(1u<<i)){
		Require(bool(resolver),"Original Preview actual image resolver is not installed");const auto& input=draw.images[i];auto image=resolver(lease,input,bool(contents&(1u<<i)),consumer);
		Require(image.texture&&image.storageOwner&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1&&
			image.original==input.binding.object&&image.allocation==input.allocation&&image.version==input.version,
			"Original Preview image resolver substituted owner/allocation/version");
		if(input.allocation)Require(image.texture.width==input.allocation->width&&image.texture.height==input.allocation->height,"Original Preview image storage differs from its original allocation");
		if(input.binding.object){const auto& a=input.binding.object->allocation;Require(image.texture.width==a.width&&image.texture.height==a.height&&image.texture.mipmapLevelCount==a.levels,"Original Preview foreign image extent differs");
			// All five original programs read only .r/.x. The actual heightmap has RRRR swizzle; unused GBA need not be identity.
			Require(input.binding.object->sampling.swizzle[0]==0x1903,"Original Preview consumed red swizzle needs its actual typed consumer");}
		Require(!(contents&(1u<<i))||bool(image.requireReadable),"Original Preview sampled texels lack actual producer-readability proof");
		if(!(contents&(1u<<i)))image.requireReadable={}; // Actual allocation-only size query, no invented contents.
		Require(!target.output||image.texture!=target.output,"Original Preview read/write framebuffer feedback is unsupported");
		uniform.imageOptions[0]|=uint32_t(image.flipV)<<i;textures[i]=image.texture;samplers[i]=image.sampler;
		lease.AddAllocation(image.texture);if(image.sampler)lease.Retain(image.sampler);out.images.push_back(std::move(image));
	}
	unsigned variant=0;
	if(program->source.kind==NativeSensorPreviewProgramKind::Sheet||program->source.kind==NativeSensorPreviewProgramKind::JammerSheet){
		variant=unsigned(PreviewDepth(textures[0]))|(unsigned(program->source.modelDepthTest&&PreviewDepth(textures[1]))<<1);
		Require(samplers[0]&&(!program->source.modelDepthTest||samplers[1]),"Original Preview sheet needs its actual depth/color samplers");
		if(program->source.kind==NativeSensorPreviewProgramKind::JammerSheet&&(mask&32u))Require(samplers[5]!=nil,"Original Jammer SDF needs its actual LINEAR sampler");
	}
	std::array<id<MTLBuffer>,5> buffers{};
	if(program->source.kind==NativeSensorPreviewProgramKind::Sheet||program->source.kind==NativeSensorPreviewProgramKind::JammerSheet){const auto& source=*draw.original;
		buffers[0]=impl->Cached(lease,source.matrices,source.matrices->bytes.data(),source.matrices->bytes.size());
		buffers[1]=impl->Cached(lease,source.params,source.params->bytes.data(),source.params->bytes.size());}
	buffers[2]=impl->Cached(lease,draw.vertices,draw.vertices->bytes.data(),draw.vertices->bytes.size());buffers[3]=PreviewUpload(lease,&uniform,sizeof(uniform));
	if(NativeSensorPreviewRequiresStorage(draw.program))buffers[4]=impl->Cached(lease,draw.storage.snapshot,draw.storage.snapshot->bytes.data(),draw.storage.snapshot->bytes.size());
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=5;descriptor.maxTextureBindCount=7;descriptor.maxSamplerStateBindCount=7;descriptor.initializeBindings=YES;
	NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(out.arguments!=nil,"Cannot allocate original Preview argument bindings");lease.Retain(out.arguments);
	for(unsigned i=0;i<buffers.size();++i)if(buffers[i])[out.arguments setAddress:buffers[i].gpuAddress+(i==4?draw.storage.offset:0) atIndex:i];
	for(unsigned i=0;i<textures.size();++i)if(textures[i]){[out.arguments setTexture:textures[i].gpuResourceID atIndex:i];if(samplers[i])[out.arguments setSamplerState:samplers[i].gpuResourceID atIndex:i];}
	out.pipeline=impl->PipelineFor(lease,*program,variant,draw,target,validation,milliseconds);out.depth=effectstate::Depth(lease,out.state,target.stencil);
	out.first=draw.first;out.count=draw.count;return prepared;
}
}
