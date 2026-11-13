#include "Rendering/Backend/NativeCubeTextureSource.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Resource:NativeCubeTextureResource{};
int main(){
 NativeCUSTextureHistory history; NativeTextureRegistry registry(history); registry.NewContext();
 NativeCubeTextureSource source; source.Attach(registry,history,{[](auto,auto,auto){return std::make_shared<Resource>();},[]{return 7;}});
 const auto main=source.Create(NativeCubeTextureKind::Reflection,4,true), sky=source.Create(NativeCubeTextureKind::SkyReflection,4,true);
 auto base=source.Image(main);
 auto first=source.AdvanceReflection(main,sky,0,false);
 source.CancelReflection(first); assert(source.Image(main)==base); assert(!source.CurrentReflectionUpdate().current);
 first=source.AdvanceReflection(main,sky,0,false);
 auto newer=source.AdvanceReflection(main,sky,1,false);
 source.CancelReflection(first); assert(source.Image(main)==newer.backing); assert(source.CurrentReflectionUpdate().current==newer.current);
 source.CancelReflection({}); assert(source.CurrentReflectionUpdate().current==newer.current);
 // Later sampler changes survive exact content rollback.
 auto sampling=registry.Find(main)->sampling;sampling.wrapS=0x2901;registry.SetSampling(main,sampling);
 source.CancelReflection();assert(source.Image(main)==first.backing);assert(registry.Find(main)->sampling.wrapS==0x2901);
 auto beforeMain=source.Image(main),beforeSky=source.Image(sky);
 auto dual=source.AdvanceReflection(main,sky,11,true);assert(dual.mipCurrent);
 source.CancelReflection(dual);assert(source.Image(main)==beforeMain);assert(source.Image(sky)==beforeSky);
 dual=source.AdvanceReflection(main,sky,11,true); newer=source.AdvanceReflection(main,sky,10,true);
 source.CancelReflection(dual);assert(source.Image(main)==newer.mipBacking);assert(source.Image(sky)==newer.backing);assert(source.CurrentReflectionUpdate().current==newer.current);
 source.CancelReflection(newer);
 // Actual production capture guard: exceptions restore exact unsubmitted writes.
 for(unsigned i=0;i<100;++i){
  const auto prior=source.Image(main);
  try{
   NativeCubeReflectionCaptureRollback rollback(source);
   auto captured=source.AdvanceReflection(main,sky,0,false);rollback.Capture(captured);
   throw std::runtime_error("capture boundary failure");
  }catch(const std::runtime_error&){}
  assert(source.Image(main)==prior);
 }
 {NativeCubeReflectionCaptureRollback rollback(source);first=source.AdvanceReflection(main,sky,0,false);rollback.Capture(first);rollback.Release();}
 assert(source.Image(main)==first.backing);source.CancelUnsubmittedReflection(first);
 // Real Begin/producer evidence forbids rollback, including the mip half.
 for(unsigned state=0;state<3;++state){
  first=source.AdvanceReflection(main,sky,11,true);
  if(state==0)first.backing->encodedSubmission=9;
  if(state==1)first.mipBacking->dependenciesSubmission=9;
  if(state==2)first.backing->completed=true;
  source.CancelUnsubmittedReflection(first);
  assert(source.Image(sky)==first.backing && source.Image(main)==first.mipBacking);
  first.backing->encodedSubmission=0;first.mipBacking->dependenciesSubmission=0;first.backing->completed=false;
  source.CancelUnsubmittedReflection(first);
 }
 {NativeCubeReflectionCaptureRollback rollback(source);first=source.AdvanceReflection(main,sky,0,false);rollback.Capture(first);
  newer=source.AdvanceReflection(main,sky,1,false);}
 assert(source.Image(main)==newer.backing && source.CurrentReflectionUpdate().current==newer.current);
 first=source.AdvanceReflection(main,sky,0,false);
 auto allocation=registry.Find(main)->allocation;allocation.width=8;registry.Allocate(main,allocation);
 source.CancelReflection(first);assert(registry.Find(main)->allocation.width==8);assert(registry.Find(main)->backing!=first.previous->backing);
 first=source.AdvanceReflection(sky,0,0,false);auto name=first.current->name;registry.Delete(name);
 auto replacement=registry.Create(NativeCUSTextureTarget::Cube,"replacement",name);
 source.CancelReflection(first);assert(registry.Find(name)->identity==replacement->identity);
 source.Detach();source.CancelReflection(first);assert(!source.CurrentReflectionUpdate().current);
 std::cout<<"Exact cube cancellation: current, newer, absent, dual, sampler, reallocation, recreation, detach and capture rollback100 / submitted guard / newer preservation passed\n";
}
