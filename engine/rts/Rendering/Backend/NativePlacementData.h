/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBlueprintData.h"
#include "NativeBuildSquareData.h"
#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include <string_view>

namespace rendering {
struct NativeS3OTexturePair;
struct NativeModelLightFrame;
struct NativeBuildingGridDraw {
	uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::PreUnit;
	NativeBufferSnapshotRef vertices;
	// Real source attachments retained even though the31657 shader reads only vertex ID.
	NativeBufferSnapshotRef unusedIndices,unusedInstances;
	bool generatedPatch31657=false;
	float cameraFade=0;
	std::array<float,3> lineParameters{};
	uint32_t firstVertex = 0, vertexCount = 0;
	std::shared_ptr<const WorldTerrainHeights> heights;
	std::array<float,2> mapWorldSize{};
	NativeBlueprintView view;
	std::array<float,3> mousePosition{};
	float waterLevel = 0;
	int waterSurfaceMode = 0;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
// Original engine DrawIndividualDefAlpha: a static bind-pose model, not the
// differently shaded Lua Blueprint API model. Its draw follows DrawWorld.
struct NativeBuildingSample {
	uint64_t sequence = 0;
	std::shared_ptr<const WorldModel> model;
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	WorldMatrix world{};
	std::array<uint8_t,4> teamColor{};
	float alpha = 0;
	// ModelVertProg obtains this from inverse(VP) * vec4(0,0,0,1).
	std::array<float,3> legacyCameraPosition{};
	unsigned requestedDynamicLights = 0;
	std::shared_ptr<const NativeModelLightFrame> modelLights;
};
struct NativePlacementFrame {
	std::vector<NativeBuildingGridDraw> grids;
	std::vector<NativeBuildSquareDraw> buildSquares;
	std::vector<NativeBuildingSample> samples;
};
void ValidateNativeBuildingGrid(const NativeBuildingGridDraw&);
void ValidateNativeBuildingSample(const NativeBuildingSample&);
void ValidateNativeBuildingGridProgram(std::string_view vertex,std::string_view fragment,
	std::string_view engine,std::string_view defines,bool game31657=false,
	std::array<float,2> mapWorldSize={});
NativeWorldRasterMesh BuildNativeBuildingGrid(const NativeBuildingGridDraw&,
	const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned targetSamples=1);
float NativeBuildingGridAlpha(std::array<float,3> world,std::array<float,3> mouse,
	std::array<float,3> eye,float vertexAlpha);
// Original SColor byte quantization, cell order, repeated shared edges and
// underwater corner/top lines. statuses are the engine's actual result.
struct NativePlacementVertex {std::array<float,3> position;ScreenColor color;};
std::vector<NativePlacementVertex> NativeBuildSquareVertices(std::array<float,3> position,
	int sizeX,int sizeZ,float buildHeight,bool canBuild,std::span<const uint8_t> statuses,
	uint8_t open,uint8_t occupied,uint8_t reclaimable);
void SetNativePlacementGridAvailable(bool) noexcept;
void SetNativePlacementSamplesAvailable(bool) noexcept;
bool IsNativePlacementAvailable() noexcept;
}
