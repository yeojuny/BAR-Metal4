// Actual publication backing lifetime; Resource is CPU-only, not an MTLTexture.
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Resource:NativeMapTextureResource {mutable bool completed=false;bool Completed()const noexcept override{return completed;}};
int main(){
 for(unsigned i=0;i<100;++i){
  auto old=std::make_shared<Resource>();auto current=old;
  auto backing=std::make_shared<NativeMapTextureBacking>();backing->Fulfill(old);
  auto complete=[backing]{std::dynamic_pointer_cast<const Resource>(backing->Resource())->completed=true;};
  current=std::make_shared<Resource>();
  complete();
  assert(backing->Resource()->Completed()&&!current->Completed());
  old.reset();assert(backing->Resource()->Completed());
  bool rejected=false;try{backing->Fulfill(current);}catch(...){rejected=true;}
  assert(rejected&&!current->Completed());
 }
 std::cout<<"Actual normal backing retained-owner/one-shot fulfillment100 passed; CPU resource only\n";
}
