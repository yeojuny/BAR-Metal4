#include <array>
#include <optional>
#include <new>
#include <memory>
#include <functional>
#include <cstdint>
#include <cassert>
#include <stdexcept>
struct NativeCUSTextureObject{};
struct NativeTerrainInfoResource{virtual ~NativeTerrainInfoResource()=default;};
struct NativeWorldInfoFrame{};
struct RuntimeLease{bool pending=false;bool PreparingWhilePending()const{return pending;}};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct PreparedWorldInfo{
 int registered=0,encoded=0,completed=0;bool fail=false;
 void RegisterResources(RuntimeLease&){++registered;}
 void EncodeDependencies(void*){++encoded;if(fail)throw std::runtime_error("encode");}
 void DidComplete(){++completed;}
};
#include "terrain-info-resource.inc"

struct CUSSourceImage {
 std::shared_ptr<const void> owner;
 std::function<void()> requireReadable;
};
#include "terrain-info-receipts.inc"
struct ThrowingCopy {
 static inline int failure=0;
 char padding[128]{};
 ThrowingCopy()=default;
 ThrowingCopy(const ThrowingCopy&){if(failure==1)throw std::bad_alloc();if(failure==2)throw std::runtime_error("copy");}
 void operator()()const{}
};
int main(){
 {
  TerrainInfoLeaseReceipts cache;
  TerrainInfoLeaseReceipts::Key key{std::make_shared<NativeCUSTextureObject>(),std::make_shared<NativeTerrainInfoResource>(),1};
  CUSSourceImage image{std::make_shared<int>(1),ThrowingCopy{}};
  ThrowingCopy::failure=1;cache.Remember(key,image);assert(!cache.Find(key));
  ThrowingCopy::failure=2;
  try{cache.Remember(key,image);assert(false);}catch(const std::runtime_error&){}
  assert(!cache.Find(key));ThrowingCopy::failure=0;
  cache.Remember(key,image);assert(cache.Find(key));
  ThrowingCopy::failure=1;
  try{auto copy=*cache.Find(key);(void)copy;assert(false);}catch(const std::bad_alloc&){}
  ThrowingCopy::failure=0;assert(cache.Find(key));auto copy=*cache.Find(key);copy.requireReadable();
 }
 {
  RuntimeLease lease;InfoResource resource;resource.plan=std::make_shared<PreparedWorldInfo>();
  resource.Register(lease,1);
  try{resource.Readable(1);assert(false);}catch(const std::runtime_error&){}
  resource.Encode(reinterpret_cast<void*>(1),1);resource.Encode(reinterpret_cast<void*>(1),1);
  resource.Readable(1);assert(resource.plan->encoded==1);
  lease.pending=true;resource.Register(lease,2);assert(resource.registeredSubmission==1);
  try{resource.Readable(2);assert(false);}catch(const std::runtime_error&){}
  try{resource.Complete(2);assert(false);}catch(const std::runtime_error&){}
  resource.Complete(1);resource.Readable(2);resource.Complete(1);assert(resource.plan->completed==1);
  InfoResource failed;failed.plan=std::make_shared<PreparedWorldInfo>();failed.plan->fail=true;lease.pending=false;failed.Register(lease,3);
  try{failed.Encode(reinterpret_cast<void*>(1),3);assert(false);}catch(const std::runtime_error&){}
  try{failed.Readable(3);assert(false);}catch(const std::runtime_error&){}
  try{failed.Register(lease,3);assert(false);}catch(const std::runtime_error&){}
 }
 std::weak_ptr<const void> released;
 {TerrainInfoLeaseReceipts cache;TerrainInfoLeaseReceipts::Key key{};
  CUSSourceImage image{std::make_shared<int>(1),[]{}};released=image.owner;
  cache.Remember(key,image);image.owner.reset();assert(!released.expired());}
 assert(released.expired());

 for(int round=0;round<1000;++round){
  TerrainInfoLeaseReceipts cache,otherLease;
  TerrainInfoLeaseReceipts::Key key{std::make_shared<NativeCUSTextureObject>(),std::make_shared<NativeTerrainInfoResource>(),1,1,0,17,true,false,false,false};
  auto owner=std::make_shared<int>(1);std::weak_ptr<int> life=owner;
  bool readable=false;int checks=0;
  CUSSourceImage image{owner,[&]{++checks;if(!readable)throw std::runtime_error("before producer");}};
  assert(!cache.Find(key));cache.Remember(key,image);owner.reset();image.owner.reset();
  assert(!life.expired());const auto* found=cache.Find(key);assert(found&&found->owner);
  assert(!otherLease.Find(key));
  try{found->requireReadable();assert(false);}catch(const std::runtime_error&){}
  readable=true;found->requireReadable();found->requireReadable();assert(checks==3);
  for(int field=0;field<10;++field){auto changed=key;
   switch(field){case 0:changed.object=std::make_shared<NativeCUSTextureObject>();break;case 1:changed.resource=std::make_shared<NativeTerrainInfoResource>();break;
    case 2:++changed.submission;break;case 3:++changed.registered;break;case 4:++changed.encoding;break;case 5:++changed.sampler;break;
    case 6:changed.validation=!changed.validation;break;case 7:changed.completed=!changed.completed;break;case 8:changed.encoded=!changed.encoded;break;case 9:changed.pending=!changed.pending;break;}
   assert(!cache.Find(changed));
  }
  for(int i=2;i<=32;++i){auto next=key;next.submission=i;cache.Remember(next,image);assert(cache.Find(next));}
  auto full=key;full.submission=33;cache.Remember(full,image);assert(!cache.Find(full));assert(cache.Find(key));
 }
}
