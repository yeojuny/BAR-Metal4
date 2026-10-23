/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PBRSource.h"
#include "Metal4WorldEnvironmentShader.h"
#include "NativePBREnvironmentShader.h"
#include <atomic>
#include <set>

namespace rendering::metal4 {
namespace {
struct Pipeline {__strong id<MTLLibrary> library=nil;__strong id<MTLComputePipelineState> state=nil;};
struct ImageStorage final:NativePBRImage {__strong id<MTLTexture> texture=nil;};
std::atomic<uint64_t> nextStorage{1};
std::shared_ptr<const ImageStorage> Storage(std::shared_ptr<const NativePBRImage> image){auto value=std::dynamic_pointer_cast<const ImageStorage>(image);Require(value&&value->texture,"Original PBR image lacks its actual GPU storage");return value;}
void Readable(const NativePBRImage& image,uint64_t submission){if(image.content==NativeTextureContent::AllocationOnly)return;Require(image.completed.load(std::memory_order_acquire)||(submission&&image.encodedSubmission.load(std::memory_order_acquire)==submission),"Original PBR producer has not executed before this consumer");}
}
struct PBRSourceGPU::Impl {
	std::shared_ptr<Pipeline> brdf,environment;
	bool validation=false,compiled=false;
	void Compile(RuntimeLease& lease,bool validate,double& elapsed){
		if(compiled&&validation==validate)return;auto start=Clock::now();NSError* error=nil;
		auto* ld=[MTL4LibraryDescriptor new];ld.source=String(std::string(WORLD_ENVIRONMENT_SHADER)+PBR_ENVIRONMENT_SHADER);ld.name=@"Exact30770 original PBR generators";
		id<MTLLibrary> library=lease.NewLibrary(ld,&error);Require(library,"Cannot compile original PBR sources: "+Error(error));
		auto make=[&](NSString* name){auto value=std::make_shared<Pipeline>();value->library=library;auto* function=[MTL4LibraryFunctionDescriptor new];function.library=library;function.name=name;auto* pd=[MTL4ComputePipelineDescriptor new];pd.computeFunctionDescriptor=function;pd.options=[MTL4PipelineOptions new];pd.options.shaderValidation=ShaderValidationMode(validate);value->state=[lease.Compiler() newComputePipelineStateWithDescriptor:pd compilerTaskOptions:nil error:&error];Require(value->state,"Cannot compile original PBR pipeline: "+Error(error));return value;};
		auto newBrdf=make(@"OriginalUnitBRDF"),newEnvironment=make(@"OriginalEnvLUT");brdf=std::move(newBrdf);environment=std::move(newEnvironment);compiled=true;validation=validate;elapsed+=Milliseconds(start);
	}
};
struct PreparedPBRSource::Impl {
	struct Draw {std::shared_ptr<const ImageStorage> image;std::shared_ptr<const Pipeline> pipeline;CUSSourceImage reflection;__strong id<MTLBuffer> params=nil;__strong id<MTL4ArgumentTable> arguments=nil;};
	std::vector<Draw> draws;uint64_t submission=0;bool completed=false;
};
PBRSourceGPU::PBRSourceGPU():impl(std::make_unique<Impl>()){}
PBRSourceGPU::~PBRSourceGPU()=default;
PreparedPBRSource::PreparedPBRSource():impl(std::make_unique<Impl>()){}
PreparedPBRSource::~PreparedPBRSource()=default;
std::shared_ptr<const NativePBRProgram> PBRSourceGPU::Compile(RuntimeLease& lease,const NativePBRProgramSource& source,bool validation,double& compileMs){auto p=std::make_shared<NativePBRProgram>(ValidateNativePBRProgram(source));impl->Compile(lease,validation,compileMs);p->pipelineOwner=p->kind==NativePBRKind::BRDF?impl->brdf:impl->environment;return p;}
std::shared_ptr<NativePBRImage> PBRSourceGPU::Allocate(RuntimeLease& lease,const NativePBRAllocation& allocation)
{
	ValidateNativePBRAllocation(allocation);auto result=std::make_shared<ImageStorage>();result->allocation=allocation;result->storageGeneration=nextStorage.fetch_add(1);result->contentVersion=1;
	const auto format=allocation.originalFormat==0x822F?MTLPixelFormatRG16Float:MTLPixelFormatRGBA16Float;
	auto* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:allocation.width height:allocation.height mipmapped:NO];td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget;
	result->texture=[lease.Device() newTextureWithDescriptor:td];Require(result->texture,"Cannot allocate original PBR target");lease.AddAllocation(result->texture);return result;
}
std::unique_ptr<PreparedPBRSource> PBRSourceGPU::Prepare(RuntimeLease& lease,std::span<const std::shared_ptr<const NativePBRImage>> images,const CUSSourceImageProvider& provider)
{
	auto output=std::unique_ptr<PreparedPBRSource>(new PreparedPBRSource);std::set<const NativePBRImage*> seen;
	for(auto& image:images){Require(image&&image->content==NativeTextureContent::ProducerVersion&&image->program&&image->program->pipelineOwner,"Original PBR pending command is incomplete");if(image->completed.load()||!seen.insert(image.get()).second)continue;
		Require(!image->encodedSubmission.load(),"Original PBR command already belongs to an unretired submission");PreparedPBRSource::Impl::Draw draw;draw.image=Storage(image);draw.pipeline=std::static_pointer_cast<const Pipeline>(image->program->pipelineOwner);
		lease.AddAllocation(draw.image->texture);lease.Retain(draw.pipeline->library);lease.Retain(draw.pipeline->state);
		auto* ad=[MTL4ArgumentTableDescriptor new];ad.maxTextureBindCount=2;ad.maxSamplerStateBindCount=1;ad.maxBufferBindCount=1;ad.initializeBindings=YES;NSError* error=nil;draw.arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(draw.arguments,"Cannot allocate original PBR argument table: "+Error(error));lease.Retain(draw.arguments);
		if(image->program->kind==NativePBRKind::BRDF){[draw.arguments setTexture:draw.image->texture.gpuResourceID atIndex:0];}
		else{
			Require(provider&&image->reflection.knowledge==NativeCUSBindingKnowledge::BoundObject&&image->reflection.object,"Original EnvLUT reflection binding has no observed image");const auto& object=*image->reflection.object;
			Require(object.target==NativeCUSTextureTarget::Cube&&object.sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906}&&object.sampling.compareMode==0,"Original EnvLUT requires its observed unswizzled reflection cube");
			draw.reflection=provider(lease,image->reflection,7);Require(draw.reflection.texture&&draw.reflection.texture.textureType==MTLTextureTypeCube&&draw.reflection.sampler&&draw.reflection.owner&&!draw.reflection.depthImage&&!draw.reflection.flipV,"Original EnvLUT reflection provider differs");
			draw.params=[lease.Device() newBufferWithBytes:&image->program->samples length:sizeof(int32_t) options:MTLResourceStorageModeShared];Require(draw.params,"Cannot allocate original EnvLUT sample count");lease.AddAllocation(draw.params);
			[draw.arguments setAddress:draw.params.gpuAddress atIndex:0];[draw.arguments setTexture:draw.reflection.texture.gpuResourceID atIndex:0];[draw.arguments setTexture:draw.image->texture.gpuResourceID atIndex:1];[draw.arguments setSamplerState:draw.reflection.sampler.gpuResourceID atIndex:0];
		}
		output->impl->draws.push_back(std::move(draw));
	}
	return output;
}
void PreparedPBRSource::Encode(id<MTL4CommandBuffer> command,uint64_t submission)
{
	Require(command&&submission&&!impl->submission,"Original PBR submission missing or encoded twice");
	for(const auto& d:impl->draws){if(d.reflection.encodeDependencies)d.reflection.encodeDependencies(command);if(d.reflection.requireReadable)d.reflection.requireReadable();auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot encode original PBR command");[encoder setComputePipelineState:d.pipeline->state];[encoder setArgumentTable:d.arguments];[encoder dispatchThreads:MTLSizeMake(d.image->allocation.width,d.image->allocation.height,1) threadsPerThreadgroup:MTLSizeMake(d.image->program->kind==NativePBRKind::BRDF?8:3,d.image->program->kind==NativePBRKind::BRDF?8:3,1)];[encoder barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];d.image->encodedSubmission.store(submission,std::memory_order_release);}
	impl->submission=submission;
}
void PreparedPBRSource::DidComplete(uint64_t submission){Require(submission&&impl->submission==submission&&!impl->completed,"Original PBR retirement has no actual encoded submission");for(const auto& d:impl->draws){d.image->completed.store(true,std::memory_order_release);if(d.reflection.didComplete)d.reflection.didComplete();}impl->completed=true;}
CUSSourceImage PBRSourceGPU::Image(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission)const
{
	auto image=GetNativePBRImage(binding);Require(bool(image),"Source binding is not an original PBR image");auto storage=Storage(image);lease.AddAllocation(storage->texture);
	const auto& state=binding.object->sampling;Require(state==image->allocation.sampling,"Original PBR sampler was changed and requires the shared general sampler provider");auto* sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=state.minFilter==0x2601?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;sd.mipFilter=MTLSamplerMipFilterNotMipmapped;sd.sAddressMode=sd.tAddressMode=sd.rAddressMode=MTLSamplerAddressModeClampToEdge;sd.supportArgumentBuffers=YES;
	id<MTLSamplerState> sampler=[lease.Device() newSamplerStateWithDescriptor:sd];Require(sampler,"Cannot allocate original PBR sampler");lease.Retain(sampler);CUSSourceImage result;result.texture=storage->texture;result.sampler=sampler;result.owner=storage;result.requireReadable=[storage,submission]{Readable(*storage,submission);};return result;
}
} // namespace rendering::metal4
