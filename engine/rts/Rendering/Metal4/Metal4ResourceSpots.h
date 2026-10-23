/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4NativeEffectState.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeResourceSpotsData.h"
namespace rendering::metal4 {
using ResourceSpotsImageProvider=std::function<CUSSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,NativeWorldDrawStage,uint64_t)>;
class ResourceSpotsGPU;
class PreparedResourceSpots {
public:
	~PreparedResourceSpots();size_t DrawCount()const;uint64_t SequenceAt(size_t)const;NativeWorldDrawStage StageAt(size_t)const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedResourceSpots();struct Impl;std::unique_ptr<Impl> impl;friend class ResourceSpotsGPU;
};
class ResourceSpotsGPU {
public:
	ResourceSpotsGPU();~ResourceSpotsGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,bool validation,double& milliseconds,unsigned samples=1);
	void ClearResources();
	std::shared_ptr<PreparedResourceSpots> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,uint64_t sourceDrawFrame,
		const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		const ResourceSpotsImageProvider&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,unsigned samples=1);
private:
	struct Impl;std::shared_ptr<Impl> impl;
};
}
