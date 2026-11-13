#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-early-cpuread220');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
(out/'receipt.inc').write_text(s[s.index('struct CPUReadReceipt220 {'):s.index('void TraceCPUReadPacket220(')])
(out/'launch.inc').write_text(s.split('// BEGIN EARLY_CPU_READ_LAUNCH220',1)[1].split('// END EARLY_CPU_READ_LAUNCH220',1)[0])
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
