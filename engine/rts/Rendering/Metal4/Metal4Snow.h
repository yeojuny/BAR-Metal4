/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeSnowData.h"
namespace rendering::metal4 {
class SnowGPU;
class PreparedSnow {
public:
	~PreparedSnow();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
	PreparedSnow(); struct Impl; std::unique_ptr<Impl> impl;
	friend class SnowGPU;
};
class SnowGPU {
public:
	SnowGPU(); ~SnowGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedSnow> Prepare(RuntimeLease&,std::shared_ptr<const NativeSnowFrame>,const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,ScreenGPU&,const PreparedScreenFrame*,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
	void ClearResources();
private:
	struct Impl; std::unique_ptr<Impl> impl;
};
}
