/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace rendering {
struct NativeProfilerHistogramInput132 {
 NativeBufferSnapshotRef vertices;
 uint32_t first=0,count=0,samples=1;
 std::array<float,2> view{};
 std::array<float,4> box{},color{};
 float lineWidth=1;
 bool zeroToOne=false;
};
namespace profilerSource132 {
inline void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline void Validate(const NativeProfilerHistogramInput132& d){
 const auto& v=d.vertices;Check(v&&v->object&&v->revision,"Profiler original immutable VBO missing");
 const auto& l=v->layout;Check(l.kind==NativeBufferKind::Vertex&&l.stride==8&&l.attributes.size()==1&&v->bytes.size()==CheckedNativeBufferByteSize(l),"Profiler original float2 storage differs");
 const auto& a=l.attributes[0];Check(a.id==0&&a.offset==0&&a.byteSize==8&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Profiler original float2 attribute differs");
 Check(d.first<=l.elements&&d.count<=l.elements-d.first&&d.count<=4u*1024u*1024u,"Profiler selected range exceeds original VBO");
 for(float x:d.view)Check(std::isfinite(x)&&x>0,"Profiler original view extent invalid");
 for(float x:d.box)Check(std::isfinite(x),"Profiler original box invalid");
 for(float x:d.color)Check(std::isfinite(x),"Profiler original color invalid");
 Check(std::isfinite(d.lineWidth)&&d.lineWidth>0,"Profiler original line width invalid");
}
struct alignas(16) Uniform {
 std::array<float,4> box{},view{},color{},viewport{},target{};
 std::array<uint32_t,4> options{};
};
static_assert(sizeof(Uniform)==96&&offsetof(Uniform,options)==80);
struct Probe {
 const char* path=std::getenv("BAR_NATIVE_PROFILER_SOURCE_PROBE_FILE");
 std::atomic<uint64_t> gpuDraws{0},segments{0},vertices{0},uniformBytes{0},sourceUploads{0},sourceBytes{0},cpuDraws{0},msaaDraws{0};
 ~Probe(){if(path&&*path)if(auto f=std::fopen(path,"w")){
  std::fprintf(f,"sourceGPUDraws,sourceSegments,sourceVertices,uniformBytes,sourceUploads,sourceBytes,cpuFallbackDraws,sourceGPUMultisampleDraws\n%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)gpuDraws.load(),(unsigned long long)segments.load(),(unsigned long long)vertices.load(),(unsigned long long)uniformBytes.load(),(unsigned long long)sourceUploads.load(),(unsigned long long)sourceBytes.load(),(unsigned long long)cpuDraws.load(),(unsigned long long)msaaDraws.load());std::fclose(f);
 }}
 void GPU(uint32_t n,unsigned samples){if(path&&*path){gpuDraws.fetch_add(1,std::memory_order_relaxed);segments.fetch_add(n,std::memory_order_relaxed);vertices.fetch_add(uint64_t(n)*4,std::memory_order_relaxed);uniformBytes.fetch_add(sizeof(Uniform),std::memory_order_relaxed);if(samples>1)msaaDraws.fetch_add(1,std::memory_order_relaxed);}}
 void Upload(size_t bytes){if(path&&*path){sourceUploads.fetch_add(1,std::memory_order_relaxed);sourceBytes.fetch_add(bytes,std::memory_order_relaxed);}}
 void CPU(){if(path&&*path)cpuDraws.fetch_add(1,std::memory_order_relaxed);}
};
inline Probe& Counters(){static Probe value;return value;}
}
}
