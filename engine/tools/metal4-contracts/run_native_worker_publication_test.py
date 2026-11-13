#!/usr/bin/env python3
"""Actual worker allocation failure, with the previous ordering as negative control."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-worker-publication');out.mkdir(exist_ok=True)
fixture=Path(__file__).with_name('TestNativeWorkerPublication.cpp')
args=['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined']
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
subprocess.run(args+['-I'+str(root/'rts'),str(fixture),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env=env,check=True,timeout=30)
# Deterministic negative control derived from the current production header:
# suppress allocation-failure credit rollback. This models the original leaked
# busy/credit bug without depending on an unpublished historical commit/API.
header=root/'rts/Rendering/Backend/NativeSerialRenderWorker.h'
s=header.read_text()
rollback='~Claim(){if(!published){std::lock_guard lock(state->mutex);state->claimed=false;}}'
assert s.count(rollback)==1
s=s.replace(rollback,'~Claim(){} // NEGATIVE CONTROL: leaked construction credit')
import hashlib,json
(out/'negative-provenance.json').write_text(json.dumps({'source':str(header.relative_to(root)), 'sha256':hashlib.sha256(header.read_bytes()).hexdigest(), 'mutation':'Remove Claim destructor allocation-failure credit rollback'},indent=2)+'\n')
p=out/'old/Rendering/Backend/NativeSerialRenderWorker.h';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s)
subprocess.run(args+['-I'+str(out/'old'),'-I'+str(root/'rts'),str(fixture),'-o',str(out/'old-test')],check=True)
r=subprocess.run([str(out/'old-test')],env=env,capture_output=True,text=True,timeout=30)
(out/'negative.log').write_text(r.stdout+r.stderr)
assert r.returncode!=0
assert 'no task credit' in r.stderr
print('Missing allocation-failure credit rollback rejected by recovery test as expected')
