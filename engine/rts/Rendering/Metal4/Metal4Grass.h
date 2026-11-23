/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWorldDrawPlan167.h"
#include "Metal4NativeEffectState.h"
#include "Metal4SourceImage.h"
#include "Metal4GrassBoundaryPolicyS01.h"
#include "Rendering/Backend/NativeGrassData.h"
namespace rendering::metal4 {
// Optional S01 proof: the exact retained producer is complete or encoded
// before the scheduler checkpoint. It never replaces requireReadable or
// establishes GPU completion on its own.
struct GrassSourceImageS01 {
 CUSSourceImage image;
 std::function<bool()> beforeCheckpoint;
};
using GrassSourceImageProviderS01=std::function<GrassSourceImageS01(RuntimeLease&,const NativeCUSTextureBinding&,unsigned)>;
class GrassGPU;
class PreparedGrass {
public:
	~PreparedGrass();size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void DidComplete();
	struct ZeroScaleStats {
		std::size_t draws=0, originalInstances=0, compactInstances=0, removedInstances=0;
		std::size_t originalIndexInstances=0, compactIndexInstances=0, distinctSnapshots=0;
	};
	struct ZeroScaleDrawStats {
		std::uint64_t sequence;
		std::size_t originalBaseInstance, originalInstanceCount, compactBaseInstance, compactInstanceCount, indexCount;
		bool visible;
	};
	ZeroScaleStats GetZeroScaleStats() const;
	ZeroScaleDrawStats GetZeroScaleDrawStats(std::size_t index) const;
	void EncodeSameInputDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder,bool compact) const;
 void EnableBoundaryDiagnosticsS01() noexcept;
 GrassReadDiagnosticS01 BoundaryDiagnosticS01() const noexcept;
 size_t VisibleDrawCountS01() const noexcept;
 bool CaptureBoundaryCheckpointS01(uint64_t submission,const std::array<id<MTLTexture>,3>& attachments) noexcept;
 bool CanSplitBoundaryS01(uint64_t submission,const std::array<id<MTLTexture>,3>& attachments) const noexcept;
private:
	PreparedGrass();struct Impl;std::unique_ptr<Impl> impl;friend class GrassGPU;
};
class GrassGPU {
public:
	GrassGPU();~GrassGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,bool validation,double& milliseconds,unsigned samples=1);
	void ClearResources();
	std::shared_ptr<PreparedGrass> Prepare(RuntimeLease&,const NativeWorldDrawSelection167&,uint64_t sourceDrawFrame,
		const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		const GrassSourceImageProviderS01&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& milliseconds,unsigned samples=1);
	std::shared_ptr<PreparedGrass> Prepare(RuntimeLease&,const NativeWorldDrawSelection167&,uint64_t sourceDrawFrame,
		const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		const GrassSourceImageProviderS01&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		bool validation,double& milliseconds,unsigned samples,bool sameInput);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
