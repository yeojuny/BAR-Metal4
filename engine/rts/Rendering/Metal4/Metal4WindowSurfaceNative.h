/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

// Private Objective-C++ bridge for the native renderer. Public engine headers
// expose no Apple objects. Borrowed native references require the surface to
// stay attached/alive until all GPU work has retired.
#ifndef __OBJC__
#error "Metal4WindowSurfaceNative.h is private to Objective-C++ native code"
#endif

#include "Metal4WindowSurface.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace rendering {

struct Metal4WindowSurfaceNativeAccess {
	static Metal4WindowSurface AttachWithDevice(SDL_Window* window, id<MTLDevice> device, const Metal4SurfaceConfig& config);
	static id<MTLDevice> Device(const Metal4WindowSurface& surface);
	// Refreshes native presentation metadata and restores the exact requested
	// pixel drawableSize after SDL's own backing-scale resize watcher.
	// Does not acquire a drawable, submit GPU work or present a frame.
	static CAMetalLayer* Layer(Metal4WindowSurface& surface);
};

} // namespace rendering
