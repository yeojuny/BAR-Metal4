/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Distortion.h"
#include "Metal4MainAttachments.h"
#include "Metal4NativeEffectState.h"
#include "Metal4ShieldNative.h"
#include "NativeDistortionShaders.h"
#include "Rendering/Backend/NativeDistortionPrograms.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
using distortionProgram::DrawUniforms;
struct LinkedProgram {
	NativeDistortionProgramSource source;
	NativeDistortionProgram kind = NativeDistortionProgram::Volume;
	mutable std::array<__strong id<MTLLibrary>,4> libraries{};
	mutable std::map<std::vector<uint64_t>, id<MTLRenderPipelineState>> pipelines;
};
struct Image {
	std::weak_ptr<const NativeDistortionImage> owner;
	__strong id<MTLTexture> texture = nil;
	__strong id<MTLSamplerState> sampler = nil;
	std::weak_ptr<const NativeDistortionVersion> encoded;
};
float Scalar(const std::map<std::string,std::vector<float>>& values, const char* name, float fallback)
{
	const auto it = values.find(name);
	if (it == values.end()) return fallback;
	Require(it->second.size() == 1, "Original Distortion scalar uniform arity differs");
	return it->second[0];
}
float Define(const NativeDistortionProgramSource& source, const char* name)
{
	const auto& v = source.defines.at(name);
	return std::holds_alternative<float>(v) ? std::get<float>(v) : std::stof(std::get<std::string>(v));
}
std::string Source(const NativeDistortionProgramSource& source,unsigned modelBuffers)
{
	std::ostringstream text;
	text << std::setprecision(std::numeric_limits<float>::max_digits10);
	for (const auto& [name,value]: source.defines) {
		text << "#define " << name << ' ';
		std::visit([&](const auto& scalar) { text << scalar; }, value);
		text << '\n';
	}
	text << "#define NATIVE_MODEL_BUFFERS " << modelBuffers << '\n';
	text << distortion::SHADER;
	return text.str();
}
// The source program identity is unchanged; only actual resource presence varies.
id<MTLLibrary> Library(RuntimeLease& lease,const LinkedProgram& program,unsigned modelBuffers,double& ms)
{
	auto& library=program.libraries[modelBuffers];
	if(library){Require(library.device==lease.Device(),"Distortion library belongs to another device");return library;}
	const auto begin=Clock::now();
	auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Original31657 Distortion GL4";descriptor.source=String(Source(program.source,modelBuffers));
	auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
	NSError* error=nil;library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original Distortion GLSL equations as MSL: "+Error(error));
	ms+=Milliseconds(begin);return library;
}
unsigned ModelBuffers(const NativeDistortionDraw& draw)
{
	const unsigned mask=unsigned(bool(draw.transforms))|(unsigned(bool(draw.modelUniforms))<<1);
	const float attached=Scalar(draw.floatUniforms,"attachedtounitID",0);
	Require(std::isfinite(attached),"Distortion attachment selector is nonfinite");
	Require(!(attached>0)||mask==3,"Attached Distortion requires actual transforms and model uniforms");
	// Type11 reads model speed independently of attachedtounitID. Do not
	// remove that calculation merely because this is a world-space draw.
	if(!(mask&2))for(size_t i=draw.firstInstance;i<size_t(draw.firstInstance)+draw.instanceCount;++i){
		float type;std::memcpy(&type,draw.instances->bytes.data()+i*draw.instances->layout.stride+92,sizeof(type));
		Require(std::isfinite(type)&&std::round(type)!=11.f,"Distortion type11 requires its actual model uniform publication");
	}
	return mask;
}
id<MTLBuffer> Allocate(RuntimeLease& lease, size_t size)
{
	Require(size && size <= lease.Device().maxBufferLength, "Original Distortion buffer extent exceeds device");
	auto value = [lease.Device() newBufferWithLength:size options:MTLResourceStorageModeShared];
	Require(value != nil, "Cannot allocate original Distortion buffer");
	lease.AddAllocation(value);
	return value;
}
id<MTLBuffer> Upload(RuntimeLease& lease, const void* bytes, size_t size)
{
	auto value = Allocate(lease,size);
	std::memcpy(value.contents,bytes,size);
	return value;
}
void RequireVersion(const std::shared_ptr<Image>& image, const std::shared_ptr<const NativeDistortionVersion>& version)
{
	Require(image && image->encoded.lock() == version, "Distortion sampled or loaded an image before its actual ordered producer, or after another version overwrote it");
}
MTLBlendOperation Equation(ScreenBlendEquation e)
{
	switch(e) {
		case ScreenBlendEquation::Add: return MTLBlendOperationAdd;
		case ScreenBlendEquation::Subtract: return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract: return MTLBlendOperationReverseSubtract;
		case ScreenBlendEquation::Minimum: return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum: return MTLBlendOperationMax;
	}
	throw std::runtime_error("Unknown original Distortion blend equation");
}
void CheckDepth(const NativeCUSTextureBinding& binding, const DeferredLightImage& resolved, unsigned slot)
{
	Require(slot < 2 && binding.object && resolved.image.texture && resolved.image.owner && resolved.image.sampler && resolved.image.requireReadable,
		"Distortion lacks its actual independent depth image owner");
	const auto backing = std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);
	Require(backing && backing->Kind() == (slot == 0 ? NativeTextureBackingKind::MapDepth : NativeTextureBackingKind::ModelDepth),
		"Distortion sampled the wrong original map/model depth role");
	const auto& image = GetShieldDepthBackingImage(*backing);
	Require(image.texture == resolved.image.texture && image.producer && resolved.epoch.actualProducer == image.producer &&
		image.role == (slot == 0 ? ShieldDepthRole::TerrainOnly : ShieldDepthRole::ModelsOnly) &&
		image.texture.pixelFormat == MTLPixelFormatDepth32Float,
		"Distortion depth binding is not its actual original deferred producer");
}
} // namespace

struct PreparedDistortion::Impl {
	struct Operation {
		std::shared_ptr<const NativeDistortionCommand> source;
		std::shared_ptr<const LinkedProgram> program;
		std::shared_ptr<Image> output, previous;
		std::array<std::shared_ptr<Image>,2> inputs;
		__strong id<MTLTexture> color = nil, depthTexture = nil, stencilTexture = nil;
		__strong id<MTLRenderPipelineState> pipeline = nil;
		__strong id<MTLDepthStencilState> depthState = nil;
		__strong id<MTL4ArgumentTable> arguments = nil;
		__strong id<MTLBuffer> indices = nil;
		MTLIndexType indexType = MTLIndexTypeUInt32;
		NativeWorldDrawState world;
		effectstate::Raster raster;
		std::vector<CUSSourceImage> images;
		bool noop = false, encoded = false;
	};
	std::shared_ptr<const NativeWorldDrawFrame> frame;
	std::vector<Operation> operations;
	bool dependenciesEncoded = false, completed = false;
	std::shared_ptr<const MainFrameAttachments> main;
};
PreparedDistortion::PreparedDistortion(): impl(std::make_unique<Impl>()) {}
PreparedDistortion::~PreparedDistortion() = default;
size_t PreparedDistortion::DrawCount() const { return impl->operations.size(); }
uint64_t PreparedDistortion::SequenceAt(size_t index) const { return impl->operations.at(index).source->sequence; }
void PreparedDistortion::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(command && !impl->dependenciesEncoded, "Distortion source dependencies encoded twice");
	std::set<const void*> seen;
	for (auto& op: impl->operations) for (auto& image: op.images)
		if (seen.insert(image.owner.get()).second && image.encodeDependencies) image.encodeDependencies(command);
	impl->dependenciesEncoded = true;
}
void PreparedDistortion::EncodeDraw(size_t index, id<MTL4CommandBuffer> command)
{
	Require(command && impl->dependenciesEncoded && !impl->completed, "Distortion operation lacks common command/dependency lifetime");
	auto& op = impl->operations.at(index);
	const auto& source = *op.source;
	Require(!op.encoded, "Distortion actual source ordinal encoded twice");
	if (source.previous) RequireVersion(op.previous,source.previous);
	for (size_t i = 0; i < source.inputs.size(); ++i) if (source.inputs[i]) RequireVersion(op.inputs[i],source.inputs[i]);
	for (const auto& image: op.images) image.requireReadable();
	if (!op.noop) {
		if (impl->main && source.kind == NativeDistortionOperation::CopyScreen)
			impl->main->EncodeResolve(command, MainReadImages::Color);
		auto pass = [MTL4RenderPassDescriptor new];
		pass.defaultRasterSampleCount = op.color.sampleCount;
		pass.renderTargetWidth = op.color.width; pass.renderTargetHeight = op.color.height;
		pass.colorAttachments[0].texture = op.color;
		pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		if (op.depthTexture) {
			pass.depthAttachment.texture = op.depthTexture;
			pass.depthAttachment.loadAction = MTLLoadActionLoad;
			pass.depthAttachment.storeAction = MTLStoreActionStore;
		}
		if (op.stencilTexture) {
			pass.stencilAttachment.texture = op.stencilTexture;
			pass.stencilAttachment.loadAction = MTLLoadActionLoad;
			pass.stencilAttachment.storeAction = MTLStoreActionStore;
		}
		auto encoder = [command renderCommandEncoderWithDescriptor:pass];
		Require(encoder != nil, "Cannot encode original Distortion operation");
		[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		effectstate::RasterState(encoder,op.world,op.raster);
		[encoder setCullMode:!op.world.cullEnabled ? MTLCullModeNone : op.world.cull == NativeWorldCull::Front ? MTLCullModeFront : MTLCullModeBack];
		[encoder setTriangleFillMode:op.world.frontPolygonMode == NativePolygonMode::Line ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
		[encoder setDepthClipMode:op.world.depthClamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
		[encoder setRenderPipelineState:op.pipeline];
		[encoder setDepthStencilState:op.depthState];
		[encoder setArgumentTable:op.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		if (source.draw) {
			const auto& draw = *source.draw;
			if (draw.indexed) {
				const size_t offset = draw.firstIndex * (op.indexType == MTLIndexTypeUInt16 ? 2 : 4);
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:op.indexType
					indexBuffer:op.indices.gpuAddress+offset indexBufferLength:op.indices.length-offset
					instanceCount:draw.instanceCount baseVertex:draw.baseVertex baseInstance:draw.firstInstance];
			} else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:draw.firstVertex vertexCount:draw.count instanceCount:draw.instanceCount baseInstance:draw.firstInstance];
		} else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
		[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder endEncoding];
	}
	// Allocation is never called produced. Even no-coverage operations retain
	// their actual prior pixels/undefined channels, exactly as the GL target did.
	if (op.output) op.output->encoded = source.output;
	op.encoded = true;
}
void PreparedDistortion::DidComplete()
{
	Require(!impl->completed && impl->dependenciesEncoded && std::all_of(impl->operations.begin(),impl->operations.end(),[](const auto& op){return op.encoded;}),
		"Distortion cannot retire before all original source operations and common wait");
	std::set<const void*> seen;
	for (const auto& op: impl->operations) for (const auto& image: op.images)
		if (seen.insert(image.owner.get()).second && image.didComplete) image.didComplete();
	impl->completed = true;
}

struct DistortionGPU::Impl {
	struct Buffer { std::weak_ptr<const void> owner; __strong id<MTLBuffer> value = nil; };
	std::unordered_map<const void*,Buffer> buffers,meshes;
	std::unordered_map<const void*,std::weak_ptr<const LinkedProgram>> programs;
	std::map<uint64_t,std::shared_ptr<Image>> images;
	template<class T,class Fill> id<MTLBuffer> Cached(RuntimeLease& lease,std::shared_ptr<const T> owner,size_t size,Fill fill)
	{
		Require(bool(owner),"Distortion immutable buffer owner missing");
		auto& cache = buffers[owner.get()];
		if (cache.owner.expired()) { auto value = Allocate(lease,size); fill(static_cast<uint8_t*>(value.contents)); cache = {owner,value}; }
		else { Require(cache.value.length == size,"Distortion immutable backing extent changed"); lease.AddAllocation(cache.value); }
		return cache.value;
	}
	id<MTLBuffer> Published(RuntimeLease& lease,const std::shared_ptr<const NativeCUSPublishedBuffer>& source)
	{
		Require(source && source->stride && source->capacityElements && source->capacityElements <= std::numeric_limits<size_t>::max()/source->stride,"Distortion original model publication extent differs");
		return Cached(lease,source,source->capacityElements*source->stride,[&](uint8_t* bytes) {
			for (const auto& range: NativeCUSObservedRanges(*source)) for (size_t offset=range.offset;offset<range.offset+range.size;) {
				const size_t page=offset/NativeCUSPublishedPage::Bytes, local=offset%NativeCUSPublishedPage::Bytes;
				const size_t count=std::min(range.offset+range.size-offset,NativeCUSPublishedPage::Bytes-local);
				std::memcpy(bytes+offset,source->pages.at(page)->bytes.data()+local,count);offset+=count;
			}
		});
	}
	id<MTLBuffer> Mesh(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		const auto attribute=std::find_if(source->layout.attributes.begin(),source->layout.attributes.end(),[](const auto& a){return a.id==0;});
		Require(attribute!=source->layout.attributes.end() && attribute->scalarType==NativeScalarType::Float32 && attribute->byteSize==16 && !attribute->normalized,"Distortion original mesh is not float4 attribute0");
		auto& cache=meshes[source.get()];
		if(cache.owner.expired()) {
			auto value=Allocate(lease,source->layout.elements*16);
			for(size_t i=0;i<source->layout.elements;++i)std::memcpy(static_cast<uint8_t*>(value.contents)+i*16,source->bytes.data()+i*source->layout.stride+attribute->offset,16);
			cache={source,value};
		} else lease.AddAllocation(cache.value);
		return cache.value;
	}
	std::shared_ptr<Image> LocalImage(RuntimeLease& lease,const std::shared_ptr<const NativeDistortionVersion>& version)
	{
		Require(version && version->image,"Distortion target has no actual private image descriptor");
		const auto& descriptor=*version->image;
		auto& image=images[descriptor.identity];
		if(!image || image->owner.expired()) {
			image=std::make_shared<Image>();image->owner=version->image;
			auto texture=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:descriptor.format==NativeDistortionFormat::RGBA16Float?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA8Unorm width:descriptor.width height:descriptor.height mipmapped:NO];
			texture.storageMode=MTLStorageModePrivate;texture.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
			image->texture=[lease.Device() newTextureWithDescriptor:texture];
			auto sampler=[MTLSamplerDescriptor new];sampler.supportArgumentBuffers=YES;sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;sampler.mipFilter=MTLSamplerMipFilterNotMipmapped;sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeClampToBorderColor;sampler.borderColor=MTLSamplerBorderColorTransparentBlack;
			image->sampler=[lease.Device() newSamplerStateWithDescriptor:sampler];
			Require(image->texture && image->sampler,"Cannot allocate original Distortion texture/sampler");
		}
		lease.AddAllocation(image->texture);lease.Retain(image->sampler);return image;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const LinkedProgram& program,NativeDistortionOperation kind,MTLPixelFormat format,const ScreenBlendState& blend,const std::array<bool,4>& mask,unsigned modelBuffers,bool validation,double& ms,unsigned samples,bool coverage)
	{
		std::vector<uint64_t> key{uint64_t(modelBuffers),uint64_t(kind),uint64_t(format),uint64_t(validation),uint64_t(blend.enabled),uint64_t(blend.sourceColor),uint64_t(blend.destinationColor),uint64_t(blend.sourceAlpha),uint64_t(blend.destinationAlpha),uint64_t(blend.colorEquation),uint64_t(blend.alphaEquation)};
		for(bool value:mask)key.push_back(value);key.push_back(lease.SampleShadingKey(samples));key.push_back(coverage);
		if(const auto it=program.pipelines.find(key);it!=program.pipelines.end())return it->second;
		const auto library=Library(lease,program,modelBuffers,ms);
		const auto begin=Clock::now();auto descriptor=[MTL4RenderPipelineDescriptor new];
		descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto vertex=[MTL4LibraryFunctionDescriptor new], fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		switch(kind) {
			case NativeDistortionOperation::Volume: vertex.name=@"DistortionVertex";fragment.name=@"DistortionFragment";break;
			case NativeDistortionOperation::Combine: vertex.name=@"DistortionCombineVertex";fragment.name=@"DistortionCombineFragment";break;
			case NativeDistortionOperation::CopyScreen: vertex.name=@"DistortionTransferVertex";fragment.name=@"DistortionCopyFragment";break;
			case NativeDistortionOperation::Clear: vertex.name=@"DistortionTransferVertex";fragment.name=@"DistortionClearFragment";break;
		}
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto color=descriptor.colorAttachments[0];color.pixelFormat=format;color.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		color.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);color.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		color.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);color.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		color.rgbBlendOperation=Equation(blend.colorEquation);color.alphaBlendOperation=Equation(blend.alphaEquation);
		color.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		NSError* error=nil;auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot link original Distortion stages/target: "+Error(error));program.pipelines.emplace(std::move(key),pipeline);ms+=Milliseconds(begin);return pipeline;
	}
};
DistortionGPU::DistortionGPU():impl(std::make_unique<Impl>()){}
DistortionGPU::~DistortionGPU()=default;
void DistortionGPU::ClearResources(){impl->buffers.clear();impl->meshes.clear();impl->images.clear();impl->programs.clear();}
std::shared_ptr<const NativeDistortionProgramConsumer> DistortionGPU::LinkOriginalStages(RuntimeLease& lease,const NativeDistortionProgramSource& source,double& ms)
{
	ValidateNativeDistortionProgramSource(source);const auto begin=Clock::now();auto program=std::make_shared<LinkedProgram>();program->source=source;
	program->kind=source.name=="Deferred Distortions GL4"?NativeDistortionProgram::Volume:NativeDistortionProgram::Combine;
	// Compile the usual complete volume contract at link time, or the resource-free
	// vertex contract for the combine/transfer family. Other masks cache on demand.
	double compileMs=0;Library(lease,*program,program->kind==NativeDistortionProgram::Volume?3u:0u,compileMs);
	auto consumer=std::make_shared<NativeDistortionProgramConsumer>();consumer->pipelineLifetime=program;consumer->kind=program->kind;consumer->vertexSHA512=source.vertexSHA512;consumer->fragmentSHA512=source.fragmentSHA512;
	consumer->uniformBufferCopy=program->kind==NativeDistortionProgram::Volume&&source.defines.contains("UNIFORMSBUFFERCOPY");
	// Source-paired uniforms: inactive original radius/intensity/windX/windZ and
	// the misspelled original distortionStrength remain inactive, not invented.
	const auto add=[&](const char* name,unsigned type,bool integer,std::vector<float> defaults) {
		consumer->uniforms.push_back({name,type,1,integer});(integer?consumer->defaultInt:consumer->defaultFloat)[name]=std::move(defaults);
	};
	if(program->kind==NativeDistortionProgram::Volume) {
		add("pointbeamcone",0x1406,false,{0});add("attachedtounitID",0x1406,false,{0});add("windXZ",0x8b50,false,{0,0});
		add("mapDepths",0x8b5e,true,{0});add("modelDepths",0x8b5e,true,{1});add("noise3DCube",0x8b5f,true,{2});
	} else {
		add("inverseScreenResolution",0x8b50,false,{1.0f/1920,1.0f/1080});add("distortionOverallStrength",0x1406,false,{1});
		add("screenCopyTexture",0x8b5e,true,{0});add("distortionTexture",0x8b5e,true,{1});
	}
	impl->programs[program.get()]=program;ms+=Milliseconds(begin);return consumer;
}

std::unique_ptr<PreparedDistortion> DistortionGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldDrawFrame> frame,uint64_t actualSourceDrawFrame,const WorldCamera& camera,
	uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture> sceneColor,id<MTLTexture> sceneDepth,id<MTLTexture> sceneStencil,
	const DistortionImageResolver& resolve,bool validation,double& ms,uint64_t submission,std::shared_ptr<const MainFrameAttachments> main)
{
	Require(frame&&sceneColor&&!sceneColor.framebufferOnly&&logicalWidth&&logicalHeight&&submission&&resolve,"Distortion lacks actual readable root scene/deferred submission");
	if(main)Require(main->Color()==sceneColor&&main->Depth()==sceneDepth&&main->Stencil()==sceneStencil,
		"Distortion main owner differs from actual scene output attachments");
	else Require(sceneColor.sampleCount==1,"Distortion MSAA source requires its ordered resolve owner");
	const auto readColor=main?main->ReadColor():sceneColor;
	Require(readColor && readColor.sampleCount==1 && (readColor.usage&MTLTextureUsageShaderRead),
		"Distortion screen copy requires an actual resolved readable image");
	std::erase_if(impl->buffers,[](const auto& v){return v.second.owner.expired();});std::erase_if(impl->meshes,[](const auto& v){return v.second.owner.expired();});
	std::erase_if(impl->images,[](const auto& v){return v.second->owner.expired();});std::erase_if(impl->programs,[](const auto& v){return v.second.expired();});
	auto result=std::make_unique<PreparedDistortion>();result->impl->frame=std::move(frame);result->impl->main=main;
	for(const auto& common:result->impl->frame->draws) {
		if(!common.distortion)continue;
		const auto& source=*common.distortion;ValidateNativeDistortionCommand(source);Require(common.stage==NativeWorldDrawStage::World&&common.sequence==source.sequence&&source.sourceDrawFrame==actualSourceDrawFrame,"Distortion source order/stage differs");
		PreparedDistortion::Impl::Operation op;op.source=common.distortion;op.world=source.world;op.color=sceneColor;op.depthTexture=sceneDepth;op.stencilTexture=sceneStencil;
		const auto consumer=source.draw?source.draw->program:source.transferProgram;Require(consumer&&consumer->pipelineLifetime,"Distortion transfer has no actual linked source owner");
		const auto found=impl->programs.find(consumer->pipelineLifetime.get());Require(found!=impl->programs.end()&&(op.program=found->second.lock()),"Distortion compiled program belongs to another native lifetime");
		if(source.output){op.output=impl->LocalImage(lease,source.output);op.color=op.output->texture;op.depthTexture=op.stencilTexture=nil;op.world.depthTest=op.world.depthWrite=op.world.stencilTest=false;}
		if(source.previous)op.previous=impl->LocalImage(lease,source.previous);
		for(size_t i=0;i<2;++i)if(source.inputs[i])op.inputs[i]=impl->LocalImage(lease,source.inputs[i]);
		Require(op.color.sampleCount && [lease.Device() supportsTextureSampleCount:op.color.sampleCount],"Distortion output sample count unsupported");
		for(auto attachment:{op.depthTexture,op.stencilTexture})if(attachment)
			Require(attachment.device==op.color.device&&attachment.width==op.color.width&&attachment.height==op.color.height&&attachment.sampleCount==op.color.sampleCount,
				"Distortion output depth/stencil differs from actual color storage");
		auto original=camera;
		if(source.surface.viewport){const auto& v=*source.surface.viewport;original.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		if(source.output)original.viewport={0,0,int32_t(op.color.width),int32_t(op.color.height)};
		auto scissor=source.surface.scissor;
		if(scissor&&!scissor->windowCoordinates){scissor->x+=camera.viewport[0];scissor->y+=camera.viewport[1];scissor->windowCoordinates=true;}
		if(source.kind==NativeDistortionOperation::CopyScreen){const auto& v=source.copyDestination;original.viewport={v[0],v[1],v[2],v[3]};scissor.reset();}
		auto raster=effectstate::Viewport(original,uint32_t(op.color.width),uint32_t(op.color.height),source.output?uint32_t(op.color.width):logicalWidth,source.output?uint32_t(op.color.height):logicalHeight,scissor);
		op.noop=!raster;if(raster)op.raster=*raster;
		auto blend=source.surface.blend;
		auto mask=source.world.colorWrite;
		if(source.kind==NativeDistortionOperation::Clear||source.kind==NativeDistortionOperation::CopyScreen){blend.enabled=false;op.world.alphaToCoverage=false;op.world.depthTest=op.world.depthWrite=op.world.stencilTest=op.world.cullEnabled=false;op.world.frontPolygonMode=op.world.backPolygonMode=NativePolygonMode::Fill;}
		if(source.kind==NativeDistortionOperation::CopyScreen)mask={true,true,true,true};
		if(op.world.cullEnabled&&op.world.cull==NativeWorldCull::FrontAndBack)op.noop=true;
		Require(op.world.frontPolygonMode==op.world.backPolygonMode&&op.world.frontPolygonMode!=NativePolygonMode::Point,"Distortion selected polygon mode differs");
		const unsigned modelBuffers=source.kind==NativeDistortionOperation::Volume?ModelBuffers(*source.draw):0u;
		op.pipeline=impl->Pipeline(lease,*op.program,source.kind,op.color.pixelFormat,blend,mask,modelBuffers,validation,ms,unsigned(op.color.sampleCount),op.world.alphaToCoverage);
		op.depthState=effectstate::Depth(lease,op.world,op.stencilTexture?op.stencilTexture.pixelFormat:MTLPixelFormatInvalid);
		auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=8;descriptor.maxTextureBindCount=descriptor.maxSamplerStateBindCount=3;descriptor.initializeBindings=YES;
		NSError* error=nil;op.arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];Require(op.arguments!=nil,"Cannot allocate actual Distortion argument table: "+Error(error));lease.Retain(op.arguments);lease.Retain(op.pipeline);lease.Retain(op.program->libraries[modelBuffers]);
		if(!source.draw) {
			struct alignas(16) Transfer {std::array<float,4> uv{},clear{};} transfer;
			transfer.clear=source.clearColor;
			if(source.kind==NativeDistortionOperation::CopyScreen){const auto& v=source.copySource;Require(v[0]>=0&&v[1]>=0&&uint64_t(v[0])+v[2]<=logicalWidth&&uint64_t(v[1])+v[3]<=logicalHeight,"Distortion source screen-copy rectangle exceeds actual root");transfer.uv={float(v[0])/logicalWidth,float(v[1])/logicalHeight,float(v[2])/logicalWidth,float(v[3])/logicalHeight};[op.arguments setTexture:readColor.gpuResourceID atIndex:0];[op.arguments setSamplerState:op.output->sampler.gpuResourceID atIndex:0];lease.AddAllocation(readColor);}
			auto buffer=Upload(lease,&transfer,sizeof(transfer));[op.arguments setAddress:buffer.gpuAddress atIndex:0];
		} else {
			const auto& draw=*source.draw;auto mesh=impl->Mesh(lease,draw.vertices);[op.arguments setAddress:mesh.gpuAddress atIndex:0];
			DrawUniforms uniforms;
			uniforms.pointbeamcone=Scalar(draw.floatUniforms,"pointbeamcone",0);uniforms.attachedtounitID=Scalar(draw.floatUniforms,"attachedtounitID",0);
			if(const auto it=draw.floatUniforms.find("windXZ");it!=draw.floatUniforms.end()){Require(it->second.size()==2,"Distortion windXZ arity differs");uniforms.windX=it->second[0];uniforms.windZ=it->second[1];}
			uniforms.overallStrength=Scalar(draw.floatUniforms,"distortionOverallStrength",1);
			uniforms.framebuffer={1,1,float(op.color.height),0};uniforms.alpha={float(draw.surface.alphaTest.enabled),draw.surface.alphaTest.reference,float(draw.surface.alphaTest.compare),0};
			if(const auto it=draw.floatUniforms.find("inverseScreenResolution");it!=draw.floatUniforms.end()){Require(it->second.size()==2,"Distortion inverse resolution arity differs");uniforms.inverseScreenResolution={it->second[0],it->second[1],0,0};}else uniforms.inverseScreenResolution={1.0f/1920,1.0f/1080,0,0};
			if(source.kind==NativeDistortionOperation::Volume) {
				auto instances=impl->Cached(lease,draw.instances,draw.instances->bytes.size(),[&](uint8_t* p){std::memcpy(p,draw.instances->bytes.data(),draw.instances->bytes.size());});[op.arguments setAddress:instances.gpuAddress atIndex:1];
				const auto& engine=*draw.engineUniforms;
				auto matrices=impl->Cached(lease,engine.matrices,engine.matrices->bytes.size(),[&](uint8_t* p){std::memcpy(p,engine.matrices->bytes.data(),engine.matrices->bytes.size());});
				auto params=impl->Cached(lease,engine.params,engine.params->bytes.size(),[&](uint8_t* p){std::memcpy(p,engine.params->bytes.data(),NativeCUSParamsPrefixBytes);});
				[op.arguments setAddress:matrices.gpuAddress atIndex:2];[op.arguments setAddress:params.gpuAddress atIndex:3];
				if(draw.transforms){Require(draw.transforms->stride==(Define(op.program->source,"USEQUATERNIONS")==0?64:32),"Distortion transform selector disagrees with actual model publication");auto buffer=impl->Published(lease,draw.transforms);[op.arguments setAddress:buffer.gpuAddress atIndex:4];}
				if(draw.modelUniforms){auto buffer=impl->Published(lease,draw.modelUniforms);[op.arguments setAddress:buffer.gpuAddress atIndex:5];}
    if(draw.program->uniformBufferCopy){const auto& range=draw.uniformBufferCopy;Require(range.snapshot&&range.size,"Distortion selected copy binding missing");auto buffer=impl->Cached(lease,range.snapshot,range.snapshot->bytes.size(),[&](uint8_t* p){std::memcpy(p,range.snapshot->bytes.data(),range.snapshot->bytes.size());});[op.arguments setAddress:buffer.gpuAddress+range.offset atIndex:7];}
				if(draw.indexed){
					if(draw.indexType==0x1401){std::vector<uint16_t> indices;for(uint8_t i:draw.indices->bytes)indices.push_back(i==0xff?0xffff:i);op.indices=Upload(lease,indices.data(),indices.size()*2);op.indexType=MTLIndexTypeUInt16;}
					else {op.indices=impl->Cached(lease,draw.indices,draw.indices->bytes.size(),[&](uint8_t* p){std::memcpy(p,draw.indices->bytes.data(),draw.indices->bytes.size());});op.indexType=draw.indexType==0x1403?MTLIndexTypeUInt16:MTLIndexTypeUInt32;}
				}
				uint32_t caps=0;std::memcpy(&caps,engine.params->bytes.data()+12,4);uniforms.transport[1]=!(caps&1u);uniforms.transport[2]=draw.firstInstance;
				for(unsigned slot=0;slot<3;++slot){
					const char* name=slot==0?"mapDepths":slot==1?"modelDepths":"noise3DCube";const auto sampler=draw.intUniforms.find(name);Require(sampler!=draw.intUniforms.end()&&sampler->second==std::vector<float>{float(slot)},"Distortion original sampler mapping differs");
					auto resolved=resolve(lease,draw.textures[slot],slot);auto& image=resolved.image;
					if(slot<2)CheckDepth(draw.textures[slot],resolved,slot);
					else Require(image.texture&&image.texture.textureType==MTLTextureType3D&&image.owner&&image.sampler&&image.requireReadable,"Distortion noise is not its genuine retained 3D DDS texture");
					if(image.flipV)uniforms.transport[0]|=1u<<slot;
					uniforms.imageSwizzle[slot]=draw.textures[slot].object->sampling.swizzle;uniforms.depthMode[slot/4][slot%4]=draw.textures[slot].object->sampling.depthMode;
					[op.arguments setTexture:image.texture.gpuResourceID atIndex:slot];[op.arguments setSamplerState:image.sampler.gpuResourceID atIndex:slot];lease.AddAllocation(image.texture);lease.Retain(image.sampler);op.images.push_back(std::move(image));
				}
			} else {
				for(unsigned slot=0;slot<2;++slot){auto image=op.inputs[slot];Require(image!=nullptr,"Distortion combine local input absent");uniforms.transport[0]|=1u<<slot;uniforms.imageSwizzle[slot]={0x1903,0x1904,0x1905,0x1906};[op.arguments setTexture:image->texture.gpuResourceID atIndex:slot];[op.arguments setSamplerState:image->sampler.gpuResourceID atIndex:slot];}
			}
			auto buffer=Upload(lease,&uniforms,sizeof(uniforms));[op.arguments setAddress:buffer.gpuAddress atIndex:6];
		}
		result->impl->operations.push_back(std::move(op));
	}
	return result;
}
} // namespace rendering::metal4
