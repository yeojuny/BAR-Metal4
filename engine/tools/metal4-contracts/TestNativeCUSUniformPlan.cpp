#include "Rendering/Backend/NativeCUSProgramData.h"
#include "Rendering/Metal4/NativeCUSGeneratedPrograms.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace rendering;
static bool Dynamic(std::string_view n){return n.starts_with("NativeCUSWater")||n=="NativeCUSImageMasks"||n=="NativeCUSSampleCodes"||n=="NativeCUSClipFlags";}
template<class F> static void Reject(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
int main(){
 size_t comparisons=0,hits=0,builds=0,stages=0;
 NativeCUSProgramSource source;NativeCUSSampleTransport transport;
 for(const auto& program:rendering::metal4::NativeCUSGeneratedPrograms){
  for(const auto* stage:{&program.vertex,&program.fragment}){
   ++stages;NativeCUSProgramUniformPlan plan(*stage);
   NativeCUSProgramUniforms values;
   for(const auto& f:stage->fields)if(f.active&&!Dynamic(f.name)){
    auto& v=(f.scalar==NativeCUSUniformScalar::Float?values.floats:values.ints)[f.name];
    v.resize(size_t(f.components)*f.count);for(size_t i=0;i<v.size();++i)v[i]=f.scalar==NativeCUSUniformScalar::Float?float(i)*0.25f-0.0f:float(i);
   }
   NativeCUSSourceDraw d;d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>(values);
   std::vector<uint8_t> bytes(NativeCUSProgramUniformBytes(*stage),0xCD);
   for(unsigned n=0;n<24;++n){
    transport.clipFlags=n%3;transport.emptyMask=n;transport.flipMask=n*2;transport.depthMask=n*3;transport.clampDepthCompareMask=n*4;
    for(size_t i=0;i<12;++i)transport.sampleCodes[i]=uint32_t(i+n);
    for(size_t i=0;i<3;++i)for(size_t j=0;j<3;++j)source.waterColors[i][j]=float(i+j+n)*0.125f;
    const bool rebuilt=plan.Pack(d,source,transport,bytes);builds+=rebuilt;hits+=!rebuilt;
    assert(rebuilt==(n==0||n==12));
    assert(bytes==PackNativeCUSProgramUniforms(*stage,d,source,transport));++comparisons;
    // Equal values, different immutable owner: must rebuild, not use address/value heuristics.
    if(n==11)d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>(values);
   }
   std::weak_ptr<const NativeCUSProgramUniforms> weak=d.uniformValues;d.uniformValues.reset();assert(weak.expired());
  }
 }
 // Independent padding/bit-level fixture with dynamic and static fields interleaved.
 NativeCUSGeneratedField fields[]={
  {"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,8,1,1,4,true},
  {"x",NativeCUSUniformScalar::Float,0,1,1,4,true},
  {"i",NativeCUSUniformScalar::Int,4,1,1,4,true}};
 NativeCUSGeneratedStage stage{"",fields,12,0};NativeCUSProgramUniformPlan plan(stage);
 NativeCUSSourceDraw d;d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>(NativeCUSProgramUniforms{{{"x",{-0.0f}}},{{"i",{-17}}}});
 auto saved=d.uniformValues;transport.clipFlags=5;std::vector<uint8_t> out(16,0xCD);
 assert(plan.Pack(d,source,transport,out));assert(!plan.Pack(d,source,transport,out));
 uint32_t x;int32_t i;std::memcpy(&x,out.data(),4);std::memcpy(&i,out.data()+4,4);assert(x==0x80000000&&i==-17&&out[8]==5&&out[12]==0);
 // B then A is a bounded-cache miss, with byte-exact reconstruction.
 auto other=std::make_shared<const NativeCUSProgramUniforms>(*saved);d.uniformValues=other;
 assert(plan.Pack(d,source,transport,out));d.uniformValues=saved;assert(plan.Pack(d,source,transport,out));
 assert(!plan.Pack(d,source,transport,out));
 // Failed new owner must not replace the valid template/key.
 d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>();Reject([&]{plan.Pack(d,source,transport,out);});
 d.uniformValues=saved;assert(!plan.Pack(d,source,transport,out));
 auto bad=NativeCUSProgramUniforms{{{"x",{std::numeric_limits<float>::infinity()}}},{{"i",{0}}}};
 d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>(bad);Reject([&]{plan.Pack(d,source,transport,out);});
 bad.floats["x"]={1};bad.ints["i"]={1.5f};d.uniformValues=std::make_shared<const NativeCUSProgramUniforms>(bad);Reject([&]{plan.Pack(d,source,transport,out);});
 d.uniformValues=saved;Reject([&]{plan.Pack(d,source,transport,std::span(out).first(12));});
 // Same address with different control block is not exact-owner reuse.
 d.uniformValues=std::shared_ptr<const NativeCUSProgramUniforms>(saved.get(),[](const auto*){});assert(plan.Pack(d,source,transport,out));
 // Same control block with different aliased object also cannot hit.
 struct Pair{NativeCUSProgramUniforms a,b;};auto pair=std::make_shared<Pair>();pair->a=*saved;pair->b=*saved;pair->b.floats["x"]={2};
 d.uniformValues={pair,&pair->a};assert(plan.Pack(d,source,transport,out));d.uniformValues={pair,&pair->b};assert(plan.Pack(d,source,transport,out));
 assert(out==PackNativeCUSProgramUniforms(stage,d,source,transport));
 // Overlapping descriptors keep original ordered writes, rather than swapping static/dynamic order.
 fields[0].offset=0;NativeCUSProgramUniformPlan overlap(stage);assert(overlap.Pack(d,source,transport,out));assert(overlap.Pack(d,source,transport,out));assert(out==PackNativeCUSProgramUniforms(stage,d,source,transport));
 // Plans own reflected field names/layout and do not observe caller mutation.
 fields[0].offset=8;std::string name="x";fields[1].name=name.c_str();NativeCUSProgramUniformPlan owned(stage);name="gone";fields[1].offset=12;assert(owned.Pack(d,source,transport,out));fields[1].name="x";fields[1].offset=0;assert(out==PackNativeCUSProgramUniforms(stage,d,source,transport));
 std::cout<<"CUS packed plan: stages="<<stages<<" byte comparisons="<<comparisons<<" static builds="<<builds<<" reuse hits="<<hits<<"; owner/ABI/failure cases passed\n";
}
