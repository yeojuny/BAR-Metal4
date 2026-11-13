#include <algorithm>
#include <vector>
#include <memory>
#include <functional>
#include <span>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <cstdint>
#include "Rendering/Backend/NativeSubmissionReadScope.h"
struct NativeWorldParticleAtlas {int id;};
using Texture=std::shared_ptr<int>;
constexpr auto nil=nullptr;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#include "particle-atlas-state.inc"
#include "particle-atlas-complete.inc"
#include "particle-atlas-journal.inc"
struct RuntimeLease {
 rendering::NativeSubmissionReadIdentity identity;
 std::shared_ptr<ParticleAtlasPreparationJournal> journal=std::make_shared<ParticleAtlasPreparationJournal>();
 auto SourceSubmission(){return identity.Current();}
 auto ParticleAtlasPreparation(){return journal;}
 explicit RuntimeLease(uint64_t value){if(value)identity.Bind(value);}
};
struct Source {
 struct Impl {
  std::shared_ptr<ParticleAtlasPreparationJournal> lastAtlasPreparation;
  bool failRecovery=false;int residency=0;
  std::shared_ptr<Atlas> GetAtlas(RuntimeLease&,std::shared_ptr<const NativeWorldParticleAtlas> owner){
   Require(!failRecovery,"injected residency failure");++residency;
   for(const auto& atlas:lastAtlasPreparation->atlases)if(atlas->owner==owner)return atlas;
   return {};
  }
 };
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 #include "particle-atlas-prepare.inc"
};
auto AtlasOwner(int id){auto a=std::make_shared<Atlas>();a->owner=std::make_shared<NativeWorldParticleAtlas>(id);a->texture=std::make_shared<int>(id);a->image2D=std::make_shared<int>(id+10);return a;}
template<class F> bool Throws(F f){try{f();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 for(int i=0;i<1000;++i){
  // Returning to an earlier lease cannot bypass the most recent producer guard.
  {Source ordering;RuntimeLease old(1),newer(2);ordering.PrepareAtlasImages(old,1);ordering.PrepareAtlasImages(newer,2);auto pending=AtlasOwner(8);newer.journal->Register(pending,2);pending->encodedSubmission=2;
   assert(Throws([&]{ordering.PrepareAtlasImages(old,1);}));assert(ordering.impl->lastAtlasPreparation==newer.journal&&old.journal->submission==1);
   pending->initialized=true;ordering.PrepareAtlasImages(old,1);assert(old.journal->atlases.empty());}
  Source source;RuntimeLease A(11),B(12),C(11),zero(0);
  assert(Throws([&]{source.PrepareAtlasImages(zero,0);}));
  assert(Throws([&]{source.PrepareAtlasImages(B,11);}));
  source.PrepareAtlasImages(A,11);auto a=AtlasOwner(1),b=AtlasOwner(2),c=AtlasOwner(3);
  A.journal->Register(a,11);A.journal->Register(b,11);A.journal->Register(a,11);assert(A.journal->atlases.size()==2);
  a->encodedSubmission=11;source.PrepareAtlasImages(A,11);assert(A.journal->atlases.size()==2);
  // Original same-submission early-return semantics include already encoded work.
  source.impl->failRecovery=true;assert(Throws([&]{source.PrepareAtlasImages(C,11);}));assert(!C.journal->submission&&source.impl->lastAtlasPreparation==A.journal);
  source.impl->failRecovery=false;source.PrepareAtlasImages(C,11);assert(C.journal->atlases==A.journal->atlases&&source.impl->residency==2);
  source.PrepareAtlasImages(C,11);assert(source.impl->residency==2);
  assert(Throws([&]{source.PrepareAtlasImages(B,12);}));assert(!B.journal->submission&&source.impl->lastAtlasPreparation==C.journal);
  C.journal->atlases.clear();assert(A.journal->atlases.size()==2);
  // Taking completion transfers the exact owner's roster, not a later request.
  source.impl->lastAtlasPreparation=A.journal;b->encodedSubmission=11;
  auto complete=A.journal->TakeCompletion(11);source.PrepareAtlasImages(B,12);B.journal->Register(c,12);
  complete();assert(a->initialized&&b->initialized&&!c->initialized&&B.journal->atlases.size()==1);
  assert(Throws([&]{B.journal->Register(c,11);}));assert(Throws([&]{B.journal->TakeCompletion(11);}));
  c->encodedSubmission=12;B.journal->Complete(12);assert(c->initialized&&!c->encodedSubmission&&!B.journal->submission);
  // Failed, never-encoded preparation may be discarded on a new submission.
  source.PrepareAtlasImages(A,11);A.journal->Register(AtlasOwner(4),11);source.PrepareAtlasImages(B,12);assert(B.journal->atlases.empty());
  assert(A.journal->atlases.size()==1);
  B.journal->Complete(999);assert(B.journal->submission==12); // original empty no-op
  B.journal->TakeCompletion(12)();assert(!B.journal->submission);
  // Preserve existing sequential completion failure semantics, not invented rollback.
  ParticleAtlasPreparationJournal partial;partial.Prepare(20,{});auto p=AtlasOwner(5),q=AtlasOwner(6);partial.Register(p,20);partial.Register(q,20);p->encodedSubmission=20;q->encodedSubmission=19;
  assert(Throws([&]{partial.Complete(20);}));assert(p->initialized&&!q->initialized&&partial.submission==20&&partial.atlases.size()==2);
 }
 std::weak_ptr<const NativeWorldParticleAtlas> owner;std::weak_ptr<int> texture;
 std::function<void()> completion;
 {auto journal=std::make_shared<ParticleAtlasPreparationJournal>();journal->Prepare(3,{});auto a=AtlasOwner(7);owner=a->owner;texture=a->texture;a->encodedSubmission=3;journal->Register(a,3);completion=journal->TakeCompletion(3);}
 assert(!owner.expired()&&!texture.expired());completion();completion={};assert(owner.expired()&&texture.expired());
 std::cout<<"1000 atlas request A/B interleavings, same-ID idempotence/recovery, unretired rejection, failure/empty completion semantics and transferred full owner lifetime PASS\n";
}
