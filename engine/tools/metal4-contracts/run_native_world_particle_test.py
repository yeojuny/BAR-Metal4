#!/usr/bin/env python3
"""CPU-only actual original projectile geometry/TLS and immutable nano tests."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


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
                return text[start:start+token.end()]
    raise RuntimeError('Unbalanced source block: '+signature)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    args = p.parse_args()
    src, out = args.source.resolve(), args.work.resolve()
    out.mkdir(parents=True, exist_ok=True)
    paths = {
        'spawn': src/'rts/Sim/Projectiles/ExpGenSpawnable.cpp',
        'scope': src/'rts/Sim/Projectiles/ExpGenSpawnable.h',
        'buffer': src/'rts/Rendering/GL/RenderBuffers.h',
        'smoke': src/'rts/Rendering/Env/Particles/Classes/SmokeProjectile.cpp',
        'sort': src/'rts/Rendering/Env/Particles/ProjectileDrawer.cpp',
        'capture': src/'rts/Rendering/Backend/NativeWorldParticleCapture.cpp',
        'geometry': src/'rts/Rendering/Backend/NativeWorldParticleGeometry.cpp',
        'recorder': src/'rts/Rendering/Backend/NativeLuaNanoRecorder.cpp',
        'harness': src/'tools/metal4-contracts/NativeWorldParticleHarness.cpp',
        'script': Path(__file__).resolve(),
    }
    sha = lambda b: hashlib.sha256(b).hexdigest()
    before = {str(path): sha(path.read_bytes()) for path in paths.values()}
    texts = {name: path.read_text() for name, path in paths.items()}
    signatures = [
        'void CExpGenSpawnable::SetGeomFillTarget(',
        'TypedRenderBuffer<VA_TYPE_PROJ>* CExpGenSpawnable::GetGeomFillTarget(',
        'TypedRenderBuffer<VA_TYPE_PROJ>& CExpGenSpawnable::GetPrimaryRenderBuffer(',
    ]
    effects1 = 'void CExpGenSpawnable::AddEffectsQuadImpl(uint32_t pageNum, const VA_TYPE_TC& tl, const VA_TYPE_TC& tr, const VA_TYPE_TC& br, const VA_TYPE_TC& bl, const float3& ap, const float& p)'
    effects2 = 'void CExpGenSpawnable::AddEffectsQuadImpl(uint32_t pageNum, const VA_TYPE_TC& tl, const VA_TYPE_TC& tr, const VA_TYPE_TC& br, const VA_TYPE_TC& bl)'
    extracted = {
        'SCOPE': block(texts['scope'], 'class ScopedGeomFillTarget'),
        'DESTINATION': '\n'.join(block(texts['spawn'], signature) for signature in signatures),
        'EFFECTS': block(texts['spawn'], effects1)+'\n'+block(texts['spawn'], effects2),
        'QUAD': block(texts['buffer'], 'void AddQuadTrianglesImpl('),
        'SMOKE': block(texts['smoke'], 'void CSmokeProjectile::Draw()'),
        'SORT': block(texts['sort'], 'static inline uint32_t DistanceSortKey(')+'\n'+block(texts['sort'], 'static inline uint64_t ParticleSortKey('),
        'APPEND': block(texts['capture'], 'void Append('),
    }
    actual = texts['harness']
    for name, value in extracted.items():
        actual = actual.replace('/*@@'+name+'@@*/', value)
    assert '/*@@' not in actual
    cpp, binary = out/'Particles.actual.cpp', out/'test-native-world-particles'
    cpp.write_text(actual)
    command = ['clang++', '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pedantic', '-ffp-contract=off', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(src/'rts'), str(paths['geometry']), str(paths['recorder']), str(cpp), '-o', str(binary)]
    compile_result = subprocess.run(command, capture_output=True, text=True)
    (out/'compile.log').write_text(compile_result.stdout+compile_result.stderr)
    if compile_result.returncode:
        raise RuntimeError(compile_result.stderr[-6000:])
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    (out/'run.log').write_text(run.stdout+run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout+run.stderr)
    # Work-only original geometry mutation. The independent expected index
    # addresses must reject it; an upload/download roundtrip is not the oracle.
    needle = 'indcs.emplace_back(baseIndex + 3);'
    assert actual.count(needle) == 2
    negative_cpp, negative_binary = out/'Particles.bad-triangle.cpp', out/'test-bad-triangle'
    negative_cpp.write_text(actual.replace(needle, 'indcs.emplace_back(baseIndex + 2);', 1))
    negative_command = [str(negative_cpp) if value == str(cpp) else str(negative_binary) if value == str(binary) else value for value in command]
    negative_compile = subprocess.run(negative_command, capture_output=True, text=True)
    (out/'negative-compile.log').write_text(negative_compile.stdout+negative_compile.stderr)
    if negative_compile.returncode:
        raise RuntimeError(negative_compile.stderr[-4000:])
    negative = subprocess.run([str(negative_binary)], capture_output=True, text=True)
    (out/'negative.log').write_text(negative.stdout+negative.stderr)
    assert negative.returncode != 0 and 'original triangle order' in negative.stderr
    after = {str(path): sha(path.read_bytes()) for path in paths.values()}
    receipt = {'status': 'PASS', 'sourceUnchanged': before == after, 'sourceSHA256': before, 'extractionSHA256': {name: sha(value.encode()) for name, value in extracted.items()}, 'command': command, 'output': run.stdout, 'negative': {'scope': 'Work-only actual original quad index mutation', 'exit': negative.returncode, 'output': negative.stderr}, 'limitations': ['CPU geometry destination/original smoke billboard/sort/schema/lifetime tests only; no engine/app/GPU execution or playback.', 'Other projectile class geometry and actual runtime atlas assets require integration acceptance.']}
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    if before != after:
        raise RuntimeError('Source changed during fixture; rerun against frozen input')
    print(run.stdout, end='')


if __name__ == '__main__':
    main()
