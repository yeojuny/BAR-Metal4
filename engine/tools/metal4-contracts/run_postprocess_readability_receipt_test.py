#!/usr/bin/env python3
"""Production-extracted owned read receipt; no Metal execution or concurrent flag access."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4PostprocessNative.mm').read_text()
start=s.index('{',s.index('std::function<void()> PostprocessGPU::CaptureReadability('))+1
depth=1
for end in range(start,len(s)):
 depth+=(s[end]=='{')-(s[end]=='}')
 if not depth:break
body=s[start:end]
world=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'postprocessGPU.RequireReadable(' not in world
assert world.count('postprocessGPU.CaptureReadability(')==3
assert world.index('result.texture=postprocessGPU.ResolveImage')<world.index('result.requireReadable=postprocessGPU.CaptureReadability')
assert world.index('target.previous=impl->postprocessGPU.ResolveImage')<world.index('target.requirePreviousReadable=impl->postprocessGPU.CaptureReadability')
source=r'''
#include <cassert>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>
void Require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
struct NativePostprocessVersion {bool image=true,cpu=false;};
bool HasNativePostprocessCPUContents(const NativePostprocessVersion& v){return v.cpu;}
struct PostprocessGPU {
 struct Image {std::weak_ptr<const NativePostprocessVersion> owner;std::shared_ptr<bool> completed=std::make_shared<bool>(false),encoded=std::make_shared<bool>(false);};
 struct Impl {std::map<const NativePostprocessVersion*,Image> images;};
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 std::function<void()> CaptureReadability(const std::shared_ptr<const NativePostprocessVersion>& version)const{
'''+body+r'''
 }
};
template<class F>bool Throws(F f){try{f();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 for(bool encoded:{false,true})for(bool completed:{false,true}){
  auto gpu=std::make_unique<PostprocessGPU>();auto version=std::make_shared<NativePostprocessVersion>();
  std::weak_ptr<const NativePostprocessVersion> weak=version;
  auto& image=gpu->impl->images[version.get()];image.owner=version;
  auto e=image.encoded,c=image.completed;
  // Receipt creation is allowed before producer encode and must not publish readiness.
  auto check=gpu->CaptureReadability(version);assert(!*e&&!*c);
  auto another=std::make_shared<NativePostprocessVersion>();
  gpu->impl->images[version.get()]={};gpu->impl->images[version.get()].owner=another;
  *gpu->impl->images[version.get()].encoded=true;
  version.reset();gpu.reset();assert(!weak.expired());
  assert(Throws(check)); // Different newer allocation's flag cannot satisfy this read.
  *e=encoded;*c=completed;assert(Throws(check)==!(encoded||completed));
  check={};assert(weak.expired());
 }
 // CPU source retains original no-GPU-check behavior and exact owner lifetime.
 {PostprocessGPU gpu;auto version=std::make_shared<NativePostprocessVersion>();version->cpu=true;
  std::weak_ptr<const NativePostprocessVersion> weak=version;auto check=gpu.CaptureReadability(version);
  version.reset();assert(!weak.expired());check();check={};assert(weak.expired());}
 // Missing version/allocation/entry/owner cannot fabricate a readable receipt.
 for(int invalid=0;invalid<5;++invalid){PostprocessGPU gpu;auto version=std::make_shared<NativePostprocessVersion>();
  if(invalid==0){assert(Throws([&]{gpu.CaptureReadability({});}));continue;}
  if(invalid==1)version->image=false;
  auto other=std::make_shared<NativePostprocessVersion>();
  if(invalid==3)gpu.impl->images[version.get()].owner=other;
  if(invalid==4)gpu.impl->images[version.get()]={};
  assert(Throws([&]{gpu.CaptureReadability(version);}));
 }
}
'''
with tempfile.TemporaryDirectory(prefix='bar-read-receipt-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
print('PASS: 10 exact readability receipt scenarios ASan/UBSan; 3 production provider callsites wired; native GPU untested')
