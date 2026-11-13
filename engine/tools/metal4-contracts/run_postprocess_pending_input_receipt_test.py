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
source=r'''
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
void Require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
struct NativePostprocessVersion {};
struct Frame {bool recording=false,pending=false;int slot=1;bool previousRetired=true;std::map<const void*,std::function<void()>> priorReadChecks;};
struct Pending {struct {std::shared_ptr<Frame> owner=std::make_shared<Frame>();} resources;};
struct Runtime {bool poisoned=false;std::shared_ptr<Pending> pendingSubmission=std::make_shared<Pending>();};
struct RuntimeLease {
 std::shared_ptr<Frame> frame=std::make_shared<Frame>();Runtime runtime;
 RuntimeLease(){runtime.pendingSubmission->resources.owner->slot=2;}
 bool PreparingWhilePending()const {return bool(frame->previousRetired)&&bool(runtime.pendingSubmission);}
 bool OrderAfterPreviousSubmission(const void* owner,std::function<void()> check){
'''+body(runtime,'bool RuntimeLease::OrderAfterPreviousSubmission(')+r'''
 }
 void Check(){auto checks=std::move(frame->priorReadChecks);frame->priorReadChecks.clear();for(auto& [key,check]:checks)check();}
};
struct Cache {
 struct Image {std::weak_ptr<const NativePostprocessVersion> owner;std::shared_ptr<bool> encoded=std::make_shared<bool>(false),completed=std::make_shared<bool>(false);};
 std::map<const NativePostprocessVersion*,Image> images;
 bool Retired(const NativePostprocessVersion& version)const {
'''+body(post,'bool Retired(const NativePostprocessVersion& version) const')+r'''
 }
 bool PreparationInput(const NativePostprocessVersion& version,RuntimeLease& lease)const {
'''+body(post,'bool PreparationInput(const NativePostprocessVersion& version,RuntimeLease& lease) const')+r'''
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
}
'''
with tempfile.TemporaryDirectory(prefix='bar-post-pending-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
print('PASS: 13 production-extracted pending-input/registration scenarios with ASan/UBSan; mocked native resources, no Metal execution')
