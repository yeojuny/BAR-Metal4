// GPL v2 or later. Real history/registry/terrain capture, no renderer execution.
#include "Rendering/Backend/NativeTerrainSourceData.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Backing:NativeTextureBacking{NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}};
int main(){
 unsigned checks=0;auto check=[&](bool c){++checks;assert(c);};
 auto changed=[](const auto&a,const auto&b){unsigned n=0;for(unsigned i=0;i<32;++i)n+=a[i]!=b[i];return n;};
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();
 auto rows=history.ResolvedRows();check(changed(rows,history.ResolvedRows())==0);
 history.ActiveTexture(5);check(changed(rows,history.ResolvedRows())==0);
 auto obj=registry.Create(NativeCUSTextureTarget::Image2D,"row-fixture");rows=history.ResolvedRows();
 registry.Bind(0,obj->name,obj->target);auto bound=history.ResolvedRows();check(changed(rows,bound)==1);
 registry.Bind(3,obj->name,obj->target);registry.Bind(7,obj->name,obj->target);rows=history.ResolvedRows();
 auto sampling=obj->sampling;sampling.minFilter=0x2601;registry.SetSampling(obj->name,sampling);
 auto sampled=history.ResolvedRows();check(changed(rows,sampled)==32);
 for(unsigned i:{0,3,7})check((*sampled[i])[0].object->sampling.minFilter==0x2601);
 check((*bound[0])[0].object->sampling.minFilter!=0x2601);
 history.Enable(1,obj->target,true);rows=history.ResolvedRows();check(changed(sampled,rows)==1);
 history.ObserveDefaultSampling(obj->target,sampling);auto defaults=history.ResolvedRows();check(changed(rows,defaults)==32);check((*defaults[2])[0].defaultSampling.minFilter==0x2601);
 history.ObserveDefaultStorage(obj->target,false);check(!(*history.ResolvedRows()[2])[0].defaultStorageKnownEmpty);
 history.PushAttributes(true,true);registry.Bind(0,0,obj->target);history.PopAttributes();check((*history.ResolvedRows()[0])[0].object->identity==obj->identity);
 NativeTextureAllocation allocation{8,8,1,1,1,0x8058};registry.Allocate(obj->name,allocation);
 auto backing=std::make_shared<Backing>();backing->storageGeneration=obj->identity;backing->contentVersion=1;backing->content=NativeTextureContent::ProducerVersion;
 registry.PublishContent(obj->name,{},backing);auto previous=registry.Find(obj->name);
 auto nextBacking=std::make_shared<Backing>(*backing);nextBacking->contentVersion=previous->version+1;
 auto current=registry.PublishRedefinedContent(previous,allocation,nextBacking);
 auto captured=CaptureNativeTerrainImages(registry,history,12);auto sequence=history.Sequence();
 check(registry.CancelContentPublication(current,previous));check(history.Sequence()==sequence);
 auto restored=CaptureNativeTerrainImages(registry,history,12);
 check((*captured.Units()[0])[0].object->backing==nextBacking);
 check((*restored.Units()[0])[0].object->backing==backing);
 for(unsigned i:{0,3,7})check((*restored.Units()[i])[0].object->backing==backing);
 auto stable=history.ResolvedRows();for(unsigned i=0;i<64;++i){auto snapshot=CaptureNativeTerrainImages(registry,history,12);check(changed(stable,snapshot.Units())==0);ValidateNativeTerrainImages(snapshot,registry.ContextGeneration(),12);}
 registry.Delete(obj->name);auto deleted=history.ResolvedRows();for(unsigned i:{0,3,7})check(!(*deleted[i])[0].object);
 registry.CloseContext();check(changed(deleted,history.ResolvedRows())==32);registry.NewContext();check(!(*history.ResolvedRows()[0])[0].object);
 // A failing resolver never publishes the partially rebuilt row.
 auto external=std::make_shared<NativeCUSTextureObject>();external->identity=99;external->source="external";history.Bind(0,external);
 bool fail=true;unsigned calls=0;
 history.SetObjectResolvers([&](auto o){++calls;if(fail)throw std::runtime_error("resolver");return o;},[](auto o){return o;});
 bool threw=false;try{history.ResolvedRows();}catch(const std::runtime_error&){threw=true;}check(threw);fail=false;auto retry=history.ResolvedRows();check((*retry[0])[0].object==external&&calls==2);
 check(changed(retry,history.ResolvedRows())==0);
 std::cout<<"PASS "<<checks<<" production row-cache checks; game/GPU not run\n";
}
