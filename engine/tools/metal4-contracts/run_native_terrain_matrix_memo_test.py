#!/usr/bin/env python3
"""Host memo contract, not Apple SIMD inverse, Metal compilation or game/FPS validation."""
import argparse,pathlib,subprocess,os,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=pathlib.Path(__file__).resolve().parents[2];fixture=pathlib.Path(__file__).with_name('TestNativeTerrainMatrixMemo.cpp')
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'rts'),str(fixture),'-o',str(out/'test')]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(build.stdout+build.stderr)
if build.returncode:print(build.stderr);raise SystemExit(build.returncode)
run=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(run.stdout+run.stderr)
source=(root/'rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm').read_text()
assert 'DrawUniform(draw,original->profile,target,inverseMemo)' in source
assert source.index('result.modelView=Matrix(draw.modelView.data())')<source.index('inverseMemo.Get(')
assert 'std::isfinite(determinant)&&determinant!=0' in source
paths=['rts/Rendering/Backend/NativeTerrainMatrixMemo.h','rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm']
receipt={'scope':__doc__,'exit':run.returncode,'summary':run.stdout,'productionSHA256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'fixtureSHA256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'sourceChecks':3,'notTested':['Apple SIMD inverse','Metal Objective-C++','macOS app build','GPU output or FPS']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(run.stdout+run.stderr);raise SystemExit(run.returncode)
