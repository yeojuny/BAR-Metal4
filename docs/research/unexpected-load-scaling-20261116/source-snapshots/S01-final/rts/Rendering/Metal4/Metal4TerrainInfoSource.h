/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"

namespace rendering::metal4 {
// Caller supplies the sampler for the actual source parameters and one nonzero
// common submission identity. Use the same serialized RuntimeLease for Prepare,
// dependency encoding, consumers and successful completion. These callbacks do
// not submit, wait, retire CPU source history, or create a sampler/cache/producer.
// The enclosing prepared graph must retain the returned owner through common
// completion, including the existing poison-and-retain path after Begin failure.
CUSSourceImage ResolveNativeTerrainInfoImage(RuntimeLease&,const NativeCUSTextureBinding&,
	uint64_t submission,id<MTLSamplerState> actualSampler,bool validation,double& shaderMs);
// Optional S01 proof for the exact already-resolved info image. No registration,
// encoding, callback omission, retirement, or new producer authority.
std::function<bool()> CaptureGrassInfoCheckpointS01(const NativeCUSTextureBinding&,
 id<MTLTexture> expected,uint64_t submission);
} // namespace rendering::metal4
