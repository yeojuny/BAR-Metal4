#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-begin-ready-pilot');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'frame.opaqueModelFrame&&!options.readback&&options.presentWorld&&!options.allowBeginReady' in s
assert 'if(beginReady&&impl->cpuJob->ReleaseAfterBegin())' in s
assert 'impl->detachedCPUJob=true;' in s and 'queued.queued=true;' in s
assert 'if(detachedCPUJob){(void)cpuJob->Consume();cpuJob.reset();detachedCPUJob=false;}' in s
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'if(options.allowBeginReady)RequireRetiredGPUWork();' in scene
assert '(overlapPreparation&&!options.allowBeginReady)?' in scene
assert 'return value&&std::string_view(value)=="1";' in scene.split('BAR_NATIVE_BEGIN_READY_PILOT',1)[1]
queued=scene.split(' if(result.queued){',1)[1].split(' if(pendingAdmission',1)[0]
assert 'sourceFrameCompleted' in queued and 'sourceDrawFrame!=frame.sourceDrawFrame' in queued
assert 'return {false,false,false,0,frame.sourceDrawFrame,true,screenUI->postprocess->serial}' in queued
assert '.reset()' not in queued
subprocess.run(['g++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeBeginReadyPilot.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Actual opt-in encoding-tail pilot wiring; prior GPU drain, no previousRetired, MRT209 disabled, exact queued result and final consume')
