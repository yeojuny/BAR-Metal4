/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeDeferredPostprocessEpoch.h"
#include "Rendering/Backend/NativeShieldDepthBacking.h"

#include <array>

namespace rendering::metal4 {

// The original outline reads three immutable logical bindings, in this order:
// map depth, model depth, model misc. Misc is the existing CUS MRT attachment4:
// unitID+1 low byte in R, unit flag in G, high byte in B, alphaBin in A.
// No second ID texture, reconstruction or geometry draw is introduced here.
struct CUSOutlineImage {
	CUSSourceImage image;
	NativeDeferredPostprocessEpoch epoch;
	NativeShieldDepthMetadata view;
	uint64_t sourceDrawFrame = 0;
	unsigned samplerSlot = 0;
	NativeTextureSampling sampling;
};

// Resolve only the retained object/version captured at the original Lua draw.
// actualSubmission is the common world submission identity. Allocation-only
// contents remain explicitly undefined; completed prior versions stay prior.
CUSOutlineImage ResolveCUSOutlineInput(RuntimeLease&, const NativeCUSTextureBinding&,
	unsigned samplerSlot, uint64_t actualSubmission);

// Model ID and depth must be outputs of one actual model MRT phase. Map has
// its own producer and may retain an older completed version on a skipped
// source phase; its source frame is never relabelled as the consumer frame.
void ValidateCUSOutlineImages(const std::array<CUSOutlineImage, 3>&);

} // namespace rendering::metal4
