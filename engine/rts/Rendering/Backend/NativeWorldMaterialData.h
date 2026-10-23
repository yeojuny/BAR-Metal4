/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace rendering {

struct WorldTexture;

// Identity of the original requested material. This is independent of which
// native producer/consumer stages have completed; it is not a quality flag.
enum class WorldMaterialFamily {
	EngineModel, UnitNormal, FeatureTreeNormal, FeatureTreeAutoNormal,
	FeatureTreeAutoNormalNoSway, FeatureMetalWreck, FeatureMetal, FeatureOther,
};

struct WorldModelMaterial {
	WorldMaterialFamily family = WorldMaterialFamily::EngineModel;
	std::string definitionSource;
	std::string definitionName;
	std::shared_ptr<const WorldTexture> normal;
	std::string requestedNormalPath; // Original selector request, retained across a verified resource repair.
	std::string normalResolutionDiagnostic; // Empty unless a known source/resource repair was applied.
	bool engineStandardGL4 = false; // Original ordinary engine family when native CUS is disabled.
	bool normalMapping = false;
	bool autoNormal = false;
	bool pbrOverride = false;
	float brightness = 1;
	bool treeWind = false;
	bool alphaShadows = false;
	bool healthDisplacement = false;
	bool healthTexturing = false;
	bool vertexAO = false;
	bool flashlights = false;
	bool environmentDiffuse = false;
	bool environmentSpecular = false;
	std::array<float, 2> autoNormalParameters{1.5f, 0.005f};
	float sunMultiplier = 1;
	// Fixed values are consumed only when the original material enables PBROVERRIDE.
	float metalness = 0.2f;
	float roughness = 0.6f;
	float emissiveness = 0;
	// Exact30770 CUS fragment uses these constants instead of its commented
	// config lookup; requested settings remain separately captured in lighting.
	std::array<float, 8> effectivePBR{4.85f,.85f,3.5f,.85f,1,.23f,1,1};
};

struct WorldRenderSettings {
	bool normalMapping = false;
	int32_t shadows = 0;
	int32_t shadowQuality = 0;
	uint32_t shadowMapSize = 0;
	uint32_t reflectionCubeSize = 0;
	uint32_t specularCubeSize = 0;
	bool reflectionMipmaps = false;
	bool deferredMap = false;
	bool deferredModel = false;
	int32_t waterRenderer = 0;
	int32_t samples = 1;
	bool softParticles = false;
	// Actual configured values, retained even when their requested producer is
	// not yet implemented. Never overwrite these with an available-path default.
	std::array<float, 8> pbrParameters{};
	float modelGamma = 1;
};

struct WorldLighting {
	std::array<float, 4> sunDirection{};
	std::array<float, 4> groundAmbient{}, groundDiffuse{}, groundSpecular{};
	std::array<float, 4> modelAmbient{}, modelDiffuse{}, modelSpecular{};
	std::array<float, 4> fogColor{}, fogParameters{};
	std::array<float, 3> waterBase{}, waterMinimum{}, waterAbsorb{};
	float specularExponent = 1;
	float groundShadowDensity = 0;
	float modelShadowDensity = 0;
	WorldRenderSettings requested;
};

} // namespace rendering
