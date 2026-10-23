/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeFireSmokeData.h"
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering::metal4 {
class FireSmokeGPU;
class PreparedFireSmoke {
public:
	~PreparedFireSmoke();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const;
private:
	PreparedFireSmoke();
	struct Impl; std::unique_ptr<Impl> impl;
	friend class FireSmokeGPU;
};
// Joins the actual original later-world sequence: no own pass, queue, wait or
// present. The caller collects shared named-image mips AFTER this Prepare.
class FireSmokeGPU {
public:
	FireSmokeGPU(); ~FireSmokeGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat colorFormat,bool validation,double& shaderMilliseconds,unsigned samples=1);
	std::unique_ptr<PreparedFireSmoke> Prepare(RuntimeLease&,const NativeFireSmokeFrame&,
		const WorldCamera&,std::uint32_t renderWidth,std::uint32_t renderHeight,
		std::uint32_t logicalWindowWidth,std::uint32_t logicalWindowHeight,ScreenGPU& sharedImages,const PreparedScreenFrame* earlierProducers,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,bool validation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Previous serialized GPU work must have retired.
private:
	struct Impl; std::unique_ptr<Impl> impl;
};
}
