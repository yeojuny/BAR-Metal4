#!/usr/bin/env python3
"""Link actual postprocess/cloud sources; no recorder algorithm stubs."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2]
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
completion=bridge.split('void ApplyDeferredFrameReceipts()',1)[1].split('void RequireRetiredGPUWork()',1)[0]
assert completion.index('LuaNativeVolumetricClouds::ValidateHeldFrame') < completion.index('CompleteDynamicCapture(')
water_completion=bridge.split('void CompleteDynamicCapture(',1)[1].split('bool AbortDynamicCapture()',1)[0]
assert water_completion.index('session->dynamicWaterCapture.DidComplete(generation,through,draw)') < water_completion.index('session->dynamicCaptureReceipt.reset()')
assert 'receipt->material->generation==generation' in water_completion and 'receipt->sourceDrawFrame==draw' in water_completion and 'receipt->throughUpdate==through' in water_completion
submit=bridge.split('const auto expectedDraw=capturedFrame.sourceDrawFrame',1)[1]
assert submit.index('LuaNativeVolumetricClouds::HasSubmittedCapacity') < submit.index('RequireRetiredGPUWork()') < submit.index('admission.Validate()')
assert submit.index('admission.Validate()') < submit.index('mailbox->Prepare') < submit.index('session->renderer->Draw')
files=[Path(__file__).with_name('TestNativeFrameJournalOwners.cpp')]+[root/'rts/Rendering/Backend'/name for name in ['NativePostprocessRecorder.cpp','NativeVolumetricCloudContext.cpp','NativePostprocessData.cpp','NativeCUSUniformData.cpp','ScreenDrawList.cpp']]
command=['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-missing-field-initializers','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I',str(root/'rts'),*[str(f) for f in files],'-o',str(out/'test')]
r=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(r.stdout+r.stderr)
hashfiles=files+[root/'rts/Rendering/Backend'/name for name in ['NativeCompletedFrameLedger.h','NativePostprocessRecorder.h','NativeVolumetricCloudContext.h']]
receipt={'scope':__doc__,'command':command,'sourceSHA256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in hashfiles},'passed':r.returncode==0,'exit':r.returncode,'output':r.stdout,'limits':['No Metal execution or async enabled; UI runtime linkage not included.','ASan/UBSan enabled; LeakSanitizer disabled in sandbox.']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout+r.stderr,end='');raise SystemExit(r.returncode)
