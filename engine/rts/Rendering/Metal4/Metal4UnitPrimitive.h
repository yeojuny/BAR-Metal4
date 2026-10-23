/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeUnitPrimitiveData.h"
namespace rendering::metal4 {
class UnitPrimitiveGPU;
class PreparedUnitPrimitive {
public:
	~PreparedUnitPrimitive();
	std::size_t DrawCount()const;
	std::uint64_t SequenceAt(std::size_t)const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>)const;
private:
	PreparedUnitPrimitive();struct Impl;std::unique_ptr<Impl> impl;friend class UnitPrimitiveGPU;
};
class UnitPrimitiveGPU {
public:
	UnitPrimitiveGPU();~UnitPrimitiveGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		std::uint32_t samples,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedUnitPrimitive> Prepare(RuntimeLease&,std::shared_ptr<const NativeUnitPrimitiveFrame>,
		const WorldCamera&,std::uint32_t renderWidth,std::uint32_t renderHeight,
		std::uint32_t logicalWidth,std::uint32_t logicalHeight,ScreenGPU&,const PreparedScreenFrame*,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,std::uint32_t samples,bool validation,double& shaderMilliseconds,std::optional<NativePostprocessConsumer> consumer={});
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
