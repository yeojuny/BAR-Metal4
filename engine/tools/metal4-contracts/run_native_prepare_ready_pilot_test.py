#!/usr/bin/env python3
"""Source-contract tests for default-off preparation release, not native overlap evidence."""
from pathlib import Path
import subprocess,sys,json,hashlib
r=Path(__file__).resolve().parents[2];here=Path(__file__).parent
scene=(r/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text();renderer=(r/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text();job=(r/'rts/Rendering/Backend/NativeRenderJobSettlement.h').read_text()
assert 'BAR_NATIVE_PREPARE_READY_PILOT' in scene
assert 'sourceCredits{PrepareReadyPilotRequested()?3u:2u}' in scene
assert 'if(queuedReturn||begin->Submission())Quarantine();else Restore();' in scene
assert '(!options.allowPrepareReady&&!options.beginReceipt->Submission())' in scene
assert '(overlapPreparation&&!options.allowBeginReady)?' in scene
assert 'if(options.allowPrepareReady)RequireRetiredGPUWork()' not in scene
assert 'preparePolicy.EligibleForPreparation()' in renderer
assert 'prepareReady&&impl->cpuJob->ReleaseForPreparation()' in renderer
assert '[dispatchReceipt]{dispatchReceipt->MarkRetirementReady();}' in renderer
release=job.split('bool ReleaseForPreparation(){',1)[1].split('\n }',1)[0]
assert 'target()' not in release and 'Wait' not in release
finish=job.split(' void Finish(){',1)[1]
assert finish.index('task->Join()')<finish.index('target()')<finish.index('DeliverRetirement()')<finish.index('Notify()')
retire=scene.split('void ApplyDeferredFrameReceipts()',1)[1].split('bool BeginReadyPilotRequested',1)[0]
assert 'sourceSettlement.reset' not in retire and 'opaqueSourcePending' not in retire
assert 'sourceCredits.ReleaseHeld(result.sourceDrawFrame)' in retire
for script in ['run_native_source_frame_credits_test.py','run_native_render_job_settlement_test.py','run_native_scene_admission_record_test.py','run_native_begin_ready_pilot_test.py']:
 subprocess.run([sys.executable,str(here/script)],check=True)
files=['rts/Rendering/Backend/NativeSourceFrameCredits.h','rts/Rendering/Backend/NativeRenderJobSettlement.h','rts/Rendering/Backend/NativeBeginReadyPolicy.h','rts/Rendering/Metal4/Metal4WorldRenderer.h','rts/Rendering/Metal4/Metal4WorldRenderer.mm','rts/Rendering/Metal4/Metal4SceneBridge.cpp']
out=Path('/tmp/bar-prepare-ready-pilot');out.mkdir(exist_ok=True)
(out/'receipt.json').write_text(json.dumps({'passed':True,'scope':__doc__,'macCompile':False,'gameRun':False,'performanceMeasured':False,'sourceHashes':{f:hashlib.sha256((r/f).read_bytes()).hexdigest() for f in files}},indent=2))
print('PASS default-off preparation-release source contracts; Metal execution/overlap unverified')
