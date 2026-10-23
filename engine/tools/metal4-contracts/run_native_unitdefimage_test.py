#!/usr/bin/env python3
"""Compile actual native build-picture engine methods against CPU image oracles.

Includes the actual SetUnitDefImage Lua-boundary body with instrumented arguments.
This is not a full Lua VM, VFS/DDS decoder, recorder replay or GPU acceptance
fixture. Those owners must validate the real helper/consumer separately.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess


def extract(source, name, signature, class_name="CUnitDrawerData"):
    for match in re.finditer(r'^[^\n]*'+class_name+'::'+name+r'\(',source,re.MULTILINE):
        begin=source.index('{',match.end())
        if signature and signature not in source[match.start():begin]:
            continue
        depth=0
        tokens=r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[{}]'
        for token in re.finditer(tokens,source[begin:],re.DOTALL):
            if token[0]=='{':
                depth+=1
            if token[0]=='}':
                depth-=1
                if depth==0:
                    return source[match.start():begin+token.end()]
    raise ValueError('Missing CUnitDrawerData::'+name+' '+signature)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[2]
    source=(root/'rts/Rendering/Units/UnitDrawerData.cpp').read_text()
    header=(root/'rts/Rendering/UnitDefImage.h').read_text()
    methods={name+signature:extract(source,name,signature) for name,signature in [
        ('SetUnitDefImage','std::string&'),('SetUnitDefImage','unsigned int'),
        ('GetUnitDefImage',''),('GetNativeUnitDefImage',''),('SetNativeUnitDefImage','')]}
    (args.output/'methods.inc').write_text('\n\n'.join(methods.values())+'\n')
    unitimage=header[header.index('struct UnitDefImage'):header.index('\n#endif // UNIT_DEF_IMAGE')]
    (args.output/'unitimage.inc').write_text(unitimage)
    control=(root/'rts/Lua/LuaUnsyncedCtrl.cpp').read_text()
    control_method=extract(control,'SetUnitDefImage','',class_name='LuaUnsyncedCtrl')
    (args.output/'ctrl.inc').write_text(control_method+'\n')
    fixture=Path(__file__).with_name('TestNativeUnitDefImage.cpp')
    binary=args.output/'test-native-buildpic'
    command=['/usr/bin/clang++','-std=c++23','-DRECOIL_METAL4_LIVE_SCENE=1',
        '-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined',
        '-ffp-contract=off','-fno-omit-frame-pointer','-I',str(root/'rts'),'-I',str(args.output),
        str(fixture),'-o',str(binary)]
    compile_result=subprocess.run(command,capture_output=True,text=True)
    (args.output/'compile.log').write_text(compile_result.stdout+compile_result.stderr)
    if compile_result.returncode:
        print(compile_result.stderr,end='')
        return compile_result.returncode
    result=subprocess.run([str(binary)],capture_output=True,text=True)
    (args.output/'run.log').write_text(result.stdout+result.stderr)
    sanitizer_failure=any(marker in result.stderr for marker in ('AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer'))
    receipt={'scope':__doc__.strip(),'compile':command,'compileExit':compile_result.returncode,
        'runExit':result.returncode,'summary':result.stdout.strip(),'sanitizerFailure':sanitizer_failure,
        'sourceSha256':hashlib.sha256(source.encode()).hexdigest(),
        'unitImageHeaderSha256':hashlib.sha256(header.encode()).hexdigest(),
        'luaControlMethodSha256':hashlib.sha256(control_method.encode()).hexdigest(),
        'methodSha256':{name:hashlib.sha256(body.encode()).hexdigest() for name,body in methods.items()},
        'fixtureSha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),
        'passed':result.returncode==0 and not sanitizer_failure}
    (args.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(result.stdout+result.stderr,end='')
    return 0 if receipt['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
