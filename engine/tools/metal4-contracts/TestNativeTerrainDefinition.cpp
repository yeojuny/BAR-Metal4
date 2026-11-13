/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Links actual NativeTerrainSourceData.cpp validator. Consumer below is a CPU
// test double; no Metal pipelines, target readiness or performance is measured.
#include "Rendering/Backend/NativeTerrainSourceData.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rendering;
namespace {
unsigned checks=0;
void Require(bool b){++checks;if(!b)throw std::runtime_error("definition check "+std::to_string(checks)+" failed");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}Require(rejected);}
struct Consumer:NativeTerrainProgramConsumer {
 NativeTerrainProgramIdentity expected;uint64_t context=1,library=3;bool supported=true,immutable=false;
 mutable unsigned matches=0,supports=0;
 bool HasImmutableDefinition()const noexcept override{return immutable;}
 uint64_t ContextGeneration()const noexcept override{return context;}
 uint64_t LibraryGeneration()const noexcept override{return library;}
 bool Matches(const NativeTerrainProgramIdentity& i)const noexcept override{++matches;return i==expected;}
 bool Supports(const NativeTerrainProgramIdentity&)const noexcept override{++supports;return supported;}
};
}
int main(){
 int provenanceToken=0;NativeTerrainProgramIdentity id;
 id.contextGeneration=1;id.originalProgram=reinterpret_cast<const Shader::IProgramObject*>(&provenanceToken);
 id.originalProgramGeneration=2;id.sourceRevision=3;id.originalName="fixture";id.vertexSourceHash=4;id.fragmentSourceHash=5;
 id.defines={{"FOO","1"},{"_BAR2",""}};
 auto consumer=std::make_shared<Consumer>();consumer->expected=id;consumer->immutable=true;
 auto certified=BuildValidatedNativeTerrainDefinition(id,consumer);
 Require(consumer->matches==1&&consumer->supports==1);
 for(unsigned i=0;i<100;++i)ValidateNativeTerrainDefinition(*certified,1);
 Require(consumer->matches==1&&consumer->supports==1);
 NativeTerrainProgramDefinition unvalidated(id,consumer);
 ValidateNativeTerrainDefinition(unvalidated,1);Require(consumer->matches==2&&consumer->supports==2);
 Reject([&]{ValidateNativeTerrainDefinition(*certified,0);});Reject([&]{ValidateNativeTerrainDefinition(*certified,2);});
 consumer->context=2;Reject([&]{ValidateNativeTerrainDefinition(*certified,1);});consumer->context=1;
 consumer->library=4;Reject([&]{ValidateNativeTerrainDefinition(*certified,1);});consumer->library=0;
 Reject([&]{ValidateNativeTerrainDefinition(*certified,1);});consumer->library=3;
 NativeTerrainProgramDefinition nullOwner(id,{});Reject([&]{ValidateNativeTerrainDefinition(nullOwner,1);});
 Reject([&]{BuildValidatedNativeTerrainDefinition(id,{});});
 auto bad=[&](auto change){auto value=id;change(value);auto c=std::make_shared<Consumer>();c->expected=value;Reject([&]{BuildValidatedNativeTerrainDefinition(value,c);});};
 bad([](auto& x){x.contextGeneration=0;});bad([](auto& x){x.originalProgram=nullptr;});
 bad([](auto& x){x.originalProgramGeneration=0;});bad([](auto& x){x.sourceRevision=0;});
 bad([](auto& x){x.originalName.clear();});bad([](auto& x){x.vertexSourceHash=0;});bad([](auto& x){x.fragmentSourceHash=0;});
 bad([](auto& x){x.selection=static_cast<NativeTerrainProgramSelection>(255);});
 bad([](auto& x){x.defines={{"DUP","1"},{"DUP","2"}};});
 bad([](auto& x){x.defines={{"1BAD","1"}};});bad([](auto& x){x.defines={{"BAD-NAME","1"}};});
 bad([](auto& x){x.defines={{"","1"}};});bad([](auto& x){x.defines={{"OK",std::string("x\0y",3)}};});
 consumer->expected.sourceRevision=999;Reject([&]{BuildValidatedNativeTerrainDefinition(id,consumer);});consumer->expected=id;
 consumer->supported=false;Reject([&]{BuildValidatedNativeTerrainDefinition(id,consumer);});consumer->supported=true;
 // Public construction must not manufacture a validated proof.
 NativeTerrainProgramDefinition malformed(id,consumer);consumer->supported=false;
 Reject([&]{ValidateNativeTerrainDefinition(malformed,1);});consumer->supported=true;
 auto dynamic=std::make_shared<Consumer>();dynamic->expected=id;
 auto dynamicDefinition=BuildValidatedNativeTerrainDefinition(id,dynamic);
 Require(dynamic->matches==1&&dynamic->supports==1);
 ValidateNativeTerrainDefinition(*dynamicDefinition,1);Require(dynamic->matches==2&&dynamic->supports==2);
 dynamic->supported=false;Reject([&]{ValidateNativeTerrainDefinition(*dynamicDefinition,1);});
 dynamic->supported=true;dynamic->expected.sourceRevision=999;
 Reject([&]{ValidateNativeTerrainDefinition(*dynamicDefinition,1);});
 ValidateNativeTerrainDefinition(*certified,1);
 std::cout<<"PASS "<<checks<<" actual terrain-definition validator checks; Metal not exercised\n";
}
