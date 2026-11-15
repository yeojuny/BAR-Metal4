#!/usr/bin/env python3
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-critical-path-trace');out.mkdir(exist_ok=True)
runtime=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
a=runtime.index('// BEGIN NATIVE_CPU_PHASE_RUNTIME');b=runtime.index('// END NATIVE_CPU_PHASE_RUNTIME',a)
(out/'cpu-phase-runtime.inc').write_text(runtime[a:b])
subprocess.run(['clang++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-DRECOIL_METAL4_LIVE_SCENE','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestNativeCriticalPathTrace.cpp')),'-o',str(out/'test')],check=True)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','BAR_NATIVE_FRAME_TRACE_EVERY_LOOP':'1','BAR_NATIVE_FRAME_TRACE_START_SIM':'300','BAR_NATIVE_FRAME_TRACE_END_SIM':'306'}
for enabled in ['1','0']:
 for mode in ['success','worker','target','retirement','enqueue','tail']:
  subprocess.run([str(out/'test'),mode],env={**env,'BAR_NATIVE_FRAME_TRACE':enabled},check=True,timeout=30)
# Actual async feedback callback must remain free of worker-buffer access.
a=runtime.index('[options addFeedbackHandler:',runtime.index('void RuntimeLease::SubmitAndDefer'))
b=runtime.index('}];',a)
assert not any(x in runtime[a:b] for x in ['NativeCPUPhase','cpuPhaseStack','traceIdentity'])
world=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'traceOwner=impl->cpuTraceBuffer' in world
assert 'if(cpuJob&&cpuJob->WorkerCallableReleased())QueueJoinedNativeCPUTrace(cpuTraceBuffer)' in world
assert 'FlushNativeCPUPhaseBuffer' not in world
print('Production async callback single-writer and safe queue-after-Join wiring PASS')
