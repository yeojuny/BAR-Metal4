// Actual production snapshot template; controlled source/handler adapters only.
// Does not execute ShadowHandler, native registry, Metal or GPU readiness code.
#include "Rendering/Backend/NativeShadowFallbackSnapshot.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace rendering;
struct Source {
 bool attached=true;unsigned imageCalls=0;
 std::array<std::shared_ptr<const NativeShadowTextureBacking>,2> images;
 bool Attached()const{return attached;}
 auto Image(unsigned name){++imageCalls;if(name<10||name>11||!images[name-10])throw std::runtime_error("missing test shadow");return images[name-10];}
};
struct Handler {
 bool owner=true;mutable unsigned checks=0,names=0;
 bool HasNativeTextureOwner()const{++checks;return owner;}
 unsigned GetShadowTextureID()const{++names;return 10;}
 unsigned GetColorTextureID()const{++names;return 11;}
};
int main(){
 for(unsigned i=0;i<1000;++i){
  auto depth=std::make_shared<NativeShadowTextureBacking>();auto color=std::make_shared<NativeShadowTextureBacking>();
  depth->request.kind=NativeShadowTextureKind::Depth;color->request.kind=NativeShadowTextureKind::Color;
  Source source;source.images={depth,color};Handler handler;
  auto snapshot=NativeShadowFallbackSnapshot::Capture(source,handler);
  assert(snapshot.attached&&snapshot.images[0]==depth&&snapshot.images[1]==color);
  assert(source.imageCalls==2 && handler.names==2);
  const auto retained=std::weak_ptr<const NativeShadowTextureBacking>(depth);
  source.images={std::make_shared<NativeShadowTextureBacking>(),std::make_shared<NativeShadowTextureBacking>()};
  source.attached=false;handler.owner=false;
  assert(snapshot.images[0]==depth&&snapshot.images[1]==color);
  // Readiness is deliberately not frozen. Exact retained producer may retire
  // after capture; consumption then observes that same producer's completion.
  depth->completed=true;assert(snapshot.images[0]->completed.load());
  depth.reset();color.reset();source.images={};
  assert(!retained.expired());snapshot={};assert(retained.expired());
  handler.checks=handler.names=source.imageCalls=0;
  auto absent=NativeShadowFallbackSnapshot::Capture(source,handler);
  assert(!absent.attached&&!absent.images[0]&&!absent.images[1]);
  assert(handler.checks==0&&handler.names==0&&source.imageCalls==0);
  source.attached=true;
  absent=NativeShadowFallbackSnapshot::Capture(source,handler);
  assert(!absent.attached&&handler.checks==1&&handler.names==0&&source.imageCalls==0);
  handler.owner=true;source.images[0]=std::make_shared<NativeShadowTextureBacking>();
  bool threw=false;try{(void)NativeShadowFallbackSnapshot::Capture(source,handler);}catch(const std::runtime_error&){threw=true;}
  assert(threw && source.imageCalls==2); // Missing second image is not fabricated.
 }
 std::cout<<"Actual shadow snapshot template:1000 exact replacement/reset/readiness/absence/failure checks PASS (controlled adapters)\n";
}
