/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rendering {
struct NativeS3OTexturePair;

struct WorldModel;

// Exact original VA_TYPE_PROJ fields. This is owned CPU data, not a GL buffer
// or a renderer-specific handle. Rows and UVs retain original atlas convention.
struct NativeWorldParticleVertex {
	std::array<float, 3> position{};
	std::array<float, 3> uvw{}; // s, t, original atlas page
	std::array<float, 4> uvInfo{}; // minS, minT, extentS, extentT
	std::array<float, 3> animation{}; // cellsX, cellsY, original anim progress
	std::array<std::uint8_t, 4> color{};
};
static_assert(sizeof(NativeWorldParticleVertex) == 56);

struct NativeWorldParticleMip {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> rgba;
};
struct NativeWorldParticleAtlas {
	std::string sourceName;
	std::vector<std::vector<NativeWorldParticleMip>> pages;
	// Legacy atlas requests GPU mip generation from its exact packed RGBA base.
	// A consumer must produce those levels before advertising availability.
	bool generateMipmaps = false;
	std::uint32_t requestedMipLevels = 0;
};

// The original water split must not be replaced by drawing both halves after
// water. A no-water main view uses AlphaAll/NanoAll once. Reflection/refraction
// have different camera/mask dependencies and are explicit additional passes.
enum class NativeWorldParticleStage {
	OpaqueProjectiles,
	AlphaAll,
	AlphaBelowWater,
	AlphaAboveWater,
	NanoAll,
	NanoBelowWater,
	NanoAboveWater,
	GroundFlashes,
	LuaNanoLaterWorld,
};

struct NativeWorldParticleRange {
	std::uint32_t firstIndex = 0;
	std::uint32_t indexCount = 0;
	NativeWorldParticleStage stage = NativeWorldParticleStage::AlphaAll;
	bool groundAtlas = false;
	bool depthTest = true;
	bool depthWrite = false;
	float alphaThreshold = 0;
	float softenThreshold = 8;
};

struct NativeWorldProjectilePiece {
	std::array<float, 16> modelSpace{};
	bool visible = false;
};

struct NativeWorldProjectileModel {
	std::int32_t projectileId = -1;
	std::shared_ptr<const WorldModel> model;
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	std::array<float, 16> world{};
	std::array<std::uint8_t, 4> teamColor{};
	std::vector<NativeWorldProjectilePiece> pieces;
	std::uint32_t modelPiece = 0;
	bool singlePiece = false;
};

// Original engine NanoParticles::Renderer::InstanceVertex, without engine
// float3/SColor dependencies. Frames preserve create/death/base/fade meanings.
struct NativeEngineNanoInstance {
	std::array<float, 3> startPosition{};
	std::array<float, 3> velocity{};
	std::array<float, 4> frames{};
	std::array<std::uint8_t, 4> color{};
};
static_assert(sizeof(NativeEngineNanoInstance) == 44);

struct NativeEngineNanoTemplateVertex {
	std::array<float,3> position{},normal{};
	std::array<float,2> glowUV{};
	float isGlow=0;
};
static_assert(sizeof(NativeEngineNanoTemplateVertex)==36);

struct NativeEngineNanoAppearance {
	float drawRadius = 0;
	float sizeVariation = 0;
	float baseAlpha = 0;
	float alphaVariation = 0;
	float glowScale = 0;
	float glowIntensity = 0;
	float glowFalloff = 0;
	float colorEqualize = 0;
	float colorTargetLuma = 0;
	float hueJitter = 0;
	float coreBoost = 0;
	float showInside = 0;
	float noiseAmount = 0;
	float noiseScale = 0;
	float whiteHotspot = 0;
	float whiteHotspotThreshold = 0;
	float rotationRange = 0;
	float rotationRatePerFrame = 0;
	float noiseSpeedPerFrame = 0;
};

struct NativeWorldParticleFrame {
	std::uint32_t drawFrame = 0;
	float animationFrame = 0;
	std::array<float, 3> cameraPosition{};
	std::array<float, 3> fogCameraPosition{}; // Original fxShader always uses CAMTYPE_PLAYER.
	std::array<float, 3> cameraRight{};
	std::array<float, 3> cameraUp{};
	// Captured while the original class Draw methods use this active camera.
	// Pairing is exact; a reflected frame cannot reuse player billboards.
	std::array<float,16> cameraView{}, cameraProjection{};
	std::array<float,16> cameraViewProjection{};
	std::array<std::array<float, 4>, 6> frustumPlanes{};
	std::array<float, 3> fogColor{};
	std::array<float, 2> fogDistances{};
	std::shared_ptr<const NativeWorldParticleAtlas> atlas;
	std::shared_ptr<const NativeWorldParticleAtlas> groundAtlas;
	std::vector<NativeWorldParticleVertex> vertices;
	std::vector<std::uint32_t> indices;
	std::vector<NativeWorldParticleRange> ranges;
	std::vector<NativeWorldProjectileModel> models;
	std::vector<NativeEngineNanoInstance> engineNano;
	std::vector<NativeEngineNanoTemplateVertex> engineNanoTemplate;
	NativeEngineNanoAppearance engineNanoAppearance;
	bool visibleWater = false;
	bool drawReflection = false;
	bool drawRefraction = false;
	bool softenRequested = false;
	float softenThreshold = 0;
	// Ownership observations, never a mode override: original Lua may suppress
	// engine emission. Lua shapes require their real original instance consumer.
	bool engineShapeConfigured = false;
	bool luaShapeConsumerAvailable = false;
	std::vector<std::string> unsupported;
};

} // namespace rendering
