/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4Pass160Policy.h"

namespace rendering::metal4 {
enum class AlphaCallbackBody166 : uint8_t { Unknown, CPULifecycleOnly, OrderedCommands };
// Pure description of one bound typed owner, never permission to omit its
// actual Dependencies/Complete calls. Missing/new callback kinds stay Unknown.
struct AlphaCallbackExecution166 {
 SourceDependencyWork160 dependencies=SourceDependencyWork160::Unknown;
 AlphaCallbackBody166 body=AlphaCallbackBody166::Unknown;
 bool noResourceReads=false,preservesEncoderState=false;
 bool HasOnlyCPULifecycle()const {
  return dependencies==SourceDependencyWork160::NoCommands&&body==AlphaCallbackBody166::CPULifecycleOnly&&
   noResourceReads&&preservesEncoderState;
 }
};
enum class AlphaKeepReason166 : uint8_t { CallbackUnknown, OrderedCommands, Dependencies, Kept };
struct AlphaKeepReport166 {
 ModelPassDependencyReport160 dependencies;
 AlphaKeepReason166 reason=AlphaKeepReason166::CallbackUnknown;
};
inline bool ModelPass166Enabled() {
 // Only CPU-only phases may use the no-in-pass-wait intervention.
 return EmptyModelPassFusionInterventionEnabled();
}
// One completed frame's LOCAL alpha boundaries, not total render encoders.
struct AlphaPassCounters166 {
 uint64_t attempts=0,queries=0,mainKept=0,planarKept=0,keptNonempty=0,keptEmpty=0;
 uint64_t disabled=0,noEncoder=0,attachment=0,callbackUnknown=0,orderedCommands=0;
 uint64_t dependencyUnknown=0,dependencyCommands=0,cubeQueries=0;
 void Observe(const AlphaKeepReport166& value,bool planar) {
  cubeQueries+=value.dependencies.cubeQueries;
  switch(value.reason){
   case AlphaKeepReason166::CallbackUnknown:++callbackUnknown;break;
   case AlphaKeepReason166::OrderedCommands:++orderedCommands;break;
   case AlphaKeepReason166::Dependencies:if(value.dependencies.unknown)++dependencyUnknown;else ++dependencyCommands;break;
   case AlphaKeepReason166::Kept:
    if(planar)++planarKept;else ++mainKept;
    if(value.dependencies.indexedDraws)++keptNonempty;else ++keptEmpty;break;
  }
 }
};
} // namespace rendering::metal4
