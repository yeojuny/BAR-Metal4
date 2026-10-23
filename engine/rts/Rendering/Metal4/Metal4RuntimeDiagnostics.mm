/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4RuntimeNative.h"
#include <filesystem>

// Explicit development target only; never part of the normal application.
namespace rendering::metal4 {
std::vector<uint8_t> RuntimeLease::Readback(id<MTLTexture> texture, uint32_t width, uint32_t height, double& milliseconds)
{
	Require(!recording && !pending && !runtime.poisoned, "Metal4 readback requires successful completed GPU work");
	Require(texture != nil && texture.width == width && texture.height == height, "Metal4 readback dimensions differ from target");
	const auto start = Clock::now();
	std::vector<uint8_t> rgba(size_t(width) * height * 4);
	[texture getBytes:rgba.data() bytesPerRow:size_t(width) * 4 fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
	milliseconds = Milliseconds(start);
	return rgba;
}

void CaptureScope::Start(id<MTLDevice> device, const std::string& filename)
{
	if (filename.empty()) return;
	const std::filesystem::path path(filename);
	manager = [MTLCaptureManager sharedCaptureManager];
	Require([manager supportsDestination:MTLCaptureDestinationGPUTraceDocument], "GPU trace capture destination unsupported");
	if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
	Require(!std::filesystem::exists(path), "GPU capture path already exists; choose a fresh output path");
	MTLCaptureDescriptor* descriptor = [MTLCaptureDescriptor new];
	descriptor.captureObject = device;
	descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
	descriptor.outputURL = [NSURL fileURLWithPath:String(path.string())];
	NSError* error = nil;
	Require([manager startCaptureWithDescriptor:descriptor error:&error], "Cannot start GPU trace: " + Error(error));
	active = true;
}

void CaptureScope::Stop() { if (active) { [manager stopCapture]; active = false; } }
CaptureScope::~CaptureScope() { Stop(); }

} // namespace rendering::metal4
