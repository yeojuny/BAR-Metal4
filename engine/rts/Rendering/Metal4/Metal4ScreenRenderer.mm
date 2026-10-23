/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ScreenRenderer.h"
#include "Metal4ScreenNative.h"
#include <cmath>
#include <filesystem>

namespace rendering {
ScreenRenderResult RenderMetal4Screen(const ScreenRenderRequest& request)
{
	@autoreleasepool {
		using namespace metal4;
		Require(request.width>0&&request.width<=4096&&request.height>0&&request.height<=4096,"Screen renderer requires explicit pixels in 1..4096");
		Require(request.measuredFrames>0&&request.measuredFrames<=300,"Screen measured-frame count outside1..300");
		Require(request.capturePath.empty()||!request.shaderValidation,"Screen capture requires shader validation off");
		for(float value:request.clearColor)Require(std::isfinite(value)&&value>=0&&value<=1,"Screen clear color outside[0,1]");
		RuntimeLease lease;
		ScreenRenderResult result;result.capabilities=lease.Capabilities();result.width=request.width;result.height=request.height;
		result.queueClass=lease.QueueClass();result.commandBufferClass=lease.CommandBufferClass();
		const auto setupStart=Clock::now();
		MTLTextureDescriptor* descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:request.width height:request.height mipmapped:NO];
		descriptor.usage=MTLTextureUsageRenderTarget;descriptor.storageMode=MTLStorageModeShared;
		id<MTLTexture> target=[lease.Device() newTextureWithDescriptor:descriptor];Require(target!=nil,"Cannot allocate native UI diagnostic target");lease.AddAllocation(target);
		ScreenGPU gpu;
		const ScreenFrameView frame{request.drawList,request.width,request.height,std::nullopt,request.targetPasses};
		auto screen=gpu.Prepare(lease,frame,request.width,request.height,request.width,request.height,MTLPixelFormatRGBA8Unorm,request.shaderValidation,result.shaderCompileMs);
		// Preparation above includes every root and cached-panel image. Keep
		// this one-shot mip plan through the first actual submission feedback.
		auto namedImageMipmaps=gpu.PrepareMipmaps(lease);
		lease.CommitResources();result.resourceSetupMs=Milliseconds(setupStart)-result.shaderCompileMs;
		result.residentBytes=lease.ResidentBytes();result.residencyAllocationCount=lease.AllocationCount();
		CaptureScope capture;capture.Start(lease.Device(),request.capturePath);
		MTL4RenderPassDescriptor* pass=[MTL4RenderPassDescriptor new];
		pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
		pass.colorAttachments[0].clearColor=MTLClearColorMake(request.clearColor[0],request.clearColor[1],request.clearColor[2],request.clearColor[3]);pass.renderTargetWidth=request.width;pass.renderTargetHeight=request.height;
		result.frames.reserve(size_t(request.measuredFrames)+1);
		for(uint32_t index=0;index<=request.measuredFrames;++index){
			const auto prepStart=Clock::now();auto command=lease.Begin("Actual recorded screen geometry, textures and font coverage");
			if(index==0){
				namedImageMipmaps->EncodeDependencies(command);
				screen->EncodeDependencies(command);
			}
			id<MTL4RenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot create native UI diagnostic encoder");
			result.renderEncoderClass=NSStringFromClass([encoder class]).UTF8String;screen->Encode(encoder);[encoder endEncoding];
			auto metrics=lease.SubmitAndWait(prepStart,index);
			if(index==0){screen->DidComplete();namedImageMipmaps->DidComplete();}
			result.frames.push_back(metrics);
			if(index&&metrics.gpuExecutionMs)result.capabilities.gpuTimestamps=true;
		}
		result.rgba=lease.Readback(target,request.width,request.height,result.readbackMs);capture.Stop();
		if(!request.capturePath.empty()){Require(std::filesystem::exists(request.capturePath),"Screen trace capture did not create output");result.capturePath=request.capturePath;}
		return result;
	}
}
} // namespace rendering
