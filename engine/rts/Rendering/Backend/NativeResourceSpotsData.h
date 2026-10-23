/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeResourceSpotsPrograms.h"
namespace rendering {
enum class NativeResourceSpotKind : uint8_t { Metal, Geothermal };
struct NativeResourceSpotsInput {
 uint64_t sourceDrawFrame=0,vertexHash=0,fragmentHash=0;
 NativeResourceSpotKind kind=NativeResourceSpotKind::Metal;
 NativeBufferSnapshotRef vertices,indices,instances;
 std::shared_ptr<const NativeCUSUniformPublication> original;
 std::shared_ptr<const void> compiledFamily;
 std::array<NativeCUSTextureBinding,2> images;
 float drawPass=0,waterLevel=0;
 uint32_t count=0,indexOffset=0,instanceCount=0,baseInstance=0;
 int32_t baseVertex=0;
};
uint64_t NativeResourceSpotsSourceHash(std::string_view);
void ValidateNativeResourceSpotsInput(const NativeResourceSpotsInput&);
void SetNativeResourceSpotsConsumerLifetime(std::shared_ptr<const void>);
std::shared_ptr<const void> NativeResourceSpotsConsumerLifetime();
bool IsNativeResourceSpotsConsumerAvailable();
}
