from pathlib import Path
import re
import subprocess

directory=Path.cwd()
native=directory/'source/engine/rts/Rendering/Metal4'
load=lambda name:(native/name).read_text()

def span(body,first,last):
    start=body.index(first)
    return body[start:body.index(last,start)]

def call_arguments(body,start):
    depth=1
    for index in range(start,len(body)):
        depth+=(body[index]=='(')-(body[index]==')')
        if depth==0:
            return body[start:index]
    raise AssertionError('Unterminated registration')

# Compile-time classification is mandatory. Inspect all actual external callers
# and the one normal per-game summary. Counter bits can overlap within a frame.
expected={'Metal4Bloom.mm':{'Bloom'},'Metal4BumpWater.mm':{'BumpWater'},
          'Metal4CubeTextureSource.mm':{'Cube'},'Metal4WorldInfo.mm':{'WorldInfo'},
          'Metal4TerrainInfoSource.mm':{'TerrainInfo'},'Metal4ScreenNative.mm':{'ScreenTarget'},
          'Metal4PostprocessNative.mm':{'Postprocess'}}
seen={}
for file in sorted(native.rglob('*')):
    if file.suffix not in ('.mm','.h','.cpp','.inc') or file.name in ('Metal4RuntimeNative.mm','Metal4RuntimeNative.h'):
        continue
    body=file.read_text()
    for site in re.finditer(r'\.(OrderAfterPreviousSubmission|OrderGPUReadAfterSubmission)\(',body):
        args=call_arguments(body,site.end())
        match=re.search(r',\s*NativePriorRead::(\w+)\s*$',args)
        assert match,(file,site.group())
        seen.setdefault(file.name,set()).add(match[1])
assert seen==expected,(seen,expected)
runtime=load('Metal4RuntimeNative.mm')
assert 'unclassified=%llu' in runtime and 'NativePriorRead::Unclassified' in runtime
assert 'kind=NativePriorRead' not in load('Metal4RuntimeNative.h')
retire=span(runtime,'void Runtime::DrainOldestLocked(','FrameMetrics RuntimeLease::SubmitAndWait(')
assert retire.index('pending->error!=nil')<retire.index('pending->orderedGPUReadChecks')<retire.index('pending->retire(')<retire.index('[pending->drawableToPresent present]')
assert retire.index('[pending->drawableToPresent present]')<retire.index('pendingSubmissions.pop_front()')
info=load('Metal4WorldInfo.mm')
encode=span(info,'void PreparedWorldInfo::EncodeDependencies(','void PreparedWorldInfo::DidComplete()')
assert encode.index('update->EncodeDependencies(command)')<encode.index('impl->encodedSubmission=sourceSubmission')<encode.index('impl->encoded=true')
minimap=span(load('Metal4MiniMapNative.mm'),'void PreparedMiniMap::EncodeDependencies(','void PreparedMiniMap::RegisterInfoResources(')
assert 'generateMipmapsForTexture:impl->info' in minimap
assert 'impl->globalInfo->EncodeDependencies(command,impl->globalInfoSubmission)' in minimap
bloom=span(load('Metal4Bloom.mm'),'struct BloomRead158 {','ResolvedBloomImage ResolveBloomRead158(').replace('texture.','texture->')
writer=span(info,'bool PreparedWorldInfo::EncodedFullMipProducerOn(','void PreparedWorldInfo::RegisterResources(')
terrain=span(load('Metal4TerrainInfoSource.mm'),'const auto priorSubmission=resource->encodingSubmission;',' if(orderedPrior){').replace('texture.','texture->')

shared=r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
using std::uint64_t;
enum class NativePriorRead {Bloom,TerrainInfo};
constexpr int MTLStorageModePrivate=1,MTLTextureType2D=2;
struct Texture {
 int device=1,storageMode=MTLStorageModePrivate,textureType=MTLTextureType2D;
 int width=32,height=16,sampleCount=1,arrayLength=1,mipmapLevelCount=1;
 bool parentTexture=false,iosurface=false,buffer=false,heap=false;
};
struct RuntimeLease {
 uint64_t current=82,prior=81,event=17;
 bool admit=true,serial=true;
 std::function<void()> gpuCheck,cpuCheck;
 uint64_t SourceSubmission()const{return current;}
 uint64_t PendingEventForSource(uint64_t source)const{return source&&source==prior?event:0;}
 int Device()const{return 1;}
 bool OrderGPUReadAfterSubmission(const void*,uint64_t source,uint64_t e,std::function<void()> check,NativePriorRead){
  assert(e&&PendingEventForSource(source)==e);
  if(!admit)return false;
  gpuCheck=std::move(check);return true;
 }
 bool OrderAfterPreviousSubmission(const void*,std::function<void()> check,NativePriorRead){
  if(!serial)return false;
  cpuCheck=std::move(check);return true;
 }
};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>bool Throws(const F& f){try{f();return false;}catch(const std::runtime_error&){return true;}}
struct ImageState;
struct NativeBloomProducerState {uint64_t submissionIdentity=81;bool encoded=true,completed=false;};
struct NativeBloomImageVersion {
 struct Logical {uint64_t identity=25;int width=32,height=16;};
 std::shared_ptr<Logical> image=std::make_shared<Logical>();
 uint64_t generation=8;
 std::shared_ptr<NativeBloomProducerState> producer=std::make_shared<NativeBloomProducerState>();
 std::shared_ptr<ImageState> resource;
 auto Resource()const{return resource;}
};
struct ImageState {
 std::weak_ptr<const NativeBloomImageVersion> owner;
 std::shared_ptr<Texture> texture=std::make_shared<Texture>();
 std::shared_ptr<NativeBloomProducerState> producer;
};
struct PreparedWorldInfo {
 struct State {bool encoded=true;uint64_t encodedSubmission=81;bool metal=false;std::vector<int> updates{1};};
 std::shared_ptr<State> impl=std::make_shared<State>();
 std::shared_ptr<Texture> image=std::make_shared<Texture>();
 auto Info()const{return image;}
 bool EncodedFullMipProducerOn(uint64_t)const;
};
struct InfoResource {
 uint64_t encodingSubmission=81,contentVersion=3;
 bool completed=false,dependenciesEncoded=true;
 std::shared_ptr<PreparedWorldInfo> plan=std::make_shared<PreparedWorldInfo>();
};
struct Backing {std::shared_ptr<InfoResource> resource;uint64_t contentVersion=3;};
struct Owner {
 std::shared_ptr<InfoResource> resource;
 std::shared_ptr<Backing> backing;
};
bool BindInfo(RuntimeLease& lease,const std::shared_ptr<Owner>& owner,uint64_t submission){
 const auto resource=owner->resource;
 struct Output {std::shared_ptr<Texture> texture;};
 Output out{resource->plan->Info()};
'''
test=r'''
 return orderedPrior;
}
int main(){
 for(int kind=0;kind<8;++kind){
  RuntimeLease lease;
  auto version=std::make_shared<NativeBloomImageVersion>();
  auto image=std::make_shared<ImageState>();version->resource=image;image->owner=version;image->producer=version->producer;
  if(kind==1)image->texture->parentTexture=true;
  if(kind==2)image->texture->storageMode=9;
  if(kind==3)image->texture->mipmapLevelCount=2;
  if(kind==4)lease.prior=55;
  if(kind==5)lease.admit=false;
  if(kind==6)image->producer->submissionIdentity=lease.current;
  if(kind==7)image->producer->completed=true;
  auto read=PrepareBloomRead158(lease,image,version,lease.current);
  assert(bool(lease.gpuCheck)==(kind==0));assert(bool(lease.cpuCheck)==(kind>0&&kind<6));
  read.RequireReadable();
  if(kind==0){
   assert(Throws(lease.gpuCheck));image->producer->completed=true;lease.gpuCheck();
   image->texture=std::make_shared<Texture>();assert(Throws(lease.gpuCheck));
  }
 }
 for(int kind=0;kind<11;++kind){
  RuntimeLease lease;
  auto owner=std::make_shared<Owner>();owner->resource=std::make_shared<InfoResource>();owner->backing=std::make_shared<Backing>();
  auto& resource=*owner->resource;owner->backing->resource=owner->resource;
  auto& impl=*resource.plan->impl;
  if(kind==1)impl.encodedSubmission=72;
  if(kind==2)impl.updates.clear();
  if(kind==3)impl.metal=true;
  if(kind==4)resource.plan->image->heap=true;
  if(kind==5)resource.plan->image->storageMode=2;
  if(kind==6)lease.admit=false;
  if(kind==7)lease.prior=23;
  if(kind==8)resource.dependenciesEncoded=false;
  if(kind==9)resource.encodingSubmission=lease.current;
  if(kind==10)resource.completed=true;
  assert(BindInfo(lease,owner,lease.current)==(kind<9));
  assert(bool(lease.gpuCheck)==(kind==0));assert(bool(lease.cpuCheck)==(kind>0&&kind<9));
  if(kind==0){
   assert(Throws(lease.gpuCheck));resource.completed=true;lease.gpuCheck();
   owner->backing->contentVersion++;assert(Throws(lease.gpuCheck));owner->backing->contentVersion--;
   resource.plan->image=std::make_shared<Texture>();assert(Throws(lease.gpuCheck));
  }
 }
}
'''
# Place the native methods before the mock caller.
insert=shared.index('bool BindInfo(')
source=shared[:insert]+bloom+writer+shared[insert:]+terrain+test
p=directory/'cpu-tests/extracted-complete-prior-reads.cpp'
p.write_text(source)
exe=p.parent/'check-complete-prior-reads'
subprocess.run(['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS: all seven prior-read caller families named, actual source Bloom and terrain-info exact/fallback paths, full mip writer and FIFO success')
