/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeCUSUniformData.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace rendering {
enum class NativeGameDecalProgram : std::uint8_t { Geometry, LargeMesh };
// The unchanged exact30770 InstanceVBOTable owns these five original vec4s.
// Small GS input is a vertex stream; the two large meshes use an instance stream.
struct NativeGameDecalInstance {
	std::array<float,4> lengthWidthRotation,uvOffsets,alphaDecayHeatDecay,worldPosition,parameters;
};
static_assert(sizeof(NativeGameDecalInstance)==80);
struct NativeGameDecalWater {
	// Original LuaShader formats each component with %f at program creation.
	std::array<float,3> absorb{},minimum{},base{};
	bool operator==(const NativeGameDecalWater&) const = default;
};
struct NativeGameDecalUniforms {
	std::uint64_t sourceDrawFrame=0;
	// Retain the independently published original UBOs through this draw.
	// Consumers never reconstruct camera, time, lighting or clip metadata.
	std::shared_ptr<const NativeCUSUniformPublication> original;
};
struct NativeGameDecalUniformValues {
	std::array<float,16> cameraViewProjection{},cameraViewInverse{},shadowView{};
	std::array<float,4> timeInfo{},mapSize{},sunDirection{},sunAmbient{},sunDiffuse{};
	ClipDepthConvention clipDepth=ClipDepthConvention::Unsupported;
};
struct NativeGameDecalImage {
	std::shared_ptr<const ScreenTexture> image;
	ScreenSampler sampler;
};
struct NativeGameDecalDraw {
	std::uint64_t sequence=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::PreDecals;
	NativeGameDecalProgram program=NativeGameDecalProgram::Geometry;
	NativeBufferSnapshotRef vertices,indices,instances;
	std::uint32_t count=0,first=0,instanceCount=1,firstInstance=0;
	std::array<NativeGameDecalImage,2> atlases;
	NativeGameDecalWater water;
	float fadeDistance=3000; // Original declaration is inactive in both VS bodies.
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeGameDecalFrame {
	NativeGameDecalUniforms uniforms;
	std::vector<NativeGameDecalDraw> draws;
};
std::uint64_t NativeGameDecalSourceHash(std::string_view source) noexcept;
NativeGameDecalWater ParseNativeGameDecalWater(std::string_view exactEngineDefinitions);
void ValidateNativeGameDecalUniforms(const NativeGameDecalUniforms&);
NativeGameDecalUniformValues ReadNativeGameDecalUniforms(const NativeGameDecalUniforms&);
void ValidateNativeGameDecalDraw(const NativeGameDecalDraw&);

class NativeGameDecalRecorder {
public:
	void Begin(const NativeGameDecalUniforms&);
	void Record(NativeGameDecalDraw);
	std::shared_ptr<const NativeGameDecalFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeGameDecalFrame> frame;
};
// Paired source-specific consumer readiness; never a global GL4 capability.
void SetNativeGameDecalConsumerAvailable(bool) noexcept;
bool IsNativeGameDecalConsumerAvailable() noexcept;
} // namespace rendering
