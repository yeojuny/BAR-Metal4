#!/usr/bin/env python3
"""Host-test production image table capture/proof functions with history/object stubs."""
import argparse, pathlib, subprocess, os, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=pathlib.Path(__file__).resolve().parents[2]
h=(r/'rts/Rendering/Backend/NativeTerrainSourceData.h').read_text();s=(r/'rts/Rendering/Backend/NativeTerrainSourceData.cpp').read_text()
unit=r'''#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <utility>
using std::size_t;
enum class NativeCUSTextureTarget {Image2D,Image3D,Cube,Image2DMultisample,Image2DArray,Rectangle,Count};
enum class NativeCUSBindingKnowledge {Unobserved,DefaultObject,BoundObject};
enum class NativeTextureStorageKnowledge {Published,Empty};
struct NativeTextureSampling {bool valid=true;};
struct NativeCUSTextureObject {uint64_t context=1;NativeCUSTextureTarget target=NativeCUSTextureTarget::Image2D;bool valid=true;NativeTextureStorageKnowledge storage=NativeTextureStorageKnowledge::Published;bool image=true,backing=false;int version=1;};
struct NativeCUSTextureBinding {NativeCUSBindingKnowledge knowledge=NativeCUSBindingKnowledge::Unobserved;std::shared_ptr<const NativeCUSTextureObject>object;NativeTextureSampling defaultSampling;};
struct NativeCUSTextureHistory {static constexpr unsigned Units=32;std::array<std::array<NativeCUSTextureBinding,6>,32> state{};uint64_t seq=1;std::optional<unsigned>active=0;uint64_t Sequence()const{return seq;}auto ActiveUnit()const{return active;}auto Binding(unsigned u,NativeCUSTextureTarget t)const{return state[u][size_t(t)];}};
struct NativeTextureRegistry {uint64_t context=1;bool matches=true;uint64_t ContextGeneration()const{return context;}bool MatchesHistory(const NativeCUSTextureHistory&)const{return matches;}};
static unsigned objectChecks=0,samplingChecks=0;
void Check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
size_t Target(NativeCUSTextureTarget t){Check(t<NativeCUSTextureTarget::Count,"target");return size_t(t);}
void Object(const NativeCUSTextureObject& o,uint64_t context,NativeCUSTextureTarget t){++objectChecks;Check(o.valid&&o.context==context&&o.target==t,"object");}
void ValidateNativeTerrainSampling(const NativeTextureSampling&s){++samplingChecks;Check(s.valid,"sampling");}
'''
unit+=h[h.index('struct NativeTerrainImageSnapshot'):h.index('struct NativeTerrainSquareBacking')]
unit+=s[s.index('namespace {\nvoid ValidateTerrainImageTable'):s.index('void ValidateNativeTerrainSquareBacking')]
unit+=r'''
int main(){
 unsigned checks=0;auto check=[&](bool v){++checks;assert(v);};auto rejects=[&](auto fn){bool threw=false;try{fn();}catch(const std::runtime_error&){threw=true;}check(threw);};
 NativeTextureRegistry registry;NativeCUSTextureHistory history;
 for(auto& row:history.state)for(auto& b:row)b.knowledge=NativeCUSBindingKnowledge::DefaultObject;
 auto object=std::make_shared<NativeCUSTextureObject>();
 history.state[0][0].knowledge=NativeCUSBindingKnowledge::BoundObject;history.state[0][0].object=object;
 auto captured=CaptureNativeTerrainImages(registry,history,7);
 check(objectChecks==1&&samplingChecks==191);
 for(unsigned u=0;u<32;++u)for(unsigned t=0;t<6;++t){const auto& b=captured.Units()[u][t];const auto& ref=history.state[u][t];check(b.knowledge==ref.knowledge&&b.object==ref.object&&b.defaultSampling.valid==ref.defaultSampling.valid);}
 auto copy=captured;check(&copy.Units()==&captured.Units());
 for(unsigned i=0;i<100;++i)ValidateNativeTerrainImages(copy,1,7);
 check(objectChecks==1&&samplingChecks==191); // No repeated static table scan.
 auto& sampled=RequireNativeTerrainImage(copy,0,NativeCUSTextureTarget::Image2D);
 check(sampled.object==object&&objectChecks==2); // Actual consumption still checks object.
 auto newer=std::make_shared<NativeCUSTextureObject>(*object);newer->version=2;
 history.state[0][0].object=newer;++history.seq;
 auto next=CaptureNativeTerrainImages(registry,history,7);
 check(next.Units()[0][0].object==newer&&captured.Units()[0][0].object==object);
 history.state[3][0]=history.state[0][0];auto shared=CaptureNativeTerrainImages(registry,history,7);
 check(shared.Units()[0][0].object==shared.Units()[3][0].object);
 auto moved=std::move(shared);ValidateNativeTerrainImages(moved,1,7);rejects([&]{ValidateNativeTerrainImages(shared,1,7);});
 history.state[0][0]={};++history.seq;auto deleted=CaptureNativeTerrainImages(registry,history,7);
 check(!deleted.Units()[0][0].object&&captured.Units()[0][0].object==object);
 rejects([&]{RequireNativeTerrainImage(deleted,0,NativeCUSTextureTarget::Image2D);});
 rejects([&]{ValidateNativeTerrainImages(captured,2,7);});rejects([&]{ValidateNativeTerrainImages(captured,1,8);});
 auto forged=captured;forged.contextGeneration=2;rejects([&]{ValidateNativeTerrainImages(forged,2,7);});
 forged=captured;forged.activeUnit=32;rejects([&]{ValidateNativeTerrainImages(forged,1,7);});
 forged=captured;forged.eventSequence=0;rejects([&]{ValidateNativeTerrainImages(forged,1,7);});
 NativeTerrainImageSnapshot empty;empty.contextGeneration=1;empty.sourceDrawFrame=7;empty.eventSequence=1;empty.activeUnit=0;
 rejects([&]{ValidateNativeTerrainImages(empty,1,7);});rejects([&]{empty.Units();});
 history.state[1][0].defaultSampling.valid=false;rejects([&]{CaptureNativeTerrainImages(registry,history,7);});history.state[1][0].defaultSampling.valid=true;
 history.state[2][0].knowledge=NativeCUSBindingKnowledge::BoundObject;rejects([&]{CaptureNativeTerrainImages(registry,history,7);});
 history.state[2][0].object=newer;auto invalid=std::make_shared<NativeCUSTextureObject>(*newer);invalid->context=2;history.state[2][0].object=invalid;rejects([&]{CaptureNativeTerrainImages(registry,history,7);});
 std::cout<<"PASS "<<checks<<" image proof checks; 100 validations without repeated table scan; GPU/history integration not executed\n";
}
'''
f=a.output/'test.cpp';f.write_text(unit);command=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(f),'-o',str(a.output/'test')]
subprocess.run(command,check=True);env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
run=subprocess.run([str(a.output/'test')],env=env,capture_output=True,text=True);print(run.stdout,end='');print(run.stderr,end='');
(a.output/'receipt.json').write_text(json.dumps({'exit':run.returncode,'summary':run.stdout,'sourceSHA256':hashlib.sha256(s.encode()).hexdigest(),'headerSHA256':hashlib.sha256(h.encode()).hexdigest(),'scope':__doc__,'asan':True,'ubsan':True,'gameRun':False},indent=2));raise SystemExit(run.returncode)
