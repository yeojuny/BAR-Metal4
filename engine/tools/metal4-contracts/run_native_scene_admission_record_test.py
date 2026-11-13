#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];here=Path(__file__).parent;out=Path('/tmp/bar-scene-admission-record');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
record=s.split('struct ScenePendingAdmission {',1)[1].split('\nvoid CompleteDynamicCapture',1)[0]
(out/'test.cpp').write_text((here/'SceneAdmissionFixturePrefix.cpp').read_text()+'\nstruct ScenePendingAdmission {'+record+(here/'SceneAdmissionFixtureSuffix.cpp').read_text())
assert 'owner->admissionSettlement.get()!=this' in record
assert 'transferred&&owner->sourceSettlement!=source' in record
assert record.index('source->ValidateRestore')<record.index('Transaction().Restore()')<record.index('source->RestoreValidated')
transfer=s.split('pendingAdmission=std::make_shared<ScenePendingAdmission>',1)[1]
assert transfer.index('session->admissionSettlement=pendingAdmission')<transfer.index('pendingAdmission->Transfer()')
assert 'const auto failedAdmission=[pendingAdmission]' in s
assert 'ReportPresentedWorldResult(*owner,submission->Frame(),*result,submission->Options().screenUI)' in record
report=s.split('DrawResult ReportPresentedWorldResult(',1)[1].split('// Stable main-owned',1)[0]
assert 'session->' not in report
assert 'sourceSettlement' not in report and 'opaqueSourcePending' not in report
assert 'return (pendingAdmission&&pendingAdmission->cpuAccepted)?' in s
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
