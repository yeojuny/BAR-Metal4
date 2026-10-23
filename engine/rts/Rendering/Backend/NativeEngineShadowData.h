/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelData.h"
namespace rendering {
enum class NativeEngineShadowProfile : uint8_t { LegacyModel, ModernModel, Projectile, Terrain };
// Actual legacy vertex/index submission. Flying-piece indices are frozen from
// the original shatter upload, never reconstructed from the current model pose.
struct NativeShadowPieceGeometry {
 std::vector<WorldModelVertex> vertices;
 std::vector<uint32_t> indices;
};
// ShadowGen owns these values. Normal model lighting/forward program values
// must never be relabeled as the original depth program.
struct NativeEngineShadowProgram {
 NativeEngineShadowProfile profile=NativeEngineShadowProfile::LegacyModel;
 bool modern=false;
 std::shared_ptr<const void> compiledOriginal;
 std::array<float,4> alphaControl{.5f,1,0,0};
 std::array<std::array<float,4>,2> modelClipPlanes{{{0,0,0,1},{0,0,0,1}}};
};
struct NativeEngineShadowDraw {
 uint64_t sequence=0;
 std::shared_ptr<const NativeEngineModelObject> object;
 std::shared_ptr<const NativeShadowPieceGeometry> pieceGeometry;
 NativeEngineShadowProfile profile=NativeEngineShadowProfile::LegacyModel;
 uint32_t firstIndex=0,indexCount=0;
 NativeEngineModelPosePublication pose;
 std::shared_ptr<const void> compiledOriginal;
 bool modern=false;
 std::array<float,4> alphaControl{};
 WorldMatrix modelView{},projection{};
 std::array<bool,2> clipEnabled{};
 std::array<std::array<float,4>,2> clipPlanes{};
 NativeWorldDrawState world;
 ScreenDrawState surface;
 NativeEngineModelRaster raster;
 NativeCUSTextureBinding alphaTexture;
};
struct NativeEngineShadowFrame {
 uint64_t sourceDrawFrame=0;
 bool feature=false;
 bool projectile=false;
 WorldCamera camera;
 std::vector<NativeEngineShadowDraw> draws;
};
// Exact CPU submissions from the original selected Basic/ROAM shadow mesh.
// Positions retain local x/z and the original y=0 / border y=-1 marker.
struct NativeTerrainShadowDraw {
 uint64_t sequence=0;
 std::shared_ptr<const void> compiledOriginal;
 std::vector<std::array<float,3>> vertices;
 std::vector<uint32_t> indices;
 bool indexed=true;
 WorldMatrix modelView{},projection{};
 std::array<float,4> mapSize{};
 std::array<int32_t,2> texSquare{};
 float borderMinHeight=0;
 std::array<bool,3> clipEnabled{};
 std::array<std::array<float,4>,3> clipPlanes{};
 NativeWorldDrawState world;
 ScreenDrawState surface;
 NativeCUSTextureBinding heightTexture;
};
struct NativeTerrainShadowFrame {
 uint64_t contextGeneration=0,sourceDrawFrame=0;
 bool drawInvoked=false,drawEntered=false;
 WorldCamera camera;
 std::vector<NativeTerrainShadowDraw> draws;
};
} // namespace rendering
