/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "System/creg/creg_cond.h"
#include "Rendering/GL/myGL.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#endif

struct UnitDefImage
{
	CR_DECLARE_STRUCT(UnitDefImage)

	UnitDefImage(): imageSizeX(-1), imageSizeY(-1), textureID(0) {
	}

	bool Free() {
#ifdef RECOIL_METAL4_LIVE_SCENE
		const bool hadNativeImage = nativeBinding->current != nullptr || nativeBinding->source != nullptr;
		if (auto& context=rendering::OriginalNativeTextureContext();context.Initialized())
			context.UI().ReleaseBuildPicture(nativeBinding);
		// Lists retain this cell, so clearing/replacing an image must never
		// replace the cell itself or retain a borrowed Lua-state resolver.
		nativeBinding->current.reset();
		nativeBinding->source.reset();
#endif
		if (textureID != 0) {
			glDeleteTextures(1, &textureID);
			textureID = 0;
			return true;
		}
#ifdef RECOIL_METAL4_LIVE_SCENE
		return hadNativeImage;
#else
		return false;
#endif
	}

	int imageSizeX;
	int imageSizeY;
	GLuint textureID;
#ifdef RECOIL_METAL4_LIVE_SCENE
	std::shared_ptr<rendering::ui::NativeUITextureBinding> nativeBinding = std::make_shared<rendering::ui::NativeUITextureBinding>();
#endif
};

