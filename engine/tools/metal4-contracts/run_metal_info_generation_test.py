#!/usr/bin/env python3
"""Compile actual production resource generation and completion closure, not MSL/Metal."""
from pathlib import Path
import subprocess, tempfile, hashlib, os
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4MetalInfo.mm').read_text()
resources=s.split(' struct Resources {',1)[1].split('\n };',1)[0]
commit=s.split('out.commit=',1)[1].split(';return result;',1)[0]
clear=s.split('void MetalInfoGPU::ClearResources(){',1)[1].split('}',1)[0]
assert clear=='impl->resources=std::make_shared<Impl::Resources>();'
prepare=s.split('MetalInfoGPU::Prepare(',1)[1]
assert 'impl->target' not in prepare and 'impl->sources' not in prepare
assert 'const auto resources=impl->resources;' in s
assert '[owner=resources,lastSource]' in commit and 'weak.lock' not in commit
encode=s.split('void PreparedMetalInfo::EncodeDependencies(',1)[1].split('struct MetalInfoGPU::Impl',1)[0]
# Includes DidComplete: producer ordering and per-plan completion logic unchanged.
assert hashlib.sha256(encode.encode()).hexdigest()=='2acecad0ffa98db3efb5779c0a403024864c7fa36e52ba0ec6a2b833c5343330'
initialize=s.split('void MetalInfoGPU::Initialize(',1)[1].split('std::shared_ptr<PreparedMetalInfo> MetalInfoGPU::Prepare',1)[0]
assert hashlib.sha256(initialize.encode()).hexdigest()=='7e76768f637d842a01d49ca25c690f14f8efd711ce1f0e4135395c6aa831e419'
harness=r'''
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <cassert>
#include <iostream>
struct NativeMetalExtraction {int id;};
struct Extraction {std::shared_ptr<const NativeMetalExtraction> owner;};
struct Target {int id;};
struct Impl {struct Resources {RESOURCES
};std::shared_ptr<Resources> resources=std::make_shared<Resources>();};
int main(){
 for(int i=0;i<1000;++i){
  auto impl=std::make_shared<Impl>();auto old=impl->resources;
  auto key=std::make_shared<NativeMetalExtraction>(NativeMetalExtraction{1});
  auto extra=std::make_shared<NativeMetalExtraction>(NativeMetalExtraction{2});
  auto source=std::make_shared<Extraction>(Extraction{key});
  old->sources[key]=source;old->sources[extra]=std::make_shared<Extraction>(Extraction{extra});
  auto resources=old;auto lastSource=source;
  std::function<void(std::shared_ptr<Target>)> finish=COMMIT;
  auto target=std::make_shared<Target>(Target{1});finish(target);
  assert(old->target==target&&old->sources.size()==1&&old->sources.at(key)==source);
  // Clear replaces only resources. Old completion remains entirely old-generation.
  CLEAR
  auto fresh=impl->resources;assert(fresh!=old&&!fresh->target&&fresh->sources.empty());
  auto newer=std::make_shared<Target>(Target{2});fresh->target=newer;
  finish(target);assert(fresh->target==newer&&fresh->sources.empty());
  std::weak_ptr<Impl::Resources> weak=old;old.reset();resources.reset();
  assert(!weak.expired());finish={};assert(weak.expired());
  // Empty source roster follows the original clear-on-completion branch.
  resources=fresh;lastSource.reset();auto empty=COMMIT;
  fresh->sources[key]=source;empty(target);assert(fresh->target==target&&fresh->sources.empty());
  // Creating a request without invoking completion does not publish a target.
  resources=fresh;auto pending=COMMIT;(void)pending;assert(fresh->target==target);
 }
 std::cout<<"Actual MetalInfo generation/commit closure:1000 old/new generation, pruning, lifetime and pending cases passed\n";
}
'''.replace('RESOURCES',resources).replace('COMMIT',commit).replace('CLEAR',clear)
with tempfile.TemporaryDirectory() as tmp:
 path=Path(tmp)/'test.cpp';path.write_text(harness);exe=Path(tmp)/'test'
 subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(path),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Original GPU Encode/DidComplete hash preserved; no native Metal execution')
