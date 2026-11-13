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
 first=source.AdvanceReflection(main,sky,0,false);
 auto allocation=registry.Find(main)->allocation;allocation.width=8;registry.Allocate(main,allocation);
 source.CancelReflection(first);assert(registry.Find(main)->allocation.width==8);assert(registry.Find(main)->backing!=first.previous->backing);
 first=source.AdvanceReflection(sky,0,0,false);auto name=first.current->name;registry.Delete(name);
 auto replacement=registry.Create(NativeCUSTextureTarget::Cube,"replacement",name);
 source.CancelReflection(first);assert(registry.Find(name)->identity==replacement->identity);
 source.Detach();source.CancelReflection(first);assert(!source.CurrentReflectionUpdate().current);
 std::cout<<"Exact cube cancellation: current, newer, absent, dual, sampler, reallocation, recreation, detach passed\n";
}
