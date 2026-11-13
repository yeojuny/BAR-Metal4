#!/usr/bin/env python3
"""Actual portable backing/producer tests plus native descriptor wiring checks."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];b=root/'rts/Rendering/Backend'
out=Path('/tmp/bar-ui-capture-readiness');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
d=s.split('SetExternalImageDescriptorProvider([]',1)[1].split('impl->screenGPU.SetExternalImageProvider',1)[0]
assert '->Resource()' not in d and 'GetDeferredColorBackingImage' not in d and 'GetShieldDepthBackingImage' not in d
assert d.count('completed.load(std::memory_order_acquire)')==4 # includes already-safe PBR
assert '!height->Completed()' in d and d.count('NativeTextureContent::ProducerVersion&&!backing->Completed()')==2
assert '!version.producer->allocationOnly' in d
assert 'actual->completed=true;height->Fulfill' in s and 'actual->completed=true;heightBacking->Fulfill' in s
assert 'normalBacking->Resource())->completed=true' in s
for file in ['Metal4ShieldNative.mm','Metal4DeferredColorNative.mm']:
 assert 'Completed()const noexcept override{return image.producer&&image.producer->completed.load(std::memory_order_acquire);}' in (root/'rts/Rendering/Metal4'/file).read_text()
assert 'ShieldDepthProducerState{role,frame,identity}' not in (root/'rts/Rendering/Metal4/Metal4ShieldNative.mm').read_text()
files=['NativeShieldDepthBacking','NativeDeferredColorData','NativeMapTexturePublication']
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeUICaptureReadiness.cpp')),*[str(b/(f+'.cpp')) for f in files],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=40,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Native UI descriptor/height/retirement wiring checked, not executed; lookahead still disabled')
