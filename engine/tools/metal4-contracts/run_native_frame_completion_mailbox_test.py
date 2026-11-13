#!/usr/bin/env python3
"""Production completion mailbox concurrency and source-wiring contract, not Metal execution."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--tsan',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
bridge=root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp';source=bridge.read_text()
callback=source.split('options.deferredCompleted=[mailbox,',1)[1].split('\n  };',1)[0]
assert 'session' not in callback and 'owner' not in callback and 'mailbox->Publish' in callback
assert 'deferredFrames' not in source
hold=source.index('ui::GetRecorder().HoldSubmittedFrame(frame.sourceDrawFrame)')
accept=source.index('session->completionMailbox->Accept(completionTicket)',hold)
assert hold < source.index('LuaNativePostprocess::GetRecorder().HoldSubmittedFrame',hold) < source.index('LuaNativeVolumetricClouds::HoldSubmittedFrame',hold) < accept
assert 'ApplyDeferredFrameReceipts();session->completionMailbox->RequireNoQueuedCompletions();return;' in source
assert 'while(auto notice=session->completionMailbox->Pop())' in source
assert '~Session(){if(completionMailbox)completionMailbox->Close();' in source
fixture=Path(__file__).with_name('TestNativeFrameCompletionMailbox.cpp');header=root/'rts/Rendering/Backend/NativeFrameCompletionMailbox.h'
command=['g++','-std=c++23','-pthread','-Wall','-Wextra','-Werror','-O1','-g','-fno-omit-frame-pointer','-fsanitize='+('thread' if a.tsan else 'address,undefined'),'-I'+str(root/'rts'),str(fixture),'-o',str(out/'test')]
r=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(r.stdout+r.stderr)
receipt={'scope':__doc__,'tsan':a.tsan,'exit':r.returncode,'passed':r.returncode==0,'output':r.stdout,'errors':r.stderr,'sourceSHA256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [bridge,header,fixture]},'limits':['No whole app, Objective-C++, Metal or new CPU lookahead execution.']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout+r.stderr,end='');raise SystemExit(r.returncode)
