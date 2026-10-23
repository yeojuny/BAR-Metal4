/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rendering {

// Capability is established by the device query and actual backend creation.
// The OS or SDK version alone must not select a supported renderer.
struct DeviceCapabilities {
	std::string deviceName;
	bool directMetal4 = false;
	bool bcTextureCompression = false;
	bool gpuTimestamps = false;
	bool unifiedMemory = false;
	bool appleGPU = false;
	// Native Metal4 family limit from Apple's implementation table. Zero
	// means unestablished/unsupported; this does not imply any OpenGL support.
	std::uint32_t maxTextureDimension2D = 0;
	// Established only after the real native sampler can be created.
	std::uint32_t maxSamplerAnisotropy = 0;
	// Bit N is the actual device query for N samples; allocation is separate.
	std::uint64_t supportedSampleCounts = 0;
};

// Durations have independent clocks/scopes; they must not be added to produce
// an FPS estimate. Absent values distinguish unmeasured work from zero work.
struct FrameMetrics {
	uint64_t frameIndex = 0;
	std::optional<double> cpuSimulationMs;
	double cpuCommandPrepMs = 0.0;
	double cpuSubmissionMs = 0.0;
	std::optional<double> gpuExecutionMs;
	double cpuCompletionWaitMs = 0.0;
	double cpuReadbackMs = 0.0;
};

struct RenderReadbackResult {
	DeviceCapabilities capabilities;
	std::vector<FrameMetrics> frames; // Frame 0 is warmup; frames 1..N are measured.
	std::vector<uint8_t> rgba; // Tightly packed RGBA8, top row first.
	uint32_t width = 0;
	uint32_t height = 0;
	double resourceSetupMs = 0.0;
	double shaderCompileMs = 0.0;
	double readbackMs = 0.0;
	uint64_t residentBytes = 0;
	uint32_t residencyAllocationCount = 0;
	std::string queueClass;
	std::string commandBufferClass;
	std::string renderEncoderClass;
	std::string capturePath;
};

} // namespace rendering
