/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainInfoSource.h"
#include "Metal4WorldInfo.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include <algorithm>
#include <bit>
#include <array>
#include <optional>
#include <new>

namespace rendering::metal4 {
// BEGIN TERRAIN_INFO_LEASE_RECEIPTS
// Only full immutable object/sampler views within this exact lease may share an
// outer callback owner. State changes force a fresh receipt; no backing cycle.
class TerrainInfoLeaseReceipts {
public:
 struct Key {
  std::shared_ptr<const NativeCUSTextureObject> object;
  std::shared_ptr<const NativeTerrainInfoResource> resource;
  uint64_t submission=0,registered=0,encoding=0;
  uintptr_t sampler=0;
  bool validation=false,completed=false,encoded=false,pending=false;
  bool operator==(const Key&)const=default;
 };
private:
 struct Entry{Key key;CUSSourceImage image;};
 std::array<std::optional<Entry>,32> entries;
 size_t count=0;
public:
 const CUSSourceImage* Find(const Key& key)const noexcept {
  for(size_t i=0;i<count;++i)if(entries[i]->key==key)return &entries[i]->image;
  return nullptr;
 }
 void Remember(const Key& key,const CUSSourceImage& image) {
  if(count==entries.size())return; // Bounded optimization, not an admission gate.
  try{entries[count].emplace(Entry{key,image});++count;}
  catch(const std::bad_alloc&){/* The original successfully resolved image remains usable. */}
 }
};
// END TERRAIN_INFO_LEASE_RECEIPTS
TerrainInfoLeaseReceipts* RuntimeLease::TerrainInfoReads() noexcept {
 try{if(!terrainInfoReads)terrainInfoReads=std::make_shared<TerrainInfoLeaseReceipts>();}
 catch(const std::bad_alloc&){return nullptr;}
 return terrainInfoReads.get();
}

namespace {
struct InfoResource final:NativeTerrainInfoResource {
	// No backing/object owner here: backing->resource must never form a cycle.
	std::shared_ptr<const NativeWorldInfoFrame> source;
	std::shared_ptr<PreparedWorldInfo> plan;
	uint64_t context=0,objectIdentity=0,name=0,contentVersion=0;
	bool validation=false;
	// The existing runtime serializes these transitions. Registration alone is
	// cancellable before Begin; an encode attempt belongs to one submission until
	// actual common completion. Partial/failed encoding is never readable/retried.
	mutable uint64_t registeredSubmission=0,encodingSubmission=0;
	mutable bool dependenciesEncoded=false,completed=false;

	void Register(RuntimeLease& lease,uint64_t submission) const
	{
  if(!completed&&encodingSubmission&&encodingSubmission!=submission&&lease.PreparingWhilePending()){
   Require(dependenciesEncoded,"Previous info plan has an incomplete encode attempt");
   // Pin the already submitted immutable image. Do not overwrite N's receipt
   // identity; N must actually retire before N+1 GPU submission.
   plan->RegisterResources(lease);return;
  }
		Require(completed||!encodingSubmission||encodingSubmission==submission,
			"Original terrain info plan was encoded by another uncompleted submission");
		Require(!encodingSubmission||dependenciesEncoded,
			"Original terrain info plan has a failed or incomplete encode attempt");
		plan->RegisterResources(lease);
		registeredSubmission=submission;
	}
	void Encode(id<MTL4CommandBuffer> command,uint64_t submission) const
	{
		Require(command!=nil,"Original terrain info dependencies lack the actual common command buffer");
		if(completed)return;
		Require(registeredSubmission==submission,"Original terrain info callback belongs to a cancelled preparation");
		if(encodingSubmission){
			Require(encodingSubmission==submission&&dependenciesEncoded,
				"Original terrain info producer cannot replay an incomplete encode attempt");
			return;
		}
		encodingSubmission=submission;
		plan->EncodeDependencies(command);
		dependenciesEncoded=true;
	}
	void Readable(uint64_t submission) const
	{
		// Same-command readability follows actual dependency encoding. This does
		// not mark CPU completion or claim that the GPU has retired this plan.
		Require(completed||(registeredSubmission==submission&&encodingSubmission==submission&&dependenciesEncoded),
			"Original terrain info consumer precedes its actual producer dependencies");
	}
	void Complete(uint64_t submission) const
	{
		if(completed)return;
		Require(registeredSubmission==submission&&encodingSubmission==submission&&dependenciesEncoded,
			"Original terrain info completion lacks its successful common producer encoding");
		// Only the caller's successful common feedback/event retirement invokes
		// this callback. Neither preparation nor Encode calls it speculatively.
		plan->DidComplete();
		completed=true;
	}
};
struct ImageOwner {
	std::shared_ptr<const NativeCUSTextureObject> object;
	std::shared_ptr<const NativeTerrainInfoBacking> backing;
	std::shared_ptr<const InfoResource> resource;
 // Exact immutable callback identities, retained by the existing image owner.
 uint64_t submission=0,priorSubmission=0;
};
SourceDependencyResult160 InspectTerrainInfoDependencies161(const ImageOwner& owner)
{
 const auto& resource=*owner.resource;
 if(owner.priorSubmission){
  // The original ordered callback only validates readability. Registration of
  // its predecessor edge and the real completion receipt remain unchanged.
  Require(resource.completed||
   (resource.encodingSubmission==owner.priorSubmission&&resource.dependenciesEncoded),
   "Borrowed terrain info lost its successfully encoded producer");
  return {SourceDependencyWork160::NoCommands,false};
 }
 // Mirror only the original Encode's early returns. Even an already completed
 // inner WorldInfo plan cannot replace the wrapper's first CPU transitions.
 if(resource.completed)return {SourceDependencyWork160::NoCommands,false};
 Require(resource.registeredSubmission==owner.submission,"Original terrain info callback belongs to a cancelled preparation");
 if(resource.encodingSubmission){
  Require(resource.encodingSubmission==owner.submission&&resource.dependenciesEncoded,
   "Original terrain info producer cannot replay an incomplete encode attempt");
  return {SourceDependencyWork160::NoCommands,false};
 }
 return {SourceDependencyWork160::CommandsRequired,false};
}
void CheckTexture(RuntimeLease& lease,const PreparedWorldInfo& plan,const NativeWorldInfoFrame& frame,
	const NativeTextureAllocation& allocation,id<MTLSamplerState> sampler)
{
	const auto texture=plan.Info();
	Require(texture!=nil&&sampler!=nil&&texture.device==lease.Device()&&sampler.device==lease.Device(),
		"Original terrain info image/sampler does not belong to the actual shared device");
	Require(plan.MatchesFrame(frame)&&plan.MapIdentity()==frame.mapIdentity&&plan.CompletionSerial()==frame.serial,
		"Original terrain info plan differs from its retained source image version");
	Require(texture.textureType==MTLTextureType2D&&texture.pixelFormat==MTLPixelFormatRGB10A2Unorm&&
		texture.width==allocation.width&&texture.height==allocation.height&&texture.depth==1&&texture.arrayLength==1&&
		texture.sampleCount==1&&texture.mipmapLevelCount==allocation.levels&&(texture.usage&MTLTextureUsageShaderRead),
		"Original terrain info output differs from its RGB10_A2/full-mip source allocation");
}
} // namespace

CUSSourceImage ResolveNativeTerrainInfoImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,
	uint64_t submission,id<MTLSamplerState> sampler,bool validation,double& shaderMs)
{
	Require(submission&&sampler!=nil&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,
		"Original terrain info requires its actual bound source, sampler and common submission");
	const auto& object=binding.object;
	const auto backing=std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(object->backing);
	Require(object->name&&object->identity&&object->contextGeneration&&object->target==NativeCUSTextureTarget::Image2D&&
		object->storage==NativeTextureStorageKnowledge::Published&&backing&&backing->originalHandler&&backing->source&&
		!object->image&&backing->storageGeneration==object->identity&&backing->contentVersion&&backing->contentVersion<=object->version&&
		backing->content==NativeTextureContent::ProducerVersion&&backing->sourceDrawFrame==backing->source->sourceDrawFrame,
		"Original terrain info object has no matching immutable source/content owner");
	const auto& frame=*backing->source;
	Require(frame.mapIdentity&&frame.modeGeneration&&frame.squaresX&&frame.squaresZ&&
		(frame.mode==NativeWorldInfoMode::Normal||frame.mode==NativeWorldInfoMode::Los||frame.mode==NativeWorldInfoMode::Metal)&&
		frame.paddedSquaresX>=frame.squaresX&&frame.paddedSquaresZ>=frame.squaresZ&&
		std::has_single_bit(frame.paddedSquaresX)&&std::has_single_bit(frame.paddedSquaresZ),
		"Original terrain info source image has no actual map/mode/extent generation");
	const uint32_t levels=std::bit_width(std::max(frame.paddedSquaresX,frame.paddedSquaresZ));
	Require(object->allocation==NativeTextureAllocation{frame.paddedSquaresX,frame.paddedSquaresZ,1,levels,1,0x8059},
		"Original terrain info logical storage differs from its existing producer's full mip chain");

	auto resource=std::dynamic_pointer_cast<const InfoResource>(backing->resource);
	Require(!backing->resource||resource,"Original terrain info source version has another resource owner type");
	if(!resource){
		// SharedWorldInfoGPU also routes Metal through SharedMetalInfoGPU and
		// requires its existing real maintenance transaction to have completed.
		auto created=std::make_shared<InfoResource>();
		created->source=backing->source;created->context=object->contextGeneration;
		created->objectIdentity=object->identity;created->name=object->name;
		created->contentVersion=backing->contentVersion;created->validation=validation;
		created->plan=SharedWorldInfoGPU().Prepare(lease,created->source,validation,shaderMs);
		Require(bool(created->plan),"Existing world info producer did not return its actual plan");
		resource=std::move(created);
	}else Require(resource->source==backing->source&&resource->context==object->contextGeneration&&
		resource->objectIdentity==object->identity&&resource->name==object->name&&
		resource->contentVersion==backing->contentVersion&&resource->validation==validation,
		"Original terrain info resource crossed its source/object/context/library generation");
	CheckTexture(lease,*resource->plan,frame,object->allocation,sampler);
	resource->Register(lease,submission);
	lease.Retain(sampler);
	backing->resource=resource;
 // Keep validation, registration and sampler retention at their original call
 // sites. Only equivalent outer owners/callbacks are shared in this first step.
 auto* receipts=lease.TerrainInfoReads();
 const TerrainInfoLeaseReceipts::Key key{object,resource,submission,
  resource->registeredSubmission,resource->encodingSubmission,
  reinterpret_cast<uintptr_t>((__bridge void*)sampler),validation,
  resource->completed,resource->dependenciesEncoded,lease.PreparingWhilePending()};
 if(receipts)if(const auto* image=receipts->Find(key))return *image;

	auto owner=std::make_shared<ImageOwner>();owner->object=object;owner->backing=backing;owner->resource=resource;owner->submission=submission;
	CUSSourceImage out;out.texture=resource->plan->Info();out.sampler=sampler;out.owner=owner;
	out.flipV=false;out.depthImage=false;out.clampDepthCompare=false; // Original map-Z RGB10_A2 output.
 out.dependencyQuery160={owner.get(),[](const void* context){
  return InspectTerrainInfoDependencies161(*static_cast<const ImageOwner*>(context));
 }};
 const auto priorSubmission=resource->encodingSubmission;
 const bool orderedPrior=!resource->completed&&priorSubmission&&priorSubmission!=submission&&
  lease.OrderAfterPreviousSubmission(resource.get(),[owner,priorSubmission]{
   Require(owner->resource->completed&&owner->resource->encodingSubmission==priorSubmission,
    "Borrowed terrain info did not complete its original submitting owner");
  });
 if(orderedPrior){
  owner->priorSubmission=priorSubmission;
  auto readable=[owner,priorSubmission]{Require(owner->resource->completed||
   (owner->resource->encodingSubmission==priorSubmission&&owner->resource->dependenciesEncoded),
   "Borrowed terrain info lost its successfully encoded producer");};
  out.encodeDependencies=[readable](id<MTL4CommandBuffer> command){Require(command!=nil,"Missing ordered terrain command buffer");readable();};
  out.requireReadable=readable;
  out.didComplete=[owner]{Require(owner->resource->completed,"Borrowed terrain info cannot publish another submission's completion");};
  if(receipts)receipts->Remember(key,out);return out;
 }
	out.encodeDependencies=[owner,submission](id<MTL4CommandBuffer> command){owner->resource->Encode(command,submission);};
	out.requireReadable=[owner,submission]{owner->resource->Readable(submission);};
	out.didComplete=[owner,submission]{owner->resource->Complete(submission);};
 if(receipts)receipts->Remember(key,out);
	return out;
}
} // namespace rendering::metal4
