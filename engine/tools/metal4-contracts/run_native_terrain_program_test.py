#!/usr/bin/env python3
"""Host contract test of the complete production terrain source translation unit.
Only external shader/VFS/consumer services are stubbed. No Write/Publish/binding
algorithm is copied or replaced. This is not Metal, Lua VM or gameplay validation.
"""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--eager-reference',action='store_true',help='Generated hook seals after accepted writes; same setter logic, not independent legacy oracle')
    ap.add_argument('--negative-control',action='store_true',help='Remove only aligned no-op return; expect allocation assertion failure')
    a=ap.parse_args(); out=a.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[2]
    source=root/'rts/Rendering/EngineBridge/NativeTerrainProgramSource.cpp'
    h=root/'rts/Rendering/EngineBridge/NativeTerrainProgramSource.h'
    terrain=(root/'rts/Rendering/Backend/NativeTerrainSourceData.h').read_text()
    water=(root/'rts/Rendering/Backend/NativeWaterGroundSourceData.h').read_text()
    shader=(root/'rts/Rendering/Shaders/Shader.h').read_text()
    structs=terrain[terrain.index('enum class NativeTerrainProgramSelection'):terrain.index('// Full source state:')]
    structs+=water[water.index('struct NativeTerrainUniformValue'):water.index('enum class NativeTerrainMeshSource')]
    declarations='\n'.join(line for line in h.read_text().splitlines() if not line.startswith('#include') and line!='#pragma once')
    cpp='\n'.join(line for line in source.read_text().splitlines() if not line.startswith('#include "'))
    if a.negative_control:
        needle='TERRAIN_WRITE_STAT(alignedNoOps);return;'
        assert cpp.count(needle)==1
        cpp=cpp.replace(needle,'TERRAIN_WRITE_STAT(alignedNoOps); /* negative control: no return */')
    if a.eager_reference:
        needle='MarkTerrainPending(this,observationLifetime,&SourceProgram::ObservePending,consumer);'
        assert cpp.count(needle)==1
        cpp=cpp.replace(needle,needle+' OriginalNativeSourceProgramBinding().Current(); /* fixture eager observation hook */')
    raw=shader[shader.index('\t\tvirtual void SetUniform1i('):shader.index('\n\tpublic:',shader.index('\t\tvirtual void SetUniform1i('))]
    prefix='''#include <array>
#include <cstdint>
#include <map>
#include <variant>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <utility>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
using GLsizei=int;
namespace Shader { struct IProgramObject; }
namespace rendering {
'''+structs+'}\n'+declarations+'''
namespace Shader {
struct IProgramObject {
 explicit IProgramObject(std::string n):name(std::move(n)){} virtual ~IProgramObject()=default;
 virtual void Link(){} virtual void Reload(bool,bool){} virtual bool Validate(){return false;}
 virtual void Enable(){bound=true;} virtual void EnableRaw(){} virtual void Disable(){bound=false;}
 virtual void DisableRaw(){} virtual void Release(){} virtual void BindAttribLocation(const std::string&,uint32_t){}
 virtual void BindOutputLocation(const std::string&,uint32_t){}
 virtual int GetUniformLoc(const char*)=0; virtual int GetUniformType(int)=0;
 int Slot(const char* n){return GetUniformLoc(n);}
 struct Flags {std::string GetString()const{return {};}; bool HashSet()const{return true;}
 bool Updated()const{return false;} void UpdateHash(){} } shaderFlags;
 bool bound=false,valid=false; unsigned objID=0;std::string name;
 std::map<std::string,int> attribLocations,outputLocations,uniformStates;
'''+raw+'''
}; }
struct Handler { Shader::IProgramObject* current=nullptr;
 auto GetCurrentlyBoundProgram(){return current;} void SetCurrentlyBoundProgram(Shader::IProgramObject* p){current=p;}
} handler;
auto* shaderHandler=&handler;
struct CFileHandler { explicit CFileHandler(const char*){} bool FileExists(){return true;}
 bool LoadStringData(std::string& s){s="fixture shader";return true;} };
#include "Rendering/Backend/NativeSourceProgramBinding.h"
'''
    # Compile the production definitions with diagnostics absent as well.
    plain=out/'terrain_program_diagnostics_off.cpp'; plain.write_text(prefix+'\n'+cpp)
    plain_command=[shutil.which('g++') or 'c++','-std=c++20','-fsyntax-only','-I',str(root/'rts'),str(plain)]
    plain_result=subprocess.run(plain_command,capture_output=True,text=True)
    (out/'diagnostics-off.log').write_text(plain_result.stdout+plain_result.stderr)
    if plain_result.returncode: print(plain_result.stderr);return plain_result.returncode
    fixture=Path(__file__).with_name('TestNativeTerrainProgram.cpp')
    unit=out/'terrain_program_test.cpp';unit.write_text(prefix+'\n'+cpp+'\n'+fixture.read_text())
    command=[shutil.which('g++') or 'c++','-std=c++20','-DBAR_NATIVE_TERRAIN_WRITE_STATS=1','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-Wno-unused-parameter','-Wno-misleading-indentation','-Wno-mismatched-new-delete','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'rts'),str(unit),'-o',str(out/'test')]
    if a.eager_reference: command.insert(2,'-DBAR_NATIVE_TERRAIN_EAGER_REFERENCE=1')
    result=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode: print(result.stderr);return result.returncode
    result=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(result.stdout+result.stderr)
    receipt={'scope':__doc__,'compilerVersion':subprocess.check_output([command[0],'--version'],text=True).splitlines()[0],'negativeControl':a.negative_control,'eagerObservationReference':a.eager_reference,'leakSanitizer':'disabled: execution sandbox uses ptrace; ASan and UBSan remain enabled','command':command,'sourceSHA256':hashlib.sha256(source.read_bytes()).hexdigest(),'compiledTranslationSHA256':hashlib.sha256(unit.read_bytes()).hexdigest(),'fixtureSHA256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'diagnosticsOffCompileExit':plain_result.returncode,'exit':result.returncode,'summary':result.stdout,'passed':result.returncode==0}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(result.stdout+result.stderr,end='');return result.returncode
if __name__=='__main__':raise SystemExit(main())
