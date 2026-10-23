#!/usr/bin/env python3
"""Actual production dispatch + exact original widgets with engine Lua5.1-float."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from run_native_shape_test import block, operation

p=argparse.ArgumentParser(description=__doc__)
for name in ['source','build','work','build-widget','attack-widget']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();src=a.source.resolve();out=a.work.resolve();build=a.build.resolve();out.mkdir(parents=True,exist_ok=True)
product=(src/'rts/Lua/LuaNativeUI.cpp').read_text()
helpers='\n'.join(block(product,x) for x in ['NativeDrawPrimitive Primitive(','std::array<float,4> Values(','std::string LuaPrimitiveOrigin('])
operations={name:operation(product,name) for name in ['CreateList','DeleteList','CallList','BeginEnd','Vertex','Color','LineWidth','DepthMask','DepthTest','PushMatrix','PopMatrix','Translate','Rotate']}
harness=(src/'tools/metal4-contracts/NativeWorldLuaHarness.cpp').read_text()
for key,value in {'HELPERS':helpers,'OPERATIONS':'\n'.join(operations.values()),'DISPATCH':block(product,'int Dispatch(lua_State*')}.items():harness=harness.replace('/*@@'+key+'@@*/',value)
cpp=out/'NativeWorld.actual-dispatch.cpp';cpp.write_text(harness);binary=out/'test-native-world-lua'
sources=['ScreenDrawList.cpp','NativeUIRecorder.cpp','NativeScreenLine.cpp','NativeWorldDrawRecorder.cpp','NativeWorldDrawGeometry.cpp']
cmd=['clang++','-std=c++23','-Wall','-Wextra','-Werror','-pedantic','-O1','-g','-ffp-contract=off','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src/'rts'),'-I'+str(src/'rts/lib/lua/include')]+[str(src/'rts/Rendering/Backend'/x) for x in sources]+[str(cpp),str(build/'rts/lib/lua/liblua.a'),str(build/'rts/lib/streflop/libstreflop.a'),str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'),'-o',str(binary)]
r=subprocess.run(cmd,capture_output=True,text=True);(out/'lua.compile.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr[-5000:])
r=subprocess.run([str(binary),str(a.build_widget),str(a.attack_widget),str(out/'original-build-placement-frame.json')],capture_output=True,text=True);(out/'lua.run.log').write_text(r.stdout+r.stderr)
receipt={'scope':'original full BuildPlacement lifecycle/cache and original full Attack chunk/helper; production dispatch verbatim; CPU only','compileExit':0,'runExit':r.returncode,'result':r.stdout+r.stderr,'productionSHA256':hashlib.sha256(product.encode()).hexdigest(),'operationsSHA256':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in operations.items()},'originalMD5':{str(x):hashlib.md5(x.read_bytes()).hexdigest() for x in [a.build_widget,a.attack_widget]},'command':cmd}
(out/'lua-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout+r.stderr,end='');raise SystemExit(r.returncode)
