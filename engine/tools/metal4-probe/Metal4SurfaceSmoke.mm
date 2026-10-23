/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "../../rts/Rendering/Metal4/Metal4WindowSurfaceNative.h"

#import <Foundation/Foundation.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

void Pump(rendering::Metal4WindowSurface& surface)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) surface.HandleEvent(event);
	surface.Refresh();
}
} // namespace

int main()
{
	@autoreleasepool {
		SDL_Window* window = nullptr;
		try {
			Require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
			window = SDL_CreateWindow("Recoil Metal surface lifecycle check", 800, 600,
				SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
			Require(window != nullptr, SDL_GetError());
			{
				rendering::Metal4SurfaceConfig config;
				config.renderWidth = 800;
				config.renderHeight = 600;
				id<MTLDevice> device = MTLCreateSystemDefaultDevice();
				auto surface = rendering::Metal4WindowSurfaceNativeAccess::AttachWithDevice(window, device, config);
				Pump(surface);
				const auto initial = surface.Snapshot();
				Require(initial.logicalWidth == 800 && initial.logicalHeight == 600, "Unexpected logical window dimensions");
				Require(initial.presentationBackingWidth > 0 && initial.presentationBackingHeight > 0, "Native backing extent is empty");
				Require(!initial.visible && !initial.CanPresent(), "Hidden test window must not be presentable");
				Require(rendering::Metal4WindowSurfaceNativeAccess::Device(surface) == device, "Surface did not retain the supplied renderer device");
				Require(SDL_GL_GetCurrentContext() == nullptr, "Metal window unexpectedly acquired an OpenGL context");
				bool duplicateRejected = false;
				try { rendering::Metal4WindowSurface duplicate(window, config); }
				catch (const std::exception&) { duplicateRejected = true; }
				Require(duplicateRejected, "A second surface owner was accepted");
				Require(SDL_SetWindowSize(window, 900, 650), SDL_GetError());
				Require(SDL_SyncWindow(window), SDL_GetError());
				Pump(surface);
				const auto resized = surface.Snapshot();
				Require(resized.logicalWidth == 900 && resized.logicalHeight == 650, "SDL resize did not reach surface state");
				Require(resized.renderWidth == 800 && resized.renderHeight == 600, "Resize changed explicit render pixels");
				CAMetalLayer* layer = rendering::Metal4WindowSurfaceNativeAccess::Layer(surface);
				Require(layer.drawableSize.width == 800 && layer.drawableSize.height == 600, "SDL resize overwrote render pixels");
				surface.SetRenderSize(960, 540);
				Require(layer.drawableSize.width == 960 && layer.drawableSize.height == 540, "Explicit render size was not applied");
				SDL_Event close{};
				close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
				close.window.windowID = initial.windowID + 1;
				Require(!surface.HandleEvent(close).relevant, "Foreign window close was consumed");
				close.window.windowID = initial.windowID;
				Require(surface.HandleEvent(close).closeRequested, "Owner close event was not recorded");
				surface.Detach();
				surface.Detach();
				Require(!surface.Snapshot().attached, "Surface remained attached after detach");
				std::printf("{\"status\":\"passed\",\"logical\":[%u,%u],\"backing\":[%u,%u],\"resizedLogical\":[%u,%u],\"renderAfterResize\":[%u,%u],\"explicitRender\":[960,540],\"hidden\":true,\"gpuPresentationTested\":false}\n",
					initial.logicalWidth, initial.logicalHeight, initial.presentationBackingWidth, initial.presentationBackingHeight,
					resized.logicalWidth, resized.logicalHeight, resized.renderWidth, resized.renderHeight);
			}
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 0;
		} catch (const std::exception& error) {
			std::fprintf(stderr, "Metal surface smoke failed: %s\n", error.what());
			if (window != nullptr) SDL_DestroyWindow(window);
			SDL_Quit();
			return 1;
		}
	}
}
