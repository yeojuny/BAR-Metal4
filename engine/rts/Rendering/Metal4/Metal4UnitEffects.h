/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeUnitEffectsData.h"
namespace rendering::metal4 {
class UnitEffectsGPU;
class PreparedUnitEffect {
public:
 ~PreparedUnitEffect();void Encode(id<MTL4RenderCommandEncoder>)const;
private:
 PreparedUnitEffect();struct Impl;std::unique_ptr<Impl> impl;friend class UnitEffectsGPU;
};
class UnitEffectsGPU {
public:
 UnitEffectsGPU();~UnitEffectsGPU();void Initialize(RuntimeLease&,MTLPixelFormat,bool,double&,unsigned samples=1);void ClearResources();
 std::shared_ptr<PreparedUnitEffect> Prepare(RuntimeLease&,const NativeWorldDrawCommand&,ScreenGPU&,const PreparedScreenFrame*,const WorldCamera&,unsigned,unsigned,unsigned,unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,double&,unsigned samples=1);
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
}
