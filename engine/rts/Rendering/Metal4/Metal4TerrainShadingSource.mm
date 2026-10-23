/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainShadingSource.h"
#include "Metal4MiniMapNative.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"

namespace rendering::metal4 {
namespace {
struct Resource final:NativeTerrainShadingResource {
	__strong id<MTLTexture> texture=nil;
	uint64_t context=0,storage=0,revision=0;
	bool validation=false;
	mutable std::shared_ptr<PreparedMiniMap> plan;
	mutable std::shared_ptr<const Resource> prior;
	mutable std::shared_ptr<const NativeTerrainShadingBacking> priorSource;
	mutable uint64_t preparedSubmission=0,encodingSubmission=0,encodedSubmission=0;
	mutable bool completed=false;
};
void Register(RuntimeLease& lease,const Resource& resource,uint64_t submission)
{
	Require(resource.texture&&submission,"Original shading version lacks its actual output/submission");
	lease.AddAllocation(resource.texture);
	if(resource.completed)return;
	Require(!resource.encodingSubmission||resource.encodingSubmission==submission,
		"Original shading version belongs to another unretired submission");
	// A pre-Begin cancellation has not written pixels. Its unchanged prepared
	// plan can be re-registered; encoded work is retained and cannot be retried.
	resource.preparedSubmission=submission;
	Require(resource.plan!=nullptr,"Original shading version lost its pending producer");
	resource.plan->RegisterInfoResources(lease);
	if(resource.prior)Register(lease,*resource.prior,submission);
}
std::shared_ptr<const Resource> Prepare(RuntimeLease& lease,MiniMapGPU& producer,
	const std::shared_ptr<const NativeTerrainShadingBacking>& source,uint64_t context,uint64_t submission,
	bool validation,double& shaderMs)
{
	Require(source&&source->source&&source->source->heights&&source->originalReadMap&&source->originalProgram&&
		source->storageGeneration&&source->contentVersion&&source->originalWriteRevision&&
		source->content==NativeTextureContent::ProducerVersion,"Original shading version has no actual FBO write/input owner");
	if(source->resource){
		auto existing=std::dynamic_pointer_cast<const Resource>(source->resource);
		Require(existing&&existing->context==context&&existing->storage==source->storageGeneration&&existing->revision==source->originalWriteRevision&&existing->validation==validation,
			"Original shading version refers to another context/storage/write");
		Register(lease,*existing,submission);return existing;
	}
	auto resource=std::make_shared<Resource>();resource->context=context;resource->storage=source->storageGeneration;
	resource->validation=validation;
	resource->revision=source->originalWriteRevision;resource->preparedSubmission=submission;
	if(source->prior){
		Require(source->prior->storageGeneration==source->storageGeneration&&source->prior->originalReadMap==source->originalReadMap&&
			source->prior->originalWriteRevision+1==source->originalWriteRevision,
			"Original partial shading lost its immediately preceding image version");
		resource->priorSource=source->prior;
		resource->prior=Prepare(lease,producer,resource->priorSource,context,submission,validation,shaderMs);
	}
	const auto& heights=*source->source->heights;
	Require(heights.width&&heights.height&&heights.width<=lease.Capabilities().maxTextureDimension2D&&
		heights.height<=lease.Capabilities().maxTextureDimension2D,"Original shading allocation exceeds actual device extent");
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:heights.width height:heights.height mipmapped:NO];
	descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget;
	resource->texture=[lease.Device() newTextureWithDescriptor:descriptor];
	Require(resource->texture!=nil,"Cannot allocate retained original shading image version");lease.AddAllocation(resource->texture);
	// The initial allocation remains undefined outside its actual first write.
	// Subsequent versions inherit the previous output before this exact rect.
	resource->plan=producer.PrepareShadingImage(lease,source->source,resource->texture,source->writtenRect,validation,shaderMs);
	source->resource=resource;return resource;
}
void Readable(const Resource& resource,uint64_t submission)
{
	Require(resource.completed||(submission&&resource.encodedSubmission==submission),
		"Original partial shading was sampled before its actual prior/copy/write dependencies");
}
void Encode(const Resource& resource,uint64_t submission,id<MTL4CommandBuffer> command)
{
	if(resource.completed)return;
	Require(command&&resource.preparedSubmission==submission,"Original shading dependencies lost their prepared common submission");
	if(resource.encodedSubmission){Require(resource.encodedSubmission==submission,"Original shading dependencies were encoded on another unretired submission");return;}
	Require(!resource.encodingSubmission,"Original shading dependency encoding failed after it began and cannot be replayed");
	resource.encodingSubmission=submission; // Any later exception is post-Begin, never a preparation cancellation.
	if(resource.prior){
		Encode(*resource.prior,submission,command);Readable(*resource.prior,submission);
		const auto prior=resource.prior->texture;const auto target=resource.texture;
		Require(prior.width==target.width&&prior.height==target.height&&prior.pixelFormat==target.pixelFormat,
			"Original partial shading prior allocation differs");
		auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot preserve the original shading image outside its actual update rectangle");
		[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageVertex|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder copyFromTexture:prior sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
			sourceSize:MTLSizeMake(target.width,target.height,1) toTexture:target destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
	}
	Require(resource.plan!=nullptr,"Original partial shading has no actual write plan");
	resource.plan->EncodeDependencies(command);resource.encodedSubmission=submission;
}
void Complete(const Resource& resource,const NativeTerrainShadingBacking& source,uint64_t submission)
{
	if(resource.completed)return;
	Require(resource.encodedSubmission==submission&&submission,"Original shading completion precedes its actual common write/retirement");
	if(resource.prior){Require(resource.priorSource!=nullptr,"Original shading prior input lifetime was lost");Complete(*resource.prior,*resource.priorSource,submission);}
	Require(resource.plan!=nullptr,"Original shading completion lost its producer owner");resource.plan->DidComplete();
	resource.completed=true;
	source.RetirePrior();resource.prior.reset();resource.priorSource.reset();resource.plan.reset();
}
struct Owner {
	std::shared_ptr<const NativeCUSTextureObject> object;
	std::shared_ptr<const NativeTerrainShadingBacking> source;
	std::shared_ptr<const Resource> resource;
	uint64_t submission=0;
};
}
CUSSourceImage ResolveNativeTerrainShadingImage(RuntimeLease& lease,MiniMapGPU& producer,
	const NativeCUSTextureBinding& binding,uint64_t submission,id<MTLSamplerState> sampler,bool validation,double& shaderMs)
{
	Require(submission&&sampler&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&
		binding.object->contextGeneration&&binding.object->target==NativeCUSTextureTarget::Image2D,
		"Original shading bind has no actual named source/sampler/submission");
	auto source=std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(binding.object->backing);
	Require(source&&source->storageGeneration==binding.object->identity&&source->contentVersion&&source->contentVersion<=binding.object->version&&
		binding.object->storage==NativeTextureStorageKnowledge::Published&&!binding.object->image&&
		binding.object->allocation.originalInternalFormat==0x8058&&binding.object->allocation.levels==1&&
		binding.object->allocation.depth==1&&binding.object->allocation.samples==1,
		"Original shading source differs from its actual logical storage/content");
	auto resource=Prepare(lease,producer,source,binding.object->contextGeneration,submission,validation,shaderMs);
	Require(resource->texture.device==lease.Device()&&sampler.device==lease.Device()&&
		resource->texture.width==binding.object->allocation.width&&resource->texture.height==binding.object->allocation.height,
		"Original shading physical extent differs from its actual allocation");
	lease.Retain(sampler);
	auto owner=std::make_shared<Owner>();owner->object=binding.object;owner->source=std::move(source);owner->resource=std::move(resource);owner->submission=submission;
	CUSSourceImage out;out.texture=owner->resource->texture;out.sampler=sampler;out.owner=owner;
	out.encodeDependencies=[owner](id<MTL4CommandBuffer> command){Encode(*owner->resource,owner->submission,command);};
	out.requireReadable=[owner]{Readable(*owner->resource,owner->submission);};
	out.didComplete=[owner]{Complete(*owner->resource,*owner->source,owner->submission);};return out;
}
}
