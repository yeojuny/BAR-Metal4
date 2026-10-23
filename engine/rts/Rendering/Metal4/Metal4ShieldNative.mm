/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ShieldNative.h"
#include "NativeShieldShaders.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeShieldRecorder.h"
#import <simd/simd.h>
#include <cstring>
#include <atomic>
#include <map>
#include <limits>
#include <tuple>
#include <unordered_map>
namespace rendering::metal4 {
std::uint64_t CreateShieldSubmissionIdentity()
{
	static std::atomic<std::uint64_t> next{1};auto identity=next.load();
	for(;;){Require(identity!=std::numeric_limits<std::uint64_t>::max(),"Original Shield submission identity exhausted");if(next.compare_exchange_weak(identity,identity+1))return identity;}
}
std::shared_ptr<ShieldDepthProducerState> CreateShieldDepthProducer(ShieldDepthRole role,std::uint64_t frame)
{
	Require(role==ShieldDepthRole::TerrainOnly||role==ShieldDepthRole::ModelsOnly,"Original Shield cannot relabel combined scene or shadow depth as a GBuffer producer");
	static std::atomic<std::uint64_t> next{1};auto identity=next.load();
	for(;;){Require(identity!=std::numeric_limits<std::uint64_t>::max(),"Original Shield producer epoch exhausted");if(next.compare_exchange_weak(identity,identity+1))break;}
	return std::make_shared<ShieldDepthProducerState>(ShieldDepthProducerState{role,frame,identity});
}
namespace {
struct DepthBackingResource final:NativeShieldDepthResource {ShieldDepthImage image;explicit DepthBackingResource(const ShieldDepthImage& source):image(source){}};
struct Uniform {
	simd_float4x4 view,projection;
	simd_float4 cameraPosition;
	simd_float4 factors;simd_uint4 options,raster;
};
static_assert(sizeof(Uniform)==192);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,std::size_t bytes)
{
	Require(bytes&&bytes<=lease.Device().maxBufferLength,"Original Shield storage exceeds actual device limit");
	auto buffer=[lease.Device()newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate original Shield immutable storage");lease.AddAllocation(buffer);return buffer;
}
void ValidateDepth(const ShieldDepthImage& image,ShieldDepthRole role,const NativeShieldFrame& frame,const NativeShieldUniforms& uniforms,const WorldCamera& camera)
{
	Require(image.texture!=nil&&image.role==role&&image.producerIdentity!=0,"Original Shield requires actual separate map/model GBuffer producer, not scene/shadow depth");
	Require(image.sourceDrawFrame==frame.sourceDrawFrame,"Original Shield GBuffer belongs to another draw frame");
	Require(image.producer&&image.producer->identity==image.producerIdentity&&image.producer->sourceDrawFrame==frame.sourceDrawFrame&&image.producer->role==role,"Original Shield has a descriptor/clear placeholder without its actual typed producer");
	Require(image.view==uniforms.view&&image.projection==uniforms.projection&&image.viewport==camera.viewport,"Original Shield GBuffer camera/viewport differs from original shader matrices");
	Require(image.texture.textureType==MTLTextureType2D&&image.texture.pixelFormat==MTLPixelFormatDepth32Float&&image.texture.sampleCount==1&&image.texture.mipmapLevelCount==1,"Original Shield outline requires real single-sample D32F GBuffer");
	Require((image.texture.usage&MTLTextureUsageShaderRead)!=0,"Original Shield GBuffer is not sampleable");
}
}
void FulfillShieldDepthBacking(const NativeShieldDepthBacking& backing,const ShieldDepthImage& image)
{
	const auto expected=backing.Kind()==NativeTextureBackingKind::MapDepth?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly;
	const auto& metadata=backing.Metadata();
	Require(image.role==expected&&image.texture&&image.storageOwner&&image.producer&&image.producerIdentity,"Original named depth backing has no actual separate GBuffer image");
	Require(image.sourceDrawFrame==backing.sourceDrawFrame&&image.producer->sourceDrawFrame==backing.sourceDrawFrame&&image.producer->identity==image.producerIdentity&&image.producer->role==expected,"Original named depth backing producer identity differs from its published version");
	Require(image.view==metadata.view&&image.projection==metadata.projection&&image.viewport==metadata.viewport&&image.texture.width==metadata.width&&image.texture.height==metadata.height,"Original named depth backing extent/camera differs from its actual producer");
	Require(image.texture.pixelFormat==MTLPixelFormatDepth32Float&&image.texture.textureType==MTLTextureType2D&&image.texture.mipmapLevelCount==1&&image.texture.sampleCount==1&&(image.texture.usage&MTLTextureUsageShaderRead),"Original named GBuffer backing is not a sampleable single-level D32F image");
	const bool allocation=image.producer->operation==ShieldDepthOperation::AllocationOnly;
	Require(allocation==(backing.content==NativeTextureContent::AllocationOnly)&&(!allocation||image.producer->allocationVerified),"Original allocation-only named depth cannot advertise rendered/cleared pixels");
	backing.Fulfill(std::make_shared<DepthBackingResource>(image));
}
const ShieldDepthImage& GetShieldDepthBackingImage(const NativeShieldDepthBacking& backing)
{
	const auto resource=std::dynamic_pointer_cast<const DepthBackingResource>(backing.Resource());
	Require(bool(resource),"Original named GBuffer version has not been fulfilled by its actual native producer/allocation");return resource->image;
}
struct PreparedShield::Impl {
	struct Draw {std::uint64_t sequence;__strong id<MTLRenderPipelineState> pipeline;__strong id<MTLDepthStencilState> depth;__strong id<MTL4ArgumentTable> arguments;std::size_t vertices,instances;NativeWorldDrawState raster;effectstate::Raster viewport;};
	std::vector<Draw> draws;
	std::vector<std::shared_ptr<const void>> imageOwners;
	struct Producer {std::shared_ptr<const ShieldDepthProducerState> state;std::uint64_t submission=0;};std::vector<Producer> producers;
	std::vector<std::function<void()>> readableChecks;
};
PreparedShield::PreparedShield():impl(std::make_unique<Impl>()){} PreparedShield::~PreparedShield()=default;
std::size_t PreparedShield::DrawCount()const{return impl->draws.size();}
std::uint64_t PreparedShield::SequenceAt(std::size_t index)const{return impl->draws.at(index).sequence;}
void PreparedShield::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder)const
{
	for(const auto& entry:impl->producers){const auto& producer=entry.state;Require(IsNativeShieldDepthReadable(producer->encoded,producer->completed,producer->operation==ShieldDepthOperation::AllocationOnly,producer->allocationVerified,producer->submissionIdentity,entry.submission),"Original Shield outline sampled before its same-submission producer, retired prior version or actual allocation-only storage exists");}
	for(const auto& check:impl->readableChecks)check();
	const auto& draw=impl->draws.at(index);effectstate::RasterState(encoder,draw.raster,draw.viewport);
	[encoder setCullMode:draw.raster.cullEnabled?(draw.raster.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack):MTLCullModeNone];
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.vertices instanceCount:draw.instances];
}
struct ShieldGPU::Impl {
	__strong id<MTLLibrary> library=nil;MTLPixelFormat initializedFormat=MTLPixelFormatInvalid;bool validation=false;
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,unsigned,unsigned,bool,unsigned,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	__strong id<MTLSamplerState> sampler=nil;
	struct Mesh {std::weak_ptr<const NativeShieldGeometry> owner;__strong id<MTLBuffer> buffer;};
	std::unordered_map<const NativeShieldGeometry*,Mesh> meshes;
	struct Sampler {NativeTextureSampling state;__strong id<MTLSamplerState> sampler;};std::vector<Sampler> historySamplers;
	id<MTLSamplerState> DepthSampler(RuntimeLease& lease,const NativeTextureSampling& state)
	{
		for(const auto& item:historySamplers)if(item.state==state)return item.sampler;
		Require((state.minFilter==0x2600||state.minFilter==0x2601)&&(state.magFilter==0x2600||state.magFilter==0x2601)&&state.compareMode==0&&state.depthMode==0x1909&&state.baseLevel==0&&state.maxLevel>=0,"Original Shield depth sampler has an unimplemented original parameter combination");
		Require(state.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original Shield depth swizzle requires a typed original texture view");
		auto d=[MTLSamplerDescriptor new];d.minFilter=state.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.magFilter=state.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNotMipmapped;
		const auto address=[](unsigned mode){switch(mode){case 0x2901:return MTLSamplerAddressModeRepeat;case 0x8370:return MTLSamplerAddressModeMirrorRepeat;case 0x812F:return MTLSamplerAddressModeClampToEdge;case 0x812D:case 0x2900:return MTLSamplerAddressModeClampToBorderColor;default:throw std::invalid_argument("Original Shield depth wrap mode is unsupported");}};
		d.sAddressMode=address(state.wrapS);d.tAddressMode=address(state.wrapT);
		d.lodMinClamp=std::max(0.0f,state.minLod);d.lodMaxClamp=std::max(0.0f,state.maxLod);d.lodBias=state.lodBias;
		if(state.borderColor==std::array<float,4>{0,0,0,0})d.borderColor=MTLSamplerBorderColorTransparentBlack;
		else if(state.borderColor==std::array<float,4>{0,0,0,1})d.borderColor=MTLSamplerBorderColorOpaqueBlack;
		else if(state.borderColor==std::array<float,4>{1,1,1,1})d.borderColor=MTLSamplerBorderColorOpaqueWhite;
		else throw std::invalid_argument("Original Shield depth custom border requires its exact shader sampler consumer");
		Require(state.anisotropy>=1&&state.anisotropy<=16&&std::floor(state.anisotropy)==state.anisotropy,"Original Shield depth anisotropy exceeds the actual native sampler contract");d.maxAnisotropy=NSUInteger(state.anisotropy);d.supportArgumentBuffers=YES;
		auto sampler=[lease.Device()newSamplerStateWithDescriptor:d];Require(sampler!=nil,"Cannot create actual retained Shield GBuffer sampler");historySamplers.push_back({state,sampler});return sampler;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned outlines,unsigned mask,bool validation,unsigned historyKinds,bool hasImpacts,unsigned samples,bool coverage)
	{
		const Key key{color,depth,stencil,outlines,mask,historyKinds,hasImpacts,lease.SampleShadingKey(samples),coverage};if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"ShieldVertexMain";
		auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=String((historyKinds?"ShieldHistory"+std::to_string(outlines)+"_"+std::to_string(historyKinds):"ShieldFragment"+std::to_string(outlines))+(hasImpacts?"Impacts":""));
		auto descriptor=[MTL4RenderPipelineDescriptor new];descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;attachment.writeMask=MTLColorWriteMask(mask);attachment.blendingState=MTL4BlendStateEnabled;attachment.sourceRGBBlendFactor=attachment.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;attachment.destinationRGBBlendFactor=attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		NSError* error=nil;auto result=lease.NewRenderPipeline(descriptor,&error);Require(result!=nil,"Cannot compile original Shield outline PSO: "+Error(error));pipelines.emplace(key,result);return result;
	}
};
ShieldGPU::ShieldGPU():impl(std::make_unique<Impl>()){} ShieldGPU::~ShieldGPU(){if(impl->library!=nil)SetNativeShieldConsumerAvailable(false);}
void ShieldGPU::ClearResources(){impl->meshes.clear();}
void ShieldGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,bool validation,double& milliseconds,unsigned samples)
{
	if(impl->library!=nil&&impl->initializedFormat==color&&impl->validation==validation)return;
	const auto start=Clock::now();auto candidate=std::make_unique<Impl>();
	auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact current31657 ShieldSphereColorGL4 instances/binding6 plus finite impact-basis correction";descriptor.source=String(shield::SHADER);
	auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
	NSError* error=nil;candidate->library=lease.NewLibrary(descriptor,&error);Require(candidate->library!=nil,"Cannot compile original Shield shader: "+Error(error));
	auto sampler=[MTLSamplerDescriptor new];sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;sampler.mipFilter=MTLSamplerMipFilterNotMipmapped;sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeClampToBorderColor;sampler.borderColor=MTLSamplerBorderColorTransparentBlack;sampler.supportArgumentBuffers=YES;
	candidate->sampler=[lease.Device()newSamplerStateWithDescriptor:sampler];Require(candidate->sampler!=nil,"Cannot create original GBuffer transparent-border linear sampler");
	for(unsigned outline:{0u,2u,4u,6u})for(bool impacts:{false,true})candidate->Pipeline(lease,color,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,outline,MTLColorWriteMaskAll,validation,0,impacts,samples,false);
	candidate->initializedFormat=color;candidate->validation=validation;impl=std::move(candidate);milliseconds+=Milliseconds(start);
}
std::unique_ptr<PreparedShield> ShieldGPU::Prepare(RuntimeLease& lease,const NativeShieldFrame& frame,const WorldCamera& camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const ShieldDepthSources& sources,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,const ShieldColorImageResolver& resolveColor,unsigned outputSamples)
{
	if(frame.draws.empty())return {};
	Require(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8,"Original Shield needs actual scene depth test attachment");
	Require(camera.clipDepth!=ClipDepthConvention::Unsupported,"Original Shield camera clip convention is unsupported");
	Initialize(lease,color,validation,milliseconds,outputSamples);lease.Retain(impl->library);lease.Retain(impl->sampler);
	auto result=std::unique_ptr<PreparedShield>(new PreparedShield);
	std::erase_if(impl->meshes,[](const auto& entry){return entry.second.owner.expired();});
	for(const auto& draw:frame.draws){
		ValidateNativeShieldDraw(draw);Require(draw.sequence!=0,"Original Shield command lacks its actual world ordinal");const auto& u=draw.uniforms;
		if(draw.raster.cullEnabled&&draw.raster.cull==NativeWorldCull::FrontAndBack)continue;
		const unsigned outlines=u.effects&6u;
		if(!draw.textures){
			if(outlines&2u)ValidateDepth(sources.map,ShieldDepthRole::TerrainOnly,frame,u,camera);
			if(outlines&4u)ValidateDepth(sources.models,ShieldDepthRole::ModelsOnly,frame,u,camera);
			if(outlines==6u)Require(sources.map.texture!=sources.models.texture&&sources.map.producerIdentity!=sources.models.producerIdentity,"Original Shield map/model depths cannot alias a combined or fake producer");
		}
		auto drawCamera=camera;if(draw.surface.viewport){const auto& v=*draw.surface.viewport;Require(v.width<=unsigned(std::numeric_limits<int32_t>::max())&&v.height<=unsigned(std::numeric_limits<int32_t>::max()),"Original Shield viewport exceeds GLsizei");drawCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto raster=effectstate::Viewport(drawCamera,width,height,logicalWidth,logicalHeight,draw.surface.scissor);if(!raster)continue;
		unsigned historyKinds=0,flips=0,legacyClamp=0;std::array<ShieldSampledImage,2> samples;
		if(draw.textures){
			for(unsigned unit=0;unit<2;++unit){
				if(!(outlines&(2u<<unit)))continue;
				const auto& binding=draw.textures->samplers[unit];const auto kind=ClassifyNativeShieldTexture(binding);
				if(kind==NativeShieldTextureKind::Empty){historyKinds|=3u<<(unit*2);continue;}
				if(kind==NativeShieldTextureKind::Depth){
					const auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);Require(bool(backing),"Original Shield named depth lacks the paired typed native backing");
					const auto& image=GetShieldDepthBackingImage(*backing);samples[unit]={image.texture,impl->DepthSampler(lease,binding.object->sampling),backing,true,{}};
					Require(sources.submissionIdentity!=0,"Original Shield shared history needs the actual common submission identity");result->impl->producers.push_back({image.producer,sources.submissionIdentity});historyKinds|=1u<<(unit*2);
				}else{
					Require(bool(resolveColor),"Original Shield actual overwritten color binding has no shared image resolver");samples[unit]=resolveColor(lease,binding);historyKinds|=2u<<(unit*2);
					Require(samples[unit].texture&&samples[unit].texture.pixelFormat!=MTLPixelFormatDepth32Float,"Original Shield color history resolver substituted unrelated scene depth");
				}
				const auto& sample=samples[unit];Require(sample.texture&&sample.sampler&&sample.owner&&sample.texture.textureType==MTLTextureType2D&&(sample.texture.usage&MTLTextureUsageShaderRead),"Original Shield actual retained image/sampler owner is missing");
				lease.AddAllocation(sample.texture);lease.Retain(sample.sampler);result->impl->imageOwners.push_back(sample.owner);if(sample.requireReadable)result->impl->readableChecks.push_back(sample.requireReadable);
				if(sample.flipV)flips|=1u<<unit;
				const auto& sampling=binding.object->sampling;legacyClamp|=unsigned(sampling.wrapS==0x2900)<<(unit*2);legacyClamp|=unsigned(sampling.wrapT==0x2900)<<(unit*2+1);
			}
		}else for(const auto* image:{outlines&2u?&sources.map:nullptr,outlines&4u?&sources.models:nullptr})if(image){// ValidateDepth already checked the exact camera/projection and producer.
			// Shield samples full source clip UV, independent of drawable scale/crop.
			Require(image->texture.width&&image->texture.height,"Original Shield GBuffer allocation is empty");lease.AddAllocation(image->texture);result->impl->imageOwners.push_back(image->storageOwner);result->impl->producers.push_back({image->producer,0});}
		auto& mesh=impl->meshes[draw.geometry.get()];if(mesh.owner.expired()){auto buffer=Upload(lease,draw.geometry->vertices.data(),draw.geometry->vertices.size()*sizeof(NativeShieldVertex));mesh={draw.geometry,buffer};}else lease.AddAllocation(mesh.buffer);
		Uniform constants{};std::memcpy(&constants.view,u.view.data(),64);std::memcpy(&constants.projection,u.projection.data(),64);std::memcpy(&constants.cameraPosition,u.cameraPosition.data(),16);
		constants.factors=simd_make_float4(u.gameFrame,0,0,draw.surface.alphaTest.reference);constants.options=simd_make_uint4(u.effects,0,u.depthClipZeroToOne,0);constants.raster=simd_make_uint4(draw.surface.alphaTest.enabled,unsigned(draw.surface.alphaTest.compare),flips,legacyClamp);
		auto uniform=Upload(lease,&constants,sizeof(constants));
		auto instances=Upload(lease,draw.instances.data(),draw.instances.size()*sizeof(NativeShieldInstance));
		const bool hasImpacts=bool(draw.impactSource.snapshot);
		// Preserve the actual bound allocation tail. Used impact indices were
		// checked against the successful original Upload prefix, not capacity.
		id<MTLBuffer> impacts=nil;if(hasImpacts)impacts=Upload(lease,draw.impactPoints.data(),draw.impactPoints.size()*16);
		auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=4;descriptor.maxTextureBindCount=2;descriptor.maxSamplerStateBindCount=2;descriptor.initializeBindings=YES;
		NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Cannot allocate original Shield arguments: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:mesh.buffer.gpuAddress atIndex:0];[arguments setAddress:uniform.gpuAddress atIndex:1];[arguments setAddress:instances.gpuAddress atIndex:2];
		if(hasImpacts)[arguments setAddress:impacts.gpuAddress atIndex:3];
		if(draw.textures){for(unsigned unit=0;unit<2;++unit)if(samples[unit].texture){[arguments setSamplerState:samples[unit].sampler.gpuResourceID atIndex:unit];[arguments setTexture:samples[unit].texture.gpuResourceID atIndex:unit];}}
		else if(outlines){[arguments setSamplerState:impl->sampler.gpuResourceID atIndex:0];if(outlines&2u)[arguments setTexture:sources.map.texture.gpuResourceID atIndex:0];if(outlines&4u)[arguments setTexture:sources.models.texture.gpuResourceID atIndex:1];}
		unsigned mask=0;for(unsigned n=0;n<4;++n)if(draw.raster.colorWrite[n])mask|=8u>>n;
		auto pipeline=impl->Pipeline(lease,color,depth,stencil,outlines,mask,validation,historyKinds,hasImpacts,outputSamples,draw.raster.alphaToCoverage);lease.Retain(pipeline);
		result->impl->draws.push_back({draw.sequence,pipeline,effectstate::Depth(lease,draw.raster,stencil),arguments,draw.geometry->vertices.size(),draw.instances.size(),draw.raster,*raster});
	}
	return result;
}
}
