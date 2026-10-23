/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeFlamethrowerData.h"
namespace rendering::metal4 {
class FlamethrowerGPU;
class PreparedFlamethrower {
public:
	~PreparedFlamethrower();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	std::uint32_t InstanceCountAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
	PreparedFlamethrower();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class FlamethrowerGPU;
};
// Original later-world source sequence, actual shared scene attachments and
// immutable named-image owner. This family creates no pass/queue/presentation.
class FlamethrowerGPU {
public:
	FlamethrowerGPU();
	~FlamethrowerGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedFlamethrower> Prepare(RuntimeLease&,std::shared_ptr<const NativeFlamethrowerFrame>,
		const WorldCamera&,unsigned renderWidth,unsigned renderHeight,unsigned logicalWidth,unsigned logicalHeight,
		ScreenGPU&,const PreparedScreenFrame* earlierProducers,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds);
	void ClearResources();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
