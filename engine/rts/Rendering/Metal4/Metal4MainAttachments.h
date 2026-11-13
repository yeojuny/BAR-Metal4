/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4RuntimeNative.h"
#include "../Backend/NativeMainSamplingData.h"
#include "../Backend/NativeIdleResourcePool.h"
#include <limits>
#include <array>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace rendering::metal4 {

enum class MainReadImages : unsigned { Color = 1, Depth = 2, ColorAndDepth = 3 };

class MainAttachmentAllocationError : public std::runtime_error {
public:
	MainAttachmentAllocationError(): std::runtime_error("Cannot allocate actual persistent main framebuffer storage") {}
};

// These are mutable scratch reads, not published logical texture versions.
// The existing copy/version owner must consume them at its original sequence.
// All methods run within the caller's one serialized RuntimeLease/submission.
// No allocation, wait, publication or presentation occurs during EncodeResolve.
class MainFrameAttachments {
public:
	id<MTLTexture> Color() const { return color; }
	id<MTLTexture> Depth() const { return depth; }
	id<MTLTexture> Stencil() const { return stencil; }
	id<MTLTexture> ReadColor() const { return samples == 1 ? color : readColor; }
	id<MTLTexture> ReadDepth() const { return samples == 1 ? depth : readDepth; }
	std::uint32_t Samples() const { return samples; }
	std::uint64_t Generation() const { return generation; }

	// Before Begin only. N=1 retains the original drawable/private-source alias.
	void BindDestination(RuntimeLease& lease, id<MTLTexture> destination)
	{
		Require(!destinationBound && destination && destination.device == lease.Device() &&
			destination.textureType == MTLTextureType2D && destination.sampleCount == 1 &&
			destination.width == depth.width && destination.height == depth.height &&
			destination.pixelFormat == format && (destination.usage & MTLTextureUsageRenderTarget),
			"Main output destination differs from its actual prepared device/extent/format");
		finalColor = destination;
		if (samples == 1) color = destination;
		lease.AddAllocation(destination);
		destinationBound = true;
	}

	// A synchronous export can split one logical frame into serialized batches.
	// Re-register this exact allocation in each lease; do not re-Prepare mutable
	// storage or follow a window resize between the prefix and its suffix.
	void RetainForContinuation(RuntimeLease& lease) const
	{
		Require(destinationBound && color && color.device==lease.Device(),
			"Main continuation lost its actual bound allocation/device");
		for(auto image:{color,depth,stencil,readColor,readDepth,finalColor})
			if(image)lease.AddAllocation(image);
	}

	// Only the final batch acquires a drawable. Keep the private color/depth/
	// stencil attachments intact; attaching a drawable cannot erase or replay
	// the already-completed prefix, including the one-sample case.
	void PrepareFinalDestination(RuntimeLease& lease,id<MTLTexture> destination)
	{
		Require(destinationBound && destination &&
			destination.device==lease.Device() && destination.textureType==MTLTextureType2D &&
			destination.sampleCount==1 && destination.width==depth.width && destination.height==depth.height &&
			destination.pixelFormat==format && (destination.usage&MTLTextureUsageRenderTarget) &&
			(samples!=1 || !destination.framebufferOnly),
			"Final presentation destination differs from retained main attachment identity");
		// Before Begin, a failed preparation may retry with a new drawable.
		// The caller serializes leases and discards this frame after final submit.
		lease.AddAllocation(destination);finalColor=destination;
	}

	MTL4RenderPassDescriptor* LoadedPass() const
	{
		Require(destinationBound, "Main color destination has not been bound before encoding");
		auto pass = [MTL4RenderPassDescriptor new];
		pass.renderTargetWidth = depth.width;
		pass.renderTargetHeight = depth.height;
		pass.defaultRasterSampleCount = samples;
		pass.colorAttachments[0].texture = color;
		pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
		pass.depthAttachment.texture = depth;
		pass.depthAttachment.loadAction = MTLLoadActionLoad;
		pass.depthAttachment.storeAction = MTLStoreActionStore;
		pass.depthAttachment.clearDepth = 1;
		pass.stencilAttachment.texture = stencil;
		pass.stencilAttachment.loadAction = MTLLoadActionLoad;
		pass.stencilAttachment.storeAction = MTLStoreActionStore;
		pass.stencilAttachment.clearStencil = 0;
		return pass;
	}

	// The caller must first end/store its active render encoder. Always refresh:
	// later world/UI writes invalidate an earlier read even within the same frame.
	void EncodeResolve(id<MTL4CommandBuffer> command, MainReadImages images) const
	{
		Require(command && destinationBound, "Main read resolve lacks its actual command/output");
		const auto mask = static_cast<unsigned>(images);
		Require(mask != 0 && (mask & ~3u) == 0, "Invalid main read resolve selection");
		if (samples == 1) return;
		Resolve(command, (mask & 1) ? readColor : nil, (mask & 2) ? readDepth : nil);
	}

	// After all original world/root-UI writes and clears, before the SAME submit.
	void EncodePresentationResolve(id<MTL4CommandBuffer> command) const
	{
		Require(command && destinationBound, "Main presentation resolve lacks its actual command/output");
		if (samples != 1) {Resolve(command, finalColor, nil);return;}
		if(color==finalColor)return; // Ordinary one-submit route already aliases.
		auto copy=[command computeCommandEncoder];
		Require(copy!=nil,"Cannot copy retained main color into final drawable");
		[copy barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit
			beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:color toTexture:finalColor];
		[copy endEncoding];
	}

private:
	void Resolve(id<MTL4CommandBuffer> command, id<MTLTexture> outputColor, id<MTLTexture> outputDepth) const
	{
		auto pass = [MTL4RenderPassDescriptor new];
		pass.renderTargetWidth = depth.width;
		pass.renderTargetHeight = depth.height;
		pass.defaultRasterSampleCount = samples;
		if (outputColor) {
			pass.colorAttachments[0].texture = color;
			pass.colorAttachments[0].resolveTexture = outputColor;
			pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
			pass.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
		}
		if (outputDepth) {
			pass.depthAttachment.texture = depth;
			pass.depthAttachment.resolveTexture = outputDepth;
			pass.depthAttachment.loadAction = MTLLoadActionLoad;
			pass.depthAttachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;
			// OpenGL leaves N->1 depth selection implementation dependent. Equal-N
			// copies below preserve every sample; this applies only to a single read.
			pass.depthAttachment.depthResolveFilter = MTLMultisampleDepthResolveFilterSample0;
		}
		auto encoder = [command renderCommandEncoderWithDescriptor:pass];
		Require(encoder != nil, "Cannot encode the actual ordered main image resolve");
		[encoder barrierAfterQueueStages:MTLStageVertex | MTLStageFragment | MTLStageBlit | MTLStageDispatch
			beforeStages:MTLStageVertex | MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex | MTLStageFragment | MTLStageBlit | MTLStageDispatch
			visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder endEncoding];
	}

	__strong id<MTLTexture> color = nil, depth = nil, stencil = nil;
	__strong id<MTLTexture> readColor = nil, readDepth = nil, finalColor = nil;
	MTLPixelFormat format = MTLPixelFormatInvalid;
	std::uint32_t samples = 1;
	std::uint64_t generation = 0;
	bool destinationBound = false;
 // Cached prototypes never bind destinations or pin themselves. Prepared
 // descriptors pin the exact physical prototype through every continuation.
 std::shared_ptr<const MainFrameAttachments> storageOwner;
	friend class MainAttachmentStorage;
};

// Two cached immutable prototypes avoid overwriting externally pinned storage.
// Prepared descriptors retain exact prototypes; continuation never re-acquires.
// Existing submission retirement barriers still protect uncounted raw aliases.
class MainAttachmentStorage {
public:
	std::shared_ptr<MainFrameAttachments> Prepare(RuntimeLease& lease, std::uint32_t width,
		std::uint32_t height, MTLPixelFormat format, std::uint32_t samples)
	{
		Require(width && height && width <= lease.Capabilities().maxTextureDimension2D &&
			height <= lease.Capabilities().maxTextureDimension2D,
			"Main framebuffer exceeds the actual device extent");
		Require(samples && [lease.Device() supportsTextureSampleCount:samples],
			"Main framebuffer sample count is unsupported by its actual device");
		Require(format == MTLPixelFormatBGRA8Unorm || format == MTLPixelFormatBGRA8Unorm_sRGB ||
			format == MTLPixelFormatRGBA8Unorm || format == MTLPixelFormatRGBA8Unorm_sRGB,
			"Main framebuffer has an unsupported presentation color format");
		std::shared_ptr<MainFrameAttachments> frame;
  auto selected=storage.Acquire([&](const MainFrameAttachments& slot){
   return slot.depth.device==lease.Device()&&slot.depth.width==width&&slot.depth.height==height&&slot.format==format&&slot.samples==samples;
  },[&]{
   Require(generation!=std::numeric_limits<std::uint64_t>::max(),"Main attachment generation exhausted");
   auto next=std::make_shared<MainFrameAttachments>();
   next->format=format;next->samples=samples;next->generation=generation+1;
   next->depth=Allocate(lease,width,height,MTLPixelFormatDepth32Float,samples,@"Main preserved depth samples");
   next->stencil=Allocate(lease,width,height,MTLPixelFormatStencil8,samples,@"Main preserved stencil samples");
   if(samples>1){
    next->color=Allocate(lease,width,height,format,samples,@"Main preserved color samples");
    next->readColor=Allocate(lease,width,height,format,1,@"Main ordered resolved color scratch");
    next->readDepth=Allocate(lease,width,height,MTLPixelFormatDepth32Float,1,@"Main ordered resolved depth scratch");
   }
   return next;
  },[&](const auto& candidate){
   auto prepared=std::make_shared<MainFrameAttachments>(*candidate);
   prepared->storageOwner=candidate;
   for(auto image:{prepared->color,prepared->depth,prepared->stencil,prepared->readColor,prepared->readDepth})
    if(image)lease.AddAllocation(image);
   frame=std::move(prepared);
  });
  generation=std::max(generation,selected->generation);
  return frame;
	}

	void Clear() { storage.Clear(); }

	static id<MTLTexture> Allocate(RuntimeLease& lease, std::uint32_t width, std::uint32_t height,
		MTLPixelFormat format, std::uint32_t samples, NSString* label)
	{
		Require(width && height && width <= lease.Capabilities().maxTextureDimension2D &&
			height <= lease.Capabilities().maxTextureDimension2D && samples &&
			[lease.Device() supportsTextureSampleCount:samples],
			"Main attachment allocation has invalid extent/sample count");
		Require(format == MTLPixelFormatBGRA8Unorm || format == MTLPixelFormatBGRA8Unorm_sRGB ||
			format == MTLPixelFormatRGBA8Unorm || format == MTLPixelFormatRGBA8Unorm_sRGB ||
			format == MTLPixelFormatDepth32Float || format == MTLPixelFormatStencil8,
			"Main attachment allocation has an unsupported storage format");
		auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
		descriptor.textureType = samples == 1 ? MTLTextureType2D : MTLTextureType2DMultisample;
		descriptor.sampleCount = samples;
		descriptor.storageMode = MTLStorageModePrivate;
		descriptor.usage = MTLTextureUsageRenderTarget;
		if (format != MTLPixelFormatStencil8) descriptor.usage |= MTLTextureUsageShaderRead;
		auto image = [lease.Device() newTextureWithDescriptor:descriptor];
		if (image == nil) throw MainAttachmentAllocationError();
		image.label = label;
		return image;
	}

private:
	NativeIdleResourcePool<MainFrameAttachments,2> storage;
	std::uint64_t generation = 0;
};

// Captured at the original terrain-only event, before model depth is written.
// HQ consumers read the N-sample copy; low-quality consumers read a separate
// single image. Neither image is refreshed at a later world/UI read sequence.
struct MainTerrainDepthStorage {
 __strong id<MTLTexture> samples=nil,single=nil;
};

class MainTerrainDepthSnapshot {
public:
	static std::shared_ptr<MainTerrainDepthSnapshot> Prepare(RuntimeLease& lease,
		std::shared_ptr<const MainFrameAttachments> main, const std::array<std::uint32_t, 4>& crop,
		bool needsSamples, bool needsSingle, NativeIdleResourcePool<MainTerrainDepthStorage,2>& storage)
	{
		Require(main && (needsSamples || needsSingle), "Terrain depth snapshot has no actual source/consumer");
		const auto depth = main->Depth();
		Require(crop[2] && crop[3] && crop[0] <= depth.width && crop[1] <= depth.height &&
			crop[2] <= depth.width - crop[0] && crop[3] <= depth.height - crop[1],
			"Original terrain depth crop exceeds its actual main attachment");
		auto result = std::make_shared<MainTerrainDepthSnapshot>();
		result->main = std::move(main);
		result->crop = crop;
  const auto compatible=[&](id<MTLTexture> image,unsigned count){
   return image&&image.device==lease.Device()&&image.width==crop[2]&&image.height==crop[3]&&
    image.pixelFormat==MTLPixelFormatDepth32Float&&image.sampleCount==count&&
    image.textureType==(count>1?MTLTextureType2DMultisample:MTLTextureType2D)&&
    (image.usage&MTLTextureUsageShaderRead)&&image.storageMode==MTLStorageModePrivate;
  };
  storage.Acquire([&](const MainTerrainDepthStorage& candidate){
   return bool(candidate.samples)==needsSamples&&bool(candidate.single)==needsSingle&&
    (!needsSamples||compatible(candidate.samples,result->main->Samples()))&&
    (!needsSingle||compatible(candidate.single,1));
  },[&]{
   auto next=std::make_shared<MainTerrainDepthStorage>();
   if(needsSamples)next->samples=MainAttachmentStorage::Allocate(lease,crop[2],crop[3],MTLPixelFormatDepth32Float,result->main->Samples(),@"Original terrain-only per-sample depth copy");
   if(needsSingle)next->single=needsSamples&&result->main->Samples()==1?next->samples:
    MainAttachmentStorage::Allocate(lease,crop[2],crop[3],MTLPixelFormatDepth32Float,1,@"Original terrain-only resolved depth copy");
   return next;
  },[&](const auto& candidate){
   if(candidate->samples)lease.AddAllocation(candidate->samples);
   if(candidate->single)lease.AddAllocation(candidate->single);
   result->storageOwner=candidate;
   result->sampleDepth=candidate->samples;result->singleDepth=candidate->single;
  });
		return result;
	}

	id<MTLTexture> SamplesImage() const { return sampleDepth; }
	id<MTLTexture> SingleImage() const { return singleDepth; }
	void Encode(id<MTL4CommandBuffer> command){EncodeScopedVertex220(command,false,nil);}
 void EncodeScopedVertex220(id<MTL4CommandBuffer> command,bool scopedVertex220,id<MTLFence> localFence220)
	{
		Require(command && !encoded, "Original terrain-only depth snapshot is missing or duplicated");
		if (singleDepth && main->Samples() > 1) main->EncodeResolve(command, MainReadImages::Depth);
		auto copy = [command computeCommandEncoder];
		Require(copy != nil, "Cannot encode the original terrain-only depth crop");
		[copy barrierAfterQueueStages:MTLStageFragment | MTLStageDispatch | MTLStageBlit
			beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		const auto blit = [&](id<MTLTexture> from, id<MTLTexture> to) {
			[copy copyFromTexture:from sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(crop[0], crop[1], 0)
				sourceSize:MTLSizeMake(crop[2], crop[3], 1) toTexture:to destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		};
		if (sampleDepth) blit(main->Depth(), sampleDepth);
		if (singleDepth && singleDepth != sampleDepth) blit(main->ReadDepth(), singleDepth);
		if(scopedVertex220){Require(localFence220,"Combined snapshot fence missing");[copy updateFence:localFence220 afterEncoderStages:MTLStageBlit];}
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:(scopedVertex220?(MTLStageFragment|MTLStageDispatch):(MTLStageVertex|MTLStageFragment|MTLStageDispatch))
			visibilityOptions:MTL4VisibilityOptionDevice];
		[copy endEncoding];
		encoded = true;
	}

private:
	std::shared_ptr<const MainFrameAttachments> main;
 std::shared_ptr<const MainTerrainDepthStorage> storageOwner;
	std::array<std::uint32_t, 4> crop{};
	__strong id<MTLTexture> sampleDepth = nil, singleDepth = nil;
	bool encoded = false;
};

} // namespace rendering::metal4
