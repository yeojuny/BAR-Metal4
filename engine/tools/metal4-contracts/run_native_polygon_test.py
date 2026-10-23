#!/usr/bin/env python3
"""Production retained state dispatch plus verified original selectionbox/formation Lua; engine Lua5.1-float."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from run_native_shape_test import block,operation
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','build','work','selection-widget','formation-widget'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();src=a.source.resolve();out=a.work.resolve();build=a.build.resolve();out.mkdir(parents=True,exist_ok=True)
product=(src/'rts/Lua/LuaNativeUI.cpp').read_text();line=(src/'rts/Rendering/LineDrawer.cpp').read_text()
helpers='\n'.join(block(product,x) for x in ('int NativeGLInt(', 'NativeDrawPrimitive Primitive(','std::array<float,4> Values(','std::string LuaPrimitiveOrigin('))
operations={name:operation(product,name)for name in ('CreateList','DeleteList','CallList','BeginEnd','Vertex','Color','Rect','LineWidth','PushMatrix','PopMatrix','PolygonMode','LineStipple','PushAttrib','PopAttrib')}
line_methods='\n'.join(block(line,x)for x in ('void CLineDrawer::SetupLineStipple()','std::pair<int,std::uint16_t> CLineDrawer::GetNativeStippleSettings() const'))
harness=(src/'tools/metal4-contracts/NativePolygonLuaHarness.cpp').read_text()
for k,v in {'HELPERS':helpers,'OPERATIONS':'\n'.join(operations.values()),'DISPATCH':block(product,'int Dispatch(lua_State*'),'LINE_METHODS':line_methods}.items():harness=harness.replace('/*@@'+k+'@@*/',v)
cpp=out/'NativePolygon.actual-dispatch.cpp';cpp.write_text(harness)
formation=a.formation_widget.read_text();first=formation.index('local function DrawFormationLines(');last=formation.index('\n\n\nlocal Xs, Ys',first);original=formation[first:last]
helper=out/'original-formation-helper.lua';helper.write_text('local glLineStipple,glLineWidth,glBeginEnd=gl.LineStipple,gl.LineWidth,gl.BeginEnd\nlocal GL_LINE_STRIP=GL.LINE_STRIP\nlocal fNodes={{-.7,-.5,0},{-.1,.2,0},{.8,.6,0}}\nlocal dimmNodes={{-.8,.7,0},{.7,-.6,0}}\nlocal usingCmd,dimmCmd,dimmAlpha=1,2,.3\nlocal function SetColor(cmd,alpha)gl.Color(cmd==1 and 1 or .2,.5,.75,alpha)end\n'+original+'\n_G.DrawFormationLines=DrawFormationLines\n')
sources=['ScreenDrawList.cpp','NativeUIRecorder.cpp','NativeScreenLine.cpp','NativeWorldDrawRecorder.cpp','NativeWorldDrawGeometry.cpp'];binary=out/'test-native-polygon-lua'
cmd=['clang++','-std=c++23','-DRECOIL_METAL4_LIVE_SCENE','-Wall','-Wextra','-Werror','-pedantic','-O1','-g','-ffp-contract=off','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(src/'rts'),'-I'+str(src/'rts/lib/lua/include')]+[str(src/'rts/Rendering/Backend'/x)for x in sources]+[str(cpp),str(build/'rts/lib/lua/liblua.a'),str(build/'rts/lib/streflop/libstreflop.a'),str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'),'-o',str(binary)]
r=subprocess.run(cmd,capture_output=True,text=True);(out/'lua.compile.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr[-5000:])
r=subprocess.run([str(binary),str(a.selection_widget),str(helper)],capture_output=True,text=True);(out/'lua.run.log').write_text(r.stdout+r.stderr)
receipt={'scope':'Actual engine Lua5.1-float full original Selectionbox lifecycle and unchanged original Formation helper body, production dispatch and original LineDrawer string-path methods. Vignette binding is an explicit1x1analytic fixture image, not original-image parity. No GPU/game/app acceptance.','compileExit':0,'runExit':r.returncode,'result':r.stdout+r.stderr,'productionSHA256':hashlib.sha256(product.encode()).hexdigest(),'operationsSHA256':{k:hashlib.sha256(v.encode()).hexdigest()for k,v in operations.items()},'lineMethodsSHA256':hashlib.sha256(line_methods.encode()).hexdigest(),'originalFormationBodySHA256':hashlib.sha256(original.encode()).hexdigest(),'originalMD5':{str(x):hashlib.md5(x.read_bytes()).hexdigest()for x in (a.selection_widget,a.formation_widget)},'command':cmd};(out/'lua-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout+r.stderr,end='');raise SystemExit(r.returncode)
