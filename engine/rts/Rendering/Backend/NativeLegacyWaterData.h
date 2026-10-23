/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "WorldFrameData.h"
#include "NativeLegacyWaterBumpData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeLegacyRefractionPublication.h"
namespace rendering {
// Observed original capability decision, not inferred from headless GLAD.
enum class LegacyRefractionTarget {Unobserved,Rectangle,PowerOfTwo};
struct NativeLegacyRefraction {
 LegacyRefractionTarget target=LegacyRefractionTarget::Unobserved;
 std::array<uint32_t,2> allocation{};
 std::array<int32_t,4> copyViewport{};
 std::array<float,2> inverseMapExtent{};
 NativeCUSTextureBinding shading;
 std::shared_ptr<const NativeCUSTextureObject> copiedColor;
};
struct NativeLegacyWaterVertex {
 std::array<float,3> position;
 std::array<float,2> reflectionUV;
 std::array<uint8_t,4> color;
};
struct NativeLegacyWaterFrame {
 uint64_t sourceDrawFrame=0;
 int simulationFrame=0;
 WorldCamera captureCamera;
 WorldMatrix viewProjection{};
 std::array<float,2> forward{}; // normalized original forward.z, forward.x
 std::array<float,4> fogColor{},fogParameters{};
 bool fogEnabled=false,wireframe=false;
 LegacyWaterBumpCull cull=LegacyWaterBumpCull::None;
 std::vector<NativeLegacyWaterVertex> vertices;
 std::vector<uint32_t> indices;
 std::shared_ptr<const NativeLegacyWaterBumpFrame> bump;
 std::shared_ptr<const NativeLegacyRefraction> refraction;
};
class NativeLegacyWaterCapture {
 std::shared_ptr<const NativeLegacyWaterBumpMaterial> material;
 std::array<float,3> surfaceColor{};
 NativeLegacyRefractionPublication refractionPublication;
 LegacyRefractionTarget refractionTarget=LegacyRefractionTarget::Unobserved;
 std::array<uint32_t,2> refractionAllocation{};
public:
 void Initialize(LegacyRefractionTarget target=LegacyRefractionTarget::Unobserved);
 void AbortUncompletedRefraction(){refractionPublication.AbortUncompleted();}
 void RefractionDrawTail(){refractionPublication.DrawTail();}
 void Reset(){refractionPublication.Destroy();material.reset();refractionTarget=LegacyRefractionTarget::Unobserved;refractionAllocation={};}
 bool Initialized()const{return bool(material);}
 std::shared_ptr<const NativeLegacyWaterFrame> Capture(const WorldFrame&,bool wireframe)const;
 std::shared_ptr<const NativeLegacyWaterFrame> FinalizeSurface(std::shared_ptr<const NativeLegacyWaterFrame>,const WorldCamera& actual,bool wireframe);
};
}
