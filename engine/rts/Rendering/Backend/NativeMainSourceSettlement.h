/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShieldDepthPublication.h"
#include "NativeEnvironmentStageReceipt.h"
#include <stdexcept>
#include <utility>
#include <type_traits>
namespace rendering {
struct NativeOpaqueModelFrame;
struct NativeOriginalShadowFrame;
struct NativeTerrainSceneSourceFrame;
struct NativeDynamicWaterFrame;
// Main-thread rollback ownership, distinct from the sealed render packet. This
// first integration still joins: historical abort over a younger frame is NOT
// authorized. Allocation and destination checks must precede every transfer.
struct NativeMainSourceSettlement {
 uint64_t context=0,draw=0;
 std::shared_ptr<const NativeOpaqueModelFrame> opaque;
 std::shared_ptr<const NativeOriginalShadowFrame> shadow;
 std::shared_ptr<const NativeTerrainSceneSourceFrame> ground;
 std::shared_ptr<const NativeDynamicWaterFrame> dynamicWater;
 NativeEnvironmentStageReceipt environment;
 NativeShieldCaptureJournal shield;
 bool detached=false;
 bool Empty()const noexcept {return !opaque&&!shadow&&!ground&&!dynamicWater&&!environment.frame&&!shield.Receipt();}
 void SwapOwners(std::shared_ptr<const NativeOpaqueModelFrame>& o,
  std::shared_ptr<const NativeOriginalShadowFrame>& s,
  std::shared_ptr<const NativeTerrainSceneSourceFrame>& g,
  std::shared_ptr<const NativeDynamicWaterFrame>& w,
  NativeEnvironmentStageReceipt& e,NativeShieldCaptureJournal& j) noexcept {
  opaque.swap(o);shadow.swap(s);ground.swap(g);dynamicWater.swap(w);
  static_assert(std::is_nothrow_swappable_v<NativeEnvironmentStageReceipt>);
  using std::swap;swap(environment,e);shield.Swap(j);
 }
 void Take(uint64_t epoch,uint64_t sourceDraw,
  std::shared_ptr<const NativeOpaqueModelFrame>& o,
  std::shared_ptr<const NativeOriginalShadowFrame>& s,
  std::shared_ptr<const NativeTerrainSceneSourceFrame>& g,
  std::shared_ptr<const NativeDynamicWaterFrame>& w,
  NativeEnvironmentStageReceipt& e,NativeShieldCaptureJournal& j) {
  if(detached||!Empty()||!epoch)throw std::logic_error("Source settlement is not an empty exact capture owner");
  context=epoch;draw=sourceDraw;SwapOwners(o,s,g,w,e,j);detached=true;
 }
 void ValidateRestore(uint64_t epoch,
  std::shared_ptr<const NativeOpaqueModelFrame>& o,
  std::shared_ptr<const NativeOriginalShadowFrame>& s,
  std::shared_ptr<const NativeTerrainSceneSourceFrame>& g,
  std::shared_ptr<const NativeDynamicWaterFrame>& w,
  NativeEnvironmentStageReceipt& e,NativeShieldCaptureJournal& j) const {
  if(!detached||context!=epoch||o||s||g||w||e.frame||j.Receipt())
   throw std::logic_error("Source settlement restore would replace another capture or context");
 }
 void RestoreValidated(
  std::shared_ptr<const NativeOpaqueModelFrame>& o,
  std::shared_ptr<const NativeOriginalShadowFrame>& s,
  std::shared_ptr<const NativeTerrainSceneSourceFrame>& g,
  std::shared_ptr<const NativeDynamicWaterFrame>& w,
  NativeEnvironmentStageReceipt& e,NativeShieldCaptureJournal& j) noexcept {
  SwapOwners(o,s,g,w,e,j);detached=false;
 }
};
}
