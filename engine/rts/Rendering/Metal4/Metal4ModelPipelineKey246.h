/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
namespace rendering::metal4::pipelinekey246 {
using CUSKey=std::array<uint64_t,23>;
using ForwardKey=std::array<uint64_t,19>;
inline bool Enabled(){static const bool enabled=[]{const char*p=std::getenv("BAR_NATIVE_MODEL_FIXED_KEY246");return p&&std::strcmp(p,"1")==0;}();return enabled;}
inline bool WitnessEnabled(){static const bool enabled=[]{const char*p=std::getenv("BAR_NATIVE_MODEL_FIXED_KEY246_WITNESS");return p&&std::strcmp(p,"1")==0;}();return enabled;}
// Full-width, exact fieldwise equality; the two PSO maps keep their original
// program/context partitions. Include even descriptor-inactive distinctions.
template<class Target,class Blend>
CUSKey CUS(const void*program,const Target&t,const Blend&b,uint64_t writeMask,bool validation,
 bool coverage,bool sampleShading,bool identity194,uint64_t samplerPolicy,bool forceFragmentRelaxed,bool indirect){
 static_assert(std::tuple_size_v<decltype(t.colors)> ==5);
 return {uint64_t(reinterpret_cast<uintptr_t>(program)),uint64_t(t.samples),uint64_t(validation),
  uint64_t(b.enabled),uint64_t(b.sourceColor),uint64_t(b.destinationColor),uint64_t(b.sourceAlpha),
  uint64_t(b.destinationAlpha),uint64_t(b.colorEquation),uint64_t(b.alphaEquation),writeMask,
  uint64_t(t.colors[0]),uint64_t(t.colors[1]),uint64_t(t.colors[2]),uint64_t(t.colors[3]),uint64_t(t.colors[4]),
  uint64_t(coverage),uint64_t(t.combinedColor),uint64_t(sampleShading),uint64_t(identity194),samplerPolicy,
  uint64_t(forceFragmentRelaxed),uint64_t(indirect)};
}
template<class Target,class Surface>
ForwardKey Forward(bool modern,bool validation,uint32_t imageKey,const Target&t,const Surface&s,uint64_t sampleShadingKey){
 const auto&b=s.blend;
 return {uint64_t(modern),uint64_t(validation),uint64_t(imageKey),uint64_t(t.colors[0]),uint64_t(t.depth),
  uint64_t(t.stencil),sampleShadingKey,uint64_t(b.enabled),uint64_t(b.sourceColor),uint64_t(b.destinationColor),
  uint64_t(b.sourceAlpha),uint64_t(b.destinationAlpha),uint64_t(b.colorEquation),uint64_t(b.alphaEquation),
  uint64_t(s.colorWrite[0]),uint64_t(s.colorWrite[1]),uint64_t(s.colorWrite[2]),uint64_t(s.colorWrite[3]),uint64_t(s.alphaToCoverage)};
}
// Opt-in whole-process census of observed temporary key storage capacity
// requests, not allocator hooks, resident bytes or GPU timing. Forward
// string requests are a lower bound; map nodes are deliberately excluded.
enum Family:unsigned { CUSFamily, ForwardFamily };
struct LegacyKeyCost{
 uint64_t requests=0,bytes=0,lastCapacity=0;
 void VectorCapacity(size_t n){if(n>lastCapacity){++requests;bytes+=n*sizeof(uint64_t);lastCapacity=n;}}
 template<class Stream,class String>void Strings(const Stream& s,const String& k){
  const auto sso=String{}.capacity();
  if(s.view().size()>sso){++requests;bytes+=s.view().size()+1;}
  if(k.size()>sso){++requests;bytes+=k.capacity()+1;}
 }
};
struct WitnessTotals{
 struct Row {std::atomic<uint64_t> calls{0},hits{0},misses{0},requests{0},bytes{0},hitRequests{0},hitBytes{0};};
 std::array<Row,2> rows;
 ~WitnessTotals(){for(unsigned i=0;i<rows.size();++i){const auto& r=rows[i];std::fprintf(stderr,"[ModelKey246] fixed=%u family=%s calls=%llu hits=%llu misses=%llu temp_requests=%llu temp_payload_bytes=%llu hit_temp_requests=%llu hit_temp_payload_bytes=%llu\n",
  unsigned(Enabled()),i==CUSFamily?"CUS":"Forward",(unsigned long long)r.calls.load(),(unsigned long long)r.hits.load(),(unsigned long long)r.misses.load(),(unsigned long long)r.requests.load(),(unsigned long long)r.bytes.load(),(unsigned long long)r.hitRequests.load(),(unsigned long long)r.hitBytes.load());}}
};
inline void Report(Family family,bool hit,const LegacyKeyCost& cost){
 static WitnessTotals totals;
 auto& r=totals.rows[family];++r.calls;if(hit){++r.hits;r.hitRequests+=cost.requests;r.hitBytes+=cost.bytes;}else ++r.misses;
 r.requests+=cost.requests;r.bytes+=cost.bytes;
}
}
