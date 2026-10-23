/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeObjectSpotlightData.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
namespace rendering::metal4 {
class ObjectSpotlightGPU;
class PreparedObjectSpotlight {
public:
	~PreparedObjectSpotlight();
	size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedObjectSpotlight();struct Impl;std::unique_ptr<Impl> impl;friend class ObjectSpotlightGPU;
};
class ObjectSpotlightGPU {
public:
	ObjectSpotlightGPU();~ObjectSpotlightGPU();
	// Availability is published only after the actual paired pipeline compiles.
	void Initialize(RuntimeLease&,MTLPixelFormat,bool validation,double& milliseconds,unsigned samples=1);
	void ClearResources();
	std::shared_ptr<PreparedObjectSpotlight> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,
		uint64_t drawFrame,const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,unsigned samples=1);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
