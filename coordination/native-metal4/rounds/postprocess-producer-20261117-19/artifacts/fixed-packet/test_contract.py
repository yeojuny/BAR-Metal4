#!/usr/bin/env python3
"""Compile actual portable predicate + extracted production read/registration methods.
Metal Objective-C++ integration and GPU ordering are deliberately NOT covered.
"""
from pathlib import Path
import subprocess,tempfile,json,hashlib
P=Path(__file__).resolve().parent
S=P if (P/'Metal4PostprocessNative.mm').exists() else P.parents[3]/'engine/rts/Rendering/Metal4'
s=(S/'Metal4PostprocessNative.mm').read_text()
a=s.index(' bool PreparationInput(');b=s.index(' struct PreparationContext',a)
methods=s[a:b].replace('(__bridge const void*)','(const void*)')
r=(S/'Metal4RuntimeNative.mm').read_text()
a=r.index('bool RuntimeLease::OrderAfterPreviousSubmission(');b=r.index('void RuntimeLease::RetirePreviousSubmission()',a)
registration=r[a:b]
cpp=r'''
#include "NativePostprocessReadContract.h"
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
using namespace rendering::metal4;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct NativePostprocessVersion {bool image=true,cpu=false;};
bool HasNativePostprocessCPUContents(const NativePostprocessVersion& v){return v.cpu;}
struct Frame {bool recording=false,pending=false;void* slot=this; std::function<void()> previousRetired;
 std::map<const void*,std::function<void()>> priorReadChecks;};
struct Pending {struct {std::uint64_t submission=1;} traceIdentity; struct {std::shared_ptr<Frame> owner=std::make_shared<Frame>();} resources;};
struct Runtime {bool poisoned=false;std::shared_ptr<Pending> pendingSubmission=std::make_shared<Pending>();};
struct RuntimeLease {
 Runtime runtime;std::shared_ptr<Frame> frame=std::make_shared<Frame>();std::uint64_t source=2;
 std::shared_ptr<int> queue=std::make_shared<int>(0);int drains=0;
 RuntimeLease(){frame->previousRetired=[]{};}
 bool PreparingWhilePending()const{return bool(frame->previousRetired)&&bool(runtime.pendingSubmission);}
 bool OrderAfterPreviousSubmission(const void*,std::function<void()>);
 bool OrderAfterSourceSubmission(const void*,std::uint64_t,std::function<void()>);
 const void* ContextIdentity(){return &runtime;}
 const void* QueueIdentity(){return queue.get();}
 std::uint64_t SourceSubmission(){return source;}
 void RetirePreviousSubmission(){++drains;runtime.pendingSubmission.reset();if(frame->previousRetired){auto f=std::exchange(frame->previousRetired,{});f();}auto checks=std::move(frame->priorReadChecks);frame->priorReadChecks.clear();for(auto& [key,check]:checks)check();}
};
'''+registration+r'''
struct Cache {
 struct Image {std::weak_ptr<const NativePostprocessVersion> owner;
 std::shared_ptr<NativePostprocessProducerIdentity> producer=std::make_shared<NativePostprocessProducerIdentity>();
 std::shared_ptr<int> producerQueue;
 std::shared_ptr<bool> completed=std::make_shared<bool>(false),encoded=std::make_shared<bool>(false);};
 std::map<const NativePostprocessVersion*,Image> images;
 bool Retired(const NativePostprocessVersion& v)const{auto i=images.find(&v);return i!=images.end()&&!i->second.owner.expired()&&*i->second.completed;}
'''+methods+r'''
};
template<class F> bool throws(F f){try{f();}catch(const std::runtime_error&){return true;}return false;}
struct Fixture {
 RuntimeLease lease;Cache cache;std::shared_ptr<const NativePostprocessVersion> v=std::make_shared<NativePostprocessVersion>();
 Fixture(){auto& e=cache.images[v.get()];e.owner=v;e.producerQueue=lease.queue;*e.producer={lease.ContextIdentity(),lease.QueueIdentity(),1};*e.encoded=true;
 lease.frame->previousRetired=[completed=e.completed]{*completed=true;};}
 Cache::Image& entry(){return cache.images.at(v.get());}
};
int main(){
 int context,other,queue,q2;unsigned matrix=0;
 for(bool sameContext:{false,true})for(bool sameQueue:{false,true})for(bool sameSubmission:{false,true})
 for(bool encoded:{false,true})for(bool completed:{false,true})for(bool ordered:{false,true}){
 NativePostprocessProducerIdentity p{&context,&queue,1},c{sameContext?&context:&other,sameQueue?&queue:&q2,sameSubmission?1u:2u};
 bool expected=completed||(encoded&&sameContext&&sameQueue&&(sameSubmission||ordered));
 assert(NativePostprocessReadAllowed(p,c,encoded,completed,ordered)==expected);++matrix;}
 assert(matrix==64);
 for(int invalid=0;invalid<6;++invalid){NativePostprocessProducerIdentity p{&context,&queue,1},c=p;auto& bad=invalid<3?p:c;switch(invalid%3){case 0:bad.context=nullptr;break;case 1:bad.queue=nullptr;break;case 2:bad.submission=0;}
 assert(!NativePostprocessReadAllowed(p,c,true,true,true));}
 // Exact predecessor allows preparation; actual completed check still gates submit.
 {Fixture f;auto read=f.cache.CaptureRead(f.lease,f.v);assert(f.lease.frame->priorReadChecks.size()==1);assert(!throws(read));
 auto check=f.lease.frame->priorReadChecks.begin()->second;assert(throws(check));f.lease.RetirePreviousSubmission();assert(!throws(check));assert(!throws(read));}
 // Identity mutation invalidates both receipts even if bool completion is published.
 {Fixture f;auto read=f.cache.CaptureRead(f.lease,f.v);auto check=f.lease.frame->priorReadChecks.begin()->second;f.entry().producer->submission=9;*f.entry().completed=true;assert(throws(read));assert(throws(check));}
 // Receipt keeps exact version and queue alive after cache and caller release them.
 {Fixture f;auto read=f.cache.CaptureRead(f.lease,f.v);std::weak_ptr<const NativePostprocessVersion> weak=f.v;std::weak_ptr<int> q=f.lease.queue;
 f.cache.images.clear();f.v.reset();f.lease.queue.reset();assert(!weak.expired()&&!q.expired());read();f.lease.RetirePreviousSubmission();read();read={};assert(weak.expired()&&q.expired());}
 // Same-submission producer must already be encoded at actual read, not capture.
 {Fixture f;f.entry().producer->submission=2;*f.entry().encoded=false;auto read=f.cache.CaptureRead(f.lease,f.v);assert(f.lease.frame->priorReadChecks.empty());assert(throws(read));*f.entry().encoded=true;read();}
 // Wrong producer id or queue takes drain fallback, never registers a false predecessor.
 for(int kind=0;kind<2;++kind){Fixture f;if(kind==0)f.entry().producer->submission=7;else f.entry().producer->queue=&q2;
 auto read=f.cache.CaptureRead(f.lease,f.v);assert(f.lease.drains==1&&f.lease.frame->priorReadChecks.empty());read();}
 // Draining unrelated work does not magically complete a producer.
 {Fixture f;f.entry().producer->submission=7;f.lease.frame->previousRetired=[]{};auto read=f.cache.CaptureRead(f.lease,f.v);assert(f.lease.drains==1);assert(throws(read));}
 {Fixture f;f.lease.runtime.pendingSubmission.reset();auto read=f.cache.CaptureRead(f.lease,f.v);assert(throws(read));}
 {Fixture f;*f.entry().completed=true;f.entry().producer->queue=&q2;auto read=f.cache.CaptureRead(f.lease,f.v);read();assert(f.lease.drains==0);}
 {Fixture f;f.entry().producer->submission=0;assert(throws([&]{f.cache.CaptureRead(f.lease,f.v);}));}
 {Fixture f;auto cpu=std::make_shared<NativePostprocessVersion>();cpu->cpu=true;auto read=f.cache.CaptureRead(f.lease,cpu);cpu.reset();read();}
 {Fixture f;f.cache.images.clear();assert(throws([&]{f.cache.CaptureRead(f.lease,f.v);}));}
 // Actual runtime registration rejects wrong/missing producer and same-slot work.
 {RuntimeLease l;int owner;assert(!l.OrderAfterSourceSubmission(&owner,0,[]{}));assert(!l.OrderAfterSourceSubmission(&owner,8,[]{}));
 assert(l.OrderAfterSourceSubmission(&owner,1,[]{}));assert(l.frame->priorReadChecks.size()==1);
 l.frame->slot=l.runtime.pendingSubmission->resources.owner->slot;assert(!l.OrderAfterSourceSubmission(&owner,1,[]{}));
 l.runtime.pendingSubmission.reset();assert(!l.OrderAfterSourceSubmission(&owner,1,[]{}));}
 {RuntimeLease l;int owner;l.frame->recording=true;assert(throws([&]{l.OrderAfterSourceSubmission(&owner,1,[]{});}));}
}
'''
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'test.cpp';exe=Path(td)/'test';src.write_text(cpp)
 subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(S),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
# Wiring audit (not an integration test): all existing external read sites accept the consuming lease.
w=(S/'Metal4WorldRenderer.mm').read_text()
assert w.count('.CaptureReadability(')==3
assert '.CaptureReadability(version)' not in w
assert 'out.previous.emplace_back(previous,image);out.reads.push_back(impl->CaptureRead(lease,operation.previous[i]));' in s
assert 'out.reads.emplace_back(entry.completed' not in s
# Runtime safety/retirement/submit unchanged outside the single exact-id registration method.
old=(P/'reference/Metal4RuntimeNative.mm').read_text()
a=r.index('bool RuntimeLease::OrderAfterSourceSubmission(');b=r.index('void RuntimeLease::RetirePreviousSubmission()',a)
assert r[:a]+r[b:]==old
result={'status':'PASS','predicate_matrix':64,'invalid_identity_cases':6,'compiled_scenarios':14,'source_wiring':'PASS','runtime_nonregistration_source_identity':'PASS','gpu_validated':False}
(P/'cpu-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
