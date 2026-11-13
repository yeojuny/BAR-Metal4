#!/usr/bin/env python3
"""Compile complete actual backend sources with section GC; no algorithm stubs."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2]
files=[Path(__file__).with_name('TestNativePostprocessPlan.cpp')]+[root/'rts/Rendering/Backend'/name for name in ['NativePostprocessData.cpp','NativeCUSUniformData.cpp','ScreenDrawList.cpp']]
# Static call-path evidence only, not measured renderer invocation counts.
backend=files[1].read_text()
start=backend.index('void ValidateNativePostprocessConsumer(const NativePostprocessPlan&')
end=backend.index('\n}',start)+2
consumer_body=backend[start:end]
assert 'OrderNativePostprocessCommands' not in consumer_body and 'plan.producers.find(key)' in consumer_body
structural={'planConsumerDirectOrderCalls':0,'planConsumerUsesProducerIndex':True,'method':'source inspection; not runtime renderer measurement'}
command=[shutil.which('g++') or 'c++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-missing-field-initializers','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I',str(root/'rts'),*[str(f) for f in files],'-o',str(out/'test')]
r=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(r.stdout+r.stderr)
receipt={'scope':__doc__,'structuralEvidence':structural,'compiler':subprocess.check_output([command[0],'--version'],text=True).splitlines()[0],'command':command,'sourceSHA256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'passed':r.returncode==0,'exit':r.returncode,'output':r.stdout,'leakSanitizer':'disabled: sandbox ptrace; ASan/UBSan active'}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout+r.stderr,end='');raise SystemExit(r.returncode)
