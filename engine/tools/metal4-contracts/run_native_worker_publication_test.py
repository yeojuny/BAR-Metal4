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
# Keep the original busy-before-allocation negative independent of the new ticket API.
s=subprocess.check_output(['git','show','62485fde819ea5ec3f6b287b701a392044767e58:engine/rts/Rendering/Backend/NativeSerialRenderWorker.h'],cwd=root.parent,text=True)
a=s.index('  // Construct the owning callable');b=s.index('  ready.notify_one();',a)
s=s[:a]+'''  {std::lock_guard lock(mutex);if(stopping||busy)throw std::logic_error("Native render worker has an outstanding invocation");
   busy=true;work=[task]{(*task)();};}
'''+s[b:]
p=out/'old/Rendering/Backend/NativeSerialRenderWorker.h';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s)
subprocess.run(args+['-I'+str(out/'old'),'-I'+str(root/'rts'),str(fixture),'-o',str(out/'old-test')],check=True)
r=subprocess.run([str(out/'old-test')],env=env,capture_output=True,text=True,timeout=30)
assert r.returncode!=0
assert 'outstanding invocation' in r.stderr
print('Previous busy-before-allocation ordering rejected by recovery test as expected')
