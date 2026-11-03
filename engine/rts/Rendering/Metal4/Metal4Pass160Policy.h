/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <cstdlib>
namespace rendering::metal4 {
enum class SourceDependencyWork160 : uint8_t { Unknown, NoCommands, CommandsRequired };
struct SourceDependencyResult160 {
 SourceDependencyWork160 work=SourceDependencyWork160::Unknown;
 bool cubeQuery=false;
};
// The context is borrowed from the CUSSourceImage's existing owner. The query
// must not encode, wait, register an ordering edge, or change producer state.
// NoCommands also proves that omitting the callback loses no producer-side CPU
// transition. It does NOT replace the separate draw-time readability check.
struct SourceDependencyQuery160 {
 const void* context=nullptr;
 SourceDependencyResult160 (*read)(const void*)=nullptr;
 SourceDependencyResult160 Inspect()const {
  return context&&read?read(context):SourceDependencyResult160{};
 }
};
struct ModelPassDependencyReport160 {
 bool noCommands=true,unknown=false;
 uint64_t cubeQueries=0,indexedDraws=0;
 void Add(SourceDependencyResult160 value) {
  noCommands&=value.work==SourceDependencyWork160::NoCommands;
  unknown|=value.work==SourceDependencyWork160::Unknown;
  cubeQueries+=value.cubeQuery;
 }
 void Merge(const ModelPassDependencyReport160& value) {
  noCommands&=value.noCommands;unknown|=value.unknown;
  cubeQueries+=value.cubeQueries;indexedDraws+=value.indexedDraws;
 }
};
struct ModelPassCounters160 {
 uint64_t kept=0,keptNonempty=0,keptEmpty=0,cubeQueries=0,unknownBoundaries=0;
 uint64_t modelMRTPhases=0,modelMRTLoadPhases=0;
 void Observe(const ModelPassDependencyReport160& report) {
  cubeQueries+=report.cubeQueries;
  if(report.noCommands){++kept;if(report.indexedDraws)++keptNonempty;else ++keptEmpty;}
  else if(report.unknown)++unknownBoundaries;
 }
};
inline bool ModelPass160Enabled() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_PASS160");
  return !(value&&value[0]=='0'&&value[1]=='\0');}();
 return enabled;
}
} // namespace rendering::metal4
