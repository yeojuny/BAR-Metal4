/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldEnvironmentData.h"
#include "NativeCubeTextureSource.h"

namespace rendering {
// Capture-owned pre-PreUnit identity and its original preparation policy.
// Copies retain that stage even when main starts collecting a later frame.
// Cube completion remains a GPU-published atomic observation, not a success claim.
struct NativeEnvironmentStageReceipt {
 std::shared_ptr<const NativeWorldEnvironmentFrame> frame;
 bool shaderValidation = false;
 bool Matches(const std::shared_ptr<const NativeWorldEnvironmentFrame>& actual) const noexcept {
  return frame==actual;
 }
 bool HasCubeSource() const noexcept { return frame && bool(frame->sourceCubeUpdate); }
 bool HasPendingCubeSource() const noexcept {
  return HasCubeSource() && frame->sourceCubeUpdate->backing &&
   !frame->sourceCubeUpdate->backing->completed.load(std::memory_order_acquire);
 }
};
}
