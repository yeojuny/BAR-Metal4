/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4EngineModelDeferred.h"
namespace rendering::metal4 {
class PreparedEngineModelForward {
public:
 ~PreparedEngineModelForward();
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void EncodeOrderedDraw(size_t,id<MTL4RenderCommandEncoder>);
 void FinishOrderedDraws();void DidComplete();
private:
 struct Impl;std::unique_ptr<Impl> impl;PreparedEngineModelForward();friend class EngineModelForwardGPU;
};
class EngineModelForwardGPU {
public:
 EngineModelForwardGPU();~EngineModelForwardGPU();
 // Returns an owner only after a real original-profile PSO was compiled in
 // this same GPU/cache. Capture retains it and Prepare verifies its origin.
 std::shared_ptr<const void> CompileOriginalProgram(RuntimeLease&,bool modern,bool shadows,
  const CUSSourceTarget&,bool validation,double& compileMilliseconds);
 std::shared_ptr<PreparedEngineModelForward> Prepare(RuntimeLease&,std::shared_ptr<const NativeEngineModelPhase>,
  const CUSSourceTarget&,const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,bool validation,double& compileMilliseconds);
private:struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
