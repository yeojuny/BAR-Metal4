/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4PostprocessNative.h is private to Objective-C++ native rendering"
#endif
#include "Metal4RuntimeNative.h"
#include "../Backend/NativePostprocessData.h"
#include <memory>
#include <functional>
namespace rendering::metal4 {
class PreparedScreenFrame;
class MainFrameAttachments;
class PreparedPostprocessFrame {
public:
	PreparedPostprocessFrame();
	~PreparedPostprocessFrame();
	// Caller has ended the actual world render encoder with color/depth stored.
	// All original root screen commands are encoded exactly once, interleaved
	// with copies and original DOF MRT/composition passes. No presentation here.
	void Encode(id<MTL4CommandBuffer> command, const PreparedScreenFrame& screen,
		std::uint32_t firstOriginalCommand=0) const;
	// Called at the exact common world sequence after ending/storing the source
	// encoder. Screen operations remain owned by Encode's insertion ordinals.
	void EncodeOperation(id<MTL4CommandBuffer> command,std::size_t operationIndex) const;
	// Only the paired external renderer calls this after EncodeDraw returned.
	void MarkExternalEncoded(const std::shared_ptr<const NativePostprocessVersion>& output) const;
	void DidComplete() const;
	// Actual produced attachment for typed inter-pass consumers/readback. Contents
	// become valid only after Encode and successful GPU completion.
	id<MTLTexture> ProducedImage(const std::shared_ptr<const NativePostprocessVersion>& version) const;
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class PostprocessGPU;
};
class PostprocessGPU {
public:
	PostprocessGPU();
	~PostprocessGPU();
	void Initialize(RuntimeLease& lease, MTLPixelFormat screenFormat, bool validation, double& compileMs);
	void ClearResources();
	// Immediate CPU TexImage versions use the same persistent UI upload cache
	// as ordinary draws. The provider retains the native image in this lease.
	using CPUImageProvider=std::function<id<MTLTexture>(RuntimeLease&,const std::shared_ptr<const ScreenTexture>&)>;
	void SetCPUImageProvider(CPUImageProvider);
	// Allocation/validation only; no declaration becomes completed here. Must
	// precede generic image preparation on this exact captured world frame.
	void PrepareFrameImages(RuntimeLease& lease,std::shared_ptr<const NativePostprocessFrame> frame,
		std::uint64_t actualSourceDrawFrame);
 // Actual undefined scalar RTT storage for textureSize only when no current
 // version exists. NEVER use this API for contents/readability or producers;
 // produced/retained versions must use ResolveImage with their real version.
 // No generation, clear, encoded/completed flag or publication is fabricated.
 id<MTLTexture> ResolveAllocation(RuntimeLease& lease,std::shared_ptr<const NativePostprocessImage> actualAllocation);
	id<MTLTexture> ResolveImage(RuntimeLease& lease,const std::shared_ptr<const NativePostprocessVersion>& version,
		std::optional<NativePostprocessConsumer> consumer = std::nullopt);
	// Encode-time proof for a captured immutable version; ordering validation alone does not mean its producer ran.
	void RequireReadable(const std::shared_ptr<const NativePostprocessVersion>& version) const;
	std::unique_ptr<PreparedPostprocessFrame> Prepare(RuntimeLease& lease,
		std::shared_ptr<const NativePostprocessFrame> frame, id<MTLTexture> actualColor, id<MTLTexture> actualDepth,
		const PreparedScreenFrame& originalScreen, bool validation, double& compileMs, id<MTLTexture> actualStencil = nil,
		std::shared_ptr<const MainFrameAttachments> main = {});
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
