/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ScreenPresenter.h"
#include "Metal4ScreenNative.h"
#include "Metal4MainAttachments.h"
#include "Metal4WindowSurfaceNative.h"
#include "Rendering/GlobalRendering.h"

// Keep the platform-only clear owner in the same feedback lifetime graph.
@interface M4PresentationMarginLifetime : NSObject {
@public
 std::shared_ptr<rendering::metal4::PreparedScreenFrame> margins;
}
@end
@implementation M4PresentationMarginLifetime
@end

namespace rendering {
using namespace metal4;

struct Metal4ScreenPresenter::Impl {
	explicit Impl(Metal4WindowSurface& target,unsigned count): surface(target),samples(count) {}
	Metal4WindowSurface& surface;
	ScreenGPU gpu;
	const unsigned samples;
	__strong id<MTLTexture> mainColor=nil;
	uint64_t submissions = 0;
};

Metal4ScreenPresenter::Metal4ScreenPresenter(Metal4WindowSurface& surface,unsigned samples): impl(std::make_unique<Impl>(surface,samples)) {}
Metal4ScreenPresenter::~Metal4ScreenPresenter() = default;

void Metal4ScreenPresenter::ClearResources()
{
	Require([NSThread isMainThread], "Native screen resource retirement requires the main thread");
	RuntimeCleanupLease lease;
	impl->gpu.ClearResources();impl->mainColor=nil;
}

Metal4ScreenPresentationResult Metal4ScreenPresenter::Draw(const ScreenFrameView& frame, bool shaderValidation)
{
	@autoreleasepool {
		Require([NSThread isMainThread], "Native screen presentation requires the main thread");
		Metal4ScreenPresentationResult result;
		if (!impl->surface.Snapshot().attached) return result;
		impl->surface.Refresh();
		const auto surface = impl->surface.Snapshot();
		const bool visible = surface.CanPresent();
		if (!visible && frame.targetPasses.empty()) return result;

		// Hidden loading retains original RTT producers but never acquires a
		// drawable or interprets final screen geometry in an offscreen target.
		ScreenDrawList emptyScreen;
		ScreenFrameView input = frame;
		if (!visible) {
			input.drawList = &emptyScreen;
			input.logicalViewport.reset();
		}
		RuntimeLease lease;
		lease.SetMinSampleShadingRate(globalRendering?globalRendering->minSampleShadingRate:0.0f);
		const auto start = Clock::now();
		CAMetalLayer* layer = visible ? Metal4WindowSurfaceNativeAccess::Layer(impl->surface) : nil;
		double shaderCompileMs = 0;
		auto prepared = impl->gpu.Prepare(lease, input,
			visible ? surface.renderWidth : frame.coordinateWidth,
			visible ? surface.renderHeight : frame.coordinateHeight,
			visible ? surface.logicalWidth : frame.coordinateWidth,
			visible ? surface.logicalHeight : frame.coordinateHeight,
			visible ? layer.pixelFormat : MTLPixelFormatRGBA8Unorm, shaderValidation, shaderCompileMs,MTLPixelFormatInvalid,MTLPixelFormatInvalid,visible?impl->samples:1);
		id<CAMetalDrawable> drawable = visible ? [layer nextDrawable] : nil;
		if (drawable == nil && frame.targetPasses.empty()) return result;
		if (drawable != nil) {
			Require(drawable.texture.width == surface.renderWidth && drawable.texture.height == surface.renderHeight,
				"Native screen drawable differs from explicit render pixels");
			lease.AddAllocation(drawable.texture);
			lease.Retain(drawable);
		}
		id<MTLTexture> color=drawable?drawable.texture:nil;
		if(drawable && impl->samples>1) {
			if(!impl->mainColor || impl->mainColor.width!=surface.renderWidth || impl->mainColor.height!=surface.renderHeight ||
				impl->mainColor.pixelFormat!=layer.pixelFormat || impl->mainColor.device!=lease.Device())
				impl->mainColor=MainAttachmentStorage::Allocate(lease,surface.renderWidth,surface.renderHeight,layer.pixelFormat,impl->samples,@"Original loading-screen multisample color");
			color=impl->mainColor;lease.AddAllocation(color);
		}
		std::shared_ptr<PreparedScreenFrame> presentationMargins;
		if(drawable != nil && globalRendering && globalRendering->fullScreen && !globalRendering->dualScreenMode) {
			const ScreenScissor safe{globalRendering->viewPosX,globalRendering->viewPosY,
				uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)};
			presentationMargins=impl->gpu.PreparePresentationMargins(lease,safe,surface.renderWidth,surface.renderHeight,
				surface.logicalWidth,surface.logicalHeight,layer.pixelFormat,shaderValidation,shaderCompileMs,impl->samples);
			if(presentationMargins){auto lifetime=[M4PresentationMarginLifetime new];lifetime->margins=presentationMargins;lease.Retain(lifetime);}
		}
		// Includes the hidden target-only path; no mip is initialized merely
		// by preparation or by a failed drawable acquisition.
		auto namedImageMipmaps=impl->gpu.PrepareMipmaps(lease);
		lease.CommitResources();
		auto command = lease.Begin("Original Lua loading-screen commands and target producers");
		namedImageMipmaps->EncodeDependencies(command);
		prepared->EncodeDependencies(command);
		if (drawable != nil) {
			MTL4RenderPassDescriptor* pass = [MTL4RenderPassDescriptor new];
			pass.colorAttachments[0].texture = color;
			pass.defaultRasterSampleCount=impl->samples;
			pass.colorAttachments[0].loadAction = MTLLoadActionClear;
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			if(impl->samples>1) {
				pass.colorAttachments[0].resolveTexture=drawable.texture;
				pass.colorAttachments[0].storeAction=MTLStoreActionStoreAndMultisampleResolve;
			}
			// Original ClearScreen clears opaque black before LuaIntro's own
			// background, progress and text; it supplies no world depth content.
			pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
			pass.renderTargetWidth = surface.renderWidth;
			pass.renderTargetHeight = surface.renderHeight;
			auto encoder = [command renderCommandEncoderWithDescriptor:pass];
			Require(encoder != nil, "Cannot encode the original Lua loading screen");
			prepared->Encode(encoder);
			if(presentationMargins)presentationMargins->Encode(encoder);
			[encoder endEncoding];
		}
		lease.SubmitAndWait(start, impl->submissions++, drawable);
		prepared->DidComplete();
		namedImageMipmaps->DidComplete();
		result.presented = drawable != nil;
		result.targetsCompleted = true;
		result.draws = result.presented ? prepared->DrawCount() : 0;
		result.textureUploads = prepared->TextureUploads();
		return result;
	}
}
} // namespace rendering
