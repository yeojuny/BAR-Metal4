/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeBlockoutData.h"
#include <functional>
#include <span>
namespace rendering::metal4 {
struct BlockoutDepthImage{
	std::shared_ptr<const NativePostprocessVersion> version;
	__strong id<MTLTexture> depth=nil;
};
class BlockoutGPU;
class PreparedBlockoutFrame{
public:
	~PreparedBlockoutFrame();
	size_t DrawCount()const;
	uint64_t SequenceAt(size_t)const;
	bool IsDepthPass(size_t)const;
	std::shared_ptr<const NativePostprocessVersion> OutputAt(size_t)const;
	void EncodeDependencies(id<MTL4CommandBuffer>)const;
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>)const;
	// Caller ends/stores the common encoder, then resumes it with LOAD.
	void EncodeDepthPass(size_t,id<MTL4CommandBuffer>)const;
	void DidComplete()const;
private:
	PreparedBlockoutFrame();
	struct Impl;std::unique_ptr<Impl> impl;friend class BlockoutGPU;
};
class BlockoutGPU{
public:
	BlockoutGPU();~BlockoutGPU();
	using NormalImageProvider=std::function<PreparedScreenImage(RuntimeLease&,const std::shared_ptr<const ScreenTexture>&,const ScreenSampler&)>;
	void SetNormalImageProvider(NormalImageProvider);
	void Initialize(RuntimeLease&,MTLPixelFormat,bool,double&,unsigned samples=1);
	std::shared_ptr<const NativeBlockoutTarget> AllocateTarget(RuntimeLease&,uint32_t,uint32_t);
	std::unique_ptr<PreparedBlockoutFrame> Prepare(RuntimeLease&,std::shared_ptr<const NativeBlockoutFrame>,
		std::span<const BlockoutDepthImage>,const WorldCamera&,uint32_t,uint32_t,uint32_t,uint32_t,
		MTLPixelFormat,MTLPixelFormat,bool,double&,unsigned samples=1);
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
