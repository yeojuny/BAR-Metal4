#!/usr/bin/env python3
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
"""Compile the actual private projectile/VBO/VFS methods against real Lua5.1-float.

Only engine GPU/VFS transport and configured projectile queries are fixture inputs;
original gadget bodies, helper, shader parameters, parser, and packet validation run.
Use extract_projectile_effect_assets.py first. Supply the exact original gadgets.lua
in WORK/exact/luarules/gadgets.lua (the routing guard checks its SHA512 itself).
"""
from pathlib import Path
import argparse,importlib.util,json,hashlib,re,subprocess,shlex,shutil
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--adapters',type=Path,required=True);a=p.parse_args();src=a.source.resolve();build=a.build.resolve();w=a.work.resolve();t=src/'tools/metal4-contracts';w.mkdir(parents=True,exist_ok=True)
def extract(text,signature):
 start=text.index(signature);brace=text.index('{',start);depth=0
 for tok in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[{}]',text[brace:],re.S):
  if tok.group()=='{':depth+=1
  elif tok.group()=='}':
   depth-=1
   if depth==0:return text[start:brace+tok.end()]
 raise RuntimeError(signature)
spec=importlib.util.spec_from_file_location('vbo_extract',t/'run_luavbo_layout_test.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
vbo=(src/'rts/Lua/LuaVBOImpl.cpp').read_text();header=(src/'rts/Lua/LuaVBOImpl.h').read_text();effect=(src/'rts/Lua/LuaNativeProjectileEffects.cpp').read_text();vfs=(src/'rts/Lua/LuaVFS.cpp').read_text()
names=['MaybeFunc','GetTypePtr','IsTypeValid','GetAttributeScalarCount','IsElementByteRangeValid','TransformAndWrite','UploadImpl','FillAttribsTableImpl','FillAttribsNumberImpl','DefineElementArray','CopyAttrMapToVec','MakeNativeLayout','AllocGLBuffer','Define','GetBufferSize','Upload','CaptureNativeSnapshot','Delete'];methods='\n\n'.join(helper.extract(vbo,n)for n in names);constructor=vbo[vbo.index('LuaVBOImpl::LuaVBOImpl('):vbo.index('/***********************/')];header=header[header.index('class VBO;'):]
core=effect[effect.index('namespace LuaNativeProjectileEffects {'):effect.index('void BeginWorldFrame()')]
for sig in ['void BeginWorldFrame()','void AbortWorldFrame()','void Reset(){AbortWorldFrame();','bool PushEntries(']:core+='\n'+extract(effect,sig)+'\n'
core+='\n}\n';tracy=extract((src/'rts/Lua/LuaUtils.cpp').read_text(),'void LuaUtils::TracyRemoveAlsoExtras(');route=extract(vfs,'int LoadNativeGadgetAdapter(')+'\n'+extract(vfs,'int LuaVFS::LoadFile(')
cpp=(t/'projectile_effect_test_prefix.inc').read_text()+tracy+route+header+'\n'+constructor+'\n'+methods+'\n'+core+(t/'projectile_effect_test_suffix.inc').read_text();(w/'TestEffects.cpp').write_text(cpp)
for fixture,name in [('setup.lua','setup.lua'),('route-negative.lua','route-negative.lua'),('handler.lua','handler.verbatim.lua')]:shutil.copy2(t/('projectile_effect_'+fixture),w/name)
db=json.loads((build/'compile_commands.json').read_text());entry=next(x for x in db if x['file'].endswith('/rts/Lua/LuaOpenGL.cpp'));raw=shlex.split(entry['command']);args=[];skip=False
for word in raw:
 if skip:skip=False;continue
 if word=='-o':skip=True;continue
 if word in ('-c',entry['file'])or word.startswith(('-O','-g','-W','-fvisibility')):continue
 args.append(word)
args+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-character-conversion','-Wno-sign-compare',str(w/'TestEffects.cpp'),str(src/'rts/Rendering/Backend/NativeProjectileEffectRecorder.cpp'),str(src/'rts/System/Sync/SHA512.cpp'),str(build/'rts/lib/lua/liblua.a'),str(build/'rts/lib/streflop/libstreflop.a'),str(build/'rts/lib/fmt/libfmt.a'),str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'),'-o',str(w/'test-effects')]
before={str(src/f):hashlib.sha256((src/f).read_bytes()).hexdigest()for f in ['rts/Lua/LuaNativeProjectileEffects.cpp','rts/Lua/LuaVBOImpl.cpp','rts/Lua/LuaVFS.cpp','rts/Rendering/Backend/NativeProjectileEffectRecorder.cpp']};compile=subprocess.run(args,cwd=entry['directory'],capture_output=True,text=True);(w/'build.log').write_text(compile.stdout+compile.stderr);receipt={'command':args,'compileExit':compile.returncode,'sourceHashes':before,'productionCoreSHA256':hashlib.sha256(core.encode()).hexdigest(),'methodHashes':{n:hashlib.sha256(helper.extract(vbo,n).encode()).hexdigest()for n in names}}
if compile.returncode==0:
 run=subprocess.run([str(w/'test-effects'),str(w),str(a.adapters.resolve())],capture_output=True,text=True);(w/'run.log').write_text(run.stdout+run.stderr);receipt.update(runExit=run.returncode,result=run.stdout+run.stderr);print(run.stdout+run.stderr)
else:print(compile.stderr)
receipt['sourceUnchanged']=all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in before.items());(w/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');raise SystemExit(compile.returncode or receipt.get('runExit',1))
