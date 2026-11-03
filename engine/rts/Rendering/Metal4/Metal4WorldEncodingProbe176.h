// CPU-side accounting only, scoped to one ordinary world command buffer.
#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif

namespace rendering {
class WorldEncodingProbe176 {
public:
 enum class Family : unsigned { Other, Shadow, Model, Effects };
 using Clock = std::chrono::steady_clock;
 using Counts = std::array<std::uint64_t,4>;
 explicit WorldEncodingProbe176(std::uint64_t sourceFrame,bool ordinary) {
#ifdef RECOIL_METAL4_LIVE_SCENE
  static const bool enabled=[]{
   const char* value=std::getenv("BAR_NATIVE_WORLD_ENCODING176");
   return value && std::strcmp(value,"1")==0;
  }();
  active=enabled && ordinary && sourceFrame>=2048 && sourceFrame<2112;
  if(active)previous=Clock::now();
#else
  (void)sourceFrame; (void)ordinary;
#endif
 }
 void Select(Family family) {
  if(!active||family==current)return;
  Charge();current=family;
 }
 class Scope {
 public:
  Scope(WorldEncodingProbe176& probe,Family selected):owner(probe),prior(probe.current) { owner.Select(selected); }
  ~Scope(){owner.Select(prior);}
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 private:
  WorldEncodingProbe176& owner;Family prior;
 };
 void LegacyModelSetup(bool overridesScene) {
  if(active){++modelSetups;sceneOverrides+=overridesScene;}
 }
 void Finish(std::uint64_t sourceFrame,std::int64_t simulationFrame) {
#ifdef RECOIL_METAL4_LIVE_SCENE
  if(!active)return;
  Charge();active=false;
  LOG("[NativeWorldEncoding176] source_frame=%llu simulation_frame=%lld shadow_ns=%llu model_ns=%llu effects_ns=%llu other_ns=%llu model_setups=%llu scene_overrides=%llu",
   (unsigned long long)sourceFrame,(long long)simulationFrame,(unsigned long long)times[1],(unsigned long long)times[2],
   (unsigned long long)times[3],(unsigned long long)times[0],(unsigned long long)modelSetups,(unsigned long long)sceneOverrides);
#else
  (void)sourceFrame;(void)simulationFrame;
#endif
 }
private:
 void Charge() {
  const auto now=Clock::now();
  times[unsigned(current)]+=std::chrono::duration_cast<std::chrono::nanoseconds>(now-previous).count();
  previous=now;
 }
 Family current=Family::Other;
 bool active=false;
 Clock::time_point previous{};
 Counts times{};
 std::uint64_t modelSetups=0,sceneOverrides=0;
};
} // namespace rendering
