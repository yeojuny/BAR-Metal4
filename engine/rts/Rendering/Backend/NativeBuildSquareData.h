/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <string_view>

namespace rendering {
struct NativeBuildSquareProgramSnapshot {
	float heightOffset=.5f,waterLevel=0,cellInset=1,cellSize=8;
	float simplifiedReferenceCells=6,simplifiedSizeFalloff=.55f,minimumScreenDiameter=3,cornerRadius=.22f;
	float simplifiedOutlineScale=.5f,simplifiedCornerRadiusScale=.5f,footprintBoundaryEnabled=1,footprintBoundaryWidth=.22f;
	std::array<float,4> invalidFootprintBoundaryColor{1,.15f,.15f,.4f};
	float showInvalidFootprintBoundary=0,alphaMultiplier=1;
	std::int32_t heightUnit=0,isMiniMap=0,rotationMiniMap=0;
};
class NativeBuildSquareProgramState {
public:
	NativeBuildSquareProgramState(std::string_view vertex,std::string_view fragment,std::string_view engine);
	int Location(std::string_view)const;
	bool Floats(int,std::span<const float>);
	bool Integers(int,std::span<const std::int32_t>);
	NativeBuildSquareProgramSnapshot Capture()const{return state;}
private: NativeBuildSquareProgramSnapshot state;
};
struct alignas(16) NativeBuildSquareUniformBlock {
	std::array<float,16> cameraViewProjection{};
	std::array<float,4> mapSize{},viewGeometry{},values0{},values1{},values2{},boundaryColor{},values3{};
	std::array<std::uint32_t,4> options{},alpha{};
};
static_assert(sizeof(NativeBuildSquareUniformBlock)==208);
// The unchanged original Lua computes every status, yardmap, queue, snap,
// pregame and merging decision. Capture its real immutable VBO generations.
struct NativeBuildSquareDraw {
	std::uint64_t sequence=0,sourceDrawFrame=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::PreUnit;
	NativeBuildSquareProgramSnapshot program;
	NativeBufferSnapshotRef vertices,instances;
	std::uint32_t firstVertex=0,vertexCount=0,firstInstance=0,instanceCount=0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	NativeCUSTextureBinding height;
	std::shared_ptr<const ScreenTexture> dependencyImage;
	bool mapSourceDependent=false;
	ScreenDrawState surface;
	NativeWorldDrawState raster;
};
std::uint64_t NativeBuildSquareSourceHash(std::string_view)noexcept;
void ValidateNativeBuildSquareDraw(const NativeBuildSquareDraw&);
NativeBuildSquareUniformBlock ReadNativeBuildSquareUniforms(const NativeBuildSquareDraw&);
}
