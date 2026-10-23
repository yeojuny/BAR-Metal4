/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineShadowData.h"
#include "NativeShadowTextureSource.h"
#include "NativeOriginalShadowParticlesData.h"
#include "NativeWaterLuaFrame.h"
#include "NativeCUSSourceData.h"
#include "NativeShadowClearContract.h"
namespace rendering {
// Source ordinals are the original common draw sequence, never a diagnostic.
// The common generic/clear recorder remains the sole owner of its commands.
struct NativeOriginalShadowFrame {
 uint64_t sourceDrawFrame=0,beginSequence=0,colorClearSequence=0,endSequence=0;
 unsigned originalSelectedBits=0;
 WorldCamera camera;
 std::shared_ptr<const NativeShadowTextureFrame> textures;
 std::shared_ptr<const NativeShadowClearProof> clearProof;
 std::shared_ptr<const NativeCUSSourceFrame> cus;
 std::array<std::shared_ptr<const NativeEngineShadowFrame>,2> models;
 std::shared_ptr<const NativeEngineShadowFrame> projectiles;
 std::shared_ptr<const NativeTerrainShadowFrame> terrain;
 std::shared_ptr<const NativeOriginalShadowParticlesFrame> transparent;
 std::shared_ptr<const NativeWaterLuaFrame> transparentLua;
};
}
