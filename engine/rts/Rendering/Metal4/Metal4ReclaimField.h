/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeReclaimFieldData.h"
namespace rendering::metal4 {
class ReclaimFieldGPU;
class PreparedReclaimField {
public:
	~PreparedReclaimField();
	size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedReclaimField();struct Impl;std::unique_ptr<Impl> impl;friend class ReclaimFieldGPU;
};
class ReclaimFieldGPU {
public:
	ReclaimFieldGPU();~ReclaimFieldGPU();
	// Caller first compiles the actual common solid line pipeline as well.
	void Initialize(RuntimeLease&,MTLPixelFormat,bool validation,double& milliseconds,unsigned samples=1);
	void ClearResources();
	std::shared_ptr<PreparedReclaimField> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,
		uint64_t drawFrame,const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,unsigned samples=1);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
