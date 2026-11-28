/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4GrassBoundaryPolicyS01.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace rendering::metal4;
int main(){
 // Exhaust all short sequences, including unknown callbacks and an ordinary
 // draw after Grass. The independent oracle is the language A*G+, never a
 // reordering/filtering of the original packet list.
 size_t cases=0;
 for(size_t length=0,power=1;length<=8;++length,power*=3){
  for(size_t code=0;code<power;++code){
   auto v=code;std::vector<GrassBoundaryDrawS01> draws;
   for(size_t n=0;n<length;++n){draws.push_back(static_cast<GrassBoundaryDrawS01>(v%3));v/=3;}
   size_t boundary=0;
   while(boundary<length&&draws[boundary]==GrassBoundaryDrawS01::AttachmentOnly)++boundary;
   bool valid=boundary<length;
   for(size_t n=boundary;n<length;++n)valid&=draws[n]==GrassBoundaryDrawS01::Grass;
   const auto original=draws;
   const auto actual=GrassSuffixS01(draws,[](auto d){return d;});
   assert(actual==(valid?boundary:NoGrassBoundaryS01));assert(draws==original);++cases;
  }
 }
 // Storage keys are ROOT allocations, so different views of one attachment
 // are intentionally the same key. Unknown/imported/aliasable heaps must not
 // become independent just because an object's pointer differs.
 const std::array<GrassStorageS01,3> attachments{{{10,true},{20,true},{30,true}}};
 assert(GrassReadDisjointS01({40,true},attachments));
 for(auto root:{uintptr_t{0},uintptr_t{10},uintptr_t{20},uintptr_t{30}})
  assert(!GrassReadDisjointS01({root,true},attachments));
 assert(!GrassReadDisjointS01({40,false},attachments));
 for(size_t i=0;i<3;++i){
  auto unknown=attachments;unknown[i].independentlyOwned=false;
  assert(!GrassReadDisjointS01({40,true},unknown));
  unknown=attachments;unknown[i].root=0;assert(!GrassReadDisjointS01({40,true},unknown));
 }
 // Actual owner transition cases: preparation/registration, failed partial
 // encoding, successful current encoding (still NOT retired), predecessor
 // encoding and completed predecessor. Queries must not advance any state.
 bool completed=false;
 assert(!GrassProducerRecordedS01(7,completed,0,false));
 assert(!GrassProducerRecordedS01(7,completed,7,false));
 assert( GrassProducerRecordedS01(7,completed,7,true));assert(!completed);
 assert(!GrassProducerRecordedS01(8,completed,7,true));
 completed=true;
 assert( GrassProducerRecordedS01(8,completed,7,true));
 assert(!GrassProducerRecordedS01(0,completed,7,true));
 // The old Shield-depth route is a different successor. Exercise every
 // combination of the five actual end facts, including an absent/empty list,
 // a forward first phase and the Pass165 encoder-keep route.
 size_t endCases=0;
 for(unsigned mask=0;mask<16;++mask)for(size_t phases:{size_t{0},size_t{1},size_t{2}}){
  const GrassOpaqueEndS01 facts{bool(mask&1),bool(mask&2),phases,bool(mask&4),bool(mask&8)};
  const bool expected=(mask==6&&phases!=0);
  assert((GrassOpaqueEndRejectionsS01(facts)==0)==expected);++endCases;
 }
 assert(GrassOpaqueEndRejectionsS01({true,true,2,true,false})&1u);
 assert(GrassOpaqueEndRejectionsS01({false,false,0,false,false})==14u);
 assert(GrassOpaqueEndRejectionsS01({false,true,2,true,true})==16u);
 // Repeated frames and the entire game timeline cannot produce more than
 // three ROI samples; startup/after-ROI frames emit none.
 unsigned emittedMask=0;std::vector<unsigned> samples;
 for(unsigned sim=0;sim<=2000;++sim)for(unsigned repeat=0;repeat<3;++repeat){
  const auto bit=GrassDiagnosticSampleS01(sim,true);
  if(bit&&!(emittedMask&bit)){emittedMask|=bit;samples.push_back(sim);}
 }
 assert((samples==std::vector<unsigned>{540,900,1590}));assert(emittedMask==7);
 assert(GrassDiagnosticSampleS01(1740,true)==4&&GrassDiagnosticSampleS01(1741,true)==0);
 unsigned emptyMask=0,delayedMask=0;std::vector<unsigned> emptySamples,delayedSamples;
 for(unsigned sim=0;sim<=2000;++sim){
  auto bit=GrassDiagnosticSampleS01(sim,false);
  if(bit&&!(emptyMask&bit)){emptyMask|=bit;emptySamples.push_back(sim);}
  const bool hasGrass=(sim>=560&&sim<900)||(sim>=920&&sim<1590)||sim>=1600;
  bit=GrassDiagnosticSampleS01(sim,hasGrass);
  if(bit&&!(delayedMask&bit)){delayedMask|=bit;delayedSamples.push_back(sim);}
 }
 assert((emptySamples==std::vector<unsigned>{1740}));
 assert((delayedSamples==std::vector<unsigned>{560,920,1600}));
 std::cout<<"PASS "<<cases<<" exhaustive draw sequences and 12 physical-root/unknown cases; 6 producer-transition checks; "<<endCases<<" successor cases; bounded ROI populated/empty/delayed timelines\n";
}
