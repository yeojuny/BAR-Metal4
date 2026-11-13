#include "Rendering/Backend/NativeCUSProgramData.h"
#include <cassert>
#include <bit>
#include <cstdint>
#include <iostream>
using namespace rendering;
int main(){
 NativeCUSProgramUniformBuilder b;
 b.Initialize({{"x",{1,2}}},{{"i",{3}}});
 auto first=b.Capture();assert(first==b.Capture());
 b.Write(false,"x",{1,2});assert(first==b.Capture());
 b.Write(true,"i",{3});assert(first==b.Capture());
 b.Write(false,"x",{2,3});auto second=b.Capture();assert(first!=second);
 assert(first->floats.at("x")==std::vector<float>({1,2}));
 assert(second->floats.at("x")==std::vector<float>({2,3}));
 b.Write(false,"zero",{0.0f});auto zero=b.Capture();
 b.Write(false,"zero",{-0.0f});auto negzero=b.Capture();assert(zero!=negzero);
 assert(std::bit_cast<uint32_t>(zero->floats.at("zero")[0])==0);
 assert(std::bit_cast<uint32_t>(negzero->floats.at("zero")[0])==0x80000000);
 b.Write(false,"x",{4});b.Write(true,"i",{9});auto changed=b.Capture();
 assert(changed==b.Capture());assert(second->ints.at("i")[0]==3);
 b.Initialize({},{});assert(b.Capture()->floats.empty());assert(!changed->floats.empty());
 // Reinitializing does not mutate previous frames or alias a same-valued relink.
 b.Initialize({{"x",{1,2}}},{{"i",{3}}});assert(b.Capture()!=first);
 NativeCUSSourceDraw d;d.uniformValues=negzero;
 const NativeCUSGeneratedField fields[]={
  {"zero",NativeCUSUniformScalar::Float,0,1,1,4,true},
  {"i",NativeCUSUniformScalar::Int,4,1,1,4,true},
  {"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,8,1,1,4,true}};
 NativeCUSGeneratedStage stage{"",fields,12,0};NativeCUSProgramSource source;NativeCUSSampleTransport transport;transport.clipFlags=5;
 auto bytes=PackNativeCUSProgramUniforms(stage,d,source,transport);
 uint32_t z=0,i=0,flags=0;std::memcpy(&z,bytes.data(),4);std::memcpy(&i,bytes.data()+4,4);std::memcpy(&flags,bytes.data()+8,4);
 assert(bytes.size()==16&&z==0x80000000&&i==3&&flags==5);
 transport.clipFlags=7;auto late=PackNativeCUSProgramUniforms(stage,d,source,transport);
 assert(std::memcmp(late.data(),bytes.data(),8)==0&&late[8]==7);
 assert(d.uniformValues==negzero);
 d.uniformValues=b.Capture();bool rejected=false;
 try{PackNativeCUSProgramUniforms(stage,d,source,transport);}catch(const std::runtime_error&){rejected=true;}
 assert(rejected); // Missing zero remains an error, not an implicit default.
 NativeCUSSourceFrame whole;whole.sourceDrawFrame=91;
 NativeCUSLoadedSource manifest;manifest.path="source.lua";whole.sourceLoads=std::make_shared<const std::vector<NativeCUSLoadedSource>>(std::vector<NativeCUSLoadedSource>{manifest});
 NativeCUSSourceDraw keep,drop;keep.sequence=1;drop.sequence=2;
 keep.uniformValues=first;drop.uniformValues=std::make_shared<const NativeCUSProgramUniforms>();
 std::weak_ptr<const NativeCUSProgramUniforms> excluded=drop.uniformValues;
 whole.draws={keep,drop};drop.uniformValues.reset();
 auto selection=SelectNativeCUSSourceFrame(whole,[](const auto& item){return item.sequence==1;});
 assert(selection->sourceDrawFrame==91&&selection->draws.size()==1&&selection->draws.front().uniformValues==first);
 assert(selection->sourceLoads==whole.sourceLoads);
 whole.draws.clear();assert(excluded.expired()); // Selection must not pin other partition.
 auto empty=SelectNativeCUSSourceFrame(*selection,[](const auto&){return false;});assert(empty->draws.empty());
 std::cout<<"CUS immutable uniform content and production packing: passed\n";
}
