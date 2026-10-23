#!/usr/bin/env python3
"""Actual native Shape parser versus pinned upstream GL commands, CPU only.

AI-assisted local verification; not an upstream submission or GPU/app result.
All generated sources/logs/oracle files remain in --work. Source Git reads
disable lazy fetch/optional locks and do not modify index or references.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

BASE = 'e9d199329538cd9078a621c5e0f9ada416e1229f'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def block(text, signature):
    start = text.index(signature)
    tokens = re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[start:], re.S)
    depth = 0
    for token in tokens:
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                end = start+token.end()
                return text[start:end]+(';' if signature.startswith('struct ') else '')
    raise RuntimeError('Unbalanced C++ block: '+signature)


def operation(text, name):
    match = re.search(r'if\s*\(op\s*==\s*"'+name+r'"', text)
    if not match:
        raise RuntimeError('Production operation missing: '+name)
    return block(text, text[match.start():text.index('{', match.start())])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--flowui', type=Path, required=True)
    args = p.parse_args()
    src, build, out = args.source.resolve(), args.build.resolve(), args.work.resolve()
    out.mkdir(parents=True, exist_ok=True)
    files = [src/'rts/Lua/LuaNativeUI.cpp', src/'rts/Rendering/Backend/NativeUIRecorder.cpp', src/'rts/Rendering/Backend/NativeUIRecorder.h', src/'rts/Rendering/Backend/ScreenDrawList.cpp', src/'rts/Rendering/Backend/ScreenDrawList.h', src/'rts/Rendering/Backend/NativeScreenLine.cpp', src/'rts/Rendering/Backend/NativeScreenLine.h', src/'tools/metal4-contracts/NativeUIShapeHarness.cpp', src/'tools/metal4-contracts/run_native_shape_test.py']
    source_hashes = {str(path): sha(path.read_bytes()) for path in files}
    env = dict(os.environ, GIT_NO_LAZY_FETCH='1', GIT_OPTIONAL_LOCKS='0')
    def original(path):
        value = subprocess.run(['git', '-C', str(src), 'show', BASE+':'+path], env=env, capture_output=True, check=True).stdout
        (out/('upstream-'+Path(path).name)).write_bytes(value)
        return value.decode()
    gl = original('rts/Lua/LuaOpenGL.cpp')
    utils = original('rts/Lua/LuaUtils.cpp')
    flow = args.flowui.read_bytes()
    assert hashlib.md5(flow).hexdigest() == 'bf7bdcbba7b06e0d7b574675c92d0bef', 'Not exact test30770 FlowUI'
    product = files[0].read_text()
    original_parts = {
        'ORIGINAL_FLOAT_ARRAY': block(utils, 'int LuaUtils::ParseFloatArray('),
        'ORIGINAL_VERTEX_DATA': block(gl, 'struct VertexData')+'\n'+block(gl, 'static bool ParseVertexData('),
        'ORIGINAL_SHAPE': block(gl, 'int LuaOpenGL::Shape('),
        'ORIGINAL_NORMAL': block(gl, 'int LuaOpenGL::Normal('),
    }
    helpers = '\n'.join(block(product, key) for key in ['NativeDrawPrimitive Primitive(', 'struct ShapeVertexData', 'void ParseShapeVertex(', 'NativeUIMatrixMode MatrixMode(', 'std::array<float,4> Values('])
    operations = {name: operation(product, name) for name in ['Shape', 'Normal', 'Color', 'TexCoord', 'Vertex', 'BeginEnd', 'CallList', 'RenderToTexture', 'Translate', 'MatrixMode', 'LoadIdentity', 'Ortho', 'PushMatrix', 'PopMatrix', 'PushAttrib', 'PopAttrib']}
    create_list = operation(product, 'CreateList')
    dispatch = block(product, 'int Dispatch(lua_State*')
    # Only the function name changes to keep the verbatim production catch path.
    dispatch = dispatch.replace('DispatchImpl(L,name)', 'NativeDispatchImpl(L,name)')
    native_parts = {'NATIVE_HELPERS': helpers, 'NATIVE_CREATE_LIST': create_list, 'NATIVE_OPERATIONS': '\n'.join(operations.values()), 'NATIVE_DISPATCH': dispatch}
    harness = files[-2].read_text()
    for name, value in {**original_parts, **native_parts}.items():
        harness = harness.replace('/*@@'+name+'@@*/', value)
    assert '/*@@' not in harness
    cpp = out/'Shape.actual.cpp'
    cpp.write_text(harness)
    binary = out/'test-native-shape'
    command = ['clang++', '-std=c++23', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g', '-ffp-contract=off', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(src/'rts'), '-I'+str(src/'rts/lib/lua/include'), str(src/'rts/Rendering/Backend/ScreenDrawList.cpp'), str(src/'rts/Rendering/Backend/NativeUIRecorder.cpp'), str(src/'rts/Rendering/Backend/NativeWorldDrawRecorder.cpp'), str(src/'rts/Rendering/Backend/NativeWorldDrawGeometry.cpp'), str(src/'rts/Rendering/Backend/NativeScreenLine.cpp'), str(cpp), str(build/'rts/lib/lua/liblua.a'), str(build/'rts/lib/streflop/libstreflop.a'), str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'), '-o', str(binary)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (out/'compile.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:
        raise RuntimeError(compiled.stderr[-6000:])
    run = subprocess.run([str(binary), str(args.flowui.resolve()), str(out/'flowui-oracle.json')], capture_output=True, text=True)
    (out/'run.log').write_text(run.stdout+run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout+run.stderr)
    # Deliberately omit actual color commands in Shape. The pinned parser oracle
    # must detect that real incomplete colors no longer replace previous state.
    needle = 'if (value.hasColor) r.Color(value.color);'
    assert needle in harness
    negative_cpp = out/'Shape.color-omitted.cpp'
    negative_cpp.write_text(harness.replace(needle, 'if (false && value.hasColor) r.Color(value.color);', 1))
    negative_binary = out/'test-shape-color-omitted'
    negative_command = [str(negative_cpp) if x == str(cpp) else str(negative_binary) if x == str(binary) else x for x in command]
    negative_compile = subprocess.run(negative_command, capture_output=True, text=True)
    (out/'negative-compile.log').write_text(negative_compile.stdout+negative_compile.stderr)
    if negative_compile.returncode:
        raise RuntimeError(negative_compile.stderr[-4000:])
    negative = subprocess.run([str(negative_binary), str(args.flowui.resolve()), str(out/'negative-oracle.json')], capture_output=True, text=True)
    (out/'negative.log').write_text(negative.stdout+negative.stderr)
    assert negative.returncode != 0 and 'color differs' in negative.stderr, 'Independent upstream oracle did not reject omitted color mutation'
    after = {str(path): sha(path.read_bytes()) for path in files}
    report = {'status': 'PASS', 'base': BASE, 'sourceSHA256': source_hashes, 'sourceUnchanged': after == source_hashes, 'upstreamSHA256': {'LuaOpenGL.cpp': sha(gl.encode()), 'LuaUtils.cpp': sha(utils.encode())}, 'originalExtractionSHA256': {name: sha(text.encode()) for name, text in original_parts.items()}, 'productionExtractionSHA256': {name: sha(text.encode()) for name, text in native_parts.items()}, 'operationSHA256': {name: sha(text.encode()) for name, text in operations.items()}, 'flowui': {'md5': hashlib.md5(flow).hexdigest(), 'sha256': sha(flow), 'bytes': len(flow)}, 'compileExit': compiled.returncode, 'runExit': run.returncode, 'output': run.stdout, 'sanitizers': ['AddressSanitizer', 'UndefinedBehaviorSanitizer'], 'command': command, 'negativeControl': {'scope': 'Work-only omitted actual Shape color command, same upstream oracle', 'compileExit': negative_compile.returncode, 'runExit': negative.returncode, 'output': negative.stderr}, 'flowuiOracleSHA256': sha((out/'flowui-oracle.json').read_bytes()), 'limitations': ['Reference captures pinned GL commands plus independent matrix/triangle expansion; no GL context/pixels or GPU runs here.', 'Only four currently implemented planar primitive modes are compared; other six modes remain explicitly unsupported.', 'Current normal is retained and tested as state; unlit planar vertex pixels do not consume normals.', 'Malformed original Shape can leave GL inside Begin after a Lua error; native abort cleanup is an intentional safety boundary. Nonfinite native screen input remains rejected.']}
    (out/'receipt.json').write_text(json.dumps(report, indent=2)+'\n')
    print(run.stdout, end='')
    if after != source_hashes:
        raise RuntimeError('Source changed during tests; rerun after shared backend stabilizes')


if __name__ == '__main__':
    main()
