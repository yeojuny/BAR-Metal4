/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "WorldFrameData.h"

namespace rendering {

struct NativeWaterLuaFrame;
struct NativeWaterGroundSourceFrame;
struct NativeCUSSourceFrame;

enum class NativeWaterScenePass : uint8_t { Main, Reflection, Refraction };

// The original IWater scene is a fresh view of the same unsynced draw, not
// an environment cube or a previous presentation. Resource owners are shared
// with the main view; camera-dependent visibility, billboards and sort differ.
struct NativeWaterSceneView {
	WorldFrame frame;
	NativeWaterScenePass pass = NativeWaterScenePass::Reflection;
	std::array<float,4> terrainClipPlane{}, modelClipPlane{};
	bool drawGround = false, drawSky = true, drawFog = true;
	bool legacyReflective = false;
 bool dynamicWater=false; // CAdvWater planes/clear differ from BumpWater.
	std::shared_ptr<const NativeWaterLuaFrame> preParticlesLua, worldLua;
	// Original ground producer completes before model source capture, in the
	// same unsafe stage. These distinct owners are consumed once before models.
	std::shared_ptr<const NativeWaterGroundSourceFrame> groundSource;
	std::shared_ptr<const NativeCUSSourceFrame> groundCUS;
	std::shared_ptr<const NativeWaterLuaFrame> groundLua;
	uint64_t initialClearSequence=0; // Actual CBumpWater color/depth clear before sky.
	uint64_t preParticlesBoundary=0; // shared sequence after the original pre-particle event
};

struct NativeWaterSceneFrame {
	uint64_t sourceDrawFrame = 0;
	std::shared_ptr<const NativeWaterSceneView> reflection, refraction;
	std::shared_ptr<const NativeWaterLuaFrame> belowWaterLua, waterPostLua, aboveWaterLua;
};

struct NativeWaterModelPasses { bool opaque = false, alpha = false; };

// Side-effect-free part of ShouldDraw{Opaque,Alpha}{Unit,Feature}. The engine
// has already performed viewer LOS, icon/void/frustum and distance decisions.
// Do not call the original helpers: they also enqueue Lua material draws.
constexpr NativeWaterModelPasses ClassifyNativeWaterModelPasses(uint8_t flags,
	uint8_t engineMask, NativeWaterScenePass pass)
{
	constexpr uint8_t OPAQUE = 1, ALPHA = 2, REFLECTION = 4, REFRACTION = 8;
	const uint8_t opaqueMask = pass == NativeWaterScenePass::Reflection ? REFLECTION :
		pass == NativeWaterScenePass::Refraction ? REFRACTION : OPAQUE;
	const uint8_t alphaMask = pass == NativeWaterScenePass::Main ? ALPHA : opaqueMask;
	return {
		(flags & opaqueMask) != 0 && (flags & ALPHA) == 0 && (engineMask & opaqueMask) == opaqueMask,
		(flags & alphaMask) != 0 && (flags & OPAQUE) == 0 && (engineMask & alphaMask) == alphaMask,
	};
}

// Fixed source planes from CBumpWater::DrawReflection/DrawRefraction. The
// terrain's five-elmo overlap hides shoreline cracks; models clip at y=0.
constexpr std::array<float,4> NativeWaterModelClipPlane(NativeWaterScenePass pass)
{
	return {0, pass == NativeWaterScenePass::Reflection ? 1.f : pass == NativeWaterScenePass::Refraction ? -1.f : 0.f,
		0, pass == NativeWaterScenePass::Main ? 1.f : 0.f};
}
constexpr std::array<float,4> NativeWaterTerrainClipPlane(NativeWaterScenePass pass)
{
	auto plane = NativeWaterModelClipPlane(pass);
	if (pass != NativeWaterScenePass::Main) plane[3] = 5;
	return plane;
}

void ValidateNativeWaterSceneView(const NativeWaterSceneView&, uint64_t expectedDrawFrame);
void ValidateNativeWaterSceneFrame(const NativeWaterSceneFrame&, uint64_t expectedDrawFrame,
	bool requireReflection, bool requireRefraction);

} // namespace rendering
