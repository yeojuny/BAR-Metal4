#!/usr/bin/env python3
"""Actual pending-input eligibility and lease registration; mock native objects only."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
post=(root/'rts/Rendering/Metal4/Metal4PostprocessNative.mm').read_text()
runtime=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
def body(source, signature):
    start=source.index('{',source.index(signature))+1
    depth=1
    for i in range(start,len(source)):
        depth+=(source[i]=='{')-(source[i]=='}')
        if depth==0:return source[start:i]
    raise AssertionError('unclosed function')
preparation=body(post,'bool PreparationInput(const NativePostprocessVersion& version,RuntimeLease& lease) const')
preparation=preparation.replace('texture.','texture->').replace('(__bridge const void*)texture','static_cast<const void*>(texture.get())')
source=r'''
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include "Rendering/Backend/NativePostprocessTextureReceipt.h"
using rendering::NativePostprocessTextureAspect;
using rendering::NativePostprocessTextureReceipt;
void Require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
enum class NativePriorRead : unsigned {Postprocess=32};
constexpr int MTLTextureType2D=2,MTLStorageModePrivate=1;
struct MockTexture {
 int device=1,storageMode=MTLStorageModePrivate,textureType=MTLTextureType2D;
 int sampleCount=1,mipmapLevelCount=1,arrayLength=1;
 bool parentTexture=false,iosurface=false,buffer=false,heap=false;
};
struct NativePostprocessVersion {
 struct Image {uint64_t object=17;int format=0;};
 std::shared_ptr<Image> image=std::make_shared<Image>();
 uint64_t generation=1;
};
bool IsNativePostprocessDepth(int format){return format!=0;}
struct Frame {bool recording=false,pending=false;int slot=1;bool previousRetired=true;unsigned priorReadClasses=0;std::map<const void*,std::function<void()>> priorReadChecks;};
struct Pending {struct {std::shared_ptr<Frame> owner=std::make_shared<Frame>();} resources;};
struct Runtime {bool poisoned=false;std::shared_ptr<Pending> pendingSubmission=std::make_shared<Pending>();};
struct RuntimeLease {
 std::shared_ptr<Frame> frame=std::make_shared<Frame>();Runtime runtime;
 bool rejectGPU=false;
 uint64_t pendingSource=3;
 std::function<void()> gpuCheck;
 RuntimeLease(){runtime.pendingSubmission->resources.owner->slot=2;}
 bool PreparingWhilePending()const {return bool(frame->previousRetired)&&bool(runtime.pendingSubmission);}
 int Device()const{return 1;}
 uint64_t PendingEventForSource(uint64_t source)const{return source&&source==pendingSource?7:0;}
 bool OrderGPUReadAfterSubmission(const void*,uint64_t source,uint64_t event,std::function<void()> check,NativePriorRead kind){
  assert(event==PendingEventForSource(source)&&kind==NativePriorRead::Postprocess);
  if(rejectGPU)return false;
  gpuCheck=std::move(check);return true;
 }
 bool OrderAfterPreviousSubmission(const void* owner,std::function<void()> check,NativePriorRead kind){
'''+body(runtime,'bool RuntimeLease::OrderAfterPreviousSubmission(')+r'''
 }
 void Check(){auto checks=std::move(frame->priorReadChecks);frame->priorReadChecks.clear();for(auto& [key,check]:checks)check();}
};
struct Cache {
 struct Image {
  std::weak_ptr<const NativePostprocessVersion> owner;
  std::shared_ptr<MockTexture> image=std::make_shared<MockTexture>();
  uint64_t producerSubmission=0;
  std::shared_ptr<bool> encoded=std::make_shared<bool>(false),completed=std::make_shared<bool>(false);
 };
 std::map<const NativePostprocessVersion*,Image> images;
 bool Retired(const NativePostprocessVersion& version)const {
'''+body(post,'bool Retired(const NativePostprocessVersion& version) const')+r'''
 }
 bool PreparationInput(const NativePostprocessVersion& version,RuntimeLease& lease)const {
'''+preparation+r'''
 }
};
template<class F>bool Throws(F f){try{f();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 // Completed/no-pending/missing/expired/wrong-owner/not-encoded/same-slot gates.
 for(int gate=0;gate<7;++gate){
  Cache c;RuntimeLease lease;auto owner=std::make_shared<const NativePostprocessVersion>();
  c.images[owner.get()].owner=owner;*c.images[owner.get()].encoded=true;
  if(gate==0)*c.images[owner.get()].completed=true;
  if(gate==1)lease.runtime.pendingSubmission.reset();
  if(gate==2)c.images.clear();
  if(gate==3)c.images[owner.get()].owner.reset();
  auto other=std::make_shared<const NativePostprocessVersion>();
  if(gate==4)c.images[owner.get()].owner=other;
  if(gate==5)*c.images[owner.get()].encoded=false;
  if(gate==6)lease.runtime.pendingSubmission->resources.owner->slot=lease.frame->slot;
  assert(c.PreparationInput(*owner,lease)==(gate==0));assert(lease.frame->priorReadChecks.empty());
 }
 // Captured version + exact flag survive cache eviction/replacement; no reread of cache.
 for(bool completed:{false,true}){
  Cache c;RuntimeLease lease;auto owner=std::make_shared<const NativePostprocessVersion>();
  std::weak_ptr<const NativePostprocessVersion> weak=owner;const auto* key=owner.get();
  auto& image=c.images[key];image.owner=owner;*image.encoded=true;auto exact=image.completed;
  assert(c.PreparationInput(*owner,lease));assert(c.PreparationInput(*owner,lease));
  assert(lease.frame->priorReadChecks.size()==1);assert(lease.frame->priorReadChecks.contains(key));
  c.images.clear();owner.reset();assert(!weak.expired());
  // Unrelated newer cache publication must not satisfy the older receipt.
  auto newer=std::make_shared<const NativePostprocessVersion>();c.images[newer.get()].owner=newer;
  *c.images[newer.get()].completed=true;*exact=completed;
  assert(Throws([&]{lease.Check();})==!completed);assert(weak.expired());
 }
 // Independent exact producers: completion of A never satisfies B.
 {Cache c;RuntimeLease lease;auto a=std::make_shared<const NativePostprocessVersion>(),b=std::make_shared<const NativePostprocessVersion>();
  for(const auto& p:{a,b}){auto& i=c.images[p.get()];i.owner=p;*i.encoded=true;assert(c.PreparationInput(*p,lease));}
  assert(lease.frame->priorReadChecks.size()==2);*c.images[a.get()].completed=true;
  assert(Throws([&]{lease.Check();}));
 }
 // Existing lease registration guards remain fail closed.
 for(int invalid=0;invalid<3;++invalid){Cache c;RuntimeLease lease;auto p=std::make_shared<const NativePostprocessVersion>();
  auto& i=c.images[p.get()];i.owner=p;*i.encoded=true;
  if(invalid==0)lease.frame->recording=true;if(invalid==1)lease.frame->pending=true;if(invalid==2)lease.runtime.poisoned=true;
  assert(Throws([&]{c.PreparationInput(*p,lease);}));assert(lease.frame->priorReadChecks.empty());
 }
 // A valid exact read keeps its own completion even after cache eviction.
 // Rejected ordering and incompatible storage retain the named CPU check.
 for(int scenario=0;scenario<4;++scenario){
  Cache c;RuntimeLease lease;auto p=std::make_shared<const NativePostprocessVersion>();
  auto& i=c.images[p.get()];i.owner=p;*i.encoded=true;i.producerSubmission=3;
  if(scenario==1)lease.rejectGPU=true;
  if(scenario==2)i.image->parentTexture=true;
  if(scenario==3)i.producerSubmission=4;
  auto exact=i.completed;
  assert(c.PreparationInput(*p,lease));
  assert(bool(lease.gpuCheck)==(scenario==0));
  if(scenario==0){
   assert(lease.frame->priorReadChecks.empty());
   c.images.clear();p.reset();assert(Throws(lease.gpuCheck));*exact=true;lease.gpuCheck();
  }else{
   assert(lease.frame->priorReadChecks.size()==1);
   assert(lease.frame->priorReadClasses==unsigned(NativePriorRead::Postprocess));
   *exact=true;lease.Check();
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='bar-post-pending-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
print('PASS: production-extracted exact and serialized postprocess receipts with ASan/UBSan; mocked textures, no Metal execution')
