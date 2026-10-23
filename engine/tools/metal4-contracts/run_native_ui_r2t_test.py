#!/usr/bin/env python3
"""Run original Lua against mechanically extracted production dispatch bodies.

No generated source/binary is written into the repository. The only fixture
adapters are the list/resource registries; real Lua callbacks, pcall control
flow, recorder operations and cleanup use verbatim current engine code.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def block(source, op):
    match = re.search(r'if\s*\(op\s*==\s*"' + op + r'"\)\s*\{', source)
    if not match:
        raise RuntimeError(f"Missing dispatch operation: {op}")
    # Tokenize comments and quoted literals before counting C++ braces.
    tokens = re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', source[match.start():], re.S)
    depth = 0
    for token in tokens:
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return match.start(), match.start() + token.end()
    raise RuntimeError(f"Unbalanced source operation: {op}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--flowui', type=Path, required=True)
    p.add_argument('--r2thelper', type=Path, required=True)
    p.add_argument('--background', type=Path, required=True)
    args = p.parse_args()
    src, out, build = args.source.resolve(), args.work.resolve(), args.build.resolve()
    out.mkdir(parents=True, exist_ok=True)
    product = (src/'rts/Lua/LuaNativeUI.cpp').read_text()
    harness = (src/'tools/metal4-contracts/NativeUIR2tFlowUIHarness.cpp').read_text()
    hashes = {}
    for op in ['BeginEnd', 'RenderToTexture', 'CallList']:
        a, b = block(product, op)
        actual = product[a:b]
        hashes[op] = hashlib.sha256(actual.encode()).hexdigest()
        a, b = block(harness, op)
        harness = harness[:a] + actual + harness[b:]
    cpp = out/'R2tFlowUI.actual-dispatch.cpp'
    cpp.write_text(harness)
    binary = out/'test-native-r2t-production-dispatch'
    command = ['clang++', '-std=c++23', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2', '-I'+str(src/'rts'), '-I'+str(src/'rts/lib/lua/include'), str(src/'rts/Rendering/Backend/ScreenDrawList.cpp'), str(src/'rts/Rendering/Backend/NativeUIRecorder.cpp'), str(src/'rts/Rendering/Backend/NativeScreenLine.cpp'), str(src/'rts/Rendering/Backend/NativeWorldDrawRecorder.cpp'), str(src/'rts/Rendering/Backend/NativeWorldDrawGeometry.cpp'), str(cpp), str(build/'rts/lib/lua/liblua.a'), str(build/'rts/lib/streflop/libstreflop.a'), str(build/'test/CMakeFiles/test_Float3.dir/__/rts/System/Misc/SpringTime.cpp.o'), '-o', str(binary)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (out/'r2t-production-dispatch.compile.log').write_text(compiled.stdout + compiled.stderr)
    if compiled.returncode:
        raise RuntimeError(compiled.stderr[-4000:])
    run = subprocess.run([str(binary), str(args.flowui), str(args.r2thelper), str(src/'tools/metal4-contracts/NativeUIR2tFlowUIFixture.lua'), str(args.background)], capture_output=True, text=True)
    (out/'r2t-production-dispatch.log').write_text(run.stdout + run.stderr)
    # Negative control: use the prior strict adapter with the same original
    # short-panel callback. It must reproduce the actual observed failure.
    strict_cpp = out/'R2tFlowUI.strict-control.cpp'
    strict_cpp.write_text(harness.replace('.BeginLegacyPrimitive(', '.Begin(').replace('.EndLegacyPrimitive()', '.End()'))
    strict_binary = out/'test-native-r2t-strict-control'
    strict_command = [str(strict_cpp) if arg==str(cpp) else str(strict_binary) if arg==str(binary) else arg for arg in command]
    strict_compiled = subprocess.run(strict_command, capture_output=True, text=True)
    (out/'r2t-strict-control.compile.log').write_text(strict_compiled.stdout+strict_compiled.stderr)
    if strict_compiled.returncode:
        raise RuntimeError(strict_compiled.stderr[-4000:])
    strict_run = subprocess.run([str(strict_binary), str(args.flowui), str(args.r2thelper), str(src/'tools/metal4-contracts/NativeUIR2tFlowUIFixture.lua'), str(args.background)], capture_output=True, text=True)
    (out/'r2t-strict-control.log').write_text(strict_run.stdout+strict_run.stderr)
    if strict_run.returncode==0 or 'nested primitive Begin' not in strict_run.stderr:
        raise RuntimeError('Strict negative control did not reproduce observed nested Begin failure: '+strict_run.stdout+strict_run.stderr)
    report = {'productionSHA256': hashlib.sha256(product.encode()).hexdigest(), 'verbatimDispatchSHA256': hashes, 'inputSHA256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in [args.flowui, args.r2thelper, args.background]}, 'compileExit': compiled.returncode, 'runExit': run.returncode, 'result': run.stdout + run.stderr, 'command': command, 'strictControl': {'expectedFailure':True,'exit':strict_run.returncode,'result':strict_run.stdout+strict_run.stderr}}
    (out/'r2t-production-dispatch.json').write_text(json.dumps(report, indent=2)+'\n')
    print(run.stdout + run.stderr, end='')
    raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
