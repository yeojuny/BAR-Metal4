/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeCUSUniformData.h"
#include <array>
#include <memory>
#include <vector>
namespace rendering {
struct NativeShieldTextureSnapshot;
// Actual current31657 BuildIcosahedronVertices positions, in original CCW order.
// Keep the existing packed vertex ABI; current31657 does not read normal/uv.
struct NativeShieldVertex {
	std::array<float,3> position, normal;
	std::array<float,2> uv;
};
static_assert(sizeof(NativeShieldVertex)==32);
struct NativeShieldGeometry {
	std::uint64_t identity=0;
	std::vector<NativeShieldVertex> vertices;
};
// The original five vec4 instance attributes, copied without repacking.
struct NativeShieldInstance {
	std::array<float,4> posRadius,rotMargin,color1,color2,params;
};
static_assert(sizeof(NativeShieldInstance)==80);
struct NativeShieldUniforms {
	std::array<float,16> view{}, projection{};
	std::array<float,4> translationScale{}, rotMargin{}, color1{}, color2{};
	std::array<std::array<float,4>,24> impacts{};
	float gameFrame=0, shieldFade=1, overlapScale=1;
	std::uint32_t effects=0, impactCount=0;
	bool depthClipZeroToOne=false;
	std::array<float,4> cameraPosition{},timeInfo{};
	// Legacy per-shield fields above remain source-compatible for offline callers;
	// the current31657 consumer reads only actual instances and storage below.
};
struct NativeShieldDraw {
	std::uint64_t sequence=0;
	std::shared_ptr<const NativeShieldGeometry> geometry;
	NativeShieldUniforms uniforms;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	NativeBufferSnapshotRef vertexSource,instanceSource;
	std::vector<NativeShieldInstance> instances;
	NativeBufferRange impactSource; // actual binding6; absent when original leaves it unbound
	std::size_t impactUploadedBytes=0; // successful original Upload prefix, relative to range
	std::vector<std::array<float,4>> impactPoints;
	NativeWorldDrawState raster;
	ScreenDrawState surface;
	std::shared_ptr<const NativeShieldTextureSnapshot> textures; // Actual shared state frozen at DrawArrays, not private assumed bindings.
	bool mapDepthBound=false, modelsDepthBound=false;
};
struct NativeShieldFrame {
	std::uint64_t sourceDrawFrame=0;
	std::vector<NativeShieldDraw> draws;
};
void ValidateNativeShieldGeometry(const NativeShieldGeometry&);
void ValidateNativeShieldDraw(const NativeShieldDraw&);
} // namespace rendering
