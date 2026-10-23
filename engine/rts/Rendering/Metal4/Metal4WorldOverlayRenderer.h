/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4ScreenNative.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <memory>
namespace rendering::metal4 {
class WorldOverlayGPU;
class PreparedWorldOverlayFrame {
public:
	PreparedWorldOverlayFrame();~PreparedWorldOverlayFrame();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	NativeWorldDrawStage StageAt(std::size_t index) const;
	// Caller merges this ordinal with original Nano/effect draws in the same
	// stage and actual shared depth/stencil render pass. No separate present.
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const;
 void EncodeDependencies(id<MTL4CommandBuffer>)const;
 void DidComplete()const;
private:
	struct Impl;std::unique_ptr<Impl> impl;friend class WorldOverlayGPU;
};
class WorldOverlayGPU {
public:
	WorldOverlayGPU();~WorldOverlayGPU();
	std::unique_ptr<PreparedWorldOverlayFrame> Prepare(RuntimeLease& lease,const NativeWorldDrawFrame& frame,
		ScreenGPU& images,const PreparedScreenFrame* targetProducers,const WorldCamera& camera,
		std::uint32_t width,std::uint32_t height,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,bool mapMRTBroadcast=false,unsigned samples=1,bool mapCombinedColor=false,const CUSSourceImageProvider& glassImages={});
	void InitializeReclaimLines(RuntimeLease&,MTLPixelFormat,bool validation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
