/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeVolumetricCloudContext.h"
#include <functional>
struct lua_State;
namespace LuaNativeVolumetricClouds {
struct CloudCallbacks {
	std::function<rendering::NativePostprocessStage()> stage;
	std::function<std::array<float,16>()> inverseDepth01ViewProjection;
	std::function<std::shared_ptr<const rendering::NativeVolumetricCloudVolume>(std::string_view)> loadOriginalVolume;
	std::function<void(std::string_view)> loadOriginalNoise2D;
	std::function<std::string(std::shared_ptr<rendering::NativePostprocessBinding>)> registerImage;
};
// Pushes a private API table whose unmodified methods delegate to the actual
// native gl table at the current Lua stack top. Never installs global GLSL IDs.
void PushCloudContext(lua_State* L, std::shared_ptr<rendering::NativeVolumetricCloudContext> context, CloudCallbacks callbacks);
}
