/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWorldParticleData.h"

namespace rendering {
struct NativeWaterLuaFrame;
// Original shadow-camera geometry. Drawing matrices intentionally differ from
// the camera's culling matrices, exactly as CShadowHandler::SetShadowCamera.
struct NativeShadowColorFrame {
	std::uint32_t sourceDrawFrame = 0, size = 0;
	std::array<float,16> view{}, projection{};
	std::array<float,3> position{};
	bool zeroToOneDepth = false;
	bool colored = true;
	// Original CreateShadows skips DrawShadowPasses at zero sun intensity:
	// preserve the previous color image instead of inventing a new clear.
	bool redraw = true;
	std::shared_ptr<const NativeWorldParticleAtlas> atlas;
	std::vector<NativeWorldParticleVertex> vertices;
	std::vector<std::uint32_t> indices;
	std::vector<std::string> unsupported;
	std::shared_ptr<const NativeWaterLuaFrame> luaEffects;
};
}
