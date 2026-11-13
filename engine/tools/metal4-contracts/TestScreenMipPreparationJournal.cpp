#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <cstdint>
struct ScreenTexture {int id;};
using Texture=std::shared_ptr<int>;
constexpr auto nil=nullptr;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#include "screen-mip-state.inc"
#include "screen-mip-journals.inc"
template<class F> bool Throws(F f){try{f();return false;}catch(const std::runtime_error&){return true;}}
auto Image(int id){return ScreenMipGeneration{std::make_shared<ScreenTexture>(id),std::make_shared<int>(id),std::make_shared<ScreenMipState>()};}
void Queue(ScreenMipPreparationJournal& journal,const ScreenMipGeneration& image){journal.Queue(image.owner,image.image,image.state);}
int main(){
 for(int i=0;i<1000;++i){
  auto gpu=std::make_shared<const ScreenMipOwnerIdentity>(),otherGPU=std::make_shared<const ScreenMipOwnerIdentity>();
  ScreenMipRecovery recovery,other;auto A=std::make_unique<ScreenMipLeaseJournals>();ScreenMipLeaseJournals B;
  auto a=recovery.Acquire(A->Get(gpu)),b=B.Get(gpu),separate=other.Acquire(A->Get(otherGPU));
  assert(a==A->Get(gpu)&&a!=separate);auto first=Image(1),second=Image(2);Queue(*a,first);Queue(*a,second);Queue(*a,first);
  assert(a->images.size()==2&&first.state->queued&&second.state->queued);
  assert(Throws([&]{recovery.Acquire(b);}));assert(a->images.size()==2&&b->images.empty());
  assert(Throws([&]{recovery.Acquire(separate);}));
  A.reset();assert(!a->leaseActive&&!separate->leaseActive);
  recovery.Acquire(b);assert(a->images.empty()&&b->images.size()==2&&b->images[0].owner==first.owner&&b->images[1].owner==second.owner);
  assert(first.state->queued&&second.state->queued);
  // A prior empty/extracted request cannot steal B's still-live nonempty queue.
  {ScreenMipLeaseJournals liveA,liveB;ScreenMipRecovery r;auto ja=r.Acquire(liveA.Get(gpu));auto x=Image(9);Queue(*ja,x);auto taken=ja->Take([](const Texture&){});
   auto jb=r.Acquire(liveB.Get(gpu));Queue(*jb,Image(10));assert(Throws([&]{r.Acquire(ja);}));assert(jb->images.size()==1&&ja->images.empty()&&taken.size()==1);}
  int retained=0;assert(Throws([&]{b->Take([&](const Texture&){if(++retained==2)throw std::runtime_error("residency failure");});}));
  assert(b->images.size()==2&&first.state->queued&&second.state->queued);
  // No initialized filter is introduced: preserve the entire original plan.
  second.state->initialized=true;retained=0;auto plan=b->Take([&](const Texture&){++retained;});assert(plan.size()==2&&retained==2&&b->images.empty());
  assert(!first.state->queued&&!second.state->queued);
  plan.clear();Queue(*b,first);Queue(*b,second);assert(b->images.size()==1); // abandoned unencoded plan can be queued again
  auto repeat=b->Take([](const Texture&){});assert(repeat.size()==1);first.state->initialized=true;
  Queue(*b,first);assert(b->images.empty());assert(b->Take([](const Texture&){}).empty());
 }
 // Actual capsule destructor must run before the containing request unlocks.
 std::shared_ptr<ScreenMipPreparationJournal> observed;
 struct LockWitness {std::shared_ptr<ScreenMipPreparationJournal>* observed;~LockWitness(){assert(!(*observed)->leaseActive);}};
 struct LeaseShape {LockWitness lock;ScreenMipLeaseJournals journals;};
 {LeaseShape lease{{&observed},{}};observed=lease.journals.Get(std::make_shared<const ScreenMipOwnerIdentity>());}
 std::weak_ptr<const ScreenTexture> source;std::weak_ptr<int> texture;std::weak_ptr<const ScreenMipOwnerIdentity> identity;
 ScreenMipRecovery recovery;std::vector<ScreenMipGeneration> retainedPlan;
 {ScreenMipLeaseJournals lease;auto owner=std::make_shared<const ScreenMipOwnerIdentity>();identity=owner;auto j=recovery.Acquire(lease.Get(owner));auto image=Image(3);source=image.owner;texture=image.image;Queue(*j,image);}
 assert(!source.expired()&&!texture.expired()&&!identity.expired());
 {ScreenMipLeaseJournals lease;auto j=recovery.Acquire(lease.Get(identity.lock()));retainedPlan=j->Take([](const Texture&){});}
 assert(!source.expired()&&!texture.expired());retainedPlan.clear();assert(source.expired()&&texture.expired());
 std::cout<<"1000 active-rejection/inactive ordered recovery, allocation failure, multi-GPU/dedup, whole-plan count and drop/requeue cases; capsule-before-unlock and strong owner lifetimes PASS\n";
}
