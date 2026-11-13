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
assert apply.index('dynamicWaterCapture.ValidateCompletion')<apply.index('CompleteDynamicCapture(')
print('Actual capture/admission/completion wiring structural PASS; Update/newdraw/firstDraw/drain semantics remain gated')

prepare=scene.split('void PrepareWorld(',1)[1].split('void AbortPreparedWorld()',1)[0]
assert prepare.index('dynamicCaptureReceipt=frame.dynamicWater')<prepare.index('dynamicWaterCapture.FinalizeSurface')
abort=scene.split('void AbortPreparedWorld()',1)[1].split('namespace {',1)[0]
assert abort.index('CanAbortCapture')<abort.index('AbortShieldPublications')<abort.index('AbortDynamicCapture()')<abort.index('session->preparedFrame.reset()')
helper=scene.split('bool AbortDynamicCapture()',1)[1].split('void ApplyDeferredFrameReceipts()',1)[0]
assert helper.index('AbortCapture(session->dynamicCaptureReceipt)')<helper.index('session->dynamicCaptureReceipt.reset()')
print('Exact dynamic session receipt retention/preflight/abort wiring structural PASS')
wrappers=capture.split('bool NativeDynamicWaterCapture::CanAbortCapture',1)[1].split('void NativeDynamicWaterCapture::ValidateSubmittedCapacity',1)[0]
assert 'receipts.CanAbort' in wrappers and 'receipts.Abort' in wrappers
assert all(field not in wrappers for field in ['pending','sequence','retired','firstDraw'])
draw=scene.split('DrawResult DrawWorld(',1)[1].split('void ResetWorld()',1)[0]
assert 'std::move(*prepared)' in draw and 'dynamicCaptureReceipt.reset()' not in draw
complete=scene.split('void CompleteDynamicCapture(',1)[1].split('bool AbortDynamicCapture()',1)[0]
assert complete.index('dynamicWaterCapture.DidComplete')<complete.index('dynamicCaptureReceipt.reset()')
assert all(test in complete for test in ['generation==generation','sourceDrawFrame==draw','throughUpdate==through'])
print('Abort preserves pending impulses/bootstrap; token survives frame move; completion clears exact tuple structural PASS')
