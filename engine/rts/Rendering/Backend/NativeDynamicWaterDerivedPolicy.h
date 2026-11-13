/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDynamicWaterData.h"
#include <cstddef>
#include <stdexcept>

namespace rendering {
// Select only the derived bootstrap prefix. Ordinary update DetailNormal draws
// are recurrence updates and are deliberately outside this policy. The caller
// must establish an exact, same-material committed predecessor before passing
// true; a merely prepared or accepted predecessor is not sufficient.
inline std::size_t NativeDynamicWaterDerivedCount(
 const NativeDynamicWaterFrame& frame, bool sameMaterialCommitted)
{
 const std::size_t expected = frame.initializesDetail ? 2u : 1u;
 if (frame.derived.size() != expected ||
     frame.derived.front().pass != DynamicWaterPass::DerivedHeight ||
     (frame.initializesDetail && frame.derived[1].pass != DynamicWaterPass::DetailNormal))
  throw std::logic_error("Dynamic-water derived bootstrap marker or source pass order is invalid");
 return frame.initializesDetail && sameMaterialCommitted ? 1u : expected;
}
}
