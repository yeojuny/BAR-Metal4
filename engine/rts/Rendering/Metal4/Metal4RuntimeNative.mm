/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4RuntimeNative.h"
#include <cmath>

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>

namespace rendering::metal4 {

double Milliseconds(Clock::time_point start, Clock::time_point end)
{
	if constexpr (NativeCPUTimingInstrumentation)
		return std::chrono::duration<double, std::milli>(end - start).count();
	return 0;
}

NSString* String(const std::string& value)
{
	NSString* result = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
	Require(result != nil, "Native renderer text is not valid UTF-8");
	return result;
}

std::string Error(NSError* error)
{
	return error == nil ? "Metal returned nil without an error" : std::string(error.localizedDescription.UTF8String);
}

void Require(bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

Runtime::Runtime()
{
	device = MTLCreateSystemDefaultDevice();
	Require(device != nil, "No Metal device available");
	capabilities.deviceName = device.name.UTF8String;
	capabilities.bcTextureCompression = device.supportsBCTextureCompression;
	capabilities.unifiedMemory = device.hasUnifiedMemory;
	capabilities.appleGPU = [device supportsFamily:MTLGPUFamilyApple1];
	if (@available(macOS 26.0, *)) {
		capabilities.directMetal4 = [device supportsFamily:MTLGPUFamilyMetal4];
		if (!capabilities.directMetal4) return;
		for (std::uint32_t samples : {1u,2u,4u,8u,16u,32u})
			if ([device supportsTextureSampleCount:samples]) capabilities.supportedSampleCounts |= std::uint64_t{1} << samples;
		// Apple Metal Feature Set Tables (May21,2026), Resources row:
		// Apple3..9:16384, Apple10:32768. Footnote1 guarantees at least
		// Apple7 limits on every Metal4 device; query the actual family,
		// never infer a resource limit from the OS, SDK or device name.
		// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
		capabilities.maxTextureDimension2D = [device supportsFamily:MTLGPUFamilyApple10] ? 32768u : 16384u;
		// Metal defines a sampler anisotropy range of 1..16. Establish the
		// actual argument-buffer sampler before publishing that native limit.
		// https://developer.apple.com/documentation/metal/mtlsamplerdescriptor/maxanisotropy
		MTLSamplerDescriptor* anisotropyDescriptor = [MTLSamplerDescriptor new];
		anisotropyDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
		anisotropyDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
		anisotropyDescriptor.mipFilter = MTLSamplerMipFilterLinear;
		anisotropyDescriptor.maxAnisotropy = 16;
		anisotropyDescriptor.supportArgumentBuffers = YES;
		id<MTLSamplerState> anisotropySampler = [device newSamplerStateWithDescriptor:anisotropyDescriptor];
		Require(anisotropySampler != nil, "Cannot create native anisotropic texture sampler");
		capabilities.maxSamplerAnisotropy = static_cast<std::uint32_t>(anisotropyDescriptor.maxAnisotropy);
		queue = [device newMTL4CommandQueue];
		allocator = [device newCommandAllocator];
		command = [device newCommandBuffer];
		completionEvent = [device newSharedEvent];
		Require(queue != nil && allocator != nil && command != nil && completionEvent != nil, "Cannot create shared direct Metal4 command objects");
		NSError* error = nil;
		MTL4CompilerDescriptor* compilerDescriptor = [MTL4CompilerDescriptor new];
		compiler = [device newCompilerWithDescriptor:compilerDescriptor error:&error];
		Require(compiler != nil, "Cannot create shared MTL4 compiler: " + Error(error));
		MTLResidencySetDescriptor* residencyDescriptor = [MTLResidencySetDescriptor new];
		residencyDescriptor.initialCapacity = 16;
		residencyDescriptor.label = @"Shared direct Metal4 request allocations";
		residency = [device newResidencySetWithDescriptor:residencyDescriptor error:&error];
		Require(residency != nil, "Cannot create shared Metal4 residency: " + Error(error));
		[residency commit];
		[queue addResidencySet:residency];
	}
}

Runtime& SharedRuntime()
{
	static Runtime runtime;
	return runtime;
}

bool Runtime::CanSafelyDetach()
{
	std::lock_guard<std::mutex> lock(requestMutex);
	return !unsafePendingWork;
}

RuntimeCleanupLease::RuntimeCleanupLease(Runtime& runtime): lock(runtime.requestMutex)
{
	Require(!runtime.unsafePendingWork, "Cannot retire native resources with outstanding GPU work");
}

void RuntimeLease::SetMinSampleShadingRate(float rate)
{
	Require(!recording && std::isfinite(rate) && rate>=0 && rate<=1,"Original sample shading rate is invalid or changed during encoding");
	minSampleShadingRate=rate;
}
unsigned RuntimeLease::MinimumShadedSamples(unsigned samples) const
{
	Require(samples>=1 && samples<=32,"Sample shading needs the actual target sample count");
	return std::max(1u,unsigned(std::ceil(double(minSampleShadingRate)*samples)));
}
bool RuntimeLease::SampleShadingEnabled(unsigned samples) const
{
	MinimumShadedSamples(samples);
	// Full-sample evaluation satisfies every positive GL minimum fraction.
	// Zero retains the original pixel shader; count1 has only one sample.
	return samples>1 && minSampleShadingRate>0;
}
unsigned RuntimeLease::SampleShadingKey(unsigned samples) const
{
	return samples | (SampleShadingEnabled(samples)?256u:0u);
}
id<MTLLibrary> RuntimeLease::NewLibrary(MTL4LibraryDescriptor* descriptor,NSError** error)
{
	auto library=[Compiler() newLibraryWithDescriptor:descriptor error:error];
	if(library)RegisterShaderSource(library,descriptor);
	return library;
}
id<MTLLibrary> RuntimeLease::NewLibraryWithSource(NSString* source,MTLCompileOptions* options,NSError** error)
{
	auto library=[Device() newLibraryWithSource:source options:options error:error];
	if(library)RegisterShaderSource(library,source,options);
	return library;
}
id<MTLRenderPipelineState> RuntimeLease::NewRenderPipeline(MTL4RenderPipelineDescriptor* descriptor,NSError** error,bool shadeSamples)
{
	MTL4RenderPipelineDescriptor* selected=[descriptor copy];
	if(shadeSamples)ConfigureSampleShading(selected);
	return [Compiler() newRenderPipelineStateWithDescriptor:selected compilerTaskOptions:nil error:error];
}
void RuntimeLease::RegisterShaderSource(id<MTLLibrary> library,MTL4LibraryDescriptor* descriptor)
{
	Require(library && library.device==Device() && descriptor && descriptor.source,"Native shader source registration lacks its actual library/source owner");
	for(auto it=runtime.shaderSources.begin();it!=runtime.shaderSources.end();)
		if(!it->second.original)it=runtime.shaderSources.erase(it);else ++it;
	const auto key=reinterpret_cast<std::uintptr_t>((__bridge void*)library);
	auto& entry=runtime.shaderSources[key];
	Require(!entry.original || (entry.original==library && [entry.descriptor.source isEqualToString:descriptor.source]),"Native shader source identity changed for a live library");
	entry.original=library;entry.descriptor=[descriptor copy];
}
void RuntimeLease::RegisterShaderSource(id<MTLLibrary> library,NSString* source,MTLCompileOptions* options)
{
	auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=source;descriptor.options=options;
	RegisterShaderSource(library,descriptor);
}
id<MTLLibrary> RuntimeLease::SampleLibrary(id<MTLLibrary> library)
{
	Require(library && library.device==Device(),"Sample shading library belongs to another device");
	const auto key=reinterpret_cast<std::uintptr_t>((__bridge void*)library);
	const auto found=runtime.shaderSources.find(key);
	Require(found!=runtime.shaderSources.end() && found->second.original==library,"Sample shading requires its registered original source-qualified library");
	auto& entry=found->second;
	Require([entry.descriptor.source rangeOfString:@"RECOIL_NATIVE_SAMPLE_SHADING_ANNOTATED"].location!=NSNotFound,
		"Sample shading requires reviewed original fragment input annotations");
	if(!entry.sampleLibrary) {
		MTL4LibraryDescriptor* descriptor=[entry.descriptor copy];
		descriptor.source=[@"#define RECOIL_NATIVE_SAMPLE_SHADING 1\n" stringByAppendingString:entry.descriptor.source];
		NSError* error=nil;
		auto variant=[Compiler() newLibraryWithDescriptor:descriptor error:&error];
		Require(variant!=nil,"Cannot compile original per-sample shader variant: "+Error(error));
		entry.sampleLibrary=variant;
	}
	Retain(entry.sampleLibrary);return entry.sampleLibrary;
}
MTL4FunctionDescriptor* RuntimeLease::SampleFunction(MTL4FunctionDescriptor* function)
{
	if(!function)return nil;
	if([function isKindOfClass:[MTL4LibraryFunctionDescriptor class]]) {
		MTL4LibraryFunctionDescriptor* copy=[(MTL4LibraryFunctionDescriptor*)function copy];
		copy.library=SampleLibrary(copy.library);return copy;
	}
	if([function isKindOfClass:[MTL4SpecializedFunctionDescriptor class]]) {
		MTL4SpecializedFunctionDescriptor* copy=[(MTL4SpecializedFunctionDescriptor*)function copy];
		copy.functionDescriptor=SampleFunction(copy.functionDescriptor);return copy;
	}
	throw std::runtime_error("Original per-sample stage requires its actual library or specialized function descriptor");
}
void RuntimeLease::ConfigureSampleShading(MTL4RenderPipelineDescriptor* descriptor)
{
	Require(descriptor!=nil,"Sample shading pipeline descriptor is absent");
	if(!SampleShadingEnabled(unsigned(descriptor.rasterSampleCount)))return;
	descriptor.vertexFunctionDescriptor=SampleFunction(descriptor.vertexFunctionDescriptor);
	descriptor.fragmentFunctionDescriptor=SampleFunction(descriptor.fragmentFunctionDescriptor);
}

RuntimeLease::RuntimeLease(Runtime& selectedRuntime): runtime(selectedRuntime), lock(runtime.requestMutex)
{
	Require(runtime.capabilities.directMetal4, "Device does not support MTLGPUFamilyMetal4; no renderer fallback exists");
	Require(runtime.capabilities.unifiedMemory, "Native diagnostic shared-resource contract requires unified memory");
	Require(!runtime.poisoned, "Shared Metal4 runtime is poisoned after an unsafe recording/execution failure; restart required");
	[ runtime.residency removeAllAllocations ];
	[ runtime.residency commit ];
	retained = [NSMutableArray arrayWithArray:@[runtime.device, runtime.queue, runtime.allocator, runtime.command,
		runtime.completionEvent, runtime.compiler, runtime.residency]];
}

RuntimeLease::~RuntimeLease()
{
	if (recording || pending) runtime.poisoned = true;
	if (runtime.poisoned) runtime.failedRetention = retained;
	if (pending) runtime.unsafePendingWork = true;
	// A poisoned request's allocations must remain resident for late completion.
	// Its immutable completion graph also retains all resources independently.
	if (!runtime.poisoned) {
		[runtime.residency removeAllAllocations];
		[runtime.residency commit];
	}
}

std::string RuntimeLease::QueueClass() const { return NSStringFromClass([runtime.queue class]).UTF8String; }
std::string RuntimeLease::CommandBufferClass() const { return NSStringFromClass([runtime.command class]).UTF8String; }

void RuntimeLease::AddAllocation(id<MTLAllocation> allocation)
{
	Require(allocation != nil && !recording && !pending, "Cannot add invalid allocation during Metal4 execution");
	[runtime.residency addAllocation:allocation];
	Retain(allocation);
}

void RuntimeLease::PoisonAfterBegin() noexcept
{
	runtime.poisoned=true;runtime.failedRetention=retained;
}

void RuntimeLease::Retain(id object)
{
	Require(object != nil && !recording && !pending, "Cannot retain invalid native object during Metal4 execution");
	[retained addObject:object];
}

void RuntimeLease::CommitResources() { [runtime.residency commit]; }
uint64_t RuntimeLease::ResidentBytes() const { return runtime.residency.allocatedSize; }
uint32_t RuntimeLease::AllocationCount() const { return uint32_t(runtime.residency.allocationCount); }

id<MTL4CommandBuffer> RuntimeLease::Begin(const std::string& label)
{
	Require(!recording && !pending && !runtime.poisoned, "Shared Metal4 command memory is not ready for recording");
	runtime.command.label = String(label);
	[runtime.command beginCommandBufferWithAllocator:runtime.allocator];
	recording = true;
	return runtime.command;
}

FrameMetrics RuntimeLease::SubmitAndWait(Clock::time_point preparationStart, uint64_t frameIndex, id<MTLDrawable> drawable)
{
	Require(recording && !pending, "Metal4 submission requires one recording command buffer");
	Require(runtime.nextEventValue != std::numeric_limits<uint64_t>::max(), "Metal4 completion event exhausted its monotonic value range");
	[runtime.command endCommandBuffer];
	recording = false;
	FrameMetrics metrics;
	metrics.frameIndex = frameIndex;
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuCommandPrepMs = Milliseconds(preparationStart);
	struct Feedback {
		__strong NSError* error = nil;
		double start = 0;
		double end = 0;
	};
	const auto feedback = std::make_shared<Feedback>();
	dispatch_semaphore_t feedbackSemaphore = dispatch_semaphore_create(0);
	if (drawable != nil) [retained addObject:drawable];
	NSArray* completionGraph = [retained copy];
	MTL4CommitOptions* options = [MTL4CommitOptions new];
	[options addFeedbackHandler:^(id<MTL4CommitFeedback> result) {
		(void)completionGraph.count;
		feedback->error = result.error;
		if constexpr (NativeGPUTimingInstrumentation) {
			feedback->start = result.GPUStartTime;
			feedback->end = result.GPUEndTime;
		}
		dispatch_semaphore_signal(feedbackSemaphore);
	}];
	const uint64_t eventValue = ++runtime.nextEventValue;
	pending = true;
	const auto submissionStart = Clock::now();
	id<MTL4CommandBuffer> commands[] = {runtime.command};
	if (drawable != nil) [runtime.queue waitForDrawable:drawable];
	[runtime.queue commit:commands count:1 options:options];
	if (drawable != nil) {
		[runtime.queue signalDrawable:drawable];
		[drawable present];
	}
	[runtime.queue signalEvent:runtime.completionEvent value:eventValue];
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuSubmissionMs = Milliseconds(submissionStart);
	const auto waitStart = Clock::now();
	Require([runtime.completionEvent waitUntilSignaledValue:eventValue timeoutMS:30000], "GPU completion shared-event timeout (30 seconds)");
	Require(dispatch_semaphore_wait(feedbackSemaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC)) == 0,
		"MTL4 commit feedback timeout (30 seconds)");
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuCompletionWaitMs = Milliseconds(waitStart);
	// Both completion mechanisms have retired the submission, even when its
	// feedback reports an error. Reuse is poisoned on error; teardown is safe.
	pending = false;
	if (feedback->error != nil) {
		runtime.poisoned = true;
		// Failure-only context: preserve the queue's detailed error without
		// enabling shader validation or collecting successful-frame timings.
		NSString* details = feedback->error.userInfo.description;
		if (details.length > 4096) details = [details substringToIndex:4096];
		const auto text = [](NSString* value) -> std::string {
			const char* utf8 = value.UTF8String;
			return utf8 != nullptr ? std::string(utf8) : std::string();
		};
		throw std::runtime_error("GPU reported failure: " + Error(feedback->error) +
			"; command=" + text(runtime.command.label) +
			"; domain=" + text(feedback->error.domain) +
			"; code=" + std::to_string(static_cast<long long>(feedback->error.code)) +
			"; userInfo=" + text(details));
	}
	if constexpr (NativeGPUTimingInstrumentation) {
		if (std::isfinite(feedback->start) && std::isfinite(feedback->end) && feedback->start > 0 && feedback->end >= feedback->start)
			metrics.gpuExecutionMs = (feedback->end - feedback->start) * 1000.0;
	}
	// Both the queue event and feedback completed before command memory reuse.
	[runtime.allocator reset];
	return metrics;
}

} // namespace rendering::metal4

namespace rendering {
DeviceCapabilities QueryMetal4Capabilities()
{
	@autoreleasepool { return metal4::SharedRuntime().Capabilities(); }
}
} // namespace rendering
