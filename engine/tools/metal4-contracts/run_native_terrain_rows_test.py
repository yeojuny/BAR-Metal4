#!/usr/bin/env python3
"""Link actual history, registry and terrain capture on host; no game/Metal execution."""
import argparse,pathlib,subprocess,os,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=pathlib.Path(__file__).resolve().parents[2]
files=['tools/metal4-contracts/TestNativeTerrainRows.cpp','rts/Rendering/Backend/NativeCUSTextureHistory.cpp','rts/Rendering/Backend/NativeTextureRegistry.cpp','rts/Rendering/Backend/NativeTerrainSourceData.cpp']
command=['g++','-std=c++20','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-I'+str(r/'rts')]+[str(r/f) for f in files]+['-Wl,--gc-sections','-o',str(a.output/'test')]
subprocess.run(command,check=True);env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
run=subprocess.run([str(a.output/'test')],env=env,capture_output=True,text=True);print(run.stdout,end='');print(run.stderr,end='')
files+=['rts/Rendering/Backend/NativeCUSTextureHistory.h','rts/Rendering/Backend/NativeTerrainSourceData.h']
(a.output/'receipt.json').write_text(json.dumps({'scope':__doc__,'exit':run.returncode,'summary':run.stdout,'asan':True,'ubsan':True,'sourceHashes':{f:hashlib.sha256((r/f).read_bytes()).hexdigest() for f in files}},indent=2));raise SystemExit(run.returncode)
