#!/usr/bin/env python3
"""Exercise production credit/worker/dispatch/mailbox helpers without Metal execution."""
import json
import os
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[2]
here=Path(__file__).parent
out=Path(__file__).resolve().parents[4]/'host-tests'
out.mkdir(exist_ok=True)
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
capability=(root/'rts/Rendering/Metal4/Metal4SceneBeginRetirement.h').read_text()
credits=(root/'rts/Rendering/Backend/NativeSourceFrameCredits.h').read_text()
assert 'AcquireWithCPUReclaim(draw,BeginPreparationOverlapRequested()' in scene
assert 'return enabled&&worker&&BeginReadyPilotRequested();' in scene
assert 'session->renderer->FinishCPUWork();' in scene
assert 'ApplyDeferredFrameReceipts();reclaimTrace.Mark(1);' in scene
assert 'if(options.allowBeginReady&&!options.beginPreparationRetirement)RequireRetiredGPUWork();' in scene
assert 'options.beginPreparationRetirement=MakeBeginPreparationRetirement' in scene
factory=scene.split('std::shared_ptr<const Metal4SceneBeginRetirement> MakeBeginPreparationRetirement(',1)[1].split('void FinishCPUReadPreparation()',1)[0]
assert 'session->sourceCredits.Validate(credit,credit->Draw())' in factory
assert 'session->completionMailbox!=mailbox' in factory and 'mailbox->Epoch()!=epoch' in factory
assert 'session.get()!=owner' in factory
assert 'ContextGeneration()' in factory and 'context' in factory
assert factory.index('ContextGeneration()')<factory.index('ApplyDeferredFrameReceipts();')
assert 'friend std::shared_ptr<const Metal4SceneBeginRetirement> scene::MakeBeginPreparationRetirement(' in capability
assert 'credit!=expectedCredit||begin!=expectedBegin' in capability
assert 'if(sceneRetirement)' in renderer and 'options.beginPreparationRetirement->Validate(options.sourceCredit,options.beginReceipt)' in renderer
assert 'Require(beginReady&&[NSThread isMainThread]' in renderer
assert 'options.beginReceipt->Matches(frame.sourceDrawFrame' in renderer
assert 'previousRetired=[request=options.beginPreparationRetirement]{request->DeliverAfterJoin();};' in renderer
assert 'if(beginReady&&!sceneRetirement)Require(SharedRuntime().CanSafelyDetach()' in renderer
assert 'if(!HasCapacity())drain();' in credits and 'if(enabled&&!HasCapacity())settle();' in credits

binary=out/'test-native-begin-cpu-reclaim'
build=subprocess.run(['g++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined',
 '-I'+str(root/'rts'),str(here/'TestNativeBeginCPUReclaim.cpp'),'-o',str(binary)],capture_output=True,text=True)
(out/'begin-cpu-reclaim-build.log').write_text(build.stdout+build.stderr)
if build.returncode:raise SystemExit(build.returncode)
run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=60,
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
(out/'begin-cpu-reclaim-run.log').write_text(run.stdout+run.stderr)
receipt={'status':'PASS' if run.returncode==0 else 'FAIL','returncode':run.returncode,
 'scope':'Production credit/serial-worker/dispatch-receipt/mailbox helpers plus source wiring; no Metal GPU execution',
 'buildLog':str(out/'begin-cpu-reclaim-build.log'),'runLog':str(out/'begin-cpu-reclaim-run.log')}
(out/'BEGIN-CPU-RECLAIM.json').write_text(json.dumps(receipt,indent=2)+'\n')
if run.returncode:raise SystemExit(run.returncode)
print(run.stdout.strip())
