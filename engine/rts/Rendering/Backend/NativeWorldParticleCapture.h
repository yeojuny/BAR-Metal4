/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeWorldParticleData.h"
#include "NativeWaterSceneData.h"
#include <functional>
#include <memory>

class CCamera;
struct S3DModel;

namespace rendering {

// Draw-side only. Uses original projectile class geometry and visibility/sort
// policy; no spawn/update/RNG or synchronized simulation changes are allowed.
class NativeWorldParticleCapture {
public:
	using ModelProvider = std::function<std::shared_ptr<const WorldModel>(const S3DModel*)>;
	NativeWorldParticleCapture();
	~NativeWorldParticleCapture();
	NativeWorldParticleCapture(const NativeWorldParticleCapture&) = delete;
	NativeWorldParticleCapture& operator=(const NativeWorldParticleCapture&) = delete;
	std::shared_ptr<const NativeWorldParticleFrame> Capture(const CCamera& viewCamera, const ModelProvider& models, NativeWaterScenePass pass = NativeWaterScenePass::Main);
	void Reset();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace rendering
