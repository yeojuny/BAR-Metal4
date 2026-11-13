#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-early-cpuread220');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
(out/'receipt.inc').write_text(s[s.index('struct CPUReadWaterRoster220 {'):s.index('void TraceCPUReadPacket220(')])
(out/'launch.inc').write_text(s.split('// BEGIN EARLY_CPU_READ_LAUNCH220',1)[1].split('// END EARLY_CPU_READ_LAUNCH220',1)[0])
(out/'tail.inc').write_text(s.split('// BEGIN CPU_READ_TAIL220',1)[1].split('// END CPU_READ_TAIL220',1)[0])
sources=['NativeCUSRenderInputs.cpp','NativeCUSSourceData.cpp','NativeCUSModelInputs.cpp','NativeCUSPublishedData.cpp','NativeCUSUniformData.cpp','NativeCUSTextureHistory.cpp']
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestEarlyCPUReadLaunch220.cpp')),*[str(root/'rts/Rendering/Backend'/f) for f in sources],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
b=s.split('void PrepareWorld(',1)[1].split('void AbortPreparedWorld()',1)[0]
assert 'earlyCPURead220=deferredParticles&&CPUReadPacketMode220()>=2' in b
callback=b.split('CaptureAtNativeSurfaceBoundary(',1)[1].split('if(deferredParticles)session->capture.CompleteDeferredMainParticles',1)[0]
assert callback.index('capture.Capture(')<callback.index('if(earlyCPURead220)launchCPURead220(')
assert 'cpuReadReceipt220.reset()' not in callback
assert b.index('std::unique_ptr<CPUReadReceipt220> cpuReceipt220;')<b.index('NativeCUSCPUReadWorker220::Join cpuJoin220;')<b.index('CaptureAtNativeSurfaceBoundary(')
assert b.index('CompleteDeferredMainParticles(')<b.index('session->cpuReadReceipt220.reset();')<b.index('if(!earlyCPURead220)launchCPURead220(frame);')<b.index('mainBeginNs=')
assert b.index('cpuJoin220.Finish();')<b.index('session->cpuReadReceipt220=std::move(cpuReceipt220);')
print('Production early mode2/3 wiring, delayed session reset, main-local Join lifetime and post-drain main timing PASS')

assert 'if(earlyCPURead220&&cpuReceipt220)cpuJoin220.TransferTo(cpuReceipt220->pending);' in b
receipt=s.split('struct CPUReadReceipt220 {',1)[1].split('void TraceCPUReadPacket220',1)[0]
assert receipt.index('::Job job;')<receipt.index('::Join pending;')
draw=s.split('DrawResult DrawWorld(',1)[1]
assert draw.index('receipt->pending.Finish()')<draw.index('receipt->Matches(captured)')<draw.index('RowDigest()')
abort=s.split('void AbortPreparedWorld()',1)[1]
assert abort.index('FinishCPUReadPreparation()')<abort.index('CanAbortCapture(')
game=(root/'rts/Game/Game.cpp').read_text()
catch=game.split('rendering::scene::FinishCPUReadPreparation();',1)[1]
assert catch.lstrip().startswith('LuaNativePostprocess::AbortFrame();')
assert 'cpuReadReceipt220.reset();cpuReadWorker220.reset();DisconnectNativeSources();' in s
print('Transferred receipt join precedes consumption, source abort, hidden reset and session worker destruction PASS')

assert b.index('job.PublishTail(')<b.index('cpuJoin220.TransferTo(')
assert 'job.ExpectTail()' in b
assert 'if(!tailAccepted)primeTail();' in draw
assert 'if(tailAccepted)primeTail();' in draw
assert 'tail_wait_ns' in s
print('Single-scope opaque/alpha/shadow continuation source order and active/wait metrics wiring PASS')

water=s.split('struct CPUReadWaterRoster220 {',1)[1].split('struct CPUReadReceipt220 {',1)[0]
assert 'out.views={out.owner->refraction,out.owner->reflection}' in water
assert 'append(view->frame.alphaModelFrames[0])' in water
assert 'if(job.optionalFailed)return false;' in receipt
assert 'job.PublishTail(std::move(tail),std::move(optionalWater))' in b
assert draw.index('receipt->pending.Finish()')<draw.index('receipt->Matches(captured)')
assert 'optional_sources,optional_failed' in s
print('Optional water model roster, actual refraction/reflection order, alpha0 only and failure rejection wiring PASS')
