#!/usr/bin/env python3
"""Real gate/worker/settlement + extracted source transaction boundary; no Metal execution."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'rts/Rendering/Backend/NativeMetalInfoCapture.cpp').read_text()
mutation=source[source.index('bool mutating=false;'):source.index('void Consume(State& candidate)')]
cpp=r'''
#include "Rendering/Backend/NativeRenderJobSettlement.h"
#include <cassert>
#include <iostream>
using namespace rendering;
std::function<void()> beforeControl;
void Check(bool b,const char* text){if(!b)throw std::logic_error(text);}
'''+mutation+r'''
template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){
 for(bool alreadyFinished:{false,true})for(unsigned i=0;i<100;++i){
  NativeSerialRenderWorker worker;NativeRendererAccessGate gate;
  std::atomic<bool> done=false;std::promise<void> release;auto wait=release.get_future();
  unsigned cpu=0,gpu=0,controls=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  NativeRenderJobSettlement<int> job(receipt,[]{},[&](const int* result,std::exception_ptr error){assert(result&&*result==7&&!error);++cpu;});
  {NativeRendererAccessGate::Scope owner(&gate);receipt->Reserve(gate);}
  job.Submit(worker,[&]{NativeRendererAccessGate::Scope owner(receipt->Handoff());wait.wait();done=true;return 7;});
  if(alreadyFinished){release.set_value();while(!done.load())std::this_thread::yield();}
  // Even finished worker code is not permission for main-thread retirement.
  Reject([&]{NativeRendererAccessGate::Scope owner(&gate);});
  beforeControl=[&]{assert(!mutating);if(!alreadyFinished)release.set_value();job.Finish();assert(cpu==1);NativeRendererAccessGate::Scope owner(&gate);++gpu;};
  PrepareControl(false);assert(cpu==0&&gpu==0); // Non-control capture does not Join.
  PrepareControl(true);
  {Mutation transaction;assert(cpu==1&&gpu==1);NativeRendererAccessGate::Scope owner(&gate);++controls;Reject([&]{PrepareControl(true);});}
  assert(controls==1&&job.Consume()==7);
 }
 beforeControl=[]{throw std::runtime_error("prior failure");};
 Reject([]{PrepareControl(true);});assert(!mutating);
 std::cout<<"PASS 200 pending/finished-before-Join controls; inactive overlap, pre-Mutation settlement, recursive rejection, prior failure propagation\n";
}
'''
# These assertions cover ObjC++ wiring only; the executable above exercises real portable owners.
assert 'PrepareControl(active);Mutation guard;' in source
assert 'PrepareControl(Current().recorder.Capture()->active);Mutation guard;' in source
bridge=(root/'rts/Rendering/Metal4/Metal4MetalInfoBridge.mm').read_text()
assert '}, [] {' in bridge and bridge.count('scene::FinishRuntimeControl();')==3
unit=(root/'rts/Rendering/Metal4/Metal4UnitUniformCopy.mm').read_text()
for body in [unit.split(' const override {',1)[1].split('};',1)[0],unit.split('CompileNativeUnitUniformCopy() {',1)[1]]:
 assert body.index('scene::FinishRuntimeControl();')<body.index('RuntimeLease lease')
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'void FinishRuntimeControl() { RequireRetiredGPUWork(); }' in scene
body=scene.split('void RequireRetiredGPUWork()',1)[1].split('} // namespace',1)[0]
assert body.index('FinishCPUWork()')<body.index('Metal4RendererCanSafelyDetachSurface()')<body.index('ApplyDeferredFrameReceipts()')
with tempfile.TemporaryDirectory(prefix='bar-runtime-control-') as out:
 p=Path(out);(p/'test.cpp').write_text(cpp)
 subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS source wiring assertions (not Objective-C++ compilation or Mac GPU execution)')
