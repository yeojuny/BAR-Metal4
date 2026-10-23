/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "WorldFrameData.h"
#include "NativeWaterSceneData.h"
#include "NativeWaterLuaFrame.h"
#include "NativeSSAOData.h"
#include "NativeBloomData.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
namespace rendering {
// These are real captured writes at the original callback, independent of
// whether the current map/model GBuffer source phase performs a redraw.
inline bool HasNativeDeferredPostprocessWrites(const WorldFrame& frame,
	const std::shared_ptr<const NativeBloomFrame>& pendingBloom = {})
{
	const auto ssaoWrites = [](const std::shared_ptr<const NativeWaterLuaFrame>& bundle) {
		return bundle && bundle->ssao && !bundle->ssao->draws.empty();
	};
	const auto alphaWrites=[&](const WorldFrame& view){
		for(const auto& models:view.alphaModelFrames)if(models)for(const auto& phase:models->passes)
			if(ssaoWrites(phase.luaEffects))return true;
		return false;
	};
	const auto& bloom = pendingBloom ? pendingBloom : frame.luaBloomFrame;
	return (frame.opaqueModelFrame && frame.opaqueModelFrame->HasWriters()) ||
		alphaWrites(frame) ||
		(frame.waterScene&&((frame.waterScene->reflection&&alphaWrites(frame.waterScene->reflection->frame))||
			(frame.waterScene->refraction&&alphaWrites(frame.waterScene->refraction->frame)))) ||
		ssaoWrites(frame.alphaAllLua) ||
		ssaoWrites(frame.waterScene ? frame.waterScene->aboveWaterLua : nullptr) ||
		(bloom && !bloom->draws.empty());
}
} // namespace rendering
