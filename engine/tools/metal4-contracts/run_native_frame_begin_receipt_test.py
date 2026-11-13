#!/usr/bin/env python3
"""Actual owned receipt and serial worker; structural renderer wiring, not Metal execution."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'options.beginReceipt->FailureStage()==NativeFrameBeginReceipt::Failure::None' in source
prepared=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert prepared.index('lease.Begin("Actual engine')<prepared.index('options.beginReceipt->MarkBegun(sourceSubmission)')<prepared.index('targetSubmissionBegan(sourceSubmission)')
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'options.beginReceipt->MarkFailed();session->lastFailedBeginReceipt=options.beginReceipt;' in bridge
fixture=Path(__file__).with_name('TestNativeFrameBeginReceipt.cpp');header=root/'rts/Rendering/Backend/NativeFrameBeginReceipt.h'
command=['g++','-std=c++23','-pthread','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'rts'),str(fixture),'-o',str(out/'test')]
subprocess.run(command,check=True)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
r=subprocess.run([str(out/'test')],capture_output=True,text=True,env=env)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
fatal={}
for name in ['zero','duplicate']:
 f=subprocess.run([str(out/'test'),name],capture_output=True,text=True,env=env);fatal[name]=f.returncode;assert f.returncode<0
receipt={'passed':True,'output':r.stdout,'fatal':fatal,'hashes':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [header,fixture]},'limits':['Not Objective-C++ compilation or Metal execution. No early journal transfer, rollback or new lookahead.']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout)
