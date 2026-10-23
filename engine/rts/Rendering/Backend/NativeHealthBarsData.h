/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeUnitPrimitiveData.h"
#include <array>
#include <vector>
namespace rendering {
// Exact original GS input, including uint attributes at locations 1 and 4.
struct NativeHealthBarsInstance {
	std::array<float,4> heightTimers;
	std::array<std::uint32_t,4> typeIndexUniform;
	std::array<float,4> minColor,maxColor;
	std::array<std::uint32_t,4> instanceData;
};
static_assert(sizeof(NativeHealthBarsInstance)==80);
struct NativeHealthBarsConfig {
	float height=.9f,corner=.16f,smallCorner=.096f;
	bool debugShow=false;
};
struct NativeHealthBarsDraw {
	NativeBufferSnapshotRef vertices;
	std::uint32_t first=0,count=0;
	NativeHealthBarsConfig config;
	float iconDistance=27,cameraDistanceMult=1,cameraDistanceMultGlyph=4,skipGlyphsNumbers=0;
	NativeUnitPrimitiveUniforms uniforms;
	// The real uploader's immutable 128-byte rows. Instance addresses are not IDs.
	std::shared_ptr<const NativeCUSPublishedBuffer> modelUniforms;
	std::shared_ptr<const ScreenTexture> atlas;
	ScreenSampler sampler;
	ScreenDrawState surface;
};
// Dense upload of the exact published row addressed by each original point.
// CPU resolves immutable addresses only; shader health/animation stays on GPU.
struct NativeHealthBarsModelRow {
	std::array<std::uint32_t,4> metadata;
	std::array<float,4> health,drawPos,speed;
	std::array<std::array<float,4>,4> user;
};
static_assert(sizeof(NativeHealthBarsModelRow)==128);
void ValidateNativeHealthBarsDraw(const NativeHealthBarsDraw&);
std::vector<NativeHealthBarsModelRow> ReadNativeHealthBarsModelRows(const NativeHealthBarsDraw&);
void AcquireNativeHealthBarsConsumer() noexcept;
void ReleaseNativeHealthBarsConsumer() noexcept;
bool IsNativeHealthBarsConsumerAvailable() noexcept;
}
