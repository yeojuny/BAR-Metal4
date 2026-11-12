/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CUSDeferred.h"
#include "Metal4MRTWriteReuse209.h"
#include "Rendering/Backend/NativeMRTClearLoad145.h"
#include "System/Log/ILog.h"
#include "Metal4DeferredStoragePool.inc"
#include "Metal4MRTObservation174.h"
#include <cstdlib>
#include <cstring>

namespace rendering::metal4 {
namespace {
void Readable(const ShieldDepthImage& image,uint64_t submission)
{
	Require(image.producer&&IsNativeShieldDepthReadable(image.producer->encoded,image.producer->completed,
		image.producer->operation==ShieldDepthOperation::AllocationOnly,image.producer->allocationVerified,
		image.producer->submissionIdentity,submission),"Original deferred phase attempted to load unencoded prior depth");
}
}
struct PreparedCUSDeferred::Impl {
	std::shared_ptr<const NativeCUSDeferredPhase> source;
	std::shared_ptr<Storage> storage;
	std::shared_ptr<ShieldDepthProducerState> producer;
	std::array<DeferredColorImage,5> colors,priorColors;
	DeferredColorImage combinedColor,priorCombinedColor;
	ShieldDepthImage depth,priorDepth;
	std::unique_ptr<PreparedCUSSource> lua;
	CUSDeferredOtherDraws other;
	struct SourceDraw {bool engine;size_t index;};
	std::vector<SourceDraw> sourceOrder;
	uint64_t submission=0;
 unsigned discardPrior145=0;
 std::unique_ptr<MRT174Observation> observation174;
	bool encoded=false,completed=false,retainsReadOnlyContents=false;
 bool reusesWritingStorage209=false;
};

namespace {
template<class A,class B>
bool SameMRTSharedOwner(const A& a,const B& b)
{
	return a&&b&&a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);
}
template<class Prepared>
bool CanRetainReadOnlyMRT(const Prepared& p)
{
	if(p.source->role!=NativeGeometryTextureRole::Models||!p.source->feature||
		p.source->clearBefore||p.source->clearOnly||p.source->combinedColor.Changed()||p.discardPrior145||
		!p.other.sourceOrder||!p.other.emptySourceCallbacksHaveNoCommands||
		p.other.beforeLua||p.other.afterLua||!p.other.drawSequences.empty()||!p.sourceOrder.empty()||
		(p.lua&&p.lua->DrawCount()!=0))return false;
	const auto& prior=p.priorDepth;const auto& m=p.source->depth.backing->Metadata();
	if(!prior.texture||!prior.storageOwner||!prior.producer||
		prior.role!=ShieldDepthRole::ModelsOnly||prior.sourceDrawFrame!=p.source->sourceDrawFrame||
		prior.producerIdentity!=prior.producer->identity||prior.producer->role!=ShieldDepthRole::ModelsOnly||
		prior.producer->sourceDrawFrame!=p.source->sourceDrawFrame||prior.producer->submissionIdentity!=p.submission||
		!prior.producer->allocationVerified||prior.producer->operation==ShieldDepthOperation::AllocationOnly||
		prior.view!=m.view||prior.projection!=m.projection||prior.viewport!=m.viewport)return false;
	for(unsigned n=0;n<5;++n){
		const auto& color=p.priorColors[n];const auto& view=color.metadata.view;
		if(!color.texture||!SameMRTSharedOwner(color.storageOwner,prior.storageOwner)||
			!SameMRTSharedOwner(color.producer,prior.producer)||
			color.metadata.role!=NativeGeometryTextureRole::Models||color.metadata.attachment!=n||
			view.width!=m.width||view.height!=m.height||view.view!=m.view||
			view.projection!=m.projection||view.viewport!=m.viewport)return false;
	}
	return true;
}
template<class Prepared>
bool CanReuseWritingMRT209(const Prepared& p,const MRTWriteReuse209& permission)
{
 const auto* unit=permission.Unit();
 // The sealed permission is unchanged. Expand only the prepared writer
 // shape: Lua-only features retain their actual ordered draws, dependency
 // callbacks and completion graph, while six preservation copies disappear.
 const bool luaOnlyWriter=MRTLuaWriteReuseInterventionEnabled()&&
  p.other.emptySourceCallbacksHaveNoCommands&&p.other.drawSequences.empty()&&
  p.lua&&p.lua->DrawCount()!=0;
 if(!unit||!unit->depth.backing||!p.source->feature||p.source->role!=NativeGeometryTextureRole::Models||
  p.source->clearBefore||p.source->clearOnly||p.source->combinedColor.Changed()||p.discardPrior145||
  !p.other.sourceOrder||(!luaOnlyWriter&&(p.other.emptySourceCallbacksHaveNoCommands||p.other.drawSequences.empty()))||
  p.sourceOrder.empty()||p.other.beforeLua||p.other.afterLua||p.other.initialAttachmentClear182||
  !p.source->depth.previous||!SameMRTSharedOwner(p.source->depth.previous->backing,unit->depth.backing))return false;
 const auto& prior=p.priorDepth;const auto& m=p.source->depth.backing->Metadata();
 if(!prior.texture||!prior.storageOwner||!prior.producer||prior.role!=ShieldDepthRole::ModelsOnly||
  prior.sourceDrawFrame!=p.source->sourceDrawFrame||prior.producerIdentity!=prior.producer->identity||
  prior.producer->role!=ShieldDepthRole::ModelsOnly||prior.producer->sourceDrawFrame!=p.source->sourceDrawFrame||
  prior.producer->submissionIdentity!=p.submission||!prior.producer->allocationVerified||
  prior.producer->operation==ShieldDepthOperation::AllocationOnly||prior.producer->encoded||prior.producer->completed||
  prior.view!=m.view||prior.projection!=m.projection||prior.viewport!=m.viewport)return false;
 for(unsigned n=0;n<5;++n){
  const auto& color=p.priorColors[n];const auto& view=color.metadata.view;
  if(!p.source->colors[n].previous||!SameMRTSharedOwner(p.source->colors[n].previous->backing,unit->colors[n].backing)||
   !color.texture||!SameMRTSharedOwner(color.storageOwner,prior.storageOwner)||
   !SameMRTSharedOwner(color.producer,prior.producer)||color.metadata.role!=NativeGeometryTextureRole::Models||
   color.metadata.attachment!=n||view.width!=m.width||view.height!=m.height||view.view!=m.view||
   view.projection!=m.projection||view.viewport!=m.viewport)return false;
 }
 return true;
}
template<class Prepared>
void BindPriorMRTContents(Prepared& p)
{
	// Physical storage is borrowed only under the caller's readonly164 or
	// sealed zero-reader209 proof. Logical F keeps its distinct producer and
	// metadata; no U lifecycle state is advanced by this binding operation.
	for(unsigned n=0;n<5;++n){
		p.colors[n]=p.priorColors[n];p.colors[n].producer=p.producer;
		p.colors[n].metadata=p.source->colors[n].backing->Metadata();
	}
	const auto& m=p.source->depth.backing->Metadata();
	p.depth=p.priorDepth;p.depth.producer=p.producer;
	p.depth.sourceDrawFrame=p.source->sourceDrawFrame;p.depth.producerIdentity=p.producer->identity;
	p.depth.view=m.view;p.depth.projection=m.projection;p.depth.viewport=m.viewport;
	p.producer->allocationVerified=true;
}
void RetainBorrowedMRTStorage(RuntimeLease& lease,const std::shared_ptr<const void>& owner)
{
	// Retaining textures alone does not pin the pool's Storage use_count.
	// Use its existing opaque owner wrapper; no unproved cast back to Storage.
	auto lifetime=[M4DeferredStorageLifetime new];lifetime->owner=owner;lease.Retain(lifetime);
}
template<class Prepared>
void RequirePriorMRTContents(const Prepared& p)
{
	for(const auto& image:p.priorColors)if(image.texture)Require(IsDeferredColorReadable(image,p.submission),"Original MRT phase attempted to load unencoded prior color");
	if(p.priorCombinedColor.texture)Require(IsDeferredColorReadable(p.priorCombinedColor,p.submission),"Combined color prior contents are not readable in this submission");
	if(p.priorDepth.texture)Readable(p.priorDepth,p.submission);
}
template<class Prepared,class Command>
void EncodeReadOnlyMRT(Prepared& p,Command command)
{
	Require(command&&!p.encoded&&!p.completed&&p.retainsReadOnlyContents&&p.sourceOrder.empty(),
		"Read-only MRT phase is missing, writing, repeated or retired");
	RequirePriorMRTContents(p);
	// The caller proved its empty lifecycle has no commands. A prepared CUS
	// draw owns all its images, so DrawCount zero also means no image callbacks.
	if(p.other.dependencies)p.other.dependencies(command);
	if(p.lua)p.lua->EncodeDependencies(command);
	p.other.didEncodeSourceDraws();
	p.encoded=true;p.producer->encoded=true;
#ifdef RECOIL_METAL4_LIVE_SCENE
	static bool reported164=false;
	if(!reported164){
		const auto& m=p.source->depth.backing->Metadata();
		LOG("[NativeMRT164] source_frame=%llu submission=%llu width=%u height=%u omitted_copies=6 logical_source_bytes=%llu distinct_producer=%u engine_draws=%llu lua_draws=%llu prepared_events=%llu borrowed_owner=%u encoded=1",
			static_cast<unsigned long long>(p.source->sourceDrawFrame),static_cast<unsigned long long>(p.submission),m.width,m.height,
			static_cast<unsigned long long>(uint64_t(24)*m.width*m.height),
			unsigned(p.producer->identity!=p.priorDepth.producerIdentity&&!SameMRTSharedOwner(p.producer,p.priorDepth.producer)),
			static_cast<unsigned long long>(p.other.drawSequences.size()),static_cast<unsigned long long>(p.lua?p.lua->DrawCount():0),
			static_cast<unsigned long long>(p.sourceOrder.size()),unsigned(SameMRTSharedOwner(p.depth.storageOwner,p.priorDepth.storageOwner)));
		reported164=true;
	}
#endif
}
}
PreparedCUSDeferred::PreparedCUSDeferred():impl(std::make_unique<Impl>()){}
PreparedCUSDeferred::~PreparedCUSDeferred()=default;
const std::array<DeferredColorImage,5>& PreparedCUSDeferred::Colors()const{return impl->colors;}
const ShieldDepthImage& PreparedCUSDeferred::Depth()const{return impl->depth;}
ModelPhaseExecution165 PreparedCUSDeferred::InspectExecution165(uint64_t frame,uint64_t submission)const
{
	const auto& p=*impl;
	Require(p.source&&frame==p.source->sourceDrawFrame&&submission&&submission==p.submission&&
		p.producer&&p.producer->sourceDrawFrame==frame&&p.producer->submissionIdentity==submission&&
		!p.encoded&&!p.completed&&!p.producer->encoded&&!p.producer->completed,
		"MRT execution inspection differs from its prepared frame/submission/lifecycle");
	// The authoritative decision was made after preparing actual engine/CUS
	// consumers and source order. Do not infer it again from draw counts here.
	if(p.retainsReadOnlyContents)return {SourceDependencyWork160::NoCommands,ModelPhaseBody165::CPULifecycleOnly};
	return {};
}

std::unique_ptr<PreparedCUSDeferred> PrepareCUSDeferred(RuntimeLease& lease,CUSSourceGPU& programs,
	std::shared_ptr<const NativeCUSDeferredPhase> source,const CUSSourceGPU::TextureProvider& images,
	CUSDeferredOtherDraws other,uint64_t submission,bool validation,double& milliseconds,
 const NativeWorldAttachmentClear* initialClear,MRTWriteReuse209* writeReuse209)
{
	Require(source&&submission&&other.observed,"Original model MRT requires actual selected non-CUS source ownership");
	Require((!other.beforeLua&&!other.afterLua)||bool(other.lifetime),"Original engine MRT callbacks lack their retained geometry owner");
	Require(other.sourceOrder?(!other.beforeLua&&!other.afterLua&&other.drawAt&&other.didEncodeSourceDraws&&other.lifetime):
		(other.drawSequences.empty()&&!other.drawAt&&!other.didEncodeSourceDraws),
		"Original MRT source order and grouped callbacks cannot be absent or mixed");
	ValidateNativeCUSDeferredPhase(*source);
	const auto& m=source->depth.backing->Metadata();
	Require(m.width&&m.height&&m.width<=lease.Capabilities().maxTextureDimension2D&&m.height<=lease.Capabilities().maxTextureDimension2D,
		"Original model MRT extent exceeds its actual native storage limit");
	auto out=std::unique_ptr<PreparedCUSDeferred>(new PreparedCUSDeferred);auto& p=*out->impl;
	p.source=std::move(source);p.other=std::move(other);p.submission=submission;
	Require(!p.other.initialAttachmentClear182||initialClear,
		"MRT attachment clear callback has no proved first original clear");
 // The caller owns the actual clear and proves it is its first source event.
 // Read prior storage only for attachments whose pixels the clear preserves.
 if(initialClear){
  Require(p.source->role==NativeGeometryTextureRole::Map && !p.source->clearBefore &&
   initialClear->stage==NativeWorldDrawStage::Ground && initialClear->sequence && p.other.sourceOrder &&
   !p.other.drawSequences.empty() && p.other.drawSequences.front()==initialClear->sequence,
   "MRT discard requires the actual first ordered Map clear");
  for(const auto& draw:p.source->lua->draws)Require(initialClear->sequence<draw.sequence,
   "MRT discard clear must precede every source Lua draw");
  p.discardPrior145=NativeMRTClearDiscard145(*initialClear,m.width,m.height,p.source->combinedColor.Changed());
 }
	p.producer=CreateShieldDepthProducer(p.source->role==NativeGeometryTextureRole::Map?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly,p.source->sourceDrawFrame);
	p.producer->submissionIdentity=submission;
 if(p.source->clearOnly)p.producer->operation=ShieldDepthOperation::ClearOne;
	if(!p.source->clearBefore){
		for(unsigned n=0;n<5;++n){
   if(p.discardPrior145&(1u<<n))continue;
			const auto previous=p.source->colors[n].previous;
			auto backing=previous?std::dynamic_pointer_cast<const NativeDeferredColorBacking>(previous->backing):nullptr;
			Require(bool(backing),"Original load-phase MRT color has no actual previous image");
			p.priorColors[n]=GetDeferredColorBackingImage(*backing);auto texture=p.priorColors[n].texture;
			Require(texture&&texture.width==m.width&&texture.height==m.height&&texture.pixelFormat==MTLPixelFormatRGBA8Unorm,
				"Original MRT prior color storage differs");lease.AddAllocation(texture);
		}
		if(p.source->combinedColor.Changed() && !(p.discardPrior145&32u)){
			const auto previous=p.source->combinedColor.previous;auto backing=previous?std::dynamic_pointer_cast<const NativeDeferredColorBacking>(previous->backing):nullptr;
			Require(bool(backing),"Original combined color lacks its real prior allocation/content");p.priorCombinedColor=GetDeferredColorBackingImage(*backing);
			auto texture=p.priorCombinedColor.texture;Require(texture&&texture.width==m.width&&texture.height==m.height&&texture.pixelFormat==MTLPixelFormatRGBA8Unorm,"Combined color prior storage differs");lease.AddAllocation(texture);
		}
  if(!(p.discardPrior145&64u)){
		const auto previous=p.source->depth.previous;
		auto backing=previous?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(previous->backing):nullptr;
		Require(bool(backing),"Original load-phase MRT depth has no actual previous image");
		p.priorDepth=GetShieldDepthBackingImage(*backing);
		Require(p.priorDepth.texture&&p.priorDepth.texture.width==m.width&&p.priorDepth.texture.height==m.height&&p.priorDepth.texture.pixelFormat==MTLPixelFormatDepth32Float,
			"Original MRT prior depth storage differs");lease.AddAllocation(p.priorDepth.texture);
  }
	}
	CUSSourceTarget target;target.colors.fill(MTLPixelFormatRGBA8Unorm);target.depth=MTLPixelFormatDepth32Float;
	if(p.source->combinedColor.Changed())target.combinedColor=MTLPixelFormatRGBA8Unorm;
	target.width=target.logicalWidth=m.width;target.height=target.logicalHeight=m.height;target.camera=p.source->camera;
	// GeometryBuffer::LoadViewport uses its actual full named allocation extent.
	target.camera.viewport={0,0,int32_t(m.width),int32_t(m.height)};
	p.lua=programs.Prepare(lease,p.source->lua,target,images,validation,milliseconds);
	if(p.other.sourceOrder){
		const size_t luaCount=p.lua?p.lua->DrawCount():0;
		for(size_t n=0;n<p.other.drawSequences.size();++n)Require(p.other.drawSequences[n]&&(!n||p.other.drawSequences[n-1]<p.other.drawSequences[n]),"Engine MRT source draw sequence is absent or out of order");
		for(size_t n=0;n<luaCount;++n)Require(p.lua->SequenceAt(n)&&(!n||p.lua->SequenceAt(n-1)<p.lua->SequenceAt(n)),"CUS MRT source draw sequence is absent or out of order");
		size_t engine=0,lua=0;
		while(engine<p.other.drawSequences.size()||lua<luaCount){
			if(engine==p.other.drawSequences.size()){p.sourceOrder.push_back({false,lua++});continue;}
			if(lua==luaCount){p.sourceOrder.push_back({true,engine++});continue;}
			const auto engineSequence=p.other.drawSequences[engine],luaSequence=p.lua->SequenceAt(lua);
			Require(engineSequence!=luaSequence,"Engine and CUS draws cannot own the same actual source event");
			if(engineSequence<luaSequence)p.sourceOrder.push_back({true,engine++});else p.sourceOrder.push_back({false,lua++});
		}
	}
 if(p.discardPrior145)Require(!p.sourceOrder.empty()&&p.sourceOrder.front().engine&&p.sourceOrder.front().index==0,
  "MRT discard lost its original first clear in the merged source order");
	// Decide only after both actual consumers and their source order exist.
	// Prepared empty engine/CUS consumers have no image dependencies. A writing
	// phase preserves COW unless the sealed zero-reader permission also holds.
	p.retainsReadOnlyContents=CanRetainReadOnlyMRT(p);
	p.reusesWritingStorage209=!p.retainsReadOnlyContents&&writeReuse209&&
  CanReuseWritingMRT209(p,*writeReuse209)&&writeReuse209->Consume(*p.source,submission);
	if(p.retainsReadOnlyContents||p.reusesWritingStorage209){
		BindPriorMRTContents(p);
		RetainBorrowedMRTStorage(lease,p.depth.storageOwner);
	}else{
		p.storage=AcquireDeferredStorage(lease,m.width,m.height,p.source->combinedColor.Changed());
		for(unsigned attachment=0;attachment<5;++attachment){
			auto texture=p.storage->colors[attachment];lease.AddAllocation(texture);
			p.colors[attachment]={texture,p.storage,p.producer,p.source->colors[attachment].backing->Metadata()};
		}
		if(p.source->combinedColor.Changed()){
			auto texture=p.storage->combinedColor;lease.AddAllocation(texture);
			p.combinedColor={texture,p.storage,p.producer,p.source->combinedColor.backing->Metadata()};
		}
		lease.AddAllocation(p.storage->depth);
		p.producer->allocationVerified=true;
		p.depth={p.storage->depth,p.source->role==NativeGeometryTextureRole::Map?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly,p.source->sourceDrawFrame,p.producer->identity,m.view,m.projection,m.viewport,p.storage,p.producer};
	}
#ifdef RECOIL_METAL4_LIVE_SCENE
 static bool reported145=false;
 if(p.discardPrior145&&!reported145){
  unsigned skipped=0;for(unsigned bit=0;bit<7;++bit)skipped+=bool(p.discardPrior145&(1u<<bit));
  LOG("[NativeMRT145] discard_mask=%u omitted_copies=%u source_bytes=%llu width=%u height=%u original_clear_retained=1",
   p.discardPrior145,skipped,static_cast<unsigned long long>(uint64_t(skipped)*m.width*m.height*4),m.width,m.height);
  reported145=true;
 }
#endif
	// Publish actual storage only after every stage resource prepared. These
	// tokens still report encoded=false until their actual ordered phase.
	for(unsigned n=0;n<5;++n)FulfillDeferredColorBacking(*p.source->colors[n].backing,p.colors[n]);
	if(p.combinedColor.texture)FulfillDeferredColorBacking(*p.source->combinedColor.backing,p.combinedColor);
	FulfillShieldDepthBacking(*p.source->depth.backing,p.depth);
	return out;
}

bool PreparedCUSDeferred::HostOnlyInitialModel220()const
{
 const auto& p=*impl;
 return !p.encoded&&!p.completed&&p.source->role==NativeGeometryTextureRole::Models&&
  !p.source->feature&&p.source->clearBefore&&!p.retainsReadOnlyContents&&!p.reusesWritingStorage209&&
  p.other.observed&&p.other.sourceOrder&&p.other.drawSequences.empty()&&p.other.emptySourceCallbacksHaveNoCommands&&
  p.lua&&p.lua->HostOnlyNoDependencyCommands220();
}
void PreparedCUSDeferred::Encode(id<MTL4CommandBuffer> command){EncodeScopedVertex220(command,false);}
void PreparedCUSDeferred::EncodeScopedVertex220(id<MTL4CommandBuffer> command,bool scopedVertex220)
{
 // Separate intervention: the existing proof is local to this fresh initial
 // model MRT, not dependent on enabling the larger terrain/forward overlap.
 // It admits only host-fed vertex inputs, no engine draws, no dependency
 // commands, and fresh cleared attachments. Unknown/readback/copy/load phases
 // keep the original full queue wait. Fragment visibility and the pass-end
 // publication are unchanged; no consumer is moved across this pass.
 static const bool hostVertexIntervention=[] {
  const char* value=std::getenv("BAR_NATIVE_INTERVENTION_HOST_VERTEX");
  return value&&std::strcmp(value,"1")==0;
 }();
 const bool localHostVertex=hostVertexIntervention&&HostOnlyInitialModel220();
 scopedVertex220=scopedVertex220||localHostVertex;
 if(scopedVertex220)Require(HostOnlyInitialModel220(),"Combined MRT vertex proof changed after admission");
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(localHostVertex){
  static bool reportedIntervention=false;
  if(!reportedIntervention){
   LOG("[NativeInterventionHostVertex] initial_model_mrt=1 host_only_proof=1 fragment_wait_retained=1 pass_boundary_retained=1");
   reportedIntervention=true;
  }
 }
#endif
	auto& p=*impl;Require(command&&!p.encoded&&!p.completed,"Original model MRT phase is missing or encoded twice");
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(MRT174Enabled()&&MRT174Window(p.source->sourceDrawFrame)){
  const unsigned row=MRT174RowQuota().fetch_add(1,std::memory_order_relaxed);
  if(row<512){
   p.observation174=std::make_unique<MRT174Observation>();
   if(p.source->sourceDrawFrame==2048)LOG("[NativeMRT174Begin] source_frame=%llu submission=%llu",static_cast<unsigned long long>(p.source->sourceDrawFrame),static_cast<unsigned long long>(p.submission));
  }
  else if(row==512)LOG("[NativeMRT174Issue] kind=phase_row_cap limit=512");
 }
#endif
	if(p.retainsReadOnlyContents){EncodeReadOnlyMRT(p,command);return;}
	// A writing alias still waits for the real preceding U producer. Only
	// preservation copies disappear; its full MRT encoder and draws remain.
	if(p.reusesWritingStorage209)RequirePriorMRTContents(p);
	if(!p.reusesWritingStorage209&&!p.source->clearBefore && (p.priorColors[0].texture||p.priorCombinedColor.texture||p.priorDepth.texture)){
		RequirePriorMRTContents(p);
		auto copy=[command computeCommandEncoder];Require(copy,"Cannot encode original model MRT prior-content load");
  if(p.observation174)p.observation174->copyEncoder=true;
  const auto issued174=[&](unsigned slot,id<MTLTexture> source,const MTLSize& size){
   if(p.observation174){const auto format=source.pixelFormat;
    p.observation174->Issued(slot,size.width,size.height,size.depth,
     format==MTLPixelFormatRGBA8Unorm||format==MTLPixelFormatDepth32Float?4u:0u);}
  };
		[copy barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		const auto extent=MTLSizeMake(p.depth.texture.width,p.depth.texture.height,1);
		for(unsigned n=0;n<5;++n)if(p.priorColors[n].texture){[copy copyFromTexture:p.priorColors[n].texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.colors[n].texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];issued174(n,p.priorColors[n].texture,extent);}
		if(p.priorCombinedColor.texture){[copy copyFromTexture:p.priorCombinedColor.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.combinedColor.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];issued174(5,p.priorCombinedColor.texture,extent);}
		if(p.priorDepth.texture){[copy copyFromTexture:p.priorDepth.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.depth.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];issued174(6,p.priorDepth.texture,extent);}
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	if(p.other.dependencies)p.other.dependencies(command);
	if(p.lua)p.lua->EncodeDependencies(command);
	auto pass=[MTL4RenderPassDescriptor new];pass.defaultRasterSampleCount=1;
	pass.renderTargetWidth=p.depth.texture.width;pass.renderTargetHeight=p.depth.texture.height;
	for(unsigned n=0;n<5;++n){auto color=pass.colorAttachments[n];color.texture=p.colors[n].texture;color.loadAction=p.source->clearBefore?MTLLoadActionClear:(p.discardPrior145&(1u<<n))?MTLLoadActionDontCare:MTLLoadActionLoad;color.storeAction=MTLStoreActionStore;color.clearColor=MTLClearColorMake(0,0,0,0);}
	if(p.combinedColor.texture){auto color=pass.colorAttachments[5];color.texture=p.combinedColor.texture;color.loadAction=p.source->clearBefore?MTLLoadActionClear:(p.discardPrior145&32u)?MTLLoadActionDontCare:MTLLoadActionLoad;color.storeAction=MTLStoreActionStore;color.clearColor=MTLClearColorMake(0,0,0,0);}
	pass.depthAttachment.texture=p.depth.texture;pass.depthAttachment.loadAction=p.source->clearBefore?MTLLoadActionClear:(p.discardPrior145&64u)?MTLLoadActionDontCare:MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;
	if(p.other.initialAttachmentClear182)p.other.initialAttachmentClear182(pass);
	auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot encode original five-MRT model phase");
	try{
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:(scopedVertex220?MTLStageFragment:(MTLStageVertex|MTLStageFragment)) visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder setViewport:MTLViewport{0,0,double(pass.renderTargetWidth),double(pass.renderTargetHeight),0,1}];[encoder setScissorRect:MTLScissorRect{0,0,pass.renderTargetWidth,pass.renderTargetHeight}];
	if(p.other.sourceOrder){
		for(const auto& draw:p.sourceOrder){if(draw.engine)p.other.drawAt(draw.index,encoder);else p.lua->EncodeDraw(draw.index,encoder);}
		p.other.didEncodeSourceDraws();
	}else{
		if(p.other.beforeLua)p.other.beforeLua(encoder);
		if(p.lua)for(size_t n=0;n<p.lua->DrawCount();++n)p.lua->EncodeDraw(n,encoder);
		if(p.other.afterLua)p.other.afterLua(encoder);
	}
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	}catch(...){[encoder endEncoding];throw;}
	[encoder endEncoding];
	p.encoded=true;p.producer->encoded=true;
}
void PreparedCUSDeferred::DidComplete()
{
	auto& p=*impl;Require(p.encoded&&!p.completed,"Original model MRT cannot retire before its actual phase or twice");
	// The common caller already observed real submission retirement. Preserve
	// that fact even if a later child retirement callback reports an error.
	p.completed=true;p.producer->completed=true;
	if(p.lua)p.lua->DidComplete();if(p.other.didComplete)p.other.didComplete();
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(p.observation174){const auto& o=*p.observation174;const auto& m=p.source->depth.backing->Metadata();
  const unsigned domain=p.source->role==NativeGeometryTextureRole::Map?0u:p.source->role==NativeGeometryTextureRole::Models?(p.source->feature?2u:1u):3u;
  LOG("[NativeMRT174] source_frame=%llu submission=%llu domain=%u width=%u height=%u readonly=%u clear_before=%u clear_only=%u discard_mask=%u copy_path_eligible=%u copy_encoder=%u calls=%llu logical_source_bytes=%llu copy_width=%llu copy_height=%llu copy_depth=%llu c0_calls=%llu c0_bytes=%llu c1_calls=%llu c1_bytes=%llu c2_calls=%llu c2_bytes=%llu c3_calls=%llu c3_bytes=%llu c4_calls=%llu c4_bytes=%llu combined_calls=%llu combined_bytes=%llu depth_calls=%llu depth_bytes=%llu unknown_format=%llu invalid_extent=%llu overflow=%llu timestamp_samples=0 gpu_time=unknown retired=1",
   static_cast<unsigned long long>(p.source->sourceDrawFrame),static_cast<unsigned long long>(p.submission),domain,m.width,m.height,
   unsigned(p.retainsReadOnlyContents),unsigned(p.source->clearBefore),unsigned(p.source->clearOnly),p.discardPrior145,unsigned(!p.retainsReadOnlyContents&&!p.source->clearBefore&&(p.priorColors[0].texture||p.priorCombinedColor.texture||p.priorDepth.texture)),unsigned(o.copyEncoder),
   static_cast<unsigned long long>(o.Calls()),static_cast<unsigned long long>(o.Bytes()),
   static_cast<unsigned long long>(o.width),static_cast<unsigned long long>(o.height),static_cast<unsigned long long>(o.depth),
   static_cast<unsigned long long>(o.calls[0]),static_cast<unsigned long long>(o.bytes[0]),
   static_cast<unsigned long long>(o.calls[1]),static_cast<unsigned long long>(o.bytes[1]),
   static_cast<unsigned long long>(o.calls[2]),static_cast<unsigned long long>(o.bytes[2]),
   static_cast<unsigned long long>(o.calls[3]),static_cast<unsigned long long>(o.bytes[3]),
   static_cast<unsigned long long>(o.calls[4]),static_cast<unsigned long long>(o.bytes[4]),
   static_cast<unsigned long long>(o.calls[5]),static_cast<unsigned long long>(o.bytes[5]),
   static_cast<unsigned long long>(o.calls[6]),static_cast<unsigned long long>(o.bytes[6]),
   static_cast<unsigned long long>(o.unknownFormat),static_cast<unsigned long long>(o.invalidExtent),static_cast<unsigned long long>(o.overflow));
 }
#endif
}
} // namespace rendering::metal4
