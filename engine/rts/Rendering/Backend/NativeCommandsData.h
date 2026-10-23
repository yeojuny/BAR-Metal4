/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <string_view>

namespace rendering {
struct NativeCommandsProgramSnapshot {
	std::array<float,16> view{}, projection{};
	float lineTextureLength=0, lineWidth=0;
	std::int32_t textureUnit=0, useTexture=0;
};
class NativeCommandsProgramState {
public:
	NativeCommandsProgramState(std::string_view vertex,std::string_view fragment);
	int Location(std::string_view) const;
	bool Floats(int,std::span<const float>);
	bool Integers(int,std::span<const std::int32_t>);
	bool Matrix(int,std::span<const float>);
	NativeCommandsProgramSnapshot Capture()const{return state;}
private:
	NativeCommandsProgramSnapshot state;
};
struct alignas(16) NativeCommandsUniformBlock {
	std::array<float,16> view{},projection{};
	std::array<float,4> values{}; // texture length, line width, alpha test reference
	std::array<std::uint32_t,4> options{}; // use texture, flip V, -1..1 clip depth, alpha test
};
static_assert(sizeof(NativeCommandsUniformBlock)==160);
struct NativeCommandsDraw {
	std::uint64_t sequence=0,sourceDrawFrame=0;
	NativeCommandsProgramSnapshot program;
	NativeBufferSnapshotRef vertices,instances;
	std::uint32_t firstVertex=0,vertexCount=0,firstInstance=0,instanceCount=0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	NativeCUSTextureBinding image;
	ScreenDrawState surface;
	NativeWorldDrawState raster;
};
struct NativeCommandsFrame {
	std::uint64_t sourceDrawFrame=0;
	std::vector<NativeCommandsDraw> draws;
};
std::uint64_t NativeCommandsSourceHash(std::string_view) noexcept;
void ValidateNativeCommandsDraw(const NativeCommandsDraw&);
NativeCommandsUniformBlock ReadNativeCommandsUniforms(const NativeCommandsDraw&);
void SetNativeCommandsConsumerAvailable(bool) noexcept;
bool IsNativeCommandsConsumerAvailable() noexcept;
}
