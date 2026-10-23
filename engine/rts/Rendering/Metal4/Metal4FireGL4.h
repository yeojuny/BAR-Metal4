/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeFireGL4Data.h"
namespace rendering::metal4 {
class FireGL4GPU;
class PreparedFireGL4 {
public:
	~PreparedFireGL4();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	std::uint32_t InstanceCountAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
	PreparedFireGL4();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class FireGL4GPU;
};
// Original later-world source sequence, actual shared scene attachments and
// immutable named-image owner. This family creates no pass/queue/presentation.
class FireGL4GPU {
public:
	FireGL4GPU();
	~FireGL4GPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedFireGL4> Prepare(RuntimeLease&,std::shared_ptr<const NativeFireGL4Frame>,
		const WorldCamera&,unsigned renderWidth,unsigned renderHeight,unsigned logicalWidth,unsigned logicalHeight,
		ScreenGPU&,const PreparedScreenFrame* earlierProducers,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds);
	void ClearResources();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
