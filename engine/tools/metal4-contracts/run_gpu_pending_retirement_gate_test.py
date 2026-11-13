#!/usr/bin/env python3
"""Compile actual retirement + submit admission with mock runtime (not Metal execution)."""
import os
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
s = (root / 'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
def body(signature):
    start = s.index('{', s.index(signature)) + 1
    depth = 1
    for i in range(start, len(s)):
        depth += (s[i] == '{') - (s[i] == '}')
        if depth == 0:
            return s[start:i]
    raise AssertionError('unclosed production function')
retire = body('void RuntimeLease::RetirePreviousSubmission(std::shared_ptr')
admission = body('void RuntimeLease::SubmitAndDefer(').split(' Require(runtime.nextEventValue', 1)[0]
# Preserve CPU/recording overlap; only GPU ticket admission is constrained.
assert 'frameSlot->third=std::move(third)' in s
assert 'for(const auto& slot:{runtime.frameSlot,runtime.frameSlot->spare,runtime.frameSlot->third})' in s
assert 'independentQueue' not in admission
assert 'gpu_pending_limit=1' in s
source = r'''
#include <cassert>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <stdexcept>
#include <utility>
constexpr auto nil=nullptr;
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int StartPreparationScope(){return 0;}
void EndPreparationScope(int,int){}
bool two=false; bool InterventionTwoInFlight(){return two;}
struct RuntimePendingSubmission {};
struct Frame {bool recording=true,pending=false;std::function<void()> previousRetired;std::map<int,std::function<void()>> priorReadChecks;};
struct Runtime {
 std::vector<std::shared_ptr<RuntimePendingSubmission>> pendingSubmissions;
 std::shared_ptr<RuntimePendingSubmission> pendingSubmission;
 std::shared_ptr<Frame> failedFrame;
 bool poisoned=false,failDrain=false,unregisteredEncodedInput=false;
 std::vector<int> events;
 void DrainPendingLocked(std::shared_ptr<RuntimePendingSubmission>* owner){
  events.push_back(1);if(failDrain)throw std::runtime_error("feedback failure");
  *owner=pendingSubmission;pendingSubmission.reset();pendingSubmissions.clear();
  unregisteredEncodedInput=false;
 }
};
struct RuntimeLease {
 Runtime runtime;std::shared_ptr<Frame> frame=std::make_shared<Frame>();
 void FinishResourceCommit(){runtime.events.push_back(0);}
 void RetirePreviousSubmission(std::shared_ptr<RuntimePendingSubmission>* completedOwner){
''' + retire + r'''
 }
 void Submit(){
 auto retire=std::function<void()>([]{});void* drawable=this;
''' + admission + r'''
 // Model only the subsequent GPU enqueue, after the extracted admission.
 assert(!runtime.unregisteredEncodedInput);
 runtime.pendingSubmission=std::make_shared<RuntimePendingSubmission>();
 runtime.pendingSubmissions.push_back(runtime.pendingSubmission);runtime.events.push_back(4);
 }
};
int main(){
 int cases=0;
 for(bool option:{false,true}) for(int pending:{0,1,2}) for(bool checks:{false,true}){
  two=option;RuntimeLease lease;
  for(int i=0;i<pending;++i){lease.runtime.pendingSubmission=std::make_shared<RuntimePendingSubmission>();lease.runtime.pendingSubmissions.push_back(lease.runtime.pendingSubmission);}
  // The essential regression: this borrow is absent from priorReadChecks.
  lease.runtime.unregisteredEncodedInput=pending!=0;
  lease.frame->previousRetired=[&]{assert(!lease.runtime.pendingSubmission);lease.runtime.events.push_back(2);};
  if(checks)lease.frame->priorReadChecks[1]=[&]{assert(!lease.runtime.unregisteredEncodedInput);lease.runtime.events.push_back(3);};
  lease.Submit();assert(lease.runtime.pendingSubmissions.size()==1);
  assert(lease.runtime.events==(checks?std::vector<int>{0,1,2,3,4}:std::vector<int>{0,1,2,4}));
  assert(lease.frame->priorReadChecks.empty());++cases;
 }
 for(int failure:{0,1,2}){
  RuntimeLease lease;lease.runtime.pendingSubmission=std::make_shared<RuntimePendingSubmission>();
  lease.runtime.pendingSubmissions.push_back(lease.runtime.pendingSubmission);
  if(failure==0)lease.runtime.failDrain=true;
  if(failure==1)lease.frame->previousRetired=[] {throw std::runtime_error("receipt");};
  if(failure==2)lease.frame->priorReadChecks[1]=[]{throw std::runtime_error("read check");};
  bool threw=false;try{lease.Submit();}catch(const std::runtime_error&){threw=true;}
  assert(threw&&lease.runtime.poisoned&&lease.runtime.failedFrame==lease.frame);
  assert(lease.runtime.events.back()!=4);++cases;
 }
 assert(cases==15);
}
'''
with tempfile.TemporaryDirectory(prefix='bar-pending-retirement-') as d:
    p=Path(d);(p/'test.cpp').write_text(source)
    subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
print('PASS: 15 extracted production retirement/admission cases; ASan/UBSan; mock GPU/borrow lifecycle, no native execution')
