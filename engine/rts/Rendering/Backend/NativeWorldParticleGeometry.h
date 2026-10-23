/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldParticleData.h"
#include <span>

namespace rendering {
// Validate the entire original range before changing its destination. The
// copied indexed order/vertex bits are preserved; no geometry is fabricated.
void AppendNativeParticleGeometry(NativeWorldParticleFrame& frame,
	std::span<const NativeWorldParticleVertex> vertices,
	std::span<const std::uint32_t> indices, NativeWorldParticleRange range);
}
