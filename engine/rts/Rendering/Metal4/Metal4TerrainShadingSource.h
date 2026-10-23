/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"

namespace rendering::metal4 {
class MiniMapGPU;
// Uses the caller's existing shading producer and actual sampler. Preparation
// is before Begin; callbacks run inside that same common lease/submission.
CUSSourceImage ResolveNativeTerrainShadingImage(RuntimeLease&,MiniMapGPU&,
	const NativeCUSTextureBinding&,uint64_t submission,id<MTLSamplerState> actualSampler,
	bool validation,double& shaderMs);
}
