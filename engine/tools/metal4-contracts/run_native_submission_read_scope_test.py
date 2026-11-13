#!/usr/bin/env python3
"""Production binding lifecycle + structural caller wiring; not Metal execution."""
import argparse,json,os,subprocess,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];a.output.mkdir(parents=True,exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'owner->shieldPending?owner->shieldPending->submissionIdentity:0' not in s
assert 'owner->submissionReads' not in s and 'pbrSubmission' not in s
assert s.count('lease.SourceSubmission()')==9
assert s.count('lease.BindSourceSubmission(')==4
assert s.count('impl->submissionEntry,')==4
assert 'submissionEntry.Current()' not in s
assert s.count('NativeSubmissionReadBinding::Scope submissionReads')==4
world=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert world.index('NativeSubmissionReadBinding::Scope')<world.index('RuntimeLease lease')<world.index('lease.BindSourceSubmission(sourceSubmission)')<world.index('PrepareFrameImages')
assert world.index('NativeSubmissionReadBinding::Scope')<world.index('lease.Begin("Actual engine')
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text()
assert h.count('void BindSourceSubmission(')==1 and h.count('uint64_t SourceSubmission()')==1
accessors=h.split(' void BindSourceSubmission(',1)[1].split('\tid<MTLDevice> Device()',1)[0]
(a.output/'lease-submission.inc').write_text('void BindSourceSubmission('+accessors)
assert 'NativeSubmissionReadIdentity sourceSubmission;' in h
for name in ['ReadIndependentTargetPrefix','SubmitMaintenance','ContinueScreen']:
 part=s.split('Metal4WorldRenderer::'+name+'(',1)[1].split('lease.BindSourceSubmission(',1)
 assert len(part)==2 and 'RuntimeLease lease' in part[0]
 # Exact bind must precede first source prepare in each scope.
 before=part[0].split('RuntimeLease lease',1)[1]
 assert '.Prepare(' not in before and '.PrepareFrameImages(' not in before
providers=s.split('void Metal4WorldRenderer::ConnectShieldTextureContext(',1)[1].split('void Metal4WorldRenderer::ConstructOriginalModelPrograms(',1)[0]
assert hashlib.sha256(providers.encode()).hexdigest()=='22f0953d5fbaac6a145434d2ad27e8bb3fb5be774d6c680bed32fd06d2f971c0'
fixture=Path(__file__).with_name('TestNativeSubmissionReadScope.cpp');binary=a.output.resolve()/'test'
subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(a.output.resolve()),str(fixture),'-o',str(binary)],check=True)
r=subprocess.run([str(binary)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True)
receipt={'passed':True,'output':r.stdout,'limits':['No Objective-C++/Metal execution. Exact lease identity plus serial entry guard only; not concurrent renderer safety, whole-world ownership or enabled lookahead.']}
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout)
