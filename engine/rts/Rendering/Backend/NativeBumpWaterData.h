/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/WorldFrameData.h"
#include <span>

namespace rendering {

// Exact CBumpWater family. These are graphical defaults, never fabricated GL
// extension flags. A complete producer and consumer must precede activation.
struct NativeBumpWaterSettings {
	uint32_t reflectionSize = 512, reflectionDepthBits = 24;
	uint32_t reflection = 1, refraction = 1;
	float anisotropy = 0;
	bool depthCopy = true, blurReflection = false, shoreWaves = true;
	bool endlessOcean = true, dynamicWaves = true;
};
struct NativeBumpWaterMaterial {
	uint64_t generation = 0;
	// Original shader #defines are frozen when InitResources compiles it.
	std::array<int32_t,4> shaderViewport{};
	WorldVector3 surfaceColor{}, diffuseColor{}, specularColor{}, sunDirection{};
	float surfaceAlpha = 0, ambientFactor = 0, diffuseFactor = 0;
	float specularFactor = 0, specularPower = 0;
	float fresnelMin = 0, fresnelMax = 0, fresnelPower = 0, reflectionDistortion = 0;
	float blurBase = 0, blurExponent = 0;
	float perlinStartFrequency = 0, perlinLacunarity = 0, perlinAmplitude = 0;
	float waveOffsetFactor = 0, waveLength = 0, waveFoamDistortion = 0, waveFoamIntensity = 0;
	float causticsResolution = 0, causticsStrength = 0, shadowDensity = 0;
	uint32_t numTiles = 1;
	bool shoreWaves = false, hasWaterPlane = false, forceRendering = false;
	bool initialVisibleWater = false;
	std::shared_ptr<const WorldTexture> normal, foam, waveRandom;
	std::vector<std::shared_ptr<const WorldTexture>> caustics;
};
struct NativeBumpWaterNormalUpdate {
	uint64_t sequence = 0;
	int64_t simulationFrame = -1;
	bool initialize = false;
	// The actual original unsynced RNG result. Neither GPU nor frame replay
	// synthesizes another permutation or advances simulation/RNG state.
	std::vector<uint8_t> tileOffsets;
};
struct NativeBumpWaterCoastRect {
	std::array<float,4> mapUV{}, atlasUV{}; // x1,y1,x2,y2; original pixel centers.
	bool coastline = false;
};
struct NativeBumpWaterCoastUpdate {
	uint64_t sequence = 0;
	bool initialize = false;
	uint32_t atlasWidth = 0, atlasHeight = 0;
	std::vector<uint8_t> rgba; // Exact CTextureAtlas CPU page, including padding.
	std::vector<NativeBumpWaterCoastRect> rectangles;
};
struct NativeBumpWaterFrame {
	uint64_t generation = 0, sourceDrawFrame = 0;
	int64_t simulationFrame = -1;
	float interpolationOffset = 0;
	// Original textures keep their InitResources extent until resource reload.
	uint32_t screenTextureWidth = 0, screenTextureHeight = 0;
	uint32_t mapSquaresX = 0, mapSquaresZ = 0, paddedSquaresX = 0, paddedSquaresZ = 0;
	uint32_t logicalWidth = 0, logicalHeight = 0;
	bool drawWater = false, voidWater = false, belowSeaLevel = false;
	bool shadows = false, infoTexture = false, wireframe = false;
	NativeBumpWaterSettings settings;
	std::shared_ptr<const NativeBumpWaterMaterial> material;
	WorldCamera camera;
	WorldMatrix shadowMatrix{};
	std::array<float,4> fogColor{}, fogParameters{};
	// Exact original GenWaterPlaneBuffer output; radial uses original fastmath.
	std::shared_ptr<const std::vector<WorldVector3>> vertices;
	bool radial = false;
	// Immutable unretired source operations in source order. A consumer retires
	// sequences only after the enclosing common submission actually completes.
	std::vector<NativeBumpWaterNormalUpdate> normalUpdates;
	std::vector<std::shared_ptr<const NativeBumpWaterCoastUpdate>> coastUpdates;
};

enum NativeBumpWaterFlag : uint32_t {
	BumpReflection=1, BumpRefraction=2, BumpShore=4, BumpDepth=8,
	BumpBlur=16, BumpEndless=32, BumpShadow=64, BumpInfo=128,
	BumpMinusOneDepth=256,
};
// Shared CPU/MSL ABI. All vectors occupy16bytes; matrices are column-major.
struct alignas(16) NativeBumpWaterUniforms {
	WorldMatrix viewProjection{}, view{}, shadow{};
	std::array<float,4> eyeFrame{}, mapMidSpecularPower{}, screenInverseTextureInverse{};
	std::array<float,4> viewportOriginProjection{}, texGenPlane{}, shadingPlane{}, surfaceColor{};
	std::array<float,4> diffuseColorFactor{}, specularColorFactor{}, sunDirectionAmbient{};
	std::array<float,4> fresnelDistortion{}, blurPerlin{}, perlinWave{}, waveCaustic{};
	std::array<float,4> fogColor{}, fogParameters{};
	// render pixel -> original logical GL window coordinate: scaleX,scaleY,
	// offsetX,offsetY. Usually(1,-1,0,logicalHeight); target size is explicit.
	std::array<float,4> fragmentToLogical{};
	// x=source flags, y=texture flipV bits, z/w reserved.
	std::array<uint32_t,4> flags{};
};
static_assert(sizeof(NativeBumpWaterUniforms)==480);

bool NativeBumpWaterVisible(const NativeBumpWaterFrame&);
bool NativeBumpWaterDynamicNormals(const NativeBumpWaterFrame&);
bool NativeBumpWaterShoreWaves(const NativeBumpWaterFrame&);
size_t NativeBumpWaterCausticIndex(int64_t simulationFrame, size_t count);
float NativeBumpWaterNormalAlpha(const NativeBumpWaterNormalUpdate&);
void ValidateNativeBumpWater(const NativeBumpWaterFrame&);
NativeBumpWaterUniforms MakeNativeBumpWaterUniforms(const NativeBumpWaterFrame&,
	uint32_t renderWidth,uint32_t renderHeight,uint32_t textureFlipMask);
std::array<uint8_t,4> NativeBumpWaterCoastPixel(float height);

// Exact AddQuadTriangles(3,0,1,3,1,2) streams. These preserve float32 source
// operations and original atlas bounds instead of substituting full-screen UVs.
struct NativeBumpWaterUpdateVertex {
	std::array<float,2> position{};
	std::array<float,4> uv{};
};
static_assert(sizeof(NativeBumpWaterUpdateVertex)==24);
std::vector<NativeBumpWaterUpdateVertex> MakeNativeBumpWaterNormalVertices(uint32_t tiles, const NativeBumpWaterNormalUpdate&);
std::vector<NativeBumpWaterUpdateVertex> MakeNativeBumpWaterCoastVertices(const NativeBumpWaterCoastUpdate&, uint32_t radius);

} // namespace rendering
