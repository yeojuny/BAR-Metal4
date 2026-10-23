/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShadowTextureSource.h"
#include "NativeWorldDrawData.h"
#include <span>
#include <vector>
#include <memory>
#include <optional>
#include <utility>
#include <stdexcept>
namespace rendering {
class NativeShadowClearContract;
// Source-only acceptance, not a GPU completion or availability flag. Created
// before the existing ONE queue writer; committed only with its real ordinal.
class NativeShadowClearAcceptance {
 std::shared_ptr<const NativeShadowTextureFrame> source;
 const NativeShadowClearContract* owner=nullptr;size_t expectedCount=0;
 NativeWorldAttachmentClear clear;
 NativeShadowClearAcceptance()=default;
 friend class NativeShadowClearContract;
};
// Private immutable source-End owner. It binds every actual accepted clear
// tuple/ordinal to this exact fresh named-frame publication, never old pixels.
class NativeShadowClearProof {
 std::shared_ptr<const NativeShadowTextureFrame> source;
 uint64_t begin=0,end=0;std::vector<NativeWorldAttachmentClear> clears;
 NativeShadowClearProof(std::shared_ptr<const NativeShadowTextureFrame> s,uint64_t b,uint64_t e,
  std::vector<NativeWorldAttachmentClear> c):source(std::move(s)),begin(b),end(e),clears(std::move(c)){}
 friend class NativeShadowClearContract;
public:
 void RequireQueue(const std::shared_ptr<const NativeShadowTextureFrame>& actual,
  std::span<const NativeWorldAttachmentClear> queued,uint64_t actualBegin,uint64_t actualEnd)const{
  if(actual.get()!=source.get()||actualBegin!=begin||actualEnd!=end)throw std::runtime_error("Original shadow clear proof has another actual frame/publication boundary");
  size_t index=0;
  for(const auto& q:queued){
   if(q.sequence<=begin||q.sequence>=end)continue;
   if(index>=clears.size())throw std::runtime_error("Original shadow queue has an unaccepted source clear");
   const auto& c=clears[index++];
   if(q.sequence!=c.sequence||q.stage!=c.stage||q.mask!=c.mask||q.values!=c.values||q.colorWrite!=c.colorWrite||q.depthWrite!=c.depthWrite||q.frontStencilWriteMask!=c.frontStencilWriteMask||q.scissor!=c.scissor)
    throw std::runtime_error("Original shadow clear queue differs from its exact accepted source event");
  }
  if(index!=clears.size())throw std::runtime_error("Original shadow clear queue omitted an accepted source event");
 }
};
class NativeShadowClearContract {
 std::shared_ptr<const NativeShadowTextureFrame> source;uint64_t begin;
 std::vector<NativeWorldAttachmentClear> accepted;
 bool depthInitialized=false,colorInitialized=false;
 static void Check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
 static bool Full(uint32_t size,const std::optional<ScreenScissor>& s){
  if(!s)return true;
  // Original GL scissor is framebuffer coordinates, independent of viewport.
  // Lua has already added viewPos; do not subtract a window origin again.
  return int64_t(s->x)<=0&&int64_t(s->y)<=0&&int64_t(s->x)+s->width>=size&&int64_t(s->y)+s->height>=size;
 }
public:
 NativeShadowClearContract(std::shared_ptr<const NativeShadowTextureFrame> s,uint64_t b):source(std::move(s)),begin(b){
  Check(source&&begin&&source->depth.backing&&source->depth.backing->request.kind==NativeShadowTextureKind::Depth&&source->depth.backing->request.size,"Original shadow clear contract has no actual fresh depth attachment");
  if(source->redrawColor)Check(source->color.backing&&source->color.backing->request.kind==NativeShadowTextureKind::Color,"Original positive-sun clear contract has no actual fresh color attachment");
  else Check(source->retainedColor&&!source->color.backing,"Original dark-sun clear contract lost its immutable retained color");
  // Even a completed/InitialClear PREVIOUS object does not populate the NEW
  // physical destination. Both newly-written attachments start uninitialized.
 }
 NativeShadowClearAcceptance Validate(unsigned mask,const ScreenClearValues& values,
  const NativeWorldDrawState& state,std::optional<ScreenScissor> scissor)const{
  Check(mask&&!(mask&~unsigned(ScreenAttachmentColor|ScreenAttachmentDepth)),"Original shadow clear requires native color/depth bits and has no stencil attachment consumer");
  if(mask&ScreenAttachmentDepth){
   Check(state.depthWrite&&Full(source->depth.backing->request.size,scissor),"Original shadow depth clear needs preserved pixels or an exact partial-clear consumer; fresh storage has no prior-copy producer");
  }
  if(mask&ScreenAttachmentColor){
   Check(source->redrawColor&&source->color.backing,"Original shadow color clear has no writable color version; dark-sun color is immutable");
   const auto& request=source->color.backing->request;
   Check(request.originalFormat==(request.colored?0x8051u:0x8229u),"Original shadow color channel availability is unobserved");
   Check(Full(request.size,scissor)&&state.colorWrite[0]&&(!request.colored||(state.colorWrite[1]&&state.colorWrite[2])),"Original shadow RGB/R clear needs preserved pixels or an exact partial-clear consumer; fresh storage has no prior-copy producer");
   // RGB8 has no source alpha channel; R8 has only red. Do not invent a
   // requirement that unavailable channels' write masks be enabled.
  }
  NativeShadowClearAcceptance result;result.source=source;result.owner=this;result.expectedCount=accepted.size();
  result.clear.stage=NativeWorldDrawStage::ShadowOpaque;result.clear.mask=static_cast<ScreenAttachmentMask>(mask);result.clear.values=values;
  result.clear.colorWrite=state.colorWrite;result.clear.depthWrite=state.depthWrite;result.clear.frontStencilWriteMask=state.frontStencil.writeMask;result.clear.scissor=std::move(scissor);return result;
 }
 void Commit(const NativeShadowClearAcceptance& pending,uint64_t actualRecordedSequence){
  Check(pending.owner==this&&pending.source.get()==source.get()&&pending.expectedCount==accepted.size()&&actualRecordedSequence>begin&&(accepted.empty()||actualRecordedSequence>accepted.back().sequence),"Original shadow clear commit is stale/repeated or lacks its actual queue ordinal");
  auto clear=pending.clear;clear.sequence=actualRecordedSequence;accepted.push_back(std::move(clear));
  if(pending.clear.mask&ScreenAttachmentDepth)depthInitialized=true;
  if(pending.clear.mask&ScreenAttachmentColor)colorInitialized=true;
 }
 std::shared_ptr<const NativeShadowClearProof> Seal(uint64_t end)const{
  Check(end>begin&&depthInitialized&&(!source->redrawColor||colorInitialized)&&(!accepted.empty()&&accepted.back().sequence<end),"Original shadow fresh attachment did not receive its actual accepted full initialization before source-End");
  return std::shared_ptr<const NativeShadowClearProof>(new NativeShadowClearProof(source,begin,end,accepted));
 }
};
}
