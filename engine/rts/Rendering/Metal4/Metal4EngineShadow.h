/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4EngineModelDeferred.h"
#include "Metal4CUSSource.h"
#include "Metal4GenericShadow.h"
#include "Rendering/Backend/NativeEngineShadowData.h"
namespace rendering::metal4 {
class PreparedEngineShadow {
public:
 ~PreparedEngineShadow();
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
 void Finish();void DidComplete();
private:
 PreparedEngineShadow();struct Impl;std::unique_ptr<Impl> impl;friend class EngineShadowGPU;
};
class EngineShadowGPU {
public:
 EngineShadowGPU();~EngineShadowGPU();
 std::shared_ptr<const void> CompileOriginalProgram(RuntimeLease&,bool modern,const CUSSourceTarget&,bool,double&);
 std::shared_ptr<const void> CompileOriginalProgram(RuntimeLease&,NativeEngineShadowProfile,const CUSSourceTarget&,bool,double&);
 std::shared_ptr<PreparedEngineShadow> Prepare(RuntimeLease&,std::shared_ptr<const NativeEngineShadowFrame>,const CUSSourceTarget&,
  const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,bool,double&);
 std::shared_ptr<PreparedEngineShadow> PrepareTerrain(RuntimeLease&,
  std::shared_ptr<const NativeTerrainShadowFrame>,const CUSSourceTarget&,const CUSSourceImageProvider&);
 // Consumes the common recorder's actual ShadowOpaque commands and ordinals.
 // Expected context comes from the retained shadow depth object's context.
 std::shared_ptr<PreparedGenericShadow> PrepareGeneric(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,
  uint64_t sourceDrawFrame,uint64_t contextGeneration,const CUSSourceTarget&,bool,double&);
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
}
