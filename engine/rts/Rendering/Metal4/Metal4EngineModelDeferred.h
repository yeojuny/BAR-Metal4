/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeEngineModelData.h"
#include <cstddef>
namespace rendering::metal4 {
struct CUSSourceTarget;
// Returned from the existing WorldRenderer model cache, with the exact
// WorldModelVertex76-byte vertex ABI and original piece-local triangle indices.
// The leaf never copies/uploads a second model mesh.
struct EngineDeferredPieceBuffers {
	__strong id<MTLBuffer> vertices=nil,indices=nil;
	uint32_t indexCount=0;
};
using EngineDeferredGeometryProvider=std::function<std::vector<EngineDeferredPieceBuffers>(RuntimeLease&,std::shared_ptr<const WorldModel>)>;
class PreparedEngineModelDeferred {
public:
	~PreparedEngineModelDeferred();
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeBeforeLua(id<MTL4RenderCommandEncoder>);
	void EncodeAfterLua(id<MTL4RenderCommandEncoder>);
	// Original per-object Lua output may interleave the default draws. These
	// indexed methods preserve source sequence without a second geometry owner.
	size_t DrawCount()const;
	uint64_t SequenceAt(size_t)const;
	void EncodeOrderedDraw(size_t,id<MTL4RenderCommandEncoder>);
	void FinishOrderedDraws();
	void DidComplete();
private:
	struct Impl;std::unique_ptr<Impl> impl;
	PreparedEngineModelDeferred();friend class EngineModelDeferredGPU;
};
class EngineModelDeferredGPU {
public:
	EngineModelDeferredGPU();~EngineModelDeferredGPU();
 // Returns an owner only after a real original-profile PSO was compiled in
 // this same GPU/cache. Capture retains it and Prepare verifies its origin.
 std::shared_ptr<const void> CompileOriginalProgram(RuntimeLease&,bool modern,bool shadows,
  const CUSSourceTarget&,bool validation,double& compileMilliseconds);
	std::shared_ptr<PreparedEngineModelDeferred> Prepare(RuntimeLease&,std::shared_ptr<const NativeEngineModelPhase>,
		const CUSSourceTarget& actualTarget,const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,bool validation,double& compileMilliseconds);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
