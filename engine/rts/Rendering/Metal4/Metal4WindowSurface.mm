/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WindowSurfaceNative.h"
#include "Metal4CaptureSurfaceBoundary.h"
#include "Metal4SurfaceEventPolicy.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#endif

#import <AppKit/AppKit.h>
#include <SDL3/SDL.h>

#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace rendering {
namespace {

thread_local const Metal4SurfaceWorkerSnapshot* workerSurface=nullptr;

constexpr const char* SURFACE_OWNER_KEY = "recoil.direct-metal4.surface-owner";

void MainThread()
{
	if (![NSThread isMainThread])
		throw std::runtime_error("Metal window surface requires the macOS main thread");
}

void Require(bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

uint32_t NativeExtent(CGFloat extent)
{
	Require(std::isfinite(extent) && extent >= 0 && extent <= std::numeric_limits<uint32_t>::max(),
		"Native Metal view extent is invalid");
	return uint32_t(std::llround(extent));
}

} // namespace

struct Metal4WindowSurface::Impl {
	Impl(SDL_Window* engineWindow, const Metal4SurfaceConfig& config, id<MTLDevice> suppliedDevice):
		window(engineWindow), state(engineWindow != nullptr ? SDL_GetWindowID(engineWindow) : 0, config.renderWidth, config.renderHeight)
	{
		MainThread();
		Require(window != nullptr && state.Snapshot().windowID != 0, "Metal surface requires a live direct SDL3 engine window");
		Require(SDL_WasInit(SDL_INIT_VIDEO) != 0, "Engine must initialize SDL video before attaching a Metal surface");
		const auto flags = SDL_GetWindowFlags(window);
		Require((flags & SDL_WINDOW_METAL) != 0 && (flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN)) == 0,
			"Engine window must use SDL_WINDOW_METAL without a GL or Vulkan window context");
		properties = SDL_GetWindowProperties(window);
		Require(properties != 0, "Cannot obtain engine SDL3 window properties: " + std::string(SDL_GetError()));
		Require(!SDL_HasProperty(properties, SURFACE_OWNER_KEY), "Engine SDL3 window already has an owned Metal4 surface");
		Require(suppliedDevice != nil, "Metal surface requires a native device");
		if (@available(macOS 26.0, *)) {
			Require([suppliedDevice supportsFamily:MTLGPUFamilyMetal4], "Metal surface device does not support direct Metal4");
		} else {
			throw std::runtime_error("Direct Metal4 window surface requires macOS 26 or newer");
		}
		device = suppliedDevice;
		view = SDL_Metal_CreateView(window);
		Require(view != nullptr, "Cannot create engine SDL Metal view: " + std::string(SDL_GetError()));
		try {
			layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(view);
			Require(layer != nil && [layer isKindOfClass:[CAMetalLayer class]], "SDL Metal view has no CAMetalLayer");
			Require(config.pixelFormat == Metal4SurfacePixelFormat::BGRA8Unorm || config.pixelFormat == Metal4SurfacePixelFormat::BGRA8UnormSRGB,
				"Unsupported Metal surface pixel format");
			layer.device = device;
			layer.pixelFormat = config.pixelFormat == Metal4SurfacePixelFormat::BGRA8Unorm ? MTLPixelFormatBGRA8Unorm : MTLPixelFormatBGRA8Unorm_sRGB;
			layer.displaySyncEnabled = config.displaySync;
			layer.framebufferOnly = config.framebufferOnly;
			layer.allowsNextDrawableTimeout = YES;
			Require(SDL_SetPointerProperty(properties, SURFACE_OWNER_KEY, this), "Cannot record engine SDL3 Metal surface owner: " + std::string(SDL_GetError()));
			Require(SDL_GetPointerProperty(properties, SURFACE_OWNER_KEY, nullptr) == this, "Cannot verify unique engine Metal surface ownership");
			Refresh();
		} catch (...) {
			if (SDL_GetPointerProperty(properties, SURFACE_OWNER_KEY, nullptr) == this &&
				!SDL_SetPointerProperty(properties, SURFACE_OWNER_KEY, nullptr)) std::terminate();
			SDL_Metal_DestroyView(view);
			view = nullptr;
			throw;
		}
	}

	~Impl()
	{
		// The SDL/window owner must retire GPU work and destroy this on the main
		// thread before its borrowed window and SDL video subsystem disappear.
		if (![NSThread isMainThread]) std::terminate();
		if (view != nullptr) {
			if (SDL_GetWindowFromID(state.Snapshot().windowID) != window) std::terminate();
			if (SDL_GetPointerProperty(properties, SURFACE_OWNER_KEY, nullptr) == this &&
				!SDL_SetPointerProperty(properties, SURFACE_OWNER_KEY, nullptr)) std::terminate();
			layer.device = nil;
			SDL_Metal_DestroyView(view);
			view = nullptr;
		}
	}

	void RequireLive() const
	{
		MainThread();
		Require(view != nullptr && state.Snapshot().attached, "Metal window surface is detached");
		Require(SDL_GetWindowFromID(state.Snapshot().windowID) == window, "Engine destroyed SDL_Window before Metal surface detachment");
	}

	void ApplyDisplaySync()
	{
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (!configHandler) return;
		const int requested = configHandler->GetInt("VSync");
		if (requested == displaySyncRequest) return;
		// Preserve the existing synchronized behavior for nonzero modes;
		// honor the explicit off setting on this actual native layer.
		const bool enabled = requested != 0;
		layer.displaySyncEnabled = enabled;
		Require(bool(layer.displaySyncEnabled) == enabled, "Metal layer did not accept the requested display synchronization");
		displaySyncRequest = requested;
		LOG("[NativeDisplaySync] requested=%d enabled=%d", requested, int(layer.displaySyncEnabled));
#endif
	}

	Metal4SurfaceObservation Observe() const
	{
		RequireLive();
		int logicalWidth = 0, logicalHeight = 0;
		Require(SDL_GetWindowSize(window, &logicalWidth, &logicalHeight), "Cannot query engine SDL3 logical window size: " + std::string(SDL_GetError()));
		Require(logicalWidth >= 0 && logicalHeight >= 0, "SDL logical window dimensions are invalid");
		NSView* nativeView = (__bridge NSView*)view;
		const NSRect backing = [nativeView convertRectToBacking:nativeView.bounds];
		const auto flags = SDL_GetWindowFlags(window);
		Metal4SurfaceObservation observation;
		observation.logicalWidth = uint32_t(logicalWidth);
		observation.logicalHeight = uint32_t(logicalHeight);
		observation.presentationBackingWidth = NativeExtent(backing.size.width);
		observation.presentationBackingHeight = NativeExtent(backing.size.height);
		observation.visible = (flags & SDL_WINDOW_HIDDEN) == 0;
		observation.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
		return observation;
	}
	bool CaptureRefreshUnchanged() const
	{
		const auto observed=Observe();
		bool syncUnchanged=false; // No configuration proof: retain old drained path.
#ifdef RECOIL_METAL4_LIVE_SCENE
		if(configHandler){const int requested=configHandler->GetInt("VSync");
			syncUnchanged=requested==displaySyncRequest&&bool(layer.displaySyncEnabled)==(requested!=0);}
#endif
		const auto drawable=layer.drawableSize;
		return NativeCaptureSurfaceUnchanged(state.Snapshot(),observed,drawable.width,drawable.height,syncUnchanged);
	}
	bool Refresh()
	{
		RequireLive();
		ApplyDisplaySync();
		const bool changed=state.Observe(Observe());
		ApplyRenderSize();
		return changed;
	}

	void ApplyRenderSize()
	{
		const auto& snapshot = state.Snapshot();
		// Never infer render pixels from bounds, contentsScale or backing extent.
		// SDL's native resize watcher may overwrite this between owner events.
		layer.drawableSize = CGSizeMake(snapshot.renderWidth, snapshot.renderHeight);
	}

	SDL_Window* window = nullptr; // Borrowed, engine-owned.
	SDL_PropertiesID properties = 0; // Borrowed window-owned property group.
	SDL_MetalView view = nullptr; // Unique owned view, destroyed before window.
	__strong CAMetalLayer* layer = nil;
	__strong id<MTLDevice> device = nil;
	Metal4SurfaceState state;
	int displaySyncRequest = std::numeric_limits<int>::min();
};

Metal4WindowSurface::Metal4WindowSurface(SDL_Window* window, const Metal4SurfaceConfig& config)
{
	MainThread();
	impl = std::make_unique<Impl>(window, config, MTLCreateSystemDefaultDevice());
}

Metal4WindowSurface::Metal4WindowSurface(std::unique_ptr<Impl> implementation): impl(std::move(implementation)) {}
Metal4WindowSurface::~Metal4WindowSurface() = default;
Metal4WindowSurface::Metal4WindowSurface(Metal4WindowSurface&&) noexcept = default;
Metal4WindowSurface& Metal4WindowSurface::operator=(Metal4WindowSurface&&) noexcept = default;

Metal4SurfaceEventResult Metal4WindowSurface::HandleEvent(const SDL_Event& event)
{
	MainThread();
 if(!impl)return {};
 const auto action=ClassifyMetal4SurfaceEvent(event,impl->state.Snapshot().windowID);
 if(action==Metal4SurfaceEventAction::Ignore)return {};
 Metal4SurfaceEventResult result;result.relevant=true;
 result.stateChanged=action==Metal4SurfaceEventAction::Close?RequestClose():Refresh();
	result.closeRequested = impl->state.Snapshot().closeRequested;
	return result;
}

bool Metal4WindowSurface::Refresh()
{
 if(workerSurface&&workerSurface->surface==this)return false;
	MainThread();
	Require(impl != nullptr, "Metal window surface is detached or moved from");
	return impl->Refresh();
}

bool Metal4WindowSurface::CaptureRefreshUnchanged() const
{
 MainThread();
 Require(impl!=nullptr,"Metal window surface is detached or moved from");
 return impl->CaptureRefreshUnchanged();
}

bool Metal4WindowSurface::SetRenderSize(uint32_t widthPixels, uint32_t heightPixels)
{
	MainThread();
	Require(impl != nullptr, "Metal window surface is detached or moved from");
	impl->RequireLive();
	const bool changed = impl->state.SetRenderSize(widthPixels, heightPixels);
	impl->ApplyRenderSize();
	return changed;
}

bool Metal4WindowSurface::RequestClose()
{
	MainThread();
	Require(impl != nullptr, "Metal window surface is detached or moved from");
	impl->RequireLive();
	return impl->state.RequestClose();
}

Metal4SurfaceSnapshot Metal4WindowSurface::Snapshot() const
{
 if(workerSurface&&workerSurface->surface==this)return workerSurface->state;
	MainThread();
	if (impl) return impl->state.Snapshot();
	Metal4SurfaceSnapshot snapshot;
	snapshot.attached = false;
	return snapshot;
}

void Metal4WindowSurface::Detach()
{
	MainThread();
	if (!impl) return;
	impl->RequireLive();
	impl->state.Detach();
	impl.reset();
}

Metal4WindowSurface Metal4WindowSurfaceNativeAccess::AttachWithDevice(SDL_Window* window, id<MTLDevice> device, const Metal4SurfaceConfig& config)
{
	MainThread();
	return Metal4WindowSurface(std::make_unique<Metal4WindowSurface::Impl>(window, config, device));
}

id<MTLDevice> Metal4WindowSurfaceNativeAccess::Device(const Metal4WindowSurface& surface)
{
 if(workerSurface&&workerSurface->surface==&surface)return workerSurface->layer.device;
	MainThread();
	if (!surface.impl) return nil;
	surface.impl->RequireLive();
	return surface.impl->device;
}

CAMetalLayer* Metal4WindowSurfaceNativeAccess::Layer(Metal4WindowSurface& surface)
{
 if(workerSurface&&workerSurface->surface==&surface)return workerSurface->layer;
	MainThread();
	if (!surface.impl) return nil;
	surface.impl->Refresh();
	return surface.impl->layer;
}

Metal4SurfaceWorkerSnapshot Metal4WindowSurfaceNativeAccess::PublishWorkerSnapshot(Metal4WindowSurface& surface)
{
 MainThread();surface.Refresh();
 return {&surface,surface.Snapshot(),Layer(surface)};
}
bool Metal4WindowSurfaceNativeAccess::WorkerAccessActive(){return workerSurface!=nullptr;}
Metal4SurfaceWorkerScope::Metal4SurfaceWorkerScope(const Metal4SurfaceWorkerSnapshot& snapshot)
{
 Require(![NSThread isMainThread]&&!workerSurface&&snapshot.surface,"Invalid native worker surface scope");
 workerSurface=&snapshot;
}
Metal4SurfaceWorkerScope::~Metal4SurfaceWorkerScope(){workerSurface=nullptr;}
} // namespace rendering
