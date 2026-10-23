/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "WorldFrameData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <deque>
namespace rendering {
// Only the pinned CDynWater program family; this is NOT a generic GL command interpreter.
enum class DynamicWaterPass {BoatWake,HoverWake,Explosion,DetailNormal,Flow,HeightFoam,Normal,DerivedHeight,Surface,OuterSurface};
enum class DynamicWaterSurfaceProgram {Unobserved,VertexTexture,NoVertexTexture};
struct DynamicWaterSourceProfile {
 DynamicWaterSurfaceProgram surfaceProgram=DynamicWaterSurfaceProgram::Unobserved;
 bool fragmentClampObserved=false,fragmentClamp=false;
 bool surfaceFragmentClampObserved=false,surfaceFragmentClamp=false;
 bool inheritedRasterObserved=false; // External texture state is validated per immutable consumed binding.
 // Unwritten dwAddSplash.vp result.fogcoord is undefined. Only actual reference
 // profile evidence may provide a value; ordinary GL_FOG=false is not evidence.
 bool impulseFogObserved=false;
 bool detailFogObserved=false,shadowProgramStateObserved=false;
 bool outerQuadDiagonalObserved=false,outerQuadDiagonal301=false;
 float detailFogCoordinate=0;
 float impulseFogCoordinate=0;
};
struct DynamicWaterVertex {
 std::array<float,3> position{};
 std::array<float,2> uv{};
 std::array<float,3> normal{}; // Original TN payload, not a geometric normal for impulses.
};
struct DynamicWaterDraw {
 DynamicWaterPass pass=DynamicWaterPass::Flow;
 std::array<std::array<float,4>,19> vertexEnv{},fragmentEnv{};
 std::vector<DynamicWaterVertex> vertices;
 std::vector<uint32_t> indices; // Source quad and alternating-strip order preserved.
 uint32_t zeroTextureMask=0; // DrawUpdateSquare exposed strips: Flow0..5; HeightFoam0 only.
};
struct NativeDynamicWaterMaterial {
 uint64_t generation=0;
 // Exact source float payloads; native RGBA16F upload/write rounding is mandatory.
 std::array<float,64*64*4> rawBump{},splash{};
 std::shared_ptr<const WorldTexture> boat,foam;
 // Source bug yields actual texture0 for hover, never an invented hovershape image.
 NativeCUSTextureBinding hoverDefault;
 uint32_t refractionExtent=0;
};
struct NativeDynamicWaterUpdate {
 uint64_t materialGeneration=0,sequence=0,sourceDrawFrame=0;
 int simulationFrame=0;
 std::array<float,3> oldDomain{},domain{};
 NativeCUSTextureBinding shading,hoverDefault;
 DynamicWaterSourceProfile profile;
 std::array<float,4> fogColor{},fogParameters{};
 std::vector<DynamicWaterDraw> draws; // Boats, hover, explosions, detail, flow, height, normal.
};
struct NativeDynamicWaterFrame {
 uint64_t sourceDrawFrame=0,throughUpdate=0;
 std::shared_ptr<const NativeDynamicWaterMaterial> material;
 DynamicWaterSourceProfile profile;
 std::vector<std::shared_ptr<const NativeDynamicWaterUpdate>> updates;
 std::vector<DynamicWaterDraw> derived; // Height8..248 then first-draw detail, exactly once.
 std::vector<DynamicWaterDraw> surface; // Inner then outer(fixedUpTex).
 std::array<int32_t,2> derivedCameraCell{};
 WorldCamera captureCamera;
 WorldMatrix viewProjection{},shadowMatrix{};
 std::array<float,4> fogColor{},fogParameters{};
 NativeCUSTextureBinding shading,shadow;
 NativeWorldDrawState raster;
 ScreenBlendState blend;
 bool wireframe=false;
};
// Native owner contract: Prepare never modifies committed recurrence images.
// Encode writes a retained staging generation in update-sequence order; only
// successful common retirement commits throughUpdate. Rejected/pre-Begin work
// is retryable, post-Begin failure invalidates staging (never replays impulses).
}
