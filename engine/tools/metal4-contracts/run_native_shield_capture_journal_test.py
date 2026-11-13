#!/usr/bin/env python3
"""Actual journal/context/registry CPU test, no renderer or GPU execution."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];b=root/'rts/Rendering/Backend';out=Path('/tmp/bar-shield-capture-journal');out.mkdir(exist_ok=True)
files=['NativeShieldDepthPublication','NativeShieldDepthBacking','NativeModelDepthPlan','NativeTextureContext','NativeTextureRegistry','NativeCUSTextureHistory','NativeUITextureHistory','NativeUIRecorder','NativePostprocessData']
cmd=['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DRECOIL_USE_SDL3']
cmd += ['-I'+str(p) for p in [root/'rts',root/'rts/lib',root/'rts/lib/tracy/public',root/'rts/lib/nowide/include',root/'rts/lib/lua/include',root/'rts/System/SDL3Compat',root.parent.parent/'bar-validation-sdl3/include']]
subprocess.run(cmd+[str(Path(__file__).with_name('TestNativeShieldCaptureJournal.cpp'))]+[str(b/(f+'.cpp')) for f in files]+['-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'shieldPending' not in s and 'shieldRegistry' not in s
abort=s.split('bool Metal4WorldRenderer::AbortShieldPublications',1)[1].split('bool Metal4WorldRenderer::HasScreenContinuation',1)[0]
assert abort.index('activeSourceSubmission)return false')<abort.index('shield->CanCancel()')<abort.index('CancelNativeOpaqueModels')<abort.index('shield->Cancel()')
assert s.index('originalLifetime->shieldSource=frame.shieldDepthFrame')<s.index('lease.Retain(originalLifetime)')
assert 'depthFrame.sealed&&depthFrame.submissionIdentity!=0' in s
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'NativeShieldCaptureJournal shieldCapture' in bridge
print('Source-owned journal/failed-Begin veto/sealed worker receipt/pre-Begin GPU pin structural PASS')
settle=bridge.split('const auto result=invokeRenderer();',1)[1].split('if(result.dynamicWaterGeneration)',1)[0]
assert settle.index('restoreAdmitted();')<settle.index('pendingAdmission->Accept(result)')
assert 'if(admitted && (!result.presented || result.sourceFrameCompleted))' in settle
fallback=bridge.split('if (!result.presented) {',1)[1]
assert fallback.index('AbortShieldPublications')<fallback.index('shieldCapture.Reset()')
print('Hidden/no-Begin restore precedes acceptance; fallback cancellation retains journal structural PASS')
assert s.index('lease.Retain(originalLifetime)')<s.index('lease.Begin("Actual engine')
assert abort.index('if(![NSThread isMainThread])return false')<abort.index('shield->CanCancel()')<abort.index('shield->Cancel()')
print('Actual main-thread abort gate and retention before actual Begin structural PASS')

record=bridge.split('struct ScenePendingAdmission {',1)[1].split('void CompleteDynamicCapture',1)[0]
assert record.index('make_shared<NativeMainSourceSettlement>')<record.index('source->Take')<record.index('owner->sourceSettlement=source')<record.index('admission.TransferValidated')
restore=record.split(' void Restore(){',1)[1].split(' void Quarantine()',1)[0]
assert restore.index('ValidateRestore')<restore.index('Transaction().Restore()')<restore.index('RestoreValidated')<restore.index('sourceSettlement.reset()')
quarantine=record.split(' void Quarantine()',1)[1].split(' void Fail()',1)[0]
assert 'sourceSettlement.reset' not in quarantine
accept=record.split(' void Accept(const Metal4WorldPresentationResult& result){',1)[1]
assert accept.index('mailbox->Accept(ticket)')<accept.index('sourceSettlement.reset()')
assert 'owner->admissionSettlement.get()!=this' in record
print('100 exact main source owner detach/restore + occupied destination/epoch rejection; durable exact admission quarantine wiring PASS')
