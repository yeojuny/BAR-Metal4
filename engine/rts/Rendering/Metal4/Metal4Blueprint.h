/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeBlueprintData.h"
namespace rendering::metal4 {
class BlueprintGPU;
class PreparedBlueprintFrame {
public:
	~PreparedBlueprintFrame();
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	NativeWorldDrawStage StageAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
	void DidComplete() const;
private:
	PreparedBlueprintFrame();
	struct Impl;std::unique_ptr<Impl> impl;friend class BlueprintGPU;
};
class BlueprintGPU {
public:
	BlueprintGPU();~BlueprintGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat,bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	std::unique_ptr<PreparedBlueprintFrame> Prepare(RuntimeLease&,const NativeBlueprintFrame&,const WorldCamera&,
		std::uint32_t width,std::uint32_t height,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
