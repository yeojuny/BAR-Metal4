#!/usr/bin/env python3
"""Host test of production immutable PBR declaration store, not Metal execution."""
import pathlib, subprocess, os
r=pathlib.Path(__file__).resolve().parents[2]
h=(r/'rts/Rendering/Backend/NativePBRDeclarationBatch.h').read_text().replace('#include "NativePBRSourceData.h"','').replace('#pragma once','')
pre='''#include <optional>
#include <atomic>
#include <cstdint>
#include <memory>
#include <cstdlib>
#include <new>
#include <cassert>
#include <iostream>
#include <thread>
static bool failAllocation=false;
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(void* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace rendering {
enum class NativePBRKind{BRDF,Environment};
struct NativePBRProgram {NativePBRKind kind;};
struct NativePBRImage {std::shared_ptr<const NativePBRProgram> program; mutable std::atomic<bool> completed{false};uint64_t sequence=0;};
}
'''
service=(r/'rts/Rendering/Metal4/Metal4PBRSourceService.mm').read_text()
prepare=service[service.index('std::unique_ptr<PreparedPBRSource> PBRSourceService::Prepare('):service.index('void PBRSourceService::ResetWorld()')]
fixture='''
namespace rendering::metal4 {
struct RuntimeLease {};
using CUSSourceImageProvider=int;
struct PreparedPBRSource {NativePBRDeclarationBatch images;};
struct TestGPU {std::unique_ptr<PreparedPBRSource> Prepare(RuntimeLease&,const NativePBRDeclarationBatch& images,const CUSSourceImageProvider&){return std::make_unique<PreparedPBRSource>(PreparedPBRSource{images});}};
class PBRSourceService {TestGPU gpu;public:
 std::unique_ptr<PreparedPBRSource> Prepare(RuntimeLease&,NativePBRKind,const CUSSourceImageProvider&,const NativePBRDeclarationReceipt&);
};
'''+prepare+'}\n'
world=(r/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
start=world.index(' std::optional<Metal4WorldRenderOptions> capturedOptions;')
end=world.index(' const auto& options=capturedOptions?*capturedOptions:sourceOptions;',start)+len(' const auto& options=capturedOptions?*capturedOptions:sourceOptions;')
branch=world[start:end]
maintenance=next(line for line in world.splitlines() if 'const auto pbrDeclarations=capturedPBR?' in line)
fixture+='\nnamespace rendering::metal4 {\n'+'''struct Metal4WorldRenderOptions {std::optional<NativePBRDeclarationReceipt> pbrDeclarations;};
NativePBRDeclarationReceipt nextCapture;unsigned captureCalls=0;
NativePBRDeclarationReceipt CapturePBRSource(){++captureCalls;return nextCapture;}
std::optional<NativePBRDeclarationReceipt> Resolve(const Metal4WorldRenderOptions& sourceOptions){
'''+branch+'return options.pbrDeclarations;}\nNativePBRDeclarationReceipt ResolveMaintenance(std::optional<NativePBRDeclarationReceipt> capturedPBR){\n'+maintenance+'\nreturn pbrDeclarations;}\n}\n'
test='''
int main(){using namespace rendering;unsigned checks=0;auto check=[&](bool v){++checks;assert(v);};
 auto make=[](NativePBRKind kind,uint64_t seq){auto p=std::make_shared<NativePBRImage>();p->program=std::make_shared<NativePBRProgram>(NativePBRProgram{kind});p->sequence=seq;return p;};
 NativePBRDeclarations store;check(!store.Capture());
 auto a=make(NativePBRKind::BRDF,1),b=make(NativePBRKind::Environment,2);
 store.Enqueue(a);auto one=store.Capture();
 metal4::PBRSourceService svc;metal4::RuntimeLease lease;
 auto initial=svc.Prepare(lease,NativePBRKind::BRDF,0,one);check(initial&&initial->images.size()==1&&initial->images.front()==a);
 check(!svc.Prepare(lease,NativePBRKind::Environment,0,one));check(one->size()==1);check(HasPendingNativePBR(one,NativePBRKind::BRDF));check(!HasPendingNativePBR(one,NativePBRKind::Environment));
 failAllocation=true;try{store.Enqueue(b);check(false);}catch(const std::bad_alloc&){}failAllocation=false;
 check(store.Capture()==one);store.Enqueue(b);auto two=store.Capture();check(one->size()==1);check(two->size()==2);
 check(!svc.Prepare(lease,NativePBRKind::Environment,0,one));
 check(svc.Prepare(lease,NativePBRKind::Environment,0,two)->images.front()==b);check((*two)[0]==a&&(*two)[1]==b);
 a->completed.store(true,std::memory_order_release);failAllocation=true;auto failedPrune=store.Capture();failAllocation=false;
 check(failedPrune==two);check(!svc.Prepare(lease,NativePBRKind::BRDF,0,two));check(!HasPendingNativePBR(failedPrune,NativePBRKind::BRDF));check(HasPendingNativePBR(failedPrune,NativePBRKind::Environment));
 metal4::nextCapture=two;
 metal4::Metal4WorldRenderOptions unspecified;auto captured=metal4::Resolve(unspecified);check(captured&&*captured==two&&metal4::captureCalls==1);
 metal4::Metal4WorldRenderOptions empty;empty.pbrDeclarations=NativePBRDeclarationReceipt{};captured=metal4::Resolve(empty);check(captured&&!*captured&&metal4::captureCalls==1);
 metal4::Metal4WorldRenderOptions supplied;supplied.pbrDeclarations=one;captured=metal4::Resolve(supplied);check(captured&&*captured==one&&metal4::captureCalls==1);
 check(metal4::ResolveMaintenance(std::nullopt)==two&&metal4::captureCalls==2);
 check(!metal4::ResolveMaintenance(NativePBRDeclarationReceipt{})&&metal4::captureCalls==2);
 check(metal4::ResolveMaintenance(one)==one&&metal4::captureCalls==2);
 auto pruned=store.Capture();check(pruned->size()==1&&pruned->front()==b);check(two->size()==2);
 store.Enqueue(b);auto duplicate=store.Capture();check(duplicate->size()==2&&(*duplicate)[0]==b&&(*duplicate)[1]==b);
 std::weak_ptr<const NativePBRImage> weak=b;b.reset();store.Reset();check(!weak.expired());check(duplicate->size()==2);
 for(unsigned i=0;i<2000;++i){
  auto x=make(NativePBRKind::BRDF,i);store.Enqueue(x);auto before=store.Capture();
  std::thread completion([x]{x->completed.store(true,std::memory_order_release);});
  auto y=make(NativePBRKind::Environment,i);store.Enqueue(y);check(before->back()==x);completion.join();
  auto after=store.Capture();check(after->size()==1&&after->front()==y);check(!HasPendingNativePBR(before,NativePBRKind::BRDF));store.Reset();check(after->front()==y);
 }
 std::cout<<"PASS "<<checks<<" production PBR declaration ownership checks; Metal not exercised\\n";
}
'''
out=pathlib.Path('/tmp/bar-pbr-declarations');out.mkdir(exist_ok=True)
f=out/'test.cpp';f.write_text(pre+h+fixture+test)
subprocess.run(['g++','-std=c++20','-pthread','-O1','-g','-fsanitize=address,undefined',str(f),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
# Real compiler and allocation hooks stay synchronous; all render selections take a receipt.
s=(r/'rts/Rendering/Metal4/Metal4PBRSourceService.mm').read_text()
assert 'RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime);' in s
assert 'return gpu.Compile(lease,*captured,validation,compileMilliseconds);' in s
assert 'InvokeNativeRendererControl(*renderWorker,*gate,' in s
assert 'return ReserveNativePBRImageForCapture(allocation);' in s
assert 'for(const auto& p:*batch)' in s
print('Real immediate compile/allocation hooks preserved; captured batch selection present')
world=(r/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
bridge=(r/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'pbrSource.HasPending(' not in world and 'pbrSource.PruneCompleted(' not in world
assert 'pbrSource.Prepare(lease,NativePBRKind::BRDF,{},pbrDeclarations)' in world
assert 'pbrSource.Prepare(lease,NativePBRKind::BRDF,pbrImages,*options.pbrDeclarations)' in world
assert 'pbrSource.Prepare(lease,NativePBRKind::Environment,pbrImages,*options.pbrDeclarations)' in world
assert 'SubmitMaintenance(&frame, {}, shaderValidation, {}, CapturePBRSource())' in world
assert bridge.count('options.pbrDeclarations=')==2
assert 'options.pbrDeclarations=session->renderer->CapturePBRSource();' in bridge
assert 'options.pbrDeclarations=pbrDeclarations;' in bridge
assert bridge.index('const auto pbrDeclarations=')<bridge.index('HasPendingPBRSource(pbrDeclarations)')
assert 'SubmitMaintenance(screenUI, std::move(worldInfo), false, luaUnitStencilFrame,pbrDeclarations)' in bridge
assert 'HasPendingEnvironmentSource(session->environmentStage,pbrDeclarations)' in bridge
assert 'HasPendingPBRSource()' not in bridge
print('Normal, SaveImage, independent target and maintenance PBR batch wiring checked')

dispatch=world.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
prepared=world.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert 'if(!sourceOptions.pbrDeclarations)' in dispatch and 'CapturePBRSource()' in dispatch
assert 'CapturePBRSource()' not in prepared and 'CapturePending()' not in prepared
assert 'capturedPBR?*capturedPBR:CapturePBRSource()' in world
print('Legacy main API captures unspecified input; explicit empty snapshots are not recaptured')
