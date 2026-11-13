#!/usr/bin/env python3
"""Production CPU ledger only; no GPU or engine-capture execution."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-dynamic-water-receipts');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeDynamicWaterReceipts.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
capture=(root/'rts/Rendering/Backend/NativeDynamicWaterCapture.cpp').read_text()
assert 'if(impl->receipts.Any())throw' in capture
assert 'impl->receipts.ValidateReset();impl=std::make_unique<Impl>()' in capture
assert 'impl->firstDraw=false;impl->receipts.Complete(sourceDrawFrame)' in capture
assert 'impl->receipts.SetFinalized(source,out)' in capture
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'cloudAdmission,waterAdmission,' in scene
assert '(session && session->dynamicWaterCapture.Initialized()))RequireRetiredGPUWork()' in scene
apply=scene.split('void ApplyDeferredFrameReceipts()',1)[1].split('void RequireRetiredGPUWork()',1)[0]
assert 'result.dynamicWaterSourceDrawFrame!=result.sourceDrawFrame' in apply
assert apply.index('dynamicWaterCapture.ValidateHeldFrame')<apply.index('dynamicWaterCapture.ValidateCompletion')
assert apply.index('dynamicWaterCapture.ValidateCompletion')<apply.index('dynamicWaterCapture.DidComplete')
print('Actual capture/admission/completion wiring structural PASS; Update/newdraw/firstDraw/drain semantics remain gated')
