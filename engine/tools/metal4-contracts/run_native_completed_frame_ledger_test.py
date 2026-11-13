#!/usr/bin/env python3
"""Production UI frame completion ledger host contract; not Metal or full recorder runtime."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 root=Path(__file__).resolve().parents[2];base=root/'rts/Rendering/Backend'
 common=['g++','-std=c++23','-I'+str(root/'rts')]
 syntax=subprocess.run(common+['-Wno-deprecated-declarations','-fsyntax-only',str(base/'NativeUIRecorder.cpp')],capture_output=True,text=True)
 (out/'recorder-syntax.log').write_text(syntax.stdout+syntax.stderr)
 fixture=Path(__file__).with_name('TestNativeCompletedFrameLedger.cpp');exe=out/'test'
 command=common+['-Wall','-Wextra','-Werror','-Wno-mismatched-new-delete','-O1','-g','-fsanitize=address,undefined',str(fixture),'-o',str(exe)]
 compiled=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(compiled.stdout+compiled.stderr)
 if syntax.returncode or compiled.returncode:
  print(syntax.stderr+compiled.stderr);return 1
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 run=subprocess.run([str(exe)],capture_output=True,text=True,env=env);(out/'run.log').write_text(run.stdout+run.stderr)
 failures={}
 files=[base/'ScreenDrawList.h',base/'NativeUIRecorder.cpp',base/'NativeUITargetPassJournal.h',base/'NativeCompletedFrameLedger.h',fixture]
 receipt={'scope':__doc__,'syntaxExit':syntax.returncode,'testExit':run.returncode,'summary':run.stdout.strip(),'fatalContracts':failures,'sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'command':command,'passed':syntax.returncode==run.returncode==0 and all(v['expectedTermination'] for v in failures.values()),'limits':['Full recorder runtime link not verified; existing target fixture requires BeginGenesisStage bool update and additional engine service linkage.','No Mac build, Metal execution, frame concurrency or FPS claim.']}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2));return 0 if receipt['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
