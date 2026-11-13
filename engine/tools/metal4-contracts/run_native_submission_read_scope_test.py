#!/usr/bin/env python3
"""Production binding lifecycle + structural caller wiring; not Metal execution."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];a.output.mkdir(parents=True,exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'owner->shieldPending?owner->shieldPending->submissionIdentity:0' not in s
assert s.count('owner->submissionReads.Current()')==2
assert s.count('NativeSubmissionReadBinding::Scope submissionReads')==4
world=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert world.index('NativeSubmissionReadBinding::Scope')<world.index('RuntimeLease lease')<world.index('PrepareFrameImages')
assert world.index('NativeSubmissionReadBinding::Scope')<world.index('lease.Begin("Actual engine')
fixture=Path(__file__).with_name('TestNativeSubmissionReadScope.cpp');binary=a.output.resolve()/'test'
subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'rts'),str(fixture),'-o',str(binary)],check=True)
r=subprocess.run([str(binary)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True)
receipt={'passed':True,'output':r.stdout,'limits':['No Objective-C++/Metal execution. Serial scoped routing only; not concurrent renderer safety, whole-world ownership or enabled lookahead.']}
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout)
