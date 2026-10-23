/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4RuntimeNative.h is private to Objective-C++ native rendering"
#endif

#include "../Backend/RenderTypes.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <chrono>
#include <mutex>
#include <map>

namespace rendering::metal4 {

// The normal game target already defines RECOIL_METAL4_LIVE_SCENE.
// Standalone development tools retain their requested shader checks and timing.
#ifdef RECOIL_METAL4_LIVE_SCENE
inline constexpr bool NativeDiagnosticInstrumentation = false;
#else
inline constexpr bool NativeDiagnosticInstrumentation = true;
#endif

// Isolated comparison builds must select all three axes explicitly and use
// identical definitions in every translation unit. With no selectors, preserve
// the production/development policy above, including constructor/cache inputs.
#if defined(RECOIL_METAL4_COMPARE_VALIDATION) || defined(RECOIL_METAL4_COMPARE_CPU_TIMING) || defined(RECOIL_METAL4_COMPARE_GPU_TIMING)
#if !defined(RECOIL_METAL4_LIVE_SCENE)
#error "Metal4 comparison selectors require the live-scene diagnostic target"
#endif
#if !defined(RECOIL_METAL4_COMPARE_VALIDATION) || !defined(RECOIL_METAL4_COMPARE_CPU_TIMING) || !defined(RECOIL_METAL4_COMPARE_GPU_TIMING)
#error "Define validation, CPU timing and GPU timing comparison selectors together"
#endif
static_assert(RECOIL_METAL4_COMPARE_VALIDATION == 0 || RECOIL_METAL4_COMPARE_VALIDATION == 1);
static_assert(RECOIL_METAL4_COMPARE_CPU_TIMING == 0 || RECOIL_METAL4_COMPARE_CPU_TIMING == 1);
static_assert(RECOIL_METAL4_COMPARE_GPU_TIMING == 0 || RECOIL_METAL4_COMPARE_GPU_TIMING == 1);
inline constexpr bool NativeCPUTimingInstrumentation = RECOIL_METAL4_COMPARE_CPU_TIMING != 0;
inline constexpr bool NativeGPUTimingInstrumentation = RECOIL_METAL4_COMPARE_GPU_TIMING != 0;
#else
inline constexpr bool NativeCPUTimingInstrumentation = NativeDiagnosticInstrumentation;
inline constexpr bool NativeGPUTimingInstrumentation = NativeDiagnosticInstrumentation;
#endif

constexpr MTLShaderValidation ShaderValidationMode(bool requested) noexcept
{
#ifdef RECOIL_METAL4_COMPARE_VALIDATION
	// Override at the actual pipeline option: live defaults/requested values
	// stay fixed across cells and must not silently turn a V1 cell back off.
	(void)requested;
	return RECOIL_METAL4_COMPARE_VALIDATION != 0 ? MTLShaderValidationEnabled : MTLShaderValidationDisabled;
#else
	return NativeDiagnosticInstrumentation && requested ? MTLShaderValidationEnabled : MTLShaderValidationDisabled;
#endif
}

// This clock is used only for renderer diagnostic durations, never simulation
// time, animation, completion events or timeout deadlines.
struct Clock : std::chrono::steady_clock {
	static time_point now() noexcept {
		if constexpr (NativeCPUTimingInstrumentation) return std::chrono::steady_clock::now();
		return {};
	}
};
double Milliseconds(Clock::time_point start, Clock::time_point end = Clock::now());
NSString* String(const std::string& value);
std::string Error(NSError* error);
void Require(bool condition, const std::string& message);
// Literal diagnostics must not allocate a temporary string on successful checks.
void Require(bool condition, const char* message);

class RuntimeLease;
class RuntimeCleanupLease;

// One actual native context for both asset and screen passes. All requests hold
// a serialized lease; the monotonically increasing shared event spans requests.
// This private device is also available for a future surface AttachWithDevice.
class Runtime {
public:
	Runtime();
	DeviceCapabilities Capabilities() const { return capabilities; }
	id<MTLDevice> Device() const { return device; }
	bool CanSafelyDetach();
private:
	struct ShaderSource {
		__weak id<MTLLibrary> original=nil;
		__strong MTL4LibraryDescriptor* descriptor=nil;
		__strong id<MTLLibrary> sampleLibrary=nil;
	};
	std::map<std::uintptr_t,ShaderSource> shaderSources;
	DeviceCapabilities capabilities;
	__strong id<MTLDevice> device = nil;
	__strong id<MTL4CommandQueue> queue = nil;
	__strong id<MTL4CommandAllocator> allocator = nil;
	__strong id<MTL4CommandBuffer> command = nil;
	__strong id<MTLSharedEvent> completionEvent = nil;
	__strong id<MTL4Compiler> compiler = nil;
	__strong id<MTLResidencySet> residency = nil;
	std::mutex requestMutex;
	uint64_t nextEventValue = 0;
	__strong NSMutableArray* failedRetention = nil;
	bool poisoned = false;
	bool unsafePendingWork = false;
	friend class RuntimeLease;
	friend class RuntimeCleanupLease;
};

Runtime& SharedRuntime();

// Hold the same serialization lock while releasing already-retired resources.
// A completed GPU error forbids another execution lease but still permits safe
// cleanup. This guard exposes no device, command, or submission operations.
class RuntimeCleanupLease {
public:
	explicit RuntimeCleanupLease(Runtime& runtime = SharedRuntime());
	RuntimeCleanupLease(const RuntimeCleanupLease&) = delete;
	RuntimeCleanupLease& operator=(const RuntimeCleanupLease&) = delete;
private:
	std::unique_lock<std::mutex> lock;
};

class RuntimeLease {
public:
	explicit RuntimeLease(Runtime& runtime = SharedRuntime());
	~RuntimeLease();
	RuntimeLease(const RuntimeLease&) = delete;
	RuntimeLease& operator=(const RuntimeLease&) = delete;
	id<MTLDevice> Device() const { return runtime.device; }
	id<MTL4Compiler> Compiler() const { return runtime.compiler; }
	DeviceCapabilities Capabilities() const { return runtime.capabilities; }
	// Per-submission original shading rate. Output count still belongs to each
	// concrete target: named MRT/RTT/shadow targets never inherit main MSAA.
	void SetMinSampleShadingRate(float rate);
	unsigned MinimumShadedSamples(unsigned samples) const;
	bool SampleShadingEnabled(unsigned samples) const;
	unsigned SampleShadingKey(unsigned samples) const;
	id<MTLLibrary> NewLibrary(MTL4LibraryDescriptor*,NSError**);
	id<MTLLibrary> NewLibraryWithSource(NSString*,MTLCompileOptions*,NSError**);
	id<MTLRenderPipelineState> NewRenderPipeline(MTL4RenderPipelineDescriptor*,NSError**,bool shadeSamples=true);
	void RegisterShaderSource(id<MTLLibrary>,MTL4LibraryDescriptor*);
	void RegisterShaderSource(id<MTLLibrary>,NSString*,MTLCompileOptions*);
	void ConfigureSampleShading(MTL4RenderPipelineDescriptor*);
	std::string QueueClass() const;
	std::string CommandBufferClass() const;
	void AddAllocation(id<MTLAllocation> allocation);
	void Retain(id object);
	void CommitResources();
	// Actual recording/completion failure: retain this request and prohibit reuse.
	void PoisonAfterBegin() noexcept;
	uint64_t ResidentBytes() const;
	uint32_t AllocationCount() const;
	id<MTL4CommandBuffer> Begin(const std::string& label);
	// Optional drawable: wait before commit, signal after commit, then present.
	// Its render texture must already be registered before Begin. The drawable
	// itself is retained through feedback, including a late completion on error.
	FrameMetrics SubmitAndWait(Clock::time_point preparationStart, uint64_t frameIndex, id<MTLDrawable> drawable = nil);
	std::vector<uint8_t> Readback(id<MTLTexture> texture, uint32_t width, uint32_t height, double& milliseconds);
private:
	Runtime& runtime;
	std::unique_lock<std::mutex> lock;
	__strong NSMutableArray* retained = nil;
	float minSampleShadingRate=0.0f;
	id<MTLLibrary> SampleLibrary(id<MTLLibrary>);
	MTL4FunctionDescriptor* SampleFunction(MTL4FunctionDescriptor*);
	bool recording = false;
	bool pending = false;
};

class CaptureScope {
public:
	void Start(id<MTLDevice> device, const std::string& path);
	void Stop();
	~CaptureScope();
private:
	__strong MTLCaptureManager* manager = nil;
	bool active = false;
};

} // namespace rendering::metal4
