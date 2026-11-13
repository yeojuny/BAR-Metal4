#!/usr/bin/env python3
"""Extract the real sampling accessor; exercise immutable reads during renderer ownership.
Not a native Metal attachment allocation test.
"""
import os, re, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
accessor=re.search(r'NativeMainSampling Metal4WorldRenderer::ActualMainSampling\(\) const \{.*?\n[^\n]*\}',s,re.S).group()
assert 'Scope' not in accessor and 'return *impl->mainSampling;' in accessor
assert 'std::unique_ptr<const NativeMainSampling> mainSampling;' in s
assert len(re.findall(r'impl->mainSampling\s*=',s))==1
assert 'mainSampling.allocationGeneration=' not in s and 'mainSampling->allocationGeneration=' not in s
assert s.index('impl->mainSampling=std::make_unique<const NativeMainSampling>') < s.index('impl->Pipelines(')
assert 'mainStorage.Prepare(lease,result.surface.renderWidth' in s
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert bridge.count('ActualMainSampling()')>=2
out=Path('/tmp/bar-sampling-capture');out.mkdir(exist_ok=True)
(out/'test.cpp').write_text('''
#include "Rendering/Backend/NativeMainSamplingData.h"
#include "Rendering/Backend/NativeRendererAccessGate.h"
#include <memory>
#include <future>
#include <cassert>
#include <iostream>
using namespace rendering;
struct Metal4WorldRenderer {
 struct Impl { std::unique_ptr<const NativeMainSampling> mainSampling; NativeRendererAccessGate gate; };
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 NativeMainSampling ActualMainSampling() const;
};
'''+accessor+'''
int main(){
 Metal4WorldRenderer renderer;
 renderer.impl->mainSampling=std::make_unique<const NativeMainSampling>(NativeMainSampling{8,4,4,11});
 std::promise<void> held,release;auto released=release.get_future();
 auto worker=std::async(std::launch::async,[&]{
   NativeRendererAccessGate::Scope owner(&renderer.impl->gate);
   held.set_value();released.wait();
   // Render-side attachment generations change independently of startup metadata.
   unsigned generation=11;for(unsigned i=0;i<10000;++i)++generation;
   assert(generation==10011);
 });
 held.get_future().wait();
 for(unsigned i=0;i<10000;++i){auto value=renderer.ActualMainSampling();
   assert(value.requested==8&&value.effective==4&&value.samples==4&&value.allocationGeneration==11);
   value.samples=1;assert(renderer.ActualMainSampling().samples==4);
 }
 release.set_value();worker.get();
 std::cout<<"10000 actual accessor reads while renderer gate held; startup witness unchanged\\n";
}
''')
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'rts'),str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Immutable storage / constructor / actual capture call sites checked; no native Metal execution')
