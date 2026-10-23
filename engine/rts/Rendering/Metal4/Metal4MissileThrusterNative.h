/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeMissileThrusterData.h"
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering::metal4 {
class MissileThrusterGPU;
class PreparedMissileThruster {
public:
	~PreparedMissileThruster();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const;
private:
	PreparedMissileThruster();
	struct Impl; std::unique_ptr<Impl> impl;
	friend class MissileThrusterGPU;
};
// Joins the actual original later-world sequence: no own pass, queue, wait or
// present. The caller collects shared named-image mips AFTER this Prepare.
class MissileThrusterGPU {
public:
	MissileThrusterGPU(); ~MissileThrusterGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat colorFormat,bool validation,double& shaderMilliseconds,unsigned samples=1);
	std::unique_ptr<PreparedMissileThruster> Prepare(RuntimeLease&,const NativeMissileThrusterFrame&,
		const WorldCamera&,std::uint32_t renderWidth,std::uint32_t renderHeight,
		std::uint32_t logicalWindowWidth,std::uint32_t logicalWindowHeight,ScreenGPU& sharedImages,const PreparedScreenFrame* earlierProducers,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,bool validation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Previous serialized GPU work must have retired.
private:
	struct Impl; std::unique_ptr<Impl> impl;
};
}
