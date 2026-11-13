#!/usr/bin/env python3
"""Host ASan/UBSan checks of production CUS immutable uniform builder, not Lua/Metal execution."""
import argparse, pathlib, subprocess, os, json, hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=pathlib.Path(__file__).resolve().parents[2];fixture=pathlib.Path(__file__).with_name('TestNativeCUSProgramUniforms.cpp');header=root/'rts/Rendering/Backend/NativeCUSProgramUniforms.h'
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'rts'),str(root/'rts/Rendering/Backend/NativeCUSProgramData.cpp'),str(fixture),'-o',str(out/'test')]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(build.stdout+build.stderr)
if build.returncode:print(build.stderr);raise SystemExit(build.returncode)
run=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(run.stdout+run.stderr)
# No claim of executing sol/Lua. These checks ensure validation stays before mutation.
lua=(root/'rts/Lua/LuaNativeCUSSource.cpp').read_text();method=lua[lua.index('void UniformValue('):lua.index('struct Buffer')]
assert method.index('location==-1')<method.index('active->uniforms.Write')
assert method.index('CUS original uniform arity differs')<method.index('active->uniforms.Write')
assert method.index('CUS uniform does not belong')<method.index('active->uniforms.Write')
assert 'draw.uniformValues=actualProgram->uniforms.Capture();' in lua
receipt={'scope':__doc__,'exit':run.returncode,'command':cmd,'summary':run.stdout,'sourceSHA256':hashlib.sha256(header.read_bytes()).hexdigest(),'fixtureSHA256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'luaSourceOrderChecks':4,'notTested':['Lua VM','Metal Objective-C++','macOS app build','GPU output or FPS']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(run.stdout+run.stderr);raise SystemExit(run.returncode)
