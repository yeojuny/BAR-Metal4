/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "MacSDL3WindowGeometry.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace recoil::mac {
namespace {
void Require(bool condition, const char* operation)
{
	if (!condition) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

bool ScreenMatchesBounds(NSScreen* screen, const SDL_Rect& bounds)
{
	if (screen == nil || bounds.w <= 0 || bounds.h <= 0) return false;
	// Cocoa SDL uses CGDisplayBounds: NSScreen order and its bottom-left frame
	// coordinates are not SDL display IDs or desktop logical coordinates.
	NSNumber* screenNumber = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
	if (screenNumber == nil) return false;
	const CGRect nativeBounds = CGDisplayBounds([screenNumber unsignedIntValue]);
	return std::abs(nativeBounds.origin.x - bounds.x) < 0.5 &&
		std::abs(nativeBounds.origin.y - bounds.y) < 0.5 &&
		std::abs(nativeBounds.size.width - bounds.w) < 0.5 &&
		std::abs(nativeBounds.size.height - bounds.h) < 0.5;
}

int ScaleInset(int inset, int extent, int originalExtent)
{
	return int((int64_t(inset) * extent + originalExtent / 2) / originalExtent);
}

} // namespace

SDL_DisplayID RequestedLaunchDisplay()
{
	Require([NSThread isMainThread], "Native launch display requires the main thread");
	const char* requested = std::getenv("BAR_MACOS_TARGET_DISPLAY");
	if (requested == nullptr) return 0;
	std::string_view remaining(requested);
	std::array<int, 4> values{};
	for (size_t i = 0; i < values.size(); ++i) {
		const size_t end = i + 1 == values.size() ? remaining.size() : remaining.find(',');
		if (end == std::string_view::npos || end == 0) return 0;
		const auto field = remaining.substr(0, end);
		const auto parsed = std::from_chars(field.data(), field.data() + field.size(), values[i]);
		if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size()) return 0;
		if (i + 1 != values.size()) remaining.remove_prefix(end + 1);
	}
	if (values[2] <= 0 || values[3] <= 0) return 0;
	int count = 0;
	std::unique_ptr<SDL_DisplayID, decltype(&SDL_free)> displays(SDL_GetDisplays(&count), SDL_free);
	if (displays == nullptr) return 0;
	for (int i = 0; i < count; ++i) {
		SDL_Rect bounds{};
		if (SDL_GetDisplayBounds(displays.get()[i], &bounds) && bounds.x == values[0] && bounds.y == values[1] &&
			bounds.w == values[2] && bounds.h == values[3]) return displays.get()[i];
	}
	// The display may have gone away or changed mode since launch preparation.
	// Do not guess by index or equal size; the original window/mouse choice owns fallback.
	return 0;
}

SDL_Rect DisplayGeometry::BorderlessBounds() const
{
	// The fixed MVK SDL3 policy uses the desktop work area for a borderless
	// window. Native fullscreen owns the separate display-mode/safe-area path.
	return usable;
}

DisplayGeometry QueryDisplayGeometry(SDL_Window* window, SDL_DisplayID display)
{
	Require([NSThread isMainThread], "Native display geometry requires the main thread");
	if (display == 0 && window != nullptr) display = SDL_GetDisplayForWindow(window);
	if (display == 0 && window == nullptr) display = RequestedLaunchDisplay();
	if (display == 0) {
		float x = 0, y = 0;
		SDL_GetGlobalMouseState(&x, &y);
		const SDL_Point mouse{int(std::lround(x)), int(std::lround(y))};
		display = SDL_GetDisplayForPoint(&mouse);
	}
	DisplayGeometry result;
	result.id = display;
	Require(display != 0 && SDL_GetDisplayBounds(display, &result.bounds), "Cannot query logical display bounds");
	if (!SDL_GetDisplayUsableBounds(display, &result.usable) || result.usable.w <= 0 || result.usable.h <= 0)
		result.usable = result.bounds;
	result.safe = result.bounds;
	NSScreen* selected = nil;
	if (window != nullptr) {
		NSWindow* native = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
		if (ScreenMatchesBounds(native.screen, result.bounds)) selected = native.screen;
	}
	if (selected == nil) {
		for (NSScreen* screen in NSScreen.screens) {
			if (ScreenMatchesBounds(screen, result.bounds)) { selected = screen; break; }
		}
	}
	if (selected == nil) return result;
	const NSEdgeInsets inset = selected.safeAreaInsets;
	const NSSize size = selected.frame.size;
	if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 || size.height <= 0) return result;
	const auto scaleInset = [](CGFloat inset, int extent, CGFloat nativeExtent) {
		if (!std::isfinite(inset) || inset <= 0) return 0;
		return int(std::ceil(std::min<double>(inset / nativeExtent, 1.0) * extent));
	};
	const int left = scaleInset(inset.left, result.bounds.w, size.width);
	const int right = scaleInset(inset.right, result.bounds.w, size.width);
	const int top = scaleInset(inset.top, result.bounds.h, size.height);
	const int bottom = scaleInset(inset.bottom, result.bounds.h, size.height);
	if (left + right >= result.bounds.w || top + bottom >= result.bounds.h) return result;
	result.safe = {result.bounds.x + left, result.bounds.y + top, result.bounds.w - left - right, result.bounds.h - top - bottom};
	return result;
}

SDL_Rect ScaledSafeBounds(const DisplayGeometry& display, int width, int height)
{
	Require(display.bounds.w > 0 && display.bounds.h > 0 && width > 0 && height > 0, "Cannot scale empty display geometry");
	const int left = ScaleInset(display.safe.x - display.bounds.x, width, display.bounds.w);
	const int top = ScaleInset(display.safe.y - display.bounds.y, height, display.bounds.h);
	// Fullscreen mode matching scales the total inset once, as the donor does.
	const int horizontal = ScaleInset(std::max(0, display.bounds.w - display.safe.w), width, display.bounds.w);
	const int vertical = ScaleInset(std::max(0, display.bounds.h - display.safe.h), height, display.bounds.h);
	return {left, top, width - horizontal, height - vertical};
}

SDL_DisplayMode SelectFullscreenMode(const DisplayGeometry& display, int width, int height)
{
	int count = 0;
	std::unique_ptr<SDL_DisplayMode*, decltype(&SDL_free)> modes(SDL_GetFullscreenDisplayModes(display.id, &count), SDL_free);
	Require(modes != nullptr, "Cannot enumerate native fullscreen modes");
	const SDL_DisplayMode* selected = nullptr;
	for (int i = 0; i < count; ++i) {
		const SDL_DisplayMode* mode = modes.get()[i];
		const auto safe = ScaledSafeBounds(display, mode->w, mode->h);
		if (mode->w == width && mode->h == height) { selected = mode; break; }
		if (selected == nullptr && std::abs(safe.w - width) <= 2 && std::abs(safe.h - height) <= 2) selected = mode;
	}
	// Preserve Run65's documented desktop choice for unlisted requests, rather
	// than stretching an arbitrary render size into a different aspect ratio.
	if (selected == nullptr) selected = SDL_GetDesktopDisplayMode(display.id);
	Require(selected != nullptr, "Cannot select native fullscreen mode");
	return *selected;
}

SDL_Rect WindowSafeViewport(SDL_Window* window)
{
	int width = 0, height = 0, x = 0, y = 0;
	Require(SDL_GetWindowSize(window, &width, &height) && SDL_GetWindowPosition(window, &x, &y), "Cannot query native window content");
	SDL_Rect viewport{0, 0, width, height};
	if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0) return viewport;
	const auto display = QueryDisplayGeometry(window);
	const SDL_Rect content{x, y, width, height};
	SDL_Rect covered{}, safe{};
	// During a Cocoa display transition retain the whole current window until
	// it is contained by its actual display. Already-safe content is intersected
	// once, so it does not receive a second cutout inset.
	if (SDL_GetRectIntersection(&display.bounds, &content, &covered) && covered.w == width && covered.h == height &&
		SDL_GetRectIntersection(&display.safe, &content, &safe))
		return {safe.x - x, safe.y - y, safe.w, safe.h};
	return viewport;
}

std::array<int, 4> WindowBorders(SDL_Window* window)
{
	Require([NSThread isMainThread], "Native window decorations require the main thread");
	NSWindow* native = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
	Require(native != nil, "Cannot query native window decorations");
	// SDL3's Cocoa driver does not implement SDL_GetWindowBordersSize. Read
	// the real frame/content difference in logical points, without backing scale.
	const NSRect frame = native.frame;
	const NSRect content = [native contentRectForFrameRect:frame];
	const auto pixels = [](CGFloat value) { return std::max(0, int(std::lround(value))); };
	return {pixels(NSMaxY(frame) - NSMaxY(content)), pixels(NSMinX(content) - NSMinX(frame)),
		pixels(NSMinY(content) - NSMinY(frame)), pixels(NSMaxX(frame) - NSMaxX(content))};
}

static void ApplyWindowGeometryImpl(SDL_Window* window, bool fullscreen, bool borderless,
	int x, int y, int width, int height, int minimumWidth, int minimumHeight)
{
	const auto display = QueryDisplayGeometry(window);
	SDL_DisplayMode mode{};
	if (fullscreen) {
		mode = SelectFullscreenMode(display, width, height);
		x = display.bounds.x; y = display.bounds.y;
		width = mode.w; height = mode.h;
	}
	// The caller already selected the borderless work area and bounded the
	// requested windowed frame against the original all-display desktop union.
	// Complete departure before changing the windowed frame/style. Otherwise
	// Cocoa's asynchronous transition can overwrite the requested small size.
	if (!fullscreen && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
		Require(SDL_SetWindowFullscreen(window, false) && SDL_SyncWindow(window), "Cannot leave native fullscreen");
	}
	Require(SDL_RestoreWindow(window), "Cannot restore native window");
	Require(SDL_SetWindowMinimumSize(window, minimumWidth, minimumHeight), "Cannot set native minimum window size");
	if (!fullscreen) {
		Require(SDL_SetWindowBordered(window, !borderless), "Cannot set native window border");
		// Finish style changes before placing the content. Cocoa can adjust the
		// frame while changing resizability, including a one-point edge shift.
		Require(SDL_SetWindowResizable(window, !borderless), "Cannot set native window resizability");
	}
	// Size first: Cocoa's frame/content conversion during a border-style change
	// can move the origin. Place the final content extent after that conversion.
	Require(SDL_SetWindowSize(window, width, height) && SDL_SetWindowPosition(window, x, y), "Cannot set native logical window geometry");
	Require(SDL_SetWindowFullscreenMode(window, fullscreen ? &mode : nullptr), "Cannot set native fullscreen display mode");
	Require(SDL_SetWindowFullscreen(window, fullscreen) && SDL_SyncWindow(window), "Cannot complete native fullscreen state");
}

void ApplyWindowGeometry(SDL_Window* window, bool fullscreen, bool borderless,
	int x, int y, int width, int height, int minimumWidth, int minimumHeight)
{
	ApplyWindowGeometryImpl(window, fullscreen, borderless, x, y, width, height, minimumWidth, minimumHeight);
}

} // namespace recoil::mac
