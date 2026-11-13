#!/usr/bin/env python3
"""Compile the production CPU trace runtime block with its real joined worker."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-cpu-phase-handoff');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
a=s.index('// BEGIN NATIVE_CPU_PHASE_RUNTIME');b=s.index('// END NATIVE_CPU_PHASE_RUNTIME',a)
block=s[a:b]
(out/'cpu-phase-runtime.inc').write_text(block)
args=['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-DRECOIL_METAL4_LIVE_SCENE','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestNativeCPUPhaseHandoff.cpp')),'-o',str(out/'test')]
subprocess.run(args,check=True)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','BAR_NATIVE_FRAME_TRACE':'1'}
for every in ['0','1']:
 subprocess.run([str(out/'test')],env={**env,'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP':every},check=True,timeout=30)
subprocess.run([str(out/'test')],env={**env,'BAR_NATIVE_FRAME_TRACE':'0'},check=True,timeout=30)
world=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
a=world.index('const auto cpuTraceContext=CaptureNativeCPUPhaseContext();')
b=world.index('impl->cpuJob->Submit(worker,[this,owned,presentation,snapshot,sourceContext,cpuTraceContext,',a)
c=world.index('NativeCPUPhaseContextScope traceContext(cpuTraceContext);',b)
assert world.index('return DrawPrepared(owned->Frame()',c)>c
# Leave sampling installed but reproduce the previous missing ancestry rule.
(out/'cpu-phase-runtime.inc').write_text(block.replace('sample.parent=stack.depth?stack.slots[stack.depth-1].sample.id:stack.inheritedParent;','sample.parent=stack.depth?stack.slots[stack.depth-1].sample.id:0;'))
subprocess.run(args,check=True)
r=subprocess.run([str(out/'test')],env=env,capture_output=True,text=True,timeout=30)
assert r.returncode!=0 and 'parents[0]' in r.stderr, r.stderr
print('Previous missing worker ancestry rejected as expected')
(out/'cpu-phase-runtime.inc').write_text(block)

renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=renderer.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('return DrawPrepared(frame,options,')<dispatch.index('CaptureNativeCPUPhaseContext()')
assert 'snapshot,sourceContext,cpuTraceContext,' in dispatch
assert dispatch.index('NativeCPUPhaseContextScope traceContext(cpuTraceContext)') < dispatch.index('return DrawPrepared(owned->Frame()')
print('Worker value-only trace handoff; direct path unchanged structural PASS')

assert 'cpuPhase.Draw(frame.sourceDrawFrame)' in renderer
assert 'prepPhase->Draw(frame.sourceDrawFrame)' in renderer
start=renderer.index('NativeCPUPhaseScope encodeWorkTrace(6,frame.simulationFrame)')
end=renderer.index('encodeWorkTrace.Mark(0);encodeWorkTrace.Finish();',start)
assert renderer.index('id<MTL4CommandBuffer> command = lease.Begin(')<start
assert renderer.index('if(options.submissionBegan)options.submissionBegan(sourceSubmission,')<start
assert end<renderer.index('lease.FinishResourceCommit();',end)
game=(root/'rts/Game/Game.cpp').read_text()
a=game.index('auto nativeFrameCredit=rendering::scene::AcquireWorldFrameCredit')
b=game.index('NativeCPUPhaseScope nativeCaptureTrace(5,gs->frameNum)',a)
c=game.index('nativeCaptureTrace.Mark(0);nativeCaptureTrace.Finish();',b)
assert a<b<game.index('nativeUI.BeginFrame',b)<c<game.index('rendering::scene::DrawWorld(&screenFrame',c)
print('Exact source draw + recording/encode wall intervals exclude outer credit/dispatch/runtime waits PASS')
