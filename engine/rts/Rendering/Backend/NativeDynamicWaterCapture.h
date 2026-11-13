/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDynamicWaterData.h"
#include "System/float3.h"
namespace rendering {
class NativeDynamicWaterCapture {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 NativeDynamicWaterCapture();~NativeDynamicWaterCapture();
 void Initialize(const DynamicWaterSourceProfile&);
 void Reset();bool Initialized()const;
 void Update();
 void AddExplosion(const float3&,float strength,float size);
 std::shared_ptr<const NativeDynamicWaterFrame> CaptureDerived(uint64_t sourceDrawFrame);
 std::shared_ptr<const NativeDynamicWaterFrame> FinalizeSurface(std::shared_ptr<const NativeDynamicWaterFrame>,const WorldCamera&,
  const std::array<float,9>& reflectionBasis,const std::array<float,9>& refractionBasis,bool wireframe);
 void ValidateSubmittedCapacity(const std::shared_ptr<const NativeDynamicWaterFrame>&) const;
 void TransferPreparedFrame(uint64_t sourceDrawFrame) noexcept;
 void ValidateRestoreHeldFrame(uint64_t sourceDrawFrame) const;
 void RestoreHeldFrame(uint64_t sourceDrawFrame) noexcept;
 void QuarantineHeldFrame(uint64_t sourceDrawFrame) noexcept;
 void ValidateHeldFrame(uint64_t sourceDrawFrame) const;
 void ValidateCompletion(uint64_t materialGeneration,uint64_t throughUpdate,uint64_t sourceDrawFrame) const;
 void DidComplete(uint64_t materialGeneration,uint64_t throughUpdate,uint64_t sourceDrawFrame);
};
}
