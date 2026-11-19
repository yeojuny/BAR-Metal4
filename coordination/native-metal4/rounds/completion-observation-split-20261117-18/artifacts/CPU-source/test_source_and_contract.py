#!/usr/bin/env python3
"""Source identity + CPU contract model, not a compiled Metal runtime test."""
from pathlib import Path
import hashlib,json,itertools,subprocess,tempfile
P=Path(__file__).resolve().parent
old=(P/'reference/Metal4RuntimeNative.mm').read_text();new=(P/'Metal4RuntimeNative.mm').read_text()
h=(P/'Metal4RuntimeNative.h').read_text();oldh=(P/'reference/Metal4RuntimeNative.h').read_text()
start=new.index('// Completion observation is not renderer retirement')
end=new.index('void Runtime::DrainOldestLocked(',start)
helpers=new[start:end]
wait_start=helpers.index('  const auto start=Clock::now();')
wait_end=helpers.index(' return CompletionObservation(',wait_start)
wait=helpers[wait_start:wait_end]
settle_start=helpers.index('  frame->pending=false;')
settle_end=helpers.index('\n}\n',settle_start)
settle=helpers[settle_start:settle_end]
stripped=new[:start]+new[end:]
stripped=stripped.replace('  auto observed=AwaitCompletionLocked(pending);\n  SettleCompletedLocked(pending,observed,completedOwner);',wait+settle)
assert stripped==old,'Any original statement outside the extraction changed'
added=''' struct CompletionObservation;
 CompletionObservation AwaitCompletionLocked(const std::shared_ptr<RuntimePendingSubmission>&);
 void SettleCompletedLocked(const std::shared_ptr<RuntimePendingSubmission>&,CompletionObservation&,
  std::shared_ptr<RuntimePendingSubmission>* completedOwner);
'''
assert h.replace(added,'')==oldh
assert 'observed.consumed=true;' in helpers and 'const auto& frame=' in helpers
assert wait.count('dispatch_semaphore_wait')==1
assert 'pending->retire' not in wait and 'allocator reset' not in wait
assert 'pendingSubmissions.pop_front' not in wait
# Compile actual receipt definition and actual Require predicate as portable C++.
receipt=helpers[helpers.index('struct Runtime::CompletionObservation'):helpers.index('\n\nRuntime::CompletionObservation')]
check=helpers[helpers.index(' Require(!observed.consumed'):helpers.index(' const auto& frame=')]
check=check.replace('(__bridge const void*)','(const void*)')
cpp='''#include <cstdint>
#include <memory>
#include <deque>
#include <stdexcept>
#include <cassert>
struct RuntimePendingSubmission {std::uint64_t eventValue=1;};
void Require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
struct Runtime {
 struct CompletionObservation;
 int q=0,e=0; const void* queue=&q; const void* completionEvent=&e;
 std::deque<std::shared_ptr<RuntimePendingSubmission>> pendingSubmissions;
 void Verify(const std::shared_ptr<RuntimePendingSubmission>& pending,CompletionObservation& observed);
};
'''+receipt+'''\nvoid Runtime::Verify(const std::shared_ptr<RuntimePendingSubmission>& pending,CompletionObservation& observed){\n'''+check+'''}
int main(){
 Runtime r,other;auto p=std::make_shared<RuntimePendingSubmission>();r.pendingSubmissions.push_back(p);
 for(int variant=0;variant<8;++variant){
  Runtime::CompletionObservation seen(variant==1?&other:&r,variant==2?nullptr:p.get(),variant==3?nullptr:r.queue,variant==4?nullptr:r.completionEvent,variant==5?2:1);
  if(variant==6)seen.consumed=true;
  if(variant==7)r.pendingSubmissions.clear();
  bool threw=false;try{r.Verify(p,seen);}catch(const std::runtime_error&){threw=true;}
  assert(threw==(variant!=0));
  if(variant==0){assert(seen.consumed);bool twice=false;try{r.Verify(p,seen);}catch(...){twice=true;}assert(twice);}
 }
}
'''
with tempfile.TemporaryDirectory() as td:
 f=Path(td)/'receipt.cpp';f.write_text(cpp);exe=Path(td)/'receipt'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(f),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
# Model old and split drain independently; compare actions and retained state.
steps=['event','feedback','gpu_error','allocator','callback','residency','residency_receipt','uniform_receipt']
def run(split,fail,keep,out):
 trace=[];state={'pending':True,'ticket':True,'poisoned':False,'unsafe':False,'handoff':False,'callbacks':0}
 def action(x):
  trace.append(x)
  if fail==x:raise RuntimeError(x)
 def wait():action('event');action('feedback')
 def settle():
  state['pending']=False;action('gpu_error');action('allocator');state['callbacks']+=1;action('callback')
  if not keep:action('residency')
  action('residency_receipt');action('uniform_receipt')
  if out:state['handoff']=True
  state['ticket']=False
 try:
  if split:wait();settle()
  else:
   action('event');action('feedback');state['pending']=False
   action('gpu_error');action('allocator');state['callbacks']+=1;action('callback')
   if not keep:action('residency')
   action('residency_receipt');action('uniform_receipt')
   if out:state['handoff']=True
   state['ticket']=False
 except RuntimeError:state['poisoned']=True;state['unsafe']=state['pending']
 return trace,state
n=0
for fail,keep,out in itertools.product([None]+steps,[False,True],[False,True]):
 before=run(False,fail,keep,out);after=run(True,fail,keep,out);assert before==after
 trace,state=after
 if fail in ('event','feedback'):assert state['pending'] and not state['callbacks'] and state['ticket'] and not state['handoff']
 if state['poisoned']:assert state['ticket'] and not state['handoff']
 n+=1
print(json.dumps({'status':'PASS','source_reconstruction':'all original runtime/header text preserved outside exact extraction','receipt_actual_cpp17_compile_and_identity_cases':9,'old_vs_split_failure_model_cases':n,'scope':'CPU only; production ObjC++/Metal compilation and live GPU/runtime not run'},indent=2))
