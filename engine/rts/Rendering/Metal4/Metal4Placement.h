/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativePlacementData.h"
namespace rendering::metal4 {
class PlacementGPU;
class PreparedPlacementGrid {
public:
	~PreparedPlacementGrid();
	size_t DrawCount()const;
	uint64_t SequenceAt(size_t)const;
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>)const;
private:
	PreparedPlacementGrid();struct Impl;std::unique_ptr<Impl> impl;friend class PlacementGPU;
};
class PlacementGPU {
public:
	PlacementGPU();~PlacementGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat,bool shaderValidation,double& milliseconds,unsigned samples=1);
	std::unique_ptr<PreparedPlacementGrid> Prepare(RuntimeLease&,const NativePlacementFrame&,const WorldCamera&,
		uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,
		MTLPixelFormat colorFormat,MTLPixelFormat stencilFormat,bool shaderValidation,double& milliseconds,unsigned samples=1);
private:struct Impl;std::unique_ptr<Impl> impl;
};
}
