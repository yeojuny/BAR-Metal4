/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldParticleData.h"
#include "WorldFrameData.h"
#include <cmath>
#include <stdexcept>

namespace rendering {

// DrawAlpha and the analytic engine nano renderer use the active camera.
// In contrast, DrawAlpha's fog distance deliberately uses CAMTYPE_PLAYER.
// Do not replace that independent source with the reflected eye position.
inline void ValidateNativeWorldParticlePass(const NativeWorldParticleFrame& frame,
	const WorldCamera& view)
{
	const auto require=[](bool value,const char* message) {
		if(!value)throw std::runtime_error(message);
	};
	require(!(frame.drawReflection&&frame.drawRefraction),
		"Original particle pass cannot be reflection and refraction together");
	require(view.clipDepth!=ClipDepthConvention::Unsupported,
		"Original particle pass requires the actual camera clip convention");
	for(const auto* matrix:{&frame.cameraView,&frame.cameraProjection,&frame.cameraViewProjection})
		for(float value:*matrix)require(std::isfinite(value),"Original particle captured camera is nonfinite");
	for(const auto* vector:{&frame.cameraPosition,&frame.cameraRight,&frame.cameraUp,&frame.fogCameraPosition})
		for(float value:*vector)require(std::isfinite(value),"Original particle captured camera basis is nonfinite");
	require(frame.cameraView==view.view&&frame.cameraProjection==view.projection&&frame.cameraPosition==view.position,
		"Original particle geometry and consumer require the same active camera");
	const bool waterPass=frame.drawReflection||frame.drawRefraction;
	require(!waterPass||!frame.softenRequested,
		"Original reflected/refracted particles do not sample main terrain soft depth");
	for(const auto& range:frame.ranges) {
		if(waterPass) {
			const auto stage=frame.drawReflection?NativeWorldParticleStage::AlphaAboveWater:NativeWorldParticleStage::AlphaBelowWater;
			require(!range.groundAtlas&&range.stage==stage,
				"Original water particle pass requires its matching alpha half and no ground flashes");
		} else if(range.stage==NativeWorldParticleStage::GroundFlashes) {
			require(range.groundAtlas,"Original ground flashes require the ground atlas");
		} else {
			const bool alpha=frame.visibleWater?
				(range.stage==NativeWorldParticleStage::AlphaBelowWater||range.stage==NativeWorldParticleStage::AlphaAboveWater):
				range.stage==NativeWorldParticleStage::AlphaAll;
			require(alpha&&!range.groundAtlas,"Original main particle pass has an inconsistent water split");
		}
	}
}

} // namespace rendering
