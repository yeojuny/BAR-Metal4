#!/usr/bin/env python3
"""CPU-only actual Lua texture-unit parser versus pinned upstream GL calls.

AI-assisted local verification. No GPU, engine, app or synced content changes.
--game-root is a read-only exact30770 extraction containing luarules/gadgets.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess

BASE = 'e9d199329538cd9078a621c5e0f9ada416e1229f'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'work', 'game-root'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--overlay', type=Path)
    args = parser.parse_args()
    src, build, out = args.source.resolve(), args.build.resolve(), args.work.resolve()
    out.mkdir(parents=True, exist_ok=True)
    overlay = args.overlay.resolve() if args.overlay else src
    def current(name):
        file = overlay/name
        return file if file.exists() else src/name
    spec = importlib.util.spec_from_file_location('shape', src/'tools/metal4-contracts/run_native_shape_test.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    product_path = current('rts/Lua/LuaNativeUI.cpp')
    product = product_path.read_text()
    env = dict(os.environ, GIT_NO_LAZY_FETCH='1', GIT_OPTIONAL_LOCKS='0')
    original = subprocess.run(['git', '-C', str(src), 'show', BASE+':rts/Lua/LuaOpenGL.cpp'], env=env, check=True, capture_output=True).stdout.decode()
    (out/'upstream-LuaOpenGL.cpp').write_text(original)
    operations = [module.operation(product, name) for name in ['TexCoord', 'Texture', 'Rect', 'Color']]
    upstream = '\n'.join(module.block(original, 'int LuaOpenGL::'+name+'(') for name in ['TexCoord', 'MultiTexCoord', 'Texture'])
    values = module.block(product, 'std::array<float,4> Values(')
    fixture = current('tools/metal4-contracts/NativeMultiTextureLuaHarness.cpp').read_text()
    fixture = fixture.replace('/*@@ORIGINAL_PARSERS@@*/', upstream).replace('/*@@NATIVE_VALUES@@*/', values)
    for index, operation in enumerate(operations):
        fixture = fixture.replace('/*@@NATIVE_OPERATION_'+str(index)+'@@*/', operation)
    assert '/*@@' not in fixture
    cpp = out/'MultiTexture.actual.cpp'
    cpp.write_text(fixture)
    callback_files = []
    callback_bindings = {}
    for name in ['unit_respawning', 'unit_evolution']:
        source = args.game_root/'luarules/gadgets'/(name+'.lua')
        data = source.read_text()
        body = re.search(r'function gadget:Initialize\(\)(.*?)\n\tend', data, re.S).group(1)
        path = out/(name+'-initialize.lua')
        path.write_text('gadgetHandler={AddSyncAction=function()end}; UnitRespawned=function()end; evolveFinished=function()end\n'+body)
        callback_files.append(str(path))
        callback_bindings[str(source)] = {'fileSHA256': sha(source), 'callbackSHA256': hashlib.sha256(body.encode()).hexdigest()}
    vfs = {'version': 0, 'use-external-names': False, 'roots': []}
    if overlay != src:
        vfs['roots'] = [{'type': 'file', 'name': str(src/f.relative_to(overlay)), 'external-contents': str(f)} for f in overlay.rglob('*') if f.is_file()]
    (out/'vfs.json').write_text(json.dumps(vfs))
    files = [cpp]+[src/'rts/Rendering/Backend'/name for name in ['ScreenDrawList.cpp', 'NativeUIRecorder.cpp', 'NativeWorldDrawRecorder.cpp', 'NativeWorldDrawGeometry.cpp', 'NativeScreenLine.cpp']]
    binary = out/'test-actual-lua'
    command = ['clang++', '-std=c++23', '-O1', '-g', '-fsanitize=address,undefined', '-ffp-contract=off', '-fno-omit-frame-pointer', '-I'+str(src/'rts'), '-I'+str(src/'rts/lib/lua/include'), '-ivfsoverlay', str(out/'vfs.json')]+list(map(str, files))+[str(build/'rts/lib/lua/liblua.a'), str(build/'rts/lib/streflop/libstreflop.a'), str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'), '-o', str(binary)]
    state_fixture = current('tools/metal4-contracts/TestNativeMultiTexture.cpp')
    state_binary = out/'test-state'
    state_command = [str(state_fixture) if x == str(cpp) else str(state_binary) if x == str(binary) else x for x in command]
    state_compile = subprocess.run(state_command, capture_output=True, text=True)
    (out/'state-compile.log').write_text(state_compile.stdout+state_compile.stderr)
    assert state_compile.returncode == 0, state_compile.stderr[-4000:]
    state = subprocess.run([str(state_binary)], capture_output=True, text=True)
    (out/'state-run.log').write_text(state.stdout+state.stderr)
    assert state.returncode == 0, state.stdout+state.stderr
    result = subprocess.run(command, capture_output=True, text=True)
    (out/'compile.log').write_text(result.stdout+result.stderr)
    assert result.returncode == 0, result.stderr[-4000:]
    result = subprocess.run([str(binary)]+callback_files, capture_output=True, text=True)
    (out/'run.log').write_text(result.stdout+result.stderr)
    receipt = {'command': command, 'compileExit': 0, 'runExit': result.returncode, 'output': result.stdout+result.stderr, 'productSHA256': sha(product_path), 'operationsSHA256': [hashlib.sha256(x.encode()).hexdigest() for x in operations], 'upstreamOracleSHA256': hashlib.sha256(upstream.encode()).hexdigest(), 'exactCallbacks': callback_bindings, 'state': {'compileExit': state_compile.returncode, 'runExit': state.returncode, 'output': state.stdout+state.stderr, 'command': state_command}}
    if result.returncode == 0:
        wrong = out/'MultiTexture.wrong-unit.cpp'
        wrong.write_text(fixture.replace('r.MultiTexCoord(unit,v[0],v[1]);', 'r.MultiTexCoord(0,v[0],v[1]);', 1))
        negative_binary = out/'test-wrong-unit'
        negative_command = [str(wrong) if x == str(cpp) else str(negative_binary) if x == str(binary) else x for x in command]
        compiled = subprocess.run(negative_command, capture_output=True, text=True)
        (out/'negative-compile.log').write_text(compiled.stdout+compiled.stderr)
        assert compiled.returncode == 0, compiled.stderr[-4000:]
        negative = subprocess.run([str(negative_binary)]+callback_files, capture_output=True, text=True)
        (out/'negative.log').write_text(negative.stdout+negative.stderr)
        receipt['negative'] = {'compileExit': compiled.returncode, 'runExit': negative.returncode, 'output': negative.stdout+negative.stderr, 'detected': negative.returncode != 0 and 'differ from original' in negative.stderr}
        assert receipt['negative']['detected'], 'Original command oracle failed to reject wrong-unit mutation'
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(receipt['output'], end='')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
