/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeDOFContext.h"
#include <functional>
struct lua_State;
namespace LuaNativePostprocess {
struct DOFCallbacks {
	std::function<std::uint32_t()> screenCommandOffset;
	std::function<std::array<std::uint32_t, 2>()> viewExtent;
	std::function<std::array<float, 16>()> projection;
	std::function<void(rendering::NativePostprocessRect)> viewport;
	std::function<void(bool)> blending;
	std::function<void()> textureUnit0Bound;
	std::function<void()> textureUnit0Disabled;
	std::function<void(float,float)> currentTexCoord;
};
// Leaves the private original-DOF API table on top. This does not install a
// general CreateShader/CreateFBO/texture-unit API in the original global gl.
void PushDOFContext(lua_State* L, std::shared_ptr<rendering::NativeDOFContext> context, DOFCallbacks callbacks);
}
