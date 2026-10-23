/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeNamedVolumeSource.h"
namespace rendering::metal4 {
// Uses the actual source-created logical volume and immutable pixel version.
// The selected DDS base upload and original full mip generation happen once
// per content version, within the caller's shared dependency/retirement path.
CUSSourceImage ResolveNativeNamedVolume(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission);
} // namespace rendering::metal4
