#!/usr/bin/env python3
"""Compile exact production readiness/sealing methods with CPU stand-ins.
This does not compile Objective-C++ or test a Metal library/PSO. The production
factory ordering is guarded structurally; runtime tests cover the extracted
certificate methods, including failed seal and independence from mutable caches.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def block(text, signature):
    start = text.index(signature)
    brace = text.index(')const {', start) + len(')const ') if signature == 'id<MTLRenderPipelineState> Pipeline(' else text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[2]
    source = root / 'rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm'
    text = source.read_text()
    cert = block(text, 'struct LinkCertificate') + ';'
    seal = block(text, 'void SealInitialLink()')
    methods = '\n'.join(block(text, sig).replace(' override', '') for sig in [
        'uint64_t ContextGeneration()', 'uint64_t LibraryGeneration()',
        'bool HasImmutableDefinition()', 'bool Matches(', 'bool Supports('])
    assert 'pipelines' not in methods and 'library&&' not in methods
    factory = block(text, 'NativeTerrainProgramLink TerrainSourceGPU::CompileOriginalProgram(')
    ordered = ['original->library=lease.NewLibraryWithSource',
               'Require(original->library!=nil', 'original->Pipeline(',
               'original->SealInitialLink();', 'ValidateNativeTerrainProgram(',
               'return {original,{},original->profile.defaults};']
    positions = [factory.index(item) for item in ordered]
    assert positions == sorted(positions), 'Real factory publication order changed'
    assert 'friend class ::rendering::metal4::TerrainSourceGPU;' in text
    # Preserve late physical-target checks independently of the new certificate.
    pipeline = block(text, 'id<MTLRenderPipelineState> Pipeline(')
    for guard in ['RequireTarget(target)', 'supportsTextureSampleCount:target.samples',
                  'library!=nil&&library.device==lease.Device()', 'Require(pipeline!=nil']:
        assert guard in pipeline, 'Late target/device/PSO check disappeared: ' + guard
    cpp = '''#include <cstdint>
#include <memory>
#include <map>
#include <stdexcept>
#include <iostream>
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
namespace rendering::metal4 {
class TerrainSourceGPU;
struct NativeTerrainProgramIdentity {uint64_t contextGeneration=1,revision=1;
 bool operator==(const NativeTerrainProgramIdentity&)const=default;};
class OriginalProgram {
private:
'''+cert+'''
 std::unique_ptr<const LinkCertificate> linkCertificate;
 friend class ::rendering::metal4::TerrainSourceGPU;
'''+seal+'''
public:
 const NativeTerrainProgramIdentity identity{};
 uint64_t generation=7;
 bool library=false;
 std::map<int,int> pipelines;
'''+methods+'''
};
class TerrainSourceGPU {public:static void Seal(OriginalProgram& p){p.SealInitialLink();}};
}
int checks=0;
void check(bool b){++checks;if(!b)throw std::runtime_error("check failed");}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected);}
int main(){using namespace rendering::metal4;
 OriginalProgram p;
 check(p.LibraryGeneration()==0&&!p.Supports(p.identity));
 rejects([&]{TerrainSourceGPU::Seal(p);});
 check(p.LibraryGeneration()==0);
 p.library=true;
 rejects([&]{TerrainSourceGPU::Seal(p);});
 check(p.LibraryGeneration()==0);
 p.pipelines.emplace(1,1);p.generation=0;
 rejects([&]{TerrainSourceGPU::Seal(p);});
 p.generation=7;TerrainSourceGPU::Seal(p);
 check(p.LibraryGeneration()==7&&p.Supports(p.identity));
 auto other=p.identity;++other.revision;check(!p.Supports(other));
 rejects([&]{TerrainSourceGPU::Seal(p);});
 // Deliberately destroy the CPU stand-in mutable state. Production never clears
 // these; this witness proves source getters have no cache dependency at all.
 p.pipelines.clear();p.library=false;p.generation=999;
 check(p.LibraryGeneration()==7&&p.Supports(p.identity));
 check(p.ContextGeneration()==1);
 std::cout<<"PASS "<<checks<<" extracted terrain link certificate checks; Metal not exercised\\n";
}
'''
    unit = out / 'terrain_link_certificate.cpp'
    unit.write_text(cpp)
    cmd = [shutil.which('g++') or 'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
           '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           str(unit), '-o', str(out/'test')]
    compile_run = subprocess.run(cmd, capture_output=True, text=True)
    (out/'compile.log').write_text(compile_run.stdout+compile_run.stderr)
    if compile_run.returncode:
        raise RuntimeError(compile_run.stderr)
    import os
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    result = subprocess.run([str(out/'test')], env=env, capture_output=True, text=True)
    (out/'run.log').write_text(result.stdout+result.stderr)
    receipt = {'scope': __doc__, 'sourceSHA256': hashlib.sha256(source.read_bytes()).hexdigest(),
               'command': cmd, 'exit': result.returncode, 'summary': result.stdout,
               'sanitizers': 'ASan+UBSan; LSan disabled for ptrace-compatible execution',
               'structuralFactoryOrder': ordered, 'passed': result.returncode == 0}
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(result.stdout+result.stderr, end='')
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
