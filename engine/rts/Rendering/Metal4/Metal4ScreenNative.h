/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4ScreenNative.h is private to Objective-C++ native rendering"
#endif
#include "Metal4RuntimeNative.h"
#include "Metal4StartBoxNative.h"
#include "Metal4WeaponRangeNative.h"
#include "Metal4SensorPreviewNative.h"
#include "Metal4SensorLOSNative.h"
#include "../Backend/ScreenDrawList.h"
#include "../Backend/NativePostprocessData.h"
#include <functional>
#include <memory>

namespace rendering::metal4 {
struct PreparedScreenExternalImage;
// Collect AFTER every screen/world image has been prepared in this request.
// Retain through real GPU retirement; skipped/aborted plans never initialize.
class PreparedScreenMipmaps {
public:
	~PreparedScreenMipmaps();
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	void DidComplete() const;
	std::uint32_t TextureCount() const;
private:
	PreparedScreenMipmaps();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class ScreenGPU;
};
class PreparedScreenFrame {
public:
	PreparedScreenFrame();
	~PreparedScreenFrame();
	void Encode(id<MTL4RenderCommandEncoder> encoder) const;
	void EncodeRange(id<MTL4RenderCommandEncoder> encoder, std::uint32_t firstOriginalCommand, std::uint32_t endOriginalCommand) const;
	std::uint32_t CommandCount() const;
	MTLViewport Viewport() const;
	// All resources have been prepared before Begin. Encode cached-panel
	// producers before opening the caller's final world/screen render pass.
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	// Only for a readback closure whose every source was proven independent.
	// Uses the prepared dependency order across World and Screen stages.
	void EncodeIndependentTargetClosure(id<MTL4CommandBuffer> command) const;
	struct TargetInsertion {std::uint32_t offset;std::uint64_t sequence;std::size_t index;
  NativePostprocessStage stage=NativePostprocessStage::Screen;std::uint64_t sourceDrawFrame=0;};
	std::vector<TargetInsertion> DeferredTargets() const;
	// Actual source callins are merged by the existing world ordinal sorter,
	// never encoded by maintenance or the pre-world dependency sweep.
	std::vector<TargetInsertion> WorldTargets() const;
	void EncodeDeferredTarget(id<MTL4CommandBuffer> command,std::size_t index) const;
	bool AllTargetsEncoded() const;
	// Publish initialized native versions only after completion AND feedback.
	void DidComplete() const;
	uint32_t DrawCount() const;
	uint32_t DrawCountFrom(std::uint32_t firstOriginalCommand) const;
	// Actual typed packet draw commands acknowledged after GPU retirement,
	// including nested targets; declarations/skipped commands are not counted.
	uint32_t PiPDrawsCompleted() const;
	uint32_t StartBoxDrawsCompleted() const;
	uint32_t TextureUploads() const;
	// Resolve the frozen prior contents of an original FBO CopyToTexture.
	// Uses this frame's actual GPU cache, producer order and readable receipt.
	PreparedScreenExternalImage ResolvePreviousImage(RuntimeLease&,const std::shared_ptr<const ScreenTexture>&,
		NativePostprocessConsumer) const;
private:
	void EncodeNonTargetDependencies(id<MTL4CommandBuffer> command) const;
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class ScreenGPU;
};
struct PreparedScreenExternalImage {
	__strong id<MTLTexture> texture = nil;
	std::shared_ptr<const void> storageOwner;
	std::function<void()> requireReadable; // Actual encode-time producer proof.
};
struct PreparedPiPSourceImage {
	__strong id<MTLTexture> texture = nil;
	__strong id<MTLSamplerState> sampler = nil;
	std::shared_ptr<const void> storageOwner;
	bool flipV = false;
	std::function<void(id<MTL4CommandBuffer>)> encodeDependencies;
	std::function<void()> requireReadable, didComplete;
};
using PiPSourceImageProvider = std::function<PreparedPiPSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,const ScreenSampler&,bool,double&)>;
struct PreparedScreenImage {
	__strong id<MTLTexture> texture = nil;
	__strong id<MTLSamplerState> sampler = nil;
};
// Exact attachment read, with no submit/present of its own. Caller encodes its
// producer prefix first, then retires the shared command before reading bytes.
class PreparedScreenReadback {
public:
	~PreparedScreenReadback();
	void Encode(id<MTL4CommandBuffer>);
	void DidComplete();
	std::vector<std::uint8_t> PixelsRGBA8() const;
private:
	PreparedScreenReadback();
	struct Impl;
	std::shared_ptr<Impl> impl;
	friend class ScreenGPU;
};

// Persistent source-image generations, sampler states and PSOs. Preparation
// registers every resource before Begin; encoding shares the caller's pass.
class ScreenGPU {
public:
	ScreenGPU();
	~ScreenGPU();
	// Compile the real typed screen program before Lua widget initialization.
	void InitializeDiagnosticPrograms(RuntimeLease&,MTLPixelFormat,bool,double&,unsigned samples);
	// Drop original widget target generations, images and minimap resources
	// between games, only after the shared runtime's previous GPU retirement.
	void ClearResources();
	using SharedImageProvider = std::function<PreparedScreenExternalImage(RuntimeLease&,
		const std::shared_ptr<const NativePostprocessVersion>&,std::optional<NativePostprocessConsumer>)>;
	void SetSharedImageProvider(SharedImageProvider provider);
	using ExternalImageProvider = std::function<PreparedScreenExternalImage(RuntimeLease&,
		const std::shared_ptr<const NativeCUSTextureObject>&,std::optional<NativePostprocessConsumer>)>;
	void SetExternalImageProvider(ExternalImageProvider provider);
	void SetPiPSourceImageProvider(PiPSourceImageProvider);
 // Shares the existing source provider and its real producer/retirement
 // callbacks with another typed consumer; no independent submission.
 PreparedPiPSourceImage ResolveOriginalSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,const ScreenSampler&,bool,double&);
	using StartBoxImageProvider=std::function<PreparedStartBoxImage(RuntimeLease&,const NativeCUSTextureBinding&,
		const PreparedScreenFrame*,std::optional<NativePostprocessConsumer>)>;
	void SetStartBoxImageProvider(StartBoxImageProvider);
 // Root owns the compiled SensorPreviewGPU/programs. ScreenGPU owns only the
 // returned immutable command for this actual cached-minimap insertion.
 using SensorPreviewProvider=std::function<std::unique_ptr<PreparedSensorPreviewCommand>(RuntimeLease&,
  const NativeSensorPreviewDraw&,const SensorPreviewTarget&,const PreparedScreenFrame*,bool,double&,NativePostprocessConsumer)>;
 void SetSensorPreviewProvider(SensorPreviewProvider);
 using SensorLOSProvider=std::function<std::unique_ptr<PreparedSensorLOSDraw>(RuntimeLease&,const NativeSensorLOSDraw&,
  const SensorPreviewTarget&,const PreparedScreenFrame*,bool,double&,NativePostprocessConsumer)>;
 void SetSensorLOSProvider(SensorLOSProvider);
	PreparedStartBoxImage ResolveStartBoxImage(RuntimeLease&,const NativeCUSTextureBinding&,const PreparedScreenFrame*,std::optional<NativePostprocessConsumer>);
	id<MTLSamplerState> PrepareSampler(RuntimeLease&,const ScreenSampler&);
	// Real same-owner allocation only; no contents/readability or submission.
	id<MTLTexture> PrepareAttachmentAllocation(RuntimeLease&,const std::shared_ptr<const ScreenTexture>&);
	// No image may be sampled before this plan's dependencies on the SAME
	// submission. Call after all Prepare/PrepareImage operations, before Begin.
	std::unique_ptr<PreparedScreenMipmaps> PrepareMipmaps(RuntimeLease& lease);
	// Shares the actual persistent source-image/RTT cache with world draws.
	// A non-null producer frame must EncodeDependencies on this same command
	// buffer before any returned image is sampled. Retirement stays with it.
	PreparedScreenImage PrepareImage(RuntimeLease& lease,const std::shared_ptr<const ScreenTexture>& source,
		const ScreenSampler& sampler,const PreparedScreenFrame* producers=nullptr,
		std::optional<NativePostprocessConsumer> consumer = std::nullopt,
		std::function<void(PreparedScreenExternalImage)> retainExternal = {},
		id<MTLSamplerState> actualSampler = nil);
	// The image is an actual retained attachment/resolve with an encode-time
	// readability receipt, never a prior presented frame or synthetic bitmap.
	std::shared_ptr<PreparedScreenReadback> PrepareReadback(RuntimeLease&,
		PreparedScreenExternalImage image,ScreenScissor rectangle);
	std::unique_ptr<PreparedScreenFrame> Prepare(RuntimeLease& lease, const ScreenFrameView& frame,
		uint32_t width, uint32_t height, uint32_t logicalWindowWidth, uint32_t logicalWindowHeight,
		MTLPixelFormat format, bool shaderValidation, double& shaderCompileMs,
		MTLPixelFormat depthFormat=MTLPixelFormatInvalid, MTLPixelFormat stencilFormat=MTLPixelFormatInvalid, std::uint32_t samples=1, std::uint32_t firstOriginalCommand=0);
	// Actual color-only attachment clear, including original scissor/color mask.
	// Reuses the existing clear geometry/PSO without restarting PiP/RTT frame state.
	std::unique_ptr<PreparedScreenFrame> PrepareColorAttachmentClear(RuntimeLease&,ScreenColor,
		uint32_t width,uint32_t height,std::optional<ScreenScissor>,std::array<bool,4> colorWrite,
		MTLPixelFormat color,bool validation,double& shaderMilliseconds, std::uint32_t samples=1);
	// Platform presentation policy only: clear cutout margins after all scene/UI
	// consumers, using logical desktop geometry and actual target pixel mapping.
	// No RTT producer, source ordinal, second submission or presentation.
	std::unique_ptr<PreparedScreenFrame> PreparePresentationMargins(RuntimeLease&, const ScreenScissor& logicalViewport,
		uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,
		MTLPixelFormat color,bool validation,double& shaderMilliseconds, std::uint32_t samples=1);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	std::unique_ptr<PreparedScreenFrame> PrepareList(RuntimeLease& lease, const ScreenFrameView& frame,
		uint32_t width, uint32_t height, uint32_t logicalWindowWidth, uint32_t logicalWindowHeight,
		MTLPixelFormat format, bool shaderValidation, double& shaderCompileMs,
		const std::vector<const ScreenTexture*>& availableTargets,bool rootScreen=false,
		std::optional<NativePostprocessConsumer> targetConsumer=std::nullopt,
		MTLPixelFormat depthFormat=MTLPixelFormatInvalid, MTLPixelFormat stencilFormat=MTLPixelFormatInvalid, std::uint32_t samples=1, std::uint32_t firstOriginalCommand=0);
};
} // namespace rendering::metal4
