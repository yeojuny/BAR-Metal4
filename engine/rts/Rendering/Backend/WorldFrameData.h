/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "MapEdgeData.h"
#include "NativeMainSamplingData.h"
#include "NativeWorldMaterialData.h"
#include "NativeCUSMaterialControl.h"

namespace rendering {
struct NativeS3OTexturePair;

struct NativeWorldEnvironmentFrame;
struct NativeWorldDrawFrame;
struct NativeWorldParticleFrame;
struct NativeLuaNanoFrame;
struct NativeProjectileEffectFrame;
struct NativeEnergyExplosionFrame;
struct NativeMetalInfoFrame;
struct NativeVolumetricCloudFrame;
struct NativeLightningEffectFrame;
struct NativeFireSmokeFrame;
struct NativeMissileThrusterFrame;
struct NativeFlamethrowerFrame;
struct NativeFireGL4Frame;
struct NativeNapalmPreviewFrame;
struct NativeSnowFrame;
struct NativeCommandsFrame;
struct NativeBloomFrame;
struct NativeDeferredLightFrame;
struct NativeUnitStencilFrame;
struct NativeOutlineFrame;
struct NativeShieldFrame;
struct NativeShieldDepthFrame;
struct NativeOpaqueModelFrame;
struct NativeCUSSourceFrame;
struct NativeBlueprintFrame;
struct NativeBlockoutFrame;
struct NativePlacementFrame;
struct NativeGroundDecalFrame;
struct NativeGameDecalFrame;
struct NativeUnitPrimitiveFrame;
struct NativeStartBoxFrame;
struct NativeWeaponRangeFrame;
struct NativeSensor31657CircleFrame;
struct NativeSensorPreviewFrame;
struct NativeMapTextureFrame;
struct NativeBumpWaterFrame;
struct NativeBasicWaterFrame;
struct NativeLegacyWaterFrame;
struct NativeDynamicWaterFrame;
struct NativeWaterSceneFrame;
struct NativeWaterLuaFrame;
struct NativeShadowColorFrame;
struct NativeOriginalShadowFrame;
struct NativeWorldInfoFrame;
struct NativeTerrainSceneSourceFrame;

using WorldMatrix = std::array<float, 16>; // Column-major engine matrix.
using WorldVector3 = std::array<float, 3>;

enum class ClipDepthConvention { MinusOneToOne, ZeroToOne, Unsupported };

struct WorldCamera {
	WorldMatrix view{};
	WorldMatrix projection{};
	WorldVector3 position{};
	// Logical SDL/picking viewport; presentation target pixels are separate.
	std::array<int32_t, 4> viewport{}; // x, y, width, height
	ClipDepthConvention clipDepth = ClipDepthConvention::Unsupported;
};

enum class WorldTextureFormat { BC1RGBA, BC2RGBA, BC3RGBA, BC4Unorm, BC5Unorm, BC7RGBA, BC7RGBASRGB, RGBA8Unorm, BGRA8Unorm };

struct WorldTextureMip {
	uint32_t width = 0;
	uint32_t height = 0;
	size_t bytesPerRow = 0;
	std::vector<uint8_t> bytes;
};

struct WorldTexture {
	uint32_t originalInternalFormat = 0; // Actual original upload format; zero is unobserved.
	std::string sourcePath; // Resolved engine VFS path, not a backend file request.
	WorldTextureFormat format = WorldTextureFormat::RGBA8Unorm;
	std::vector<WorldTextureMip> mips;
	// Original DDS rows are retained. Unit DDS sampling applies this explicit
	// V inversion to match the engine's load-time flip; ordinary images do not.
	bool flipV = false;
	bool generateMipmaps = false; // Original single-level CBitmap upload contract.
	bool mipmappedSampling = true; // Pinned DDS two-total-level upload selects GL_LINEAR.
};

struct WorldModelVertex {
	WorldVector3 position{}; // Exact piece-local engine geometry.
	WorldVector3 normal{};
	WorldVector3 sTangent{};
	WorldVector3 tTangent{};
	std::array<std::array<float, 2>, 2> uv{};
	std::array<uint16_t, 4> boneIds{};
	std::array<uint8_t, 4> boneWeights{};
};

struct WorldModelPiece {
	std::string name;
	uint32_t parent = std::numeric_limits<uint32_t>::max();
	WorldMatrix bindPose{};
	std::vector<WorldModelVertex> vertices;
	std::vector<uint32_t> indices; // Local indices; original triangulation retained.
};

enum class WorldModelGL4BoneLayout : uint8_t { Unclassified, RigidPerPiece, Unsupported };

struct WorldModel {
	int32_t engineModelId = -1;
	std::string sourceName;
	std::array<std::string, 2> textureReferences;
	std::array<std::shared_ptr<const WorldTexture>, 2> textures;
	std::vector<WorldModelPiece> pieces; // Engine preorder, root index 0.
	// Classified while copying geometry, before publishing this immutable owner.
	// A replacement geometry owner must receive its own classification.
	WorldModelGL4BoneLayout gl4BoneLayout = WorldModelGL4BoneLayout::Unclassified;
	WorldVector3 minimum{};
	WorldVector3 maximum{};
};

struct WorldPiecePose {
	WorldMatrix modelSpace{}; // Current pose maps piece-local vertices to model.
	bool visible = false;
};

struct WorldUnit {
	int32_t unitId = -1;
	int32_t teamId = -1;
	std::array<uint8_t, 4> teamColor{}; // Exact engine RGBA bytes.
	bool selected = false;
	WorldMatrix world{};
	std::shared_ptr<const WorldModel> model;
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	std::vector<WorldPiecePose> pieces; // Same order as model->pieces.
	std::shared_ptr<const WorldModelMaterial> material;
	float healthFraction = 1;
	bool opaque = true;
	bool castsShadow = false;
	bool alpha = false;
	float drawAlpha = 1;
};

struct WorldFeature {
	int32_t featureId = -1;
	int32_t definitionId = -1;
	int32_t teamId = -1;
	std::array<uint8_t, 4> teamColor{};
	WorldMatrix world{}; // Actual interpolated CFeature cached transform.
	std::shared_ptr<const WorldModel> model;
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	std::shared_ptr<const WorldModelMaterial> material;
	std::vector<WorldPiecePose> pieces;
	float drawAlpha = 1; // Exact original FeatureDrawerData distance fade.
	float healthFraction = 1;
	bool opaque = false;
	bool fading = false;
	bool castsShadow = false;
	bool inWater = false;
};

struct WorldShadow {
	WorldMatrix view{}, projection{}; // Original drawing matrices.
	ClipDepthConvention clipDepth = ClipDepthConvention::Unsupported;
	std::array<float, 4> parameters{};
	uint32_t size = 0;
	uint32_t generationBits = 0;
	float terrainSlopeBias = 0;
	float terrainDepthBias = 0;
};

struct WorldTerrainTiles {
	static constexpr uint32_t TILE_TEXELS = 32;
	static constexpr uint32_t TILE_SQUARES = 4;
	static constexpr size_t BYTES_PER_TILE = 680;
	static constexpr std::array<uint32_t, 4> MIP_TEXELS = {32, 16, 8, 4};
	static constexpr std::array<size_t, 4> MIP_OFFSETS = {0, 512, 640, 672};
	static constexpr std::array<size_t, 4> MIP_BYTES = {512, 128, 32, 8};
	uint32_t gridWidth = 0;
	uint32_t gridHeight = 0;
	uint32_t uniqueTileCount = 0;
	std::vector<uint32_t> tileIndices; // Original SMF order: z*gridWidth+x.
	std::vector<uint8_t> bc1Bytes; // Tile i begins at i*BYTES_PER_TILE.
	std::vector<std::string> sourcePaths;
	// Original SMT rows and tile-grid rows increase map z. Terrain sampling
	// uses that order directly, without a unit-DDS V inversion.
};

struct WorldTerrainHeights {
	uint32_t width = 0;  // map squares in x + 1
	uint32_t height = 0; // map squares in z + 1
	std::vector<float> corners; // Exact unsynced values, z*width+x.
};

struct WorldTerrain {
	std::string sourceMap;
	uint32_t squaresX = 0;
	uint32_t squaresZ = 0;
	float squareSize = 8.0f;
	// Tile identity remains stable when a fresh height snapshot is published.
	std::shared_ptr<const WorldTerrainTiles> tiles;
	std::shared_ptr<const WorldTerrainHeights> heights;
	std::shared_ptr<const struct WorldTerrainMaterial> material;
};

struct WorldTerrainMaterial {
	// Actual original groundDrawer mode, not the persisted config or GL flags.
	bool advancedShading = true;
	std::shared_ptr<const struct NativeMiniMapBackground> standardShading;
	uint64_t sourceRevision = 0;
	std::shared_ptr<const WorldTexture> detail, specular, blendNormals, emission;
	std::shared_ptr<const WorldTexture> splatDetail, splatDistribution;
	std::array<std::shared_ptr<const WorldTexture>, 4> splatNormals;
	std::array<float, 4> splatScales{}, splatMultipliers{};
	bool splatNormalDiffuseAlpha = false;
	bool voidWater = false, voidGround = false, waterAbsorption = false;
	float voidAlphaMinimum = 0;
	float minimumHeight = 0;
	// Resources requiring additional ordered producers remain named, never
	// silently replaced by a constant or a lower material specialization.
	std::string skyReflectionModulation, parallaxHeight;
	// Original requested settings, independent of GL capability flags. The
	// native consumer resolves these against its actual sampler contract.
	float smfAnisotropy = 4;
	float ssmfAnisotropy = 4;
	float textureLodBias = 0;
};

struct WorldMapEdgeResources {
	uint64_t sourceRevision = 0;
	std::shared_ptr<const WorldTexture> color; // Actual $grass selection/default SMF minimap.
	std::shared_ptr<const WorldTexture> normal; // Actual $ssmf_normals / detailNormalTex.
};
struct WorldMapEdge {
	std::shared_ptr<const mapedge::DrawPacket> draw;
	std::shared_ptr<const WorldMapEdgeResources> resources;
	std::array<float,4> sunDirection{};
	std::array<float,4> fogColor{};
	std::array<float,4> fogParameters{}; // Original start/end/0/inverseRange in elmos.
};

struct WorldFrame {
	NativeMainSampling mainSampling;
	// Config notifications run at the engine main-loop boundary. Capture the
	// effective rate once with the original frame; it never reallocates targets.
	float minSampleShadingRate = 0.0f;
	uint64_t simulationFrame = 0;
	// Actual unsynced draw generation; simulationFrame may remain unchanged
	// while paused or before Ready and cannot identify a framebuffer source.
	uint64_t sourceDrawFrame = 0;
	float interpolationOffset = 0.0f;
	WorldCamera viewCamera;
	WorldTerrain terrain;
	WorldLighting lighting;
	NativeCUSMaterialState cusMaterial; // Effective original cus2/actions, captured once for all instances.
	std::optional<WorldShadow> shadow;
	std::shared_ptr<const NativeShadowColorFrame> shadowColorFrame;
	std::shared_ptr<const NativeOriginalShadowFrame> originalShadow;
	std::shared_ptr<const NativeWorldInfoFrame> worldInfoFrame;
	std::optional<WorldMapEdge> mapEdge; // Absent unless original enabled widget submitted this frame.
	std::vector<WorldUnit> units;
	std::vector<WorldFeature> features;
	std::shared_ptr<const NativeWorldEnvironmentFrame> environmentFrame;
	std::shared_ptr<const NativeTerrainSceneSourceFrame> mainGround;
	std::shared_ptr<const NativeWorldDrawFrame> luaWorldDrawFrame;
	std::shared_ptr<const NativeWorldParticleFrame> particleFrame;
	std::shared_ptr<const NativeLuaNanoFrame> luaNanoFrame;
	std::shared_ptr<const NativeProjectileEffectFrame> luaProjectileEffectsFrame;
	std::shared_ptr<const NativeEnergyExplosionFrame> luaEnergyExplosionFrame;
	std::shared_ptr<const NativeVolumetricCloudFrame> luaVolumetricCloudFrame;
	std::shared_ptr<const NativeLightningEffectFrame> luaLightningEffectFrame;
	std::shared_ptr<const NativeFireSmokeFrame> luaFireSmokeFrame;
	std::shared_ptr<const NativeMissileThrusterFrame> luaMissileThrusterFrame;
	std::shared_ptr<const NativeFlamethrowerFrame> luaFlamethrowerFrame;
	std::shared_ptr<const NativeFireGL4Frame> luaFireGL4Frame;
	std::shared_ptr<const NativeNapalmPreviewFrame> luaNapalmPreviewFrame;
	std::shared_ptr<const NativeSnowFrame> luaSnowFrame;
	std::shared_ptr<const NativeCommandsFrame> luaCommandsFrame;
	std::shared_ptr<const NativeBloomFrame> luaBloomFrame;
	std::shared_ptr<const NativeDeferredLightFrame> luaDeferredLightFrame;
	std::shared_ptr<const NativeUnitStencilFrame> luaUnitStencilFrame; // Actual DrawGenesis producer; independent of presentation.
	std::shared_ptr<const NativeOutlineFrame> luaOutlineFrame;
	std::shared_ptr<const NativeShieldFrame> luaShieldFrame;
	std::shared_ptr<const NativeShieldDepthFrame> shieldDepthFrame;
	// Real original source capture, supplied only by its installed PBR owner.
	// Its presence selects actual MRT model-depth ownership, never standalone.
	std::shared_ptr<const NativeOpaqueModelFrame> opaqueModelFrame;
	// Actual original alpha model ownership: below, above, or All when no water.
	std::array<std::shared_ptr<const NativeOpaqueModelFrame>,3> alphaModelFrames;
	std::shared_ptr<const NativeCUSSourceFrame> genesisCUSFrame;
	std::shared_ptr<const NativeBlueprintFrame> luaBlueprintFrame;
	std::shared_ptr<const NativeBlockoutFrame> luaBlockoutFrame;
	std::shared_ptr<const NativePlacementFrame> placementFrame;
	std::shared_ptr<const NativeGroundDecalFrame> groundDecals;
	std::shared_ptr<const NativeGameDecalFrame> luaGameDecalFrame;
	std::shared_ptr<const NativeUnitPrimitiveFrame> luaUnitPrimitiveFrame;
	std::shared_ptr<const NativeStartBoxFrame> luaStartBoxFrame;
	std::shared_ptr<const NativeWeaponRangeFrame> luaWeaponRangeFrame;
	std::shared_ptr<const NativeSensor31657CircleFrame> luaSensor31657CircleFrame;
	std::shared_ptr<const NativeSensorPreviewFrame> luaSensorPreviewFrame;
	std::shared_ptr<const NativeMapTextureFrame> mapTextures;
	std::shared_ptr<const NativeBumpWaterFrame> bumpWater;
	std::shared_ptr<const NativeBasicWaterFrame> basicWater;
	std::shared_ptr<const NativeLegacyWaterFrame> legacyWater;
 std::shared_ptr<const NativeDynamicWaterFrame> dynamicWater;
	std::shared_ptr<const NativeWaterSceneFrame> waterScene;
	std::shared_ptr<const NativeWaterLuaFrame> alphaAllLua; // Original no-water PreParticles(true,true,false,false).
	std::shared_ptr<const NativeMetalInfoFrame> metalInfoFrame;
	std::shared_ptr<const WorldTexture> featureBRDF;
	std::vector<std::string> unsupported;
};

// All resource storage is owned and immutable through shared_ptr<const>.
// Frames retain no engine pointers. This experimental scene snapshot is not a
// declaration of full engine/Lua/PBR/shadow/water/UI rendering capabilities.

} // namespace rendering
