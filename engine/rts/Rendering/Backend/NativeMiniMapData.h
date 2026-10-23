/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "WorldFrameData.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rendering {

struct ScreenTexture;
struct NativeMetalInfoFrame;
struct NativeWorldInfoFrame;

// Actual CMiniMap CPU state. Position/size stay in logical SDL coordinates;
// raster extents and Retina backing dimensions never rewrite these values.
struct NativeMiniMapState {
	std::uint64_t updateSerial = 0;
	std::array<std::int32_t, 2> position{};
	std::array<std::uint32_t, 2> size{};
	std::uint32_t rotation = 0; // original quarter turns: 0, 1, 2, 3
	bool minimized = false;
	bool maximized = false;
	bool slave = false;
	bool dualScreen = false;
	// Actual window rectangles from CGlobalRendering. position/size above is
	// the original aspect-fitted map rectangle, not the full dual clear area.
	std::array<std::int32_t, 4> mainViewport{}, dualViewport{};
	bool useIcons = true;
	bool simpleColors = false;
	bool drawProjectiles = false;
	bool drawPings = false;
	std::int32_t drawCommands = 0;
	float unitSizeX = 0;
	float unitSizeY = 0;
	float selectRadius = 0;
	std::array<float, 4> myColor{}, allyColor{}, enemyColor{};
	std::array<WorldMatrix, 3> viewMatrices{}, projectionMatrices{};
};

// Shared original UnitDrawer quad output, after its actual viewer/filter/sort
// and minimap rotation. Minimap positions remain map-space XZ. The main-screen
// icon collector also reuses this quad transport with normalized 0..1 positions;
// its separate consumer applies the original SCREEN alpha/blend/depth state.
struct NativeMiniMapIcon {
	std::array<std::array<float, 2>, 4> positions{};
	std::array<std::array<float, 2>, 4> uv{};
	std::array<std::uint8_t, 4> color{};
	std::shared_ptr<const ScreenTexture> atlas;
};

// Original DrawUnitIcons billboard output in actual world XYZ, before the
// camera projection. Atlas lifetime and stable source order survive recording.
struct NativeWorldUnitIcon {
	std::array<std::array<float, 3>, 4> positions{};
	std::array<std::array<float, 2>, 4> uv{};
	std::array<std::uint8_t, 4> color{};
	std::shared_ptr<const ScreenTexture> atlas;
};

// Original world-XZ line output. The consuming recorder applies the actual
// minimap rotation/view/projection and logical pixel width.
struct NativeMiniMapLine {
	std::array<float,2> start{}, end{};
	std::array<float,4> color{};
	float width = 1;
	std::optional<std::array<float,4>> endColor;
};

struct NativeMiniMapPoint {
	std::array<float,2> position{};
	std::array<float,4> color{};
	float sizePixels = 1;
};

// Original SMF shading inputs. These are the actual unsynced terrain/sun/water
// values, not a neutral multiplier or an approximation derived from a thumbnail.
struct NativeMiniMapLighting {
	std::array<float, 4> groundAmbient{}, groundDiffuse{}, lightDirection{};
	std::array<float, 3> waterBase{}, waterAbsorb{}, waterMinimum{};
	float waterLevel = 0;
	bool operator==(const NativeMiniMapLighting&) const = default;
};

// Requested original info mode is explicit even on the native build, whose
// legacy GL info handler is null. Never infer a disabled mode from a GL id.
enum class NativeMiniMapInfoMode : std::uint8_t {
	Normal, Los, Metal, Height, Path, Heat, Flow, PathCost, Custom,
};

struct NativeMiniMapCoverage {
	std::uint32_t width = 0, height = 0;
	std::vector<std::uint16_t> counts; // original CLosMap rows, increasing map Z
};

struct NativeMiniMapInfo {
	NativeMiniMapInfoMode mode = NativeMiniMapInfoMode::Normal;
	std::string customMode;
	std::uint64_t modeGeneration=0;
	std::shared_ptr<const NativeMetalInfoFrame> metal;
	std::shared_ptr<const NativeWorldInfoFrame> worldInfo; // actual global update timeline, not minimap cadence
	bool globalLos = false;
	bool highQuality = true;
	float time = 0; // actual gu->gameTime used by original infoLOS shader
	std::array<float, 4> alwaysColor{}, losColor{}, radarColor{}, jammerColor{}, radarColor2{};
	std::shared_ptr<const NativeMiniMapCoverage> groundLos, airLos, radar, jammer;
};

// One immutable source snapshot for the original MiniMapFragProg background.
// The source map image retains every original mip and its loader orientation.
// Each generation is held by recorded draws through successful GPU retirement.
struct NativeMiniMapBackground {
	std::uint64_t sourceRevision = 0;
	std::uint64_t updateSerial = 0;
	std::uint32_t squaresX = 0, squaresZ = 0;
	std::uint32_t paddedSquaresX = 0, paddedSquaresZ = 0;
	std::shared_ptr<const ScreenTexture> base;
	std::shared_ptr<const WorldTerrainHeights> heights;
	NativeMiniMapLighting lighting;
	std::shared_ptr<const NativeMiniMapInfo> info;
};

} // namespace rendering
