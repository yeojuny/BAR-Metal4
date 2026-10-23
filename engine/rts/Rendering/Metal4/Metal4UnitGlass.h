/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Metal4ScreenNative.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeUnitGlassData.h"
namespace rendering::metal4 {
class UnitGlassGPU;
class PreparedUnitGlass {
public:
 ~PreparedUnitGlass();void Encode(id<MTL4RenderCommandEncoder>)const;
 void EncodeDependencies(id<MTL4CommandBuffer>)const;void DidComplete()const;
private:
 PreparedUnitGlass();struct Impl;std::unique_ptr<Impl> impl;friend class UnitGlassGPU;
};
class UnitGlassGPU {
public:
 UnitGlassGPU();~UnitGlassGPU();void Initialize(RuntimeLease&,MTLPixelFormat,bool,double&,unsigned);void ClearResources();
 std::shared_ptr<PreparedUnitGlass> Prepare(RuntimeLease&,const NativeWorldDrawCommand&,const CUSSourceImageProvider&,const WorldCamera&,unsigned,unsigned,unsigned,unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,double&,unsigned);
private:struct Impl;std::unique_ptr<Impl> impl;
};
}
