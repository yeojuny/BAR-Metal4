#include "Rendering/Backend/NativeLegacyRefractionPublication.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Resource:NativeLegacyRefractionResource {bool encoded=false,completed=false;bool Completed()const noexcept override{return completed;}bool CanCancelUnsubmitted()const noexcept override{return !encoded&&!completed;}};
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();history.ActiveTexture(0);
 NativeLegacyRefractionPublication publication;publication.Create(registry,history,NativeCUSTextureTarget::Rectangle,4,4);
 uint64_t name=0;
 for(unsigned i=0;i<100;++i){
  auto object=publication.Copy(i+1);name=object->name;auto backing=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(object->backing);
  if(i%2)backing->Fulfill(std::make_shared<Resource>());
  assert(publication.AbortUncompleted());assert(registry.Find(name)->backing!=backing);
 }
 // Actual Begin evidence must quarantine even before a resource/copy encoder.
 auto begunObject=publication.Copy(101);auto begunBacking=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(begunObject->backing);begunBacking->MarkBegun(77);
 assert(!publication.AbortUncompleted());try{publication.Copy(102);assert(false);}catch(const std::logic_error&){}try{publication.Destroy();assert(false);}catch(const std::logic_error&){}
 auto begunResource=std::make_shared<Resource>();begunBacking->Fulfill(begunResource);assert(!publication.AbortUncompleted());begunResource->completed=true;assert(publication.AbortUncompleted());
 auto object=publication.Copy(103);auto backing=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(object->backing);auto resource=std::make_shared<Resource>();backing->Fulfill(resource);resource->encoded=true;
 for(unsigned i=0;i<100;++i){
  assert(!publication.AbortUncompleted());assert(registry.Find(name)->backing==backing);
  try{publication.Copy(102);assert(false);}catch(const std::logic_error&){}
  try{publication.Destroy();assert(false);}catch(const std::logic_error&){}
  assert(registry.Find(name)->backing==backing);
 }
 resource->completed=true;assert(publication.AbortUncompleted());assert(registry.Find(name)->backing==backing);
 auto next=publication.Copy(103);assert(next->backing!=backing);assert(publication.AbortUncompleted());assert(registry.Find(name)->backing==backing);
 publication.Destroy();assert(!registry.Find(name));
 std::cout<<"Legacy refraction:100 unsubmitted rollback and100 encoded rejection/replacement/destruction cycles;completed cleanup retains contents and unlocks next copy passed\n";
}
