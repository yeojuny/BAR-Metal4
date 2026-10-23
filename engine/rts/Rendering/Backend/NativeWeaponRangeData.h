/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <string_view>

namespace rendering {
enum class NativeWeaponRangeFamily : std::uint8_t { Attack, Defense };
struct NativeWeaponRangeDefines {
	NativeWeaponRangeFamily family=NativeWeaponRangeFamily::Attack;
	std::string originalText;
	float gravity=0,mouseMultiplier=1;
	bool maskPass=false,composite=false;
	std::uint32_t heightmapSteps=8;
};
struct NativeWeaponRangeProgramSnapshot {
	std::shared_ptr<const NativeWeaponRangeDefines> defines;
	float lineAlpha=1,cannon=0,fadeOffset=0,inMiniMap=0;
	float selectionCount=1,drawAlpha=1,drawMode=0,staticUnits=0;
	std::int32_t rotation=0,heightUnit=0;
	std::int32_t maskUnit=3;
	float builderCount=1,maskClip=0,maskChannelBit=1,maskWriteValue=0,maskDepthBase=0.1f;
	std::array<std::array<float,4>,4> fillColors{};
	std::array<float,4> pipVisibleArea{0,1,0,1};
};
// Consumes the complete strings produced by the unchanged exact31657
// LuaShader, including its actual float32-Lua formatter and engine block.
class NativeWeaponRangeProgramState {
public:
	NativeWeaponRangeProgramState(std::string_view vertex,std::string_view fragment);
	int Location(std::string_view)const;
	bool Floats(int,std::span<const float>);
	bool Integers(int,std::span<const std::int32_t>);
	NativeWeaponRangeProgramSnapshot Capture()const{return state;}
	static std::span<const std::string_view> ActiveUniforms();
private:
	NativeWeaponRangeProgramSnapshot state;
};
struct alignas(16) NativeWeaponRangeUniformBlock {
	std::array<float,16> cameraView{},cameraViewProjection{},cameraViewInverse{},miniMapViewProjection{};
	std::array<float,4> mapSize{},fogColor{},fogParams{},mouseWorldPos{};
	std::array<float,4> values0{},values1{},pipVisibleArea{},raster{};
	std::array<std::uint32_t,4> options{}; // rotation, GL clip depth, height flip, count
	std::array<float,4> viewport{}; // actual Metal raster origin/extent
	std::array<std::uint32_t,4> renderOptions{}; // line path, alpha test
	std::array<float,4> outputViewport{}; // line quads use full attachment viewport
	std::array<float,4> maskParameters{}; // builder count, clip, class bit, write value
	std::array<std::array<float,4>,4> fillColors{};
	std::array<float,4> viewGeometry{};
	std::array<float,4> sourceViewport{}; // original GL window viewport, before native scaling
	std::array<std::uint32_t,4> programOptions{}; // mask pass, steps, composite, mask image flip
};
static_assert(sizeof(NativeWeaponRangeUniformBlock)==576);
struct NativeWeaponRangeDraw {
	std::uint64_t sequence=0,sourceDrawFrame=0;
	NativeWeaponRangeProgramSnapshot program;
	NativeDrawPrimitive primitive=NativeDrawPrimitive::LineLoop;
	NativeWorldDrawStage stage=NativeWorldDrawStage::World;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::shared_ptr<const NativeCUSPublishedBuffer> models;
	NativeBufferSnapshotRef vertices,instances;
	std::uint32_t firstVertex=0,vertexCount=0,firstInstance=0,instanceCount=0;
	NativeCUSTextureBinding height;
	NativeCUSTextureBinding coverageMask;
	std::shared_ptr<const ScreenTexture> coverageDependencyImage;
	// Preserve the original unit1 $info binding event and version. The pinned
	// exact31657 shaders do not sample it, so this is not an image dependency.
	NativeCUSTextureBinding info;
	std::shared_ptr<const ScreenTexture> dependencyImage;
	bool mapSourceDependent=false;
	ScreenDrawState surface;
	NativeWorldDrawState raster;
};
struct NativeWeaponRangeFrame {
	std::uint64_t sourceDrawFrame=0;
	std::vector<NativeWeaponRangeDraw> draws;
};
std::uint64_t NativeWeaponRangeSourceHash(std::string_view) noexcept;
NativeWeaponRangeDefines ParseNativeWeaponRangeDefines(std::string_view);
void ValidateNativeWeaponRangeDraw(const NativeWeaponRangeDraw&);
NativeWeaponRangeUniformBlock ReadNativeWeaponRangeUniforms(const NativeWeaponRangeDraw&);
// Only the original dynamic branch reads the source SSBO. Static Defense
// rows leave instData at zero, and its dead selectedness expression must not
// cause a fabricated read of an unobserved model row.
std::vector<std::array<float,4>> ReadNativeWeaponRangeModelPositions(const NativeWeaponRangeDraw&);
} // namespace rendering
