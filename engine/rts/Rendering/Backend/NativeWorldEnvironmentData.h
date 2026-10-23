/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "WorldFrameData.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace rendering {
struct NativeCUSTextureObject;
struct NativeCubeReflectionUpdate;

enum class NativeSkyKind { CubeMap, Equirectangular, Modern, OriginalNull };

// Owned original sky input, not an environment-color approximation. Cube face
// order is the pinned nv_dds order after its original flip and +/-Y face swap.
struct NativeWorldSkyResource {
	NativeSkyKind kind = NativeSkyKind::OriginalNull;
	std::string sourcePath;
	std::string sourceIdentity;
	WorldTextureFormat format = WorldTextureFormat::RGBA8Unorm;
	std::array<std::vector<WorldTextureMip>, 6> cubeFaces;
	std::shared_ptr<const WorldTexture> equirectangular;
	bool generateMipmaps = false;
	bool mipmappedSampling = true;
	std::string diagnostic;
};

struct NativeWorldSpecularResource {
	uint32_t size=0;
	uint64_t revision=0;
	std::array<std::vector<uint8_t>,6> faces; // Original CPU CreateSpecularFace/Part RGBA8 bytes.
};

// Captured at the original IBL CPU stage, after DrawGenesis and before
// DrawWorldPreUnit. Revisions retain dirty events even if no drawable submits.
// They are acknowledged only by the GPU consumer after successful retirement.
struct NativeWorldEnvironmentFrame {
	uint64_t captureSerial = 0;
	uint64_t sunDirectionRevision = 0, sunLightingRevision = 0;
	uint64_t skyRevision = 0, waterRevision = 0;
	std::shared_ptr<const NativeWorldSkyResource> sky;
	std::shared_ptr<const NativeWorldSpecularResource> specular;
	std::shared_ptr<const NativeMetalInfoFrame> metalInfoFrame;
	std::shared_ptr<const NativeWorldInfoFrame> worldInfoFrame; // Original pre-PreUnit info version, before later Lua mode changes.
	WorldTerrain terrain;
	WorldLighting lighting;
	WorldLighting reflectionLighting; // Captured before the original SunLight::Update.
	std::shared_ptr<const NativeCUSTextureObject> sourceReflection, sourceSkyReflection, sourceSpecular;
	std::shared_ptr<const NativeCubeReflectionUpdate> sourceCubeUpdate;
	std::shared_ptr<const NativeTerrainSceneSourceFrame> reflectionGround; // Actual selected original face/camera, once.
	std::array<WorldCamera, 6> reflectionCameras;
	std::array<float, 4> skyAxisAngle{};
	WorldMatrix skyRotation{};
	std::array<float, 4> skyColor{}, sunColor{}, cloudColor{}, waterPlaneColor{};
	bool drawSky = true, drawGround = true, drawFog = true;
	bool skyOnlyReflections = false;
	bool reflectionMipmaps = false;
	uint32_t reflectionSize = 0, specularSize = 0;
	float animationTime = 0;
	uint32_t unitBRDFSize = 512, unitBRDFGOption = 1; // Exact30770 api_pbr_enabler.
	std::vector<std::string> unsupported;
};

} // namespace rendering
