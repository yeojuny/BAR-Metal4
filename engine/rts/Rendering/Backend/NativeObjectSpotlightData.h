/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSUniformData.h"
#include <array>
#include <string_view>
namespace rendering {
// One actual original DrawArrays. No generated replacement geometry or lights.
struct NativeObjectSpotlightInput {
	bool commanderWreck=false;
 uint64_t sourceDrawFrame=0,vertexHash=0,fragmentHash=0;
	NativeBufferSnapshotRef vertices,instances;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::shared_ptr<const NativeCUSPublishedBuffer> models;
	std::shared_ptr<const void> compiledFamily;
	uint32_t first=0,count=0,firstInstance=0,instanceCount=0;
};
uint64_t NativeObjectSpotlightSourceHash(std::string_view);
void ValidateNativeObjectSpotlightInput(const NativeObjectSpotlightInput&);
// Gather model drawPos bytes only; the original override remains in the VS.
// A missing model row is allowed only when the original nonzero-XYZ override
// makes that slot unused. Zero-initialized unused backing is never a model
// position; every model slot the VS may consume must be an observed real row.
std::vector<std::array<float,4>> ReadNativeObjectSpotlightModelPositions(const NativeObjectSpotlightInput&);
void SetNativeObjectSpotlightConsumer(std::shared_ptr<const void>);
std::shared_ptr<const void> NativeObjectSpotlightConsumer();
bool IsNativeObjectSpotlightConsumerAvailable();
namespace objectspotlight {
inline constexpr size_t VertexStride=12,InstanceStride=60,ModelStride=128;
inline constexpr size_t ViewProjectionByte=5*64,ViewInverseByte=7*64,DrawSecondsByte=20;
inline constexpr uint64_t VertexHash=0x099924b2dcccce74ULL,FragmentHash=0x599f9fb5ebeeceaeULL;
}
}
