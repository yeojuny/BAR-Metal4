/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeLegacyWaterBumpData.h"

namespace rendering::metal4 {
// First native Water1/2 dependency only: this does not advertise a complete
// water renderer. Reflection, surface geometry and source shaders are separate.
class PreparedLegacyWaterBump {
public:
	~PreparedLegacyWaterBump();
	void Encode(id<MTL4CommandBuffer>);
	id<MTLTexture> Texture() const;
	// Render-target rows have the opposite storage orientation to source GL.
	bool FlipV() const { return true; }
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	explicit PreparedLegacyWaterBump(std::unique_ptr<Impl>);
	friend class LegacyWaterBumpGPU;
};
class LegacyWaterBumpGPU {
public:
	LegacyWaterBumpGPU();
	~LegacyWaterBumpGPU();
	std::unique_ptr<PreparedLegacyWaterBump> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeLegacyWaterBumpFrame>, bool validation, double& compileMs);
	void ClearResources(); // After the enclosing common submissions retire.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
