#include <array>
#include <optional>
#include <new>
#include <memory>
#include <functional>
#include <cstdint>
#include <cassert>
#include <stdexcept>
struct NativeCUSTextureObject{};
struct NativeTerrainShadingResource{virtual ~NativeTerrainShadingResource()=default;};
struct NativeTerrainShadingBacking{mutable unsigned retired=0;void RetirePrior()const{++retired;}};
struct PreparedMiniMap{unsigned completed=0;void DidComplete(){++completed;}};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct CUSSourceImage{std::shared_ptr<const void> owner;std::function<void()> requireReadable;};
#include "shading-resource.inc"
#include "shading-receipts.inc"
struct ThrowCopy{static inline bool fail=false;char data[128]{};ThrowCopy()=default;ThrowCopy(const ThrowCopy&){if(fail)throw std::bad_alloc();}void operator()()const{}};
int main(){
 for(int run=0;run<1000;++run){
  TerrainShadingLeaseReceipts cache,other;
  TerrainShadingLeaseReceipts::Key key{std::make_shared<NativeCUSTextureObject>(),std::make_shared<NativeTerrainShadingBacking>(),std::make_shared<NativeTerrainShadingBacking>(),std::make_shared<NativeTerrainShadingResource>(),std::make_shared<NativeTerrainShadingResource>(),std::make_shared<int>(1),7,9,1,1,0,0,true,false};
  bool readable=false;unsigned checks=0;
  CUSSourceImage image{std::make_shared<int>(1),[&]{++checks;if(!readable)throw std::runtime_error("early");}};
  cache.Remember(key,image);assert(cache.Find(key)&&!other.Find(key));
  try{cache.Find(key)->requireReadable();assert(false);}catch(const std::runtime_error&){}
  readable=true;cache.Find(key)->requireReadable();cache.Find(key)->requireReadable();assert(checks==3);
  for(int field=0;field<14;++field){auto k=key;switch(field){
   case 0:k.object=std::make_shared<NativeCUSTextureObject>();break;case 1:k.source=std::make_shared<NativeTerrainShadingBacking>();break;
   case 2:k.priorSource=std::make_shared<NativeTerrainShadingBacking>();break;case 3:k.resource=std::make_shared<NativeTerrainShadingResource>();break;
   case 4:k.prior=std::make_shared<NativeTerrainShadingResource>();break;case 5:k.plan=std::make_shared<int>(2);break;
   case 6:++k.sampler;break;case 7:++k.producer;break;case 8:++k.submission;break;case 9:++k.prepared;break;
   case 10:++k.encoding;break;case 11:++k.encoded;break;case 12:k.validation=!k.validation;break;case 13:k.completed=!k.completed;break;}
   assert(!cache.Find(k));
  }
  for(int i=2;i<=32;++i){auto k=key;k.submission=i;cache.Remember(k,image);assert(cache.Find(k));}
  key.submission=33;cache.Remember(key,image);assert(!cache.Find(key));
 }
 {TerrainShadingLeaseReceipts cache;TerrainShadingLeaseReceipts::Key key{};CUSSourceImage image{std::make_shared<int>(1),ThrowCopy{}};
  ThrowCopy::fail=true;cache.Remember(key,image);assert(!cache.Find(key));ThrowCopy::fail=false;cache.Remember(key,image);assert(cache.Find(key));
  ThrowCopy::fail=true;try{auto copy=*cache.Find(key);(void)copy;assert(false);}catch(const std::bad_alloc&){}
  ThrowCopy::fail=false;assert(cache.Find(key));}
 std::weak_ptr<const void> weak;
 {TerrainShadingLeaseReceipts cache;TerrainShadingLeaseReceipts::Key key{};CUSSourceImage image{std::make_shared<int>(1),[]{}};weak=image.owner;cache.Remember(key,image);image.owner.reset();assert(!weak.expired());}assert(weak.expired());
 {Resource value;value.plan=std::make_shared<PreparedMiniMap>();auto prior=std::make_shared<Resource>();prior->plan=std::make_shared<PreparedMiniMap>();
  value.prior=prior;value.priorSource=std::make_shared<NativeTerrainShadingBacking>();NativeTerrainShadingBacking source;
  value.preparedSubmission=value.encodingSubmission=2;
  try{Readable(value,2);assert(false);}catch(const std::runtime_error&){}
  try{Complete(value,source,2);assert(false);}catch(const std::runtime_error&){}
  assert(!value.completed&&value.prior&&value.plan&&source.retired==0);
  value.encodedSubmission=2;prior->encodedSubmission=1;
  try{Complete(value,source,2);assert(false);}catch(const std::runtime_error&){}
  assert(!value.completed&&value.prior&&value.plan&&source.retired==0);
  prior->encodedSubmission=2;auto plan=value.plan;auto priorPlan=prior->plan;auto priorSource=value.priorSource;
  Readable(value,2);Complete(value,source,2);Complete(value,source,2);Readable(value,3);
  assert(value.completed&&prior->completed&&!value.prior&&!value.plan&&!value.priorSource);
  assert(plan->completed==1&&priorPlan->completed==1&&source.retired==1&&priorSource->retired==1);
 }
}
