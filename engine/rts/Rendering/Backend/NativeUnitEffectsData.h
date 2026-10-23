/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
namespace rendering {
struct NativeEffectModel {int unitID=-1;std::shared_ptr<const WorldModel> model;};
struct NativeUnitEffectInput {
 bool orb=false,paralyze=false,empRework=false;
 std::vector<NativeEffectModel> submissions;
 uint64_t sourceDrawFrame=0;
 std::shared_ptr<const void> compiled;
 NativeBufferSnapshotRef vertices,indices,instances;
 std::shared_ptr<const NativeCUSUniformPublication> original;
 std::shared_ptr<const NativeCUSPublishedBuffer> transforms,models;
 std::array<std::shared_ptr<const ScreenTexture>,2> images;
 std::array<ScreenSampler,2> samplers;
 int reflectionPass=0;
 uint32_t count=0,indexOffset=0,instanceCount=0,baseInstance=0;
 int32_t baseVertex=0;
};
struct NativeAirjetPublished {
 std::array<float,8> root0,root1,piece0,piece1;
 std::array<uint8_t,128> model;
};
static_assert(sizeof(NativeAirjetPublished)==256);
struct NativeOrbPublished {std::array<uint8_t,128> model;std::array<float,4> team;};
static_assert(sizeof(NativeOrbPublished)==144);
std::vector<NativeOrbPublished> ReadNativeOrbPublished(const NativeUnitEffectInput&);
struct NativeParalyzePublished {std::vector<std::array<float,8>> transforms;std::array<uint8_t,128> model;};
NativeParalyzePublished ReadNativeParalyzePublished(const NativeUnitEffectInput&,size_t row);
void ValidateNativeUnitEffect(const NativeUnitEffectInput&);
std::vector<NativeAirjetPublished> ReadNativeAirjetPublished(const NativeUnitEffectInput&);
std::shared_ptr<const void> NativeUnitEffectsConsumer();
void SetNativeUnitEffectsConsumer(std::shared_ptr<const void>);
}
