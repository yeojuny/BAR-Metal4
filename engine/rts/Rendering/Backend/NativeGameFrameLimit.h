/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdlib>
#include <string_view>

namespace rendering {
struct NativeGameFrameLimit {
 unsigned submissions;
 bool configured;
};

inline NativeGameFrameLimit GameFrameLimit() noexcept {
 static const NativeGameFrameLimit limit=[]() -> NativeGameFrameLimit {
  if(const char* value=std::getenv("BAR_NATIVE_MAX_PENDING_GAME_FRAMES")) {
   const std::string_view setting(value);
   if(setting=="1")return {1,true};
   if(setting=="2")return {2,true};
   if(setting=="3")return {3,true};
  }
  if(const char* value=std::getenv("BAR_NATIVE_INTERVENTION_TWO_INFLIGHT")) {
   const std::string_view legacy(value);
   if(legacy=="0")return {1,true};
   if(legacy=="1")return {2,true};
  }
  return {1,false};
 }();
 return limit;
}

// Cap1 and cap2 already reserve two receipts at scene admission.
inline unsigned GameFrameReceiptCapacity() noexcept {
 return GameFrameLimit().submissions==3 ? 3u : 2u;
}
}
