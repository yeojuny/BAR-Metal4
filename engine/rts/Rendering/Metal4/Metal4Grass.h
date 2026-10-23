/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeGrassData.h"
namespace rendering::metal4 {
class GrassGPU;
class PreparedGrass {
public:
	~PreparedGrass();size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedGrass();struct Impl;std::unique_ptr<Impl> impl;friend class GrassGPU;
};
class GrassGPU {
public:
	GrassGPU();~GrassGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,bool validation,double& milliseconds,unsigned samples=1);
	void ClearResources();
	std::shared_ptr<PreparedGrass> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,uint64_t sourceDrawFrame,
		const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		const CUSSourceImageProvider&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,unsigned samples=1);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
