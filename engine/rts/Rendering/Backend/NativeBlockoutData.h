/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBlueprintData.h"
#include "NativeCUSUniformData.h"
#include "NativePostprocessData.h"
#include <atomic>
namespace rendering {
struct NativeBlockoutTarget {
	uint32_t width=0,height=0;
	// Only the paired GPU allocator creates this actual R8+D32 storage owner.
	std::shared_ptr<const void> storage;
};
struct NativeBlockoutModel {
	int definition=-1;
	std::shared_ptr<const WorldModel> model;
	std::vector<NativeBlueprintTransform> transforms;
	uint32_t sourceTransformBase=0;
	std::array<float,4> positionYaw{},parameters{};
};
struct NativeBlockoutDraw {
	uint64_t sequence=0;
	std::vector<NativeBlockoutModel> models;
	std::shared_ptr<const NativeS3OTexturePair> textures;
	std::shared_ptr<const ScreenTexture> normal;
	ScreenSampler normalSampler;
	bool hasNormal=false;
	std::shared_ptr<const NativeCUSUniformPublication> uniforms;
	std::array<float,16> viewProjection{};
	ClipDepthConvention clipDepth=ClipDepthConvention::Unsupported;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeBlockoutDepthPass {
	uint64_t sequence=0;
	std::shared_ptr<const NativeBlockoutTarget> target;
	std::shared_ptr<const NativePostprocessVersion> output;
	std::vector<NativeBlockoutDraw> draws;
	float clearDepth=1;
	bool clearObserved=false;
};
struct NativeBlockoutFrame {
	uint64_t sourceDrawFrame=0;
	std::vector<NativeBlockoutDraw> draws;
	std::vector<NativeBlockoutDepthPass> depthPasses;
};
void ValidateNativeBlockoutDraw(const NativeBlockoutDraw&);
void ValidateNativeBlockoutDepthPass(const NativeBlockoutDepthPass&);
void SetNativeBlockoutConsumerAvailable(bool) noexcept;
bool IsNativeBlockoutConsumerAvailable() noexcept;
}
