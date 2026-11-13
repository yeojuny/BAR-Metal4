#include "Metal4PublishedRequestCache.h"
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4RuntimeNative.h"
#include "Metal4PersistentResidencyIntervention.h"
#include "Metal4TransientUpload177.h"
#include "Metal4FrameResources.h"
#include "Rendering/Backend/NativePreparationTrace.h"
#include "Rendering/Backend/NativeSubmissionTrace.h"
#include "Rendering/Backend/NativeCPUPhaseTrace.h"
#include <atomic>
#include <cstdlib>
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif
#include <cmath>

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "Metal4DeferredResidencyOwners.inc"
#include "Metal4TerrainUniformRelease220.inc"
#include "Metal4FrameOverlapProbe.inc"

@implementation M4PersistentResidencySnapshot
@end

@implementation M4TransientUploadLifetime177
@end

namespace rendering::metal4 {

TransientUploadPool177::~TransientUploadPool177()
{
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(requests)LOG("[NativeTransientUpload177] family=%s requests=%llu reused=%llu created=%llu fallback=%llu uploaded_bytes=%llu cached_count=%zu cached_bytes=%zu peak_cached_count=%zu peak_cached_bytes=%zu scope=owner_lifetime",
  family,(unsigned long long)requests,(unsigned long long)reused,
  (unsigned long long)created,(unsigned long long)fallbacks,
  (unsigned long long)uploadedBytes,storage.CachedCount(),storage.CachedBytes(),peakCachedCount,peakCachedBytes);
#endif
}

namespace {
std::uint64_t SubmissionTraceNow() noexcept {
 return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
  std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct SubmissionTraceState {
 const bool enabled=[] {
#ifdef RECOIL_METAL4_LIVE_SCENE
  const auto* value=std::getenv("BAR_NATIVE_SUBMISSION_TRACE");
  return value && value[0]=='1' && value[1]=='\0';
#else
  return false;
#endif
 }();
 std::atomic<bool> finished{false};
 std::mutex mutex;
 NativeSubmissionTraceBudget budget;
};
SubmissionTraceState& SubmissionTrace() {static SubmissionTraceState state;return state;}
struct SubmissionTraceLease {
 const void* owner=nullptr;
 bool active=false,encoding=false;
 std::uint64_t lease=0,encode=0,deadline=0;
 std::uint64_t drawableBegin=0,drawableEnd=0;
 bool drawableStarted=false,drawableCompleted=false,drawableInvalid=false;
};
thread_local SubmissionTraceLease submissionTraceLease;

void BeginSubmissionTraceLease(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return;
 auto& trace=submissionTraceLease;trace={};trace.owner=owner;
 if(state.finished.load(std::memory_order_relaxed))return;
 try {
  const auto now=SubmissionTraceNow();std::lock_guard<std::mutex> lock(state.mutex);
  if(!state.budget.Open(now)||state.budget.tickets>=NativeSubmissionTraceBudget::MaxRecords){state.finished=true;return;}
  trace.active=true;trace.lease=now;trace.deadline=state.budget.Deadline();
 } catch(...) {state.finished=true;}
}
void EndSubmissionTraceLease(const void* owner) noexcept {
 if(!SubmissionTrace().enabled)return;
 if(submissionTraceLease.owner==owner)submissionTraceLease={};
}
void BeginSubmissionTraceEncoding(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return;
 auto& trace=submissionTraceLease;if(trace.owner!=owner||!trace.active)return;
 if(state.finished.load(std::memory_order_relaxed)){trace.active=false;return;}
 const auto now=SubmissionTraceNow();
 if(now>=trace.deadline){trace.active=false;return;}
 trace.encode=now;trace.encoding=true;
}
NativeSubmissionTraceSample BeginSubmissionTraceSubmit(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return {};
 auto& trace=submissionTraceLease;
 if(trace.owner!=owner||!trace.active||!trace.encoding)return {};
 if(state.finished.load(std::memory_order_relaxed)){trace.active=false;return {};}
 try {
  const auto now=SubmissionTraceNow();std::lock_guard<std::mutex> lock(state.mutex);
  if(!state.budget.TakeSample(now)){state.finished=true;trace.active=false;return {};}
  return {true,trace.deadline,trace.lease,trace.encode,now,
   trace.drawableStarted&&trace.drawableCompleted&&!trace.drawableInvalid,trace.drawableBegin,trace.drawableEnd};
 } catch(...) {state.finished=true;return {};}
}
std::uint64_t SubmissionTraceStamp(const NativeSubmissionTraceSample& sample) noexcept {
 return sample.active?SubmissionTraceNow():0;
}
bool SubmissionTraceWantsGPU(const NativeSubmissionTraceSample& sample) noexcept {
 return sample.active && SubmissionTraceNow()<sample.deadline;
}
void RestartSubmissionTraceRequest(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return;
 auto& trace=submissionTraceLease;if(trace.owner!=owner||!trace.active)return;
 trace.encoding=false;
 trace.drawableBegin=trace.drawableEnd=0;
 trace.drawableStarted=trace.drawableCompleted=trace.drawableInvalid=false;
 if(state.finished.load(std::memory_order_relaxed)){trace.active=false;return;}
 // A lease may submit more than once. Start its next request after this
 // submission's reset and diagnostic output, not at the old lease start.
 trace.lease=SubmissionTraceNow();trace.active=trace.lease<trace.deadline;
}
void FinishSubmissionTrace(const NativeSubmissionTraceSample& sample,std::uint64_t submitted,
 std::uint64_t eventDone,std::uint64_t feedbackDone,std::uint64_t event,std::uint64_t frame,
 bool drawable,double gpuStart,double gpuEnd,std::uint64_t cleanupDone=0) noexcept
{
 if(!sample.active)return;
 auto& state=SubmissionTrace();
 try {
  char text[768];const auto length=FormatNativeSubmissionTrace(text,sizeof(text),sample,
   submitted,eventDone,feedbackDone,event,frame,drawable,gpuStart,gpuEnd,cleanupDone);
  if(!length)return;
  const auto now=SubmissionTraceNow();std::lock_guard<std::mutex> lock(state.mutex);
  // Match the existing trace's conservative allowance for the engine logger's
  // timestamp/frame prefix. Diagnostic output never changes render results.
  if(!state.budget.TakeRecord(now,length+160)){state.finished=true;return;}
#ifdef RECOIL_METAL4_LIVE_SCENE
  LOG("%s",text);
#endif
 } catch(...) {state.finished=true;}
}
} // namespace

void BeginNativeSubmissionTraceDrawable(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return;
 auto& trace=submissionTraceLease;if(trace.owner!=owner||!trace.active)return;
 if(trace.encoding||trace.drawableStarted){trace.drawableInvalid=true;return;}
 if(state.finished.load(std::memory_order_relaxed))return;
 trace.drawableBegin=SubmissionTraceNow();trace.drawableStarted=true;
}
void EndNativeSubmissionTraceDrawable(const void* owner) noexcept {
 auto& state=SubmissionTrace();if(!state.enabled)return;
 auto& trace=submissionTraceLease;if(trace.owner!=owner||!trace.active)return;
 if(trace.encoding||!trace.drawableStarted||trace.drawableCompleted){trace.drawableInvalid=true;return;}
 if(state.finished.load(std::memory_order_relaxed))return;
 trace.drawableEnd=SubmissionTraceNow();trace.drawableCompleted=true;
}


#include "Metal4PreparationTrace.inc"

// BEGIN NATIVE_CPU_PHASE_RUNTIME
namespace {
struct CPUPhaseTraceState {
 const bool enabled=[] {
#ifdef RECOIL_METAL4_LIVE_SCENE
  const auto* value=std::getenv("BAR_NATIVE_FRAME_TRACE");
  return value && value[0]=='1' && value[1]=='\0';
#else
  return false;
#endif
 }();
 std::atomic<bool> finished{false};
 std::mutex mutex;
 NativeSubmissionTraceBudget budget;
};
CPUPhaseTraceState& CPUPhaseTrace() {static CPUPhaseTraceState state;return state;}
struct CPUPhaseSlot {const void* owner=nullptr;NativeCPUPhaseSample sample;};
struct CPUPhaseStack {
 std::array<CPUPhaseSlot,4> slots{};
 std::uint64_t loopOrdinal=0;
 bool captureLoop=false;
 std::uint64_t inheritedParent=0;
 std::array<std::uint64_t,5> previous{};
 unsigned depth=0;
};
thread_local CPUPhaseStack cpuPhaseStack;
} // namespace

NativeCPUPhaseContext CaptureNativeCPUPhaseContext() noexcept {
 const auto& stack=cpuPhaseStack;
 return {stack.depth?stack.slots[stack.depth-1].sample.id:stack.inheritedParent,stack.captureLoop};
}
NativeCPUPhaseContextScope::NativeCPUPhaseContextScope(NativeCPUPhaseContext context) noexcept {
 auto& stack=cpuPhaseStack;
 // A handoff cannot overwrite a locally active scope. Nested renderer calls
 // keep their local ancestry instead of impersonating a different frame.
 if(stack.depth)return;
 previous={stack.inheritedParent,stack.captureLoop};installed=true;
 stack.inheritedParent=context.parent;stack.captureLoop=context.sampled;
}
NativeCPUPhaseContextScope::~NativeCPUPhaseContextScope() {
 if(!installed)return;
 auto& stack=cpuPhaseStack;
 stack.inheritedParent=previous.parent;stack.captureLoop=previous.sampled;
}

bool BeginNativeCPUPhase(const void* owner,unsigned kind,std::int64_t frame) noexcept {
 if(frame<=0)return false; // Loading/pregame loops must not consume the gameplay budget.
 auto& state=CPUPhaseTrace();if(!state.enabled || state.finished.load(std::memory_order_relaxed))return false;
 auto& stack=cpuPhaseStack;
 if(kind==1)stack.captureLoop=false;
 if(!owner || kind==0 || kind>=stack.previous.size() || stack.depth>=stack.slots.size())return false;
 if(kind==1){
  static const bool everyLoop=[] {const char* v=std::getenv("BAR_NATIVE_FRAME_TRACE_EVERY_LOOP");return v&&v[0]=='1'&&v[1]=='\0';}();
  stack.captureLoop=everyLoop || (++stack.loopOrdinal%16==1);
 }
 if(!stack.captureLoop)return false;
 try {
  const auto now=SubmissionTraceNow();std::lock_guard<std::mutex> lock(state.mutex);
  if(!state.budget.TakeSample(now)){state.finished=true;return false;}
  NativeCPUPhaseSample sample;sample.id=state.budget.tickets;sample.kind=kind;sample.frame=frame;
  sample.parent=stack.depth?stack.slots[stack.depth-1].sample.id:stack.inheritedParent;
  sample.start=now;sample.previous=stack.previous[kind];stack.previous[kind]=now;
  stack.slots[stack.depth++]={owner,sample};return true;
 } catch(...) {state.finished=true;return false;}
}
void MarkNativeCPUPhase(const void* owner,unsigned phase) noexcept {
 auto& stack=cpuPhaseStack;
 if(!stack.depth || stack.slots[stack.depth-1].owner!=owner)return;
 auto& sample=stack.slots[stack.depth-1].sample;
 if(phase!=sample.count || phase>=sample.marks.size()){sample.invalid=true;return;}
 sample.marks[sample.count++]=SubmissionTraceNow();
}
void CountNativeCPUPhase(const void* owner,std::uint64_t a,std::uint64_t b) noexcept {
 auto& stack=cpuPhaseStack;
 if(!stack.depth || stack.slots[stack.depth-1].owner!=owner)return;
 auto& sample=stack.slots[stack.depth-1].sample;sample.bytes=a;sample.allocations=b;
}
void FinishNativeCPUPhase(const void* owner) noexcept {
 auto& stack=cpuPhaseStack;
 if(!stack.depth || stack.slots[stack.depth-1].owner!=owner)return;
 const auto sample=stack.slots[--stack.depth].sample;
 if(sample.kind==1)stack.captureLoop=false;
 auto& state=CPUPhaseTrace();if(state.finished.load(std::memory_order_relaxed))return;
 try {
  const auto now=SubmissionTraceNow();char text[768];
  const auto length=FormatNativeCPUPhase(text,sizeof(text),sample,now);
  if(!length)return;
  std::lock_guard<std::mutex> lock(state.mutex);
  if(!state.budget.TakeRecord(now,length+160)){state.finished=true;return;}
#ifdef RECOIL_METAL4_LIVE_SCENE
  LOG("%s",text);
#endif
 } catch(...) {state.finished=true;}
}
// END NATIVE_CPU_PHASE_RUNTIME

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


// The GPU handler writes feedback only. Renderer retirement runs on the caller
// thread under requestMutex; engine recorder acknowledgments run after that lock.
struct RuntimePendingSubmission {
 explicit RuntimePendingSubmission(const std::shared_ptr<RuntimeFrameResources>& frame):
  resources(frame, RuntimeSubmissionResources::DeferredPresentation{}) {}
 RuntimeSubmissionResources resources;
 __strong NSHashTable* retirementTable220=nil;
 __strong M4TerrainUniformSources220* uniformSources220=nil;
 TerrainUniformReceipt220 uniformReceipt220;
 __strong NSError* error = nil;
 dispatch_semaphore_t feedback = dispatch_semaphore_create(0);
 std::function<void(const FrameMetrics&)> retire;
 FrameMetrics metrics;
 std::uint64_t eventValue=0;
 FrameOverlapSample* probe=nullptr;
};

namespace {
bool InterventionTwoInFlight() {
 static const bool enabled=[] {const char* p=std::getenv("BAR_NATIVE_INTERVENTION_TWO_INFLIGHT");return p&&std::string_view(p)=="1";}();return enabled;
}
std::shared_ptr<RuntimeFrameSlot> SelectRecordingSlot158(const std::shared_ptr<RuntimeFrameSlot>& primary,
 const std::shared_ptr<RuntimeFrameResources>& pending) {
 Require(bool(primary),"Missing primary command slot");
 if(!pending)return primary;
 Require(pending->slot==primary||pending->slot==primary->spare,"Pending submission has an unknown command slot");
 return pending->slot==primary&&primary->spare?primary->spare:primary;
}
bool RecordOverlap158() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_RECORD158");return !value||std::string_view(value)=="1";}();
 return enabled;
}
void CreateFrameSlot158(id<MTLDevice> device,RuntimeFrameSlot& slot) {
 slot.allocator=[device newCommandAllocator];slot.command=[device newCommandBuffer];
 NSError* error=nil;auto descriptor=[MTLResidencySetDescriptor new];descriptor.initialCapacity=16;
 descriptor.label=@"Independent Metal4 recording-slot allocations";
 slot.residency=[device newResidencySetWithDescriptor:descriptor error:&error];
 Require(slot.allocator&&slot.command&&slot.residency,"Cannot allocate independent Metal4 recording slot: "+Error(error));
 [slot.residency commit];
}
void ClearFrameSlot158(RuntimeFrameSlot& slot) {
 if(!slot.residency)return;
 [slot.residency removeAllAllocations];[slot.residency commit];slot.residentAllocations=nil;
}
}

Runtime::Runtime(): frameSlot(std::make_shared<RuntimeFrameSlot>())
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
		frameSlot->allocator = [device newCommandAllocator];
		frameSlot->command = [device newCommandBuffer];
		completionEvent = [device newSharedEvent];
		Require(queue != nil && frameSlot->allocator != nil && frameSlot->command != nil && completionEvent != nil, "Cannot create shared direct Metal4 command objects");
		NSError* error = nil;
		MTL4CompilerDescriptor* compilerDescriptor = [MTL4CompilerDescriptor new];
		compiler = [device newCompilerWithDescriptor:compilerDescriptor error:&error];
		Require(compiler != nil, "Cannot create shared MTL4 compiler: " + Error(error));
		MTLResidencySetDescriptor* residencyDescriptor = [MTLResidencySetDescriptor new];
		residencyDescriptor.initialCapacity = 16;
		residencyDescriptor.label = @"Shared direct Metal4 request allocations";
		frameSlot->residency = [device newResidencySetWithDescriptor:residencyDescriptor error:&error];
		Require(frameSlot->residency != nil, "Cannot create shared Metal4 residency: " + Error(error));
		[frameSlot->residency commit];
		// Each command buffer declares only its own retired slot residency.
		if(RecordOverlap158() || InterventionTwoInFlight()) {
			auto spare=std::make_shared<RuntimeFrameSlot>();CreateFrameSlot158(device,*spare);frameSlot->spare=std::move(spare);
   if(InterventionTwoInFlight()){auto third=std::make_shared<RuntimeFrameSlot>();CreateFrameSlot158(device,*third);frameSlot->third=std::move(third);}
		}
		RegisterPublishedRuntime(this);
	}
}

// A command-owned set can outlive the last lease. Clear completed slots before
// releasing its explicit allocation owners. Outstanding GPU work retains its
// original immutable completion graph and keeps the existing poisoned policy.
Runtime::~Runtime()
{
	if (!poisoned) { try { DrainPendingLocked(); } catch (...) {} }
	StopTerrainUniformWorker220(queue);
	StopResidencyWorker220(queue);
	UnregisterPublishedRuntime(this);
	if (!unsafePendingWork && frameSlot->residency != nil) {
		ClearFrameSlot158(*frameSlot);
		if(frameSlot->spare)ClearFrameSlot158(*frameSlot->spare);
  if(frameSlot->third)ClearFrameSlot158(*frameSlot->third);
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
	if (!poisoned) DrainPendingLocked();
	return !unsafePendingWork;
}

RuntimeCleanupLease::RuntimeCleanupLease(Runtime& runtime,Retirement retirement): lock(runtime.requestMutex)
{
 if(retirement==Retirement::RequireAlreadyRetired)
  Require(runtime.pendingSubmissions.empty(),"Renderer control requires main-retired completion receipts");
 else if (!runtime.poisoned) runtime.DrainPendingLocked();
	Require(!runtime.unsafePendingWork, "Cannot retire native resources with outstanding GPU work");
 runtime.persistentResidency.reset();
	if (runtime.capabilities.directMetal4) ClearPublishedPool(&runtime);
	if (!runtime.poisoned) {
		ClearFrameSlot158(*runtime.frameSlot);
		if(runtime.frameSlot->spare)ClearFrameSlot158(*runtime.frameSlot->spare);
  if(runtime.frameSlot->third)ClearFrameSlot158(*runtime.frameSlot->third);
	}
}

void RuntimeLease::SetMinSampleShadingRate(float rate)
{
	Require(!frame->recording && std::isfinite(rate) && rate>=0 && rate<=1,"Original sample shading rate is invalid or changed during encoding");
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
	__strong id<MTLLibrary> library=nil;
	__strong NSError* retainedError=error?*error:nil;
	@autoreleasepool {
		NSError* localError=retainedError;
		library=[Compiler() newLibraryWithDescriptor:descriptor error:error?&localError:nullptr];
		retainedError=localError;
		if(library)RegisterShaderSource(library,descriptor);
	}
	// Publish only after the inner pool drains; both outputs have strong owners.
	if(error)*error=retainedError;
	return library;
}
id<MTLLibrary> RuntimeLease::NewLibraryWithSource(NSString* source,MTLCompileOptions* options,NSError** error)
{
	__strong id<MTLLibrary> library=nil;
	__strong NSError* retainedError=error?*error:nil;
	@autoreleasepool {
		NSError* localError=retainedError;
		library=[Device() newLibraryWithSource:source options:options error:error?&localError:nullptr];
		retainedError=localError;
		if(library)RegisterShaderSource(library,source,options);
	}
	if(error)*error=retainedError;
	return library;
}
id<MTLRenderPipelineState> RuntimeLease::NewRenderPipeline(MTL4RenderPipelineDescriptor* descriptor,NSError** error,bool shadeSamples)
{
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong NSError* retainedError=error?*error:nil;
	@autoreleasepool {
		MTL4RenderPipelineDescriptor* selected=[descriptor copy];
		if(shadeSamples)ConfigureSampleShading(selected);
		NSError* localError=retainedError;
		pipeline=[Compiler() newRenderPipelineStateWithDescriptor:selected compilerTaskOptions:nil error:error?&localError:nullptr];
		retainedError=localError;
	}
	if(error)*error=retainedError;
	return pipeline;
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

RuntimeLease::RuntimeLease(Runtime& selectedRuntime): RuntimeLease({},selectedRuntime) {}
RuntimeLease::RuntimeLease(std::function<void()> previousRetired, Runtime& selectedRuntime): runtime(selectedRuntime), lock(runtime.requestMutex)
{
	Require(runtime.capabilities.directMetal4, "Device does not support MTLGPUFamilyMetal4; no renderer fallback exists");
	Require(runtime.capabilities.unifiedMemory, "Native diagnostic shared-resource contract requires unified memory");
	Require(!runtime.poisoned, "Shared Metal4 runtime is poisoned after an unsafe recording/execution failure; restart required");
	const auto drainProbe91=StartPreparationScope();
	if(!previousRetired)runtime.DrainPendingLocked();
	EndPreparationScope(39,drainProbe91);
 auto selected=InterventionTwoInFlight()?std::shared_ptr<RuntimeFrameSlot>{}:SelectRecordingSlot158(runtime.frameSlot,
  previousRetired&&runtime.pendingSubmission?runtime.pendingSubmission->resources.owner:nullptr);
 if(InterventionTwoInFlight()){
  selected.reset();
  for(const auto& slot:{runtime.frameSlot,runtime.frameSlot->spare,runtime.frameSlot->third})
   if(slot&&!runtime.SlotPending(slot)){selected=slot;break;}
  Require(bool(selected),"Bounded native queue has no retired recording slot");
 }
	frame = std::make_shared<RuntimeFrameResources>(std::move(selected));
 frame->previousRetired=std::move(previousRetired);
	frame->publications.context = &runtime;
	frame->publications.device = runtime.device;
	frame->publications.pool = FindPublishedPool(&runtime);
	BeginSubmissionTraceLease(this);
	frame->requestedAllocations = [NSHashTable hashTableWithOptions:
		NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality];
	frame->retained = [NSMutableArray arrayWithArray:@[runtime.device, runtime.queue, frame->slot->allocator, frame->slot->command,
		runtime.completionEvent, runtime.compiler, frame->slot->residency]];
	// Capability/allocation helpers may open a lease without committing. Their
	// requested allocations never enter the residency set; preserve the last
	// completed, bounded set and its owners until an actual commit replaces it.
	frame->keepResidency = frame->slot->residentAllocations != nil;
}

RuntimeLease::~RuntimeLease()
{
	// Join even on abandoned preparation or an encoder exception before cleanup.
	try { FinishResourceCommit(); } catch (...) { runtime.poisoned=true; }
	if (frame->recording || (frame->pending && !frame->completionTransferred)) runtime.poisoned = true;
	if (runtime.poisoned) runtime.failedFrame = frame;
	if (frame->pending && runtime.poisoned) runtime.unsafePendingWork = true;
 if(runtime.poisoned&&runtime.pendingSubmission&&runtime.pendingSubmission->resources.owner->pending)runtime.unsafePendingWork=true;
	// A poisoned request's allocations must remain resident for late completion.
	// Its immutable completion graph also retains all resources independently.
	if (!runtime.poisoned && !runtime.pendingSubmission && !frame->pending && !frame->keepResidency) {
		[frame->slot->residency removeAllAllocations];
		[frame->slot->residency commit];
		frame->slot->residentAllocations = nil;
	}
	EndSubmissionTraceLease(this);
}

PublishedRequestState& RuntimeLease::Publications() { return frame->publications; }

std::string RuntimeLease::QueueClass() const { return NSStringFromClass([runtime.queue class]).UTF8String; }
std::string RuntimeLease::CommandBufferClass() const { return NSStringFromClass([frame->slot->command class]).UTF8String; }

#include "Metal4MemoryLedger.inc"

void RuntimeLease::AddAllocation(id<MTLAllocation> allocation)
{
	Require(allocation != nil && !frame->recording && !frame->pending && !frame->residencyCommit.Active(), "Cannot add invalid allocation during Metal4 execution");
	// Only stage this lease's request. CommitResources invalidates retention
	// before it mutates the shared set, including its exception cleanup path.
	++frame->allocationRequests148;
 if(runtime.persistentResidency&&runtime.persistentResidency->Contains(allocation))return;
	// The strong, pointer-identity request set already owns the exact object.
	// Repeated consumers cannot add residency or extend its request lifetime.
	// Keep the execution guards above this lookup, even for an existing object.
	if ([frame->requestedAllocations containsObject:allocation]) return;
	frame->AddUniqueRequestedAllocation(allocation);
	Retain(allocation);
	TrackResourceMemory(allocation);
}

void RuntimeLease::AddPersistentAllocation(id<MTLAllocation> allocation)
{
 Require(allocation!=nil&&!frame->recording&&!frame->pending&&!frame->residencyCommit.Active(),"Cannot add persistent allocation during Metal4 execution");
 if(PersistentResidencyInterventionEnabled()){
  if(!runtime.persistentResidency)runtime.persistentResidency=std::make_unique<InterventionPersistentResidency>(Device());
  if(runtime.persistentResidency->TryAdd(allocation)){++frame->allocationRequests148;TrackResourceMemory(allocation);return;}
 }
 AddAllocation(allocation);
}

void RuntimeLease::PoisonAfterBegin() noexcept
{
	runtime.poisoned=true;runtime.failedFrame=frame;
}

void RuntimeLease::RetainTerrainUniformSources220(
 const std::shared_ptr<const NativeWaterGroundSourceFrame>& main,
 const std::shared_ptr<const NativeWaterGroundSourceFrame>& reflection)
{
 if(!TerrainUniformReleaseEnabled220()&&!TerrainUniformReleaseCounters220())return;
 if(!main&&!reflection)return;
 auto holder=[M4TerrainUniformSources220 new];if(!holder)return;
 holder->sources={main,reflection};
 // Caller-only ownership attached to the actual immutable graph. A copied
 // prefix graph receives no receipt and cannot trigger asynchronous release.
 objc_setAssociatedObject(frame->WritableRetained(),&terrainUniformSourcesKey220,holder,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void RuntimeLease::Retain(id object)
{
	Require(object != nil && !frame->recording && !frame->pending && !frame->residencyCommit.Active(), "Cannot retain invalid native object during Metal4 execution");
	[frame->WritableRetained() addObject:object];
}

bool RuntimeLease::RetainPostprocessTexture181(const std::shared_ptr<PostprocessTextureStorage181>& owner)
{
	// Preserve the mutation guards even when this request already owns the slot.
	Require(owner && !frame->recording && !frame->pending && !frame->residencyCommit.Active(), "Cannot retain postprocess storage during Metal4 execution");
	auto& owners=frame->postprocessTextureOwners181;
	std::size_t checked=0;
	for(const auto& held:owners) {
		// The physical cache has at most32 slots. Oversized/uncached requests
		// can exceed that: retain them conservatively instead of growing scans.
		if(checked++==32) break;
		// Shared ownership identity also handles aliases without confusing two
		// different control blocks which happen to expose the same raw address.
		if(!held.owner_before(owner) && !owner.owner_before(held)) return false;
	}
	owners.push_back(owner);return true;
}

id<MTLDepthStencilState> RuntimeLease::CachedDepthState(MTLDepthStencilDescriptor* descriptor)
{
 Require(!frame->recording&&!frame->pending&&!runtime.poisoned&&!frame->residencyCommit.Active(),"Cannot prepare depth state during Metal4 execution");
 auto state=frame->slot->depthStates.Acquire(Device(),descriptor);Retain(state);return state;
}

namespace { std::atomic<unsigned> residencyCensusSamples{0}; }

bool RuntimeLease::PreparingWhilePending() const {return bool(frame->previousRetired)&&bool(runtime.pendingSubmission);}
bool RuntimeLease::OrderAfterPreviousSubmission(const void* owner,std::function<void()> check)
{
 Require(owner&&bool(check)&&!frame->recording&&!frame->pending&&!runtime.poisoned,
  "Prior producer ordering requires owned preparation");
 if(!PreparingWhilePending()||frame->slot==runtime.pendingSubmission->resources.owner->slot)return false;
 frame->priorReadChecks.try_emplace(owner,std::move(check));return true;
}
void RuntimeLease::RetirePreviousSubmission()
{
 RetirePreviousSubmission(nullptr);
}
void RuntimeLease::RetirePreviousSubmission(std::shared_ptr<RuntimePendingSubmission>* completedOwner)
{
 try {
 const auto started=StartPreparationScope();
 runtime.DrainPendingLocked(completedOwner);
 EndPreparationScope(39,started);
 if(frame->previousRetired){auto receipt=std::exchange(frame->previousRetired,{});receipt();}
 auto checks=std::move(frame->priorReadChecks);frame->priorReadChecks.clear();
 for(const auto& [owner,check]:checks)check();
 } catch(...) {runtime.poisoned=true;runtime.failedFrame=frame;throw;}
}
static bool ResidencyDiffEnabled220(){static const bool value=[](){const char* p=std::getenv("BAR_NATIVE_RESIDENCY_DIFF220");return p&&std::string_view(p)=="1";}();return value;}
static bool ResidencyDiffCounters220(){static const bool value=[](){const char* p=std::getenv("BAR_NATIVE_RESIDENCY_DIFF220_COUNTERS");return p&&std::string_view(p)=="1";}();return value;}
void RuntimeLease::CommitResources() { StageResources(false); }
void RuntimeLease::CommitResourcesWhileEncoding() { StageResources(true); }
void RuntimeLease::StageResources(bool overlapEncoding)
{
 if(!runtime.pendingSubmission||runtime.SlotPending(frame->slot))RetirePreviousSubmission();
	Require(!frame->recording && !frame->pending && !runtime.poisoned && !frame->residencyCommit.Active(), "Cannot update residency during Metal4 execution");
 if(runtime.persistentResidency){auto snapshot=runtime.persistentResidency->SnapshotForRecording();if(snapshot)Retain(snapshot);}
	frame->keepResidency = false;
	const bool census = cpuPhaseStack.depth && cpuPhaseStack.captureLoop && !CPUPhaseTrace().finished.load(std::memory_order_relaxed) && residencyCensusSamples.load(std::memory_order_relaxed)<128;
	if(census)residencyCensusSamples.fetch_add(1,std::memory_order_relaxed);
	const auto censusStart = census ? SubmissionTraceNow() : 0;
	const auto previousCount = census ? frame->slot->residentAllocations.count : 0;
	uint64_t added=0,removed=0,addedBytes=0,removedBytes=0,textureAdds=0,textureBytes=0,bufferAdds=0,bufferBytes=0;
	id<MTLAllocation> largest[4]={nil,nil,nil,nil};
	uint64_t largestBytes[4]={0,0,0,0};
	// Take ownership before touching staged membership. The original Retain
	// graph also keeps every requested allocation through completion/poison.
	const auto prepCopyStart=StartPreparationScope();
	NSHashTable* next = frame->PublishRequestedAllocations();
	EndPreparationScope(22,prepCopyStart);
 // The exact original synchronous/census and flag-off paths remain below.
 // Publish all caller metadata before dispatch: no table/set reads while active.
 if(overlapEncoding&&!census&&ResidencyDiffEnabled220()){
  const auto ownerStart=StartPreparationScope();
  NSHashTable* old=frame->slot->residentAllocations;
  const auto requested=next.count;
  if(old)DeferCompletedResidencyOwners(frame->WritableRetained(),old,next,runtime.queue);
  InvalidateResidencyTable220(next);
  EndPreparationScope(25,ownerStart);CountPreparationTrace(6,requested);
  frame->residencyCommit.StartDiff220(frame->slot->residencyWorker,frame->slot->residency,
   old,next,requested,StartPreparationScope()!=0||ResidencyDiffCounters220());
  // Admission can allocate/throw before dispatch. Publish only after success;
  // this pointer assignment does not read either immutable table or the set.
  frame->slot->residentAllocations=next;
  return;
 }
	const auto prepDiffStart=StartPreparationScope();
	for (id<MTLAllocation> allocation in frame->slot->residentAllocations) {
		if (![next containsObject:allocation]) {
			if(census){++removed;removedBytes+=allocation.allocatedSize;}
			[frame->slot->residency removeAllocation:allocation];
		}
	}
	for (id<MTLAllocation> allocation in next) {
		if (![frame->slot->residentAllocations containsObject:allocation]) {
			if(census){
				++added;const uint64_t bytes=allocation.allocatedSize;addedBytes+=bytes;
				if([allocation conformsToProtocol:@protocol(MTLTexture)]){++textureAdds;textureBytes+=bytes;}
				else if([allocation conformsToProtocol:@protocol(MTLBuffer)]){++bufferAdds;bufferBytes+=bytes;}
				for(unsigned i=0;i<4;++i)if(bytes>largestBytes[i]){for(unsigned j=3;j>i;--j){largest[j]=largest[j-1];largestBytes[j]=largestBytes[j-1];}largest[i]=allocation;largestBytes[i]=bytes;break;}
			}
			[frame->slot->residency addAllocation:allocation];
		}
	}
	EndPreparationScope(23,prepDiffStart);
	// Legacy allocation census remains synchronous so its per-resource timing
	// keeps its old meaning. The bounded resource probe measures this async path.
	if(overlapEncoding && !census) {
		const auto ownerStart=StartPreparationScope();
		if (frame->slot->residentAllocations) DeferCompletedResidencyOwners(frame->WritableRetained(),frame->slot->residentAllocations,next,runtime.queue);
		InvalidateResidencyTable220(next);
		frame->slot->residentAllocations=next;
		EndPreparationScope(25,ownerStart);CountPreparationTrace(6,next.count);
		frame->residencyCommit.Start(frame->slot->residencyWorker,frame->slot->residency,StartPreparationScope()!=0);
		return;
	}
	const auto prepCommitStart=StartPreparationScope();
	const auto censusBeforeCommit=census?SubmissionTraceNow():0;
	[frame->slot->residency commit];
	const auto censusAfterCommit=census?SubmissionTraceNow():0;
	EndPreparationScope(24,prepCommitStart);
	const auto prepOwnerStart=StartPreparationScope();
	if (frame->slot->residentAllocations) DeferCompletedResidencyOwners(frame->WritableRetained(),frame->slot->residentAllocations,next,runtime.queue);
	InvalidateResidencyTable220(next);
	frame->slot->residentAllocations = next;
	EndPreparationScope(25,prepOwnerStart);
	CountPreparationTrace(6,next.count);
	const auto censusAfterOwners=census?SubmissionTraceNow():0;
	// Bound additional between-lease residency; larger requests keep the
	// original completed-lease clear behavior, with no omitted allocation.
	frame->keepResidency = next.count <= 4096 && frame->slot->residency.allocatedSize <= (uint64_t{2} << 30);
	if(cpuPhaseStack.depth)SnapshotResourceMemory(Device(),cpuPhaseStack.slots[cpuPhaseStack.depth-1].sample.frame,frame->slot->allocator);
#ifdef RECOIL_METAL4_LIVE_SCENE
	if(census){
		const auto parent=cpuPhaseStack.slots[cpuPhaseStack.depth-1].sample.id;
		LOG("[NativeResidencyChurn] parent=%llu previous=%llu requested=%llu added=%llu removed=%llu added_bytes=%llu removed_bytes=%llu texture_adds=%llu texture_bytes=%llu buffer_adds=%llu buffer_bytes=%llu diff_with_probe_us=%.3f commit_us=%.3f release_old_owners_us=%.3f keep=%d",
			(unsigned long long)parent,(unsigned long long)previousCount,(unsigned long long)next.count,(unsigned long long)added,(unsigned long long)removed,(unsigned long long)addedBytes,(unsigned long long)removedBytes,
			(unsigned long long)textureAdds,(unsigned long long)textureBytes,(unsigned long long)bufferAdds,(unsigned long long)bufferBytes,
			double(censusBeforeCommit-censusStart)/1000.0,double(censusAfterCommit-censusBeforeCommit)/1000.0,double(censusAfterOwners-censusAfterCommit)/1000.0,int(frame->keepResidency));
		for(unsigned i=0;i<4;++i)if(largest[i]){
			const bool texture=[largest[i] conformsToProtocol:@protocol(MTLTexture)];
			id<MTLTexture> image=texture?(id<MTLTexture>)largest[i]:nil;
			NSString* label=[largest[i] respondsToSelector:@selector(label)]?[(id<MTLResource>)largest[i] label]:nil;if(label.length>160)label=[label substringToIndex:160];
			LOG("[NativeResidencyLargest] parent=%llu rank=%u bytes=%llu texture=%d width=%llu height=%llu format=%llu label=%s",
				(unsigned long long)parent,i,(unsigned long long)largestBytes[i],int(texture),(unsigned long long)image.width,(unsigned long long)image.height,(unsigned long long)image.pixelFormat,label.UTF8String?:"");
		}
	}
#endif
}
void RuntimeLease::FinishResourceCommit()
{
	if(!frame->residencyCommit.Active())return;
	try {
		const auto timing=frame->residencyCommit.Finish();
		CountPreparationTrace(42,timing.commitNs);CountPreparationTrace(43,timing.joinNs);
		CountPreparationTrace(44,timing.dispatchNs);CountPreparationTrace(45,1);
#ifdef RECOIL_METAL4_LIVE_SCENE
  if(timing.workerDiff220&&ResidencyDiffCounters220()){
   static std::atomic<unsigned> receipts{0};
   const auto receipt=receipts.fetch_add(1,std::memory_order_relaxed)+1;
   if(receipt<=32)LOG("[NativeResidencyDiff220] worker_diff=1 receipt=%u requested=%llu added=%llu removed=%llu diff_ns=%llu commit_ns=%llu join_ns=%llu dispatch_ns=%llu",
    receipt,(unsigned long long)timing.requested,(unsigned long long)timing.added,(unsigned long long)timing.removed,
    (unsigned long long)timing.diffNs,(unsigned long long)timing.commitNs,(unsigned long long)timing.joinNs,(unsigned long long)timing.dispatchNs);
  }
#endif
		frame->keepResidency=frame->slot->residentAllocations.count<=4096 && frame->slot->residency.allocatedSize<=(uint64_t{2}<<30);
		if(cpuPhaseStack.depth)SnapshotResourceMemory(Device(),cpuPhaseStack.slots[cpuPhaseStack.depth-1].sample.frame,frame->slot->allocator);
	} catch (...) { runtime.poisoned=true;runtime.failedFrame=frame;throw; }
}
uint64_t RuntimeLease::ResidentBytes() const {
	Require(!frame->residencyCommit.Active(),"Residency size requires completed registration");
	return frame->slot->residency.allocatedSize+(runtime.persistentResidency?runtime.persistentResidency->Bytes():0);
}
uint32_t RuntimeLease::AllocationCount() const {
	Require(!frame->residencyCommit.Active(),"Residency count requires completed registration");
	return uint32_t(frame->slot->residency.allocationCount+(runtime.persistentResidency?runtime.persistentResidency->Count():0));
}

id<MTL4CommandBuffer> RuntimeLease::Begin(const std::string& label)
{
	Require(!frame->recording && !frame->pending && !runtime.poisoned &&
  (!runtime.pendingSubmission||(frame->previousRetired&&!runtime.SlotPending(frame->slot))),
  "Metal4 command memory is not independently retired for recording");
 frame->recordedWhilePending=bool(runtime.pendingSubmission);
	BeginSubmissionTraceEncoding(this);
	frame->slot->command.label = String(label);
	[frame->slot->command beginCommandBufferWithAllocator:frame->slot->allocator];
	frame->recording = true;
 if(runtime.persistentResidency)runtime.persistentResidency->Bind(frame->slot->command);
	return frame->slot->command;
}

namespace {
void ReportAllocationPreparation148(const RuntimeFrameResources& frame,uint64_t frameIndex) {
#ifdef RECOIL_METAL4_LIVE_SCENE
 static std::atomic<bool> overlapReported{false};
 if(!InterventionTwoInFlight()&&frameIndex>=300&&frame.recordedWhilePending&&!overlapReported.exchange(true,std::memory_order_relaxed))
  LOG("[NativeRecord158] frame=%llu pending_at_begin=1 independent_command_slot=1 gpu_pending_limit=1 prior_retired_before_submit=1",(unsigned long long)frameIndex);
 static std::atomic<bool> reported{false};
 if(frameIndex<300 || frame.requestedAllocations.count<256 || reported.load(std::memory_order_relaxed))return;
 if(reported.exchange(true,std::memory_order_relaxed))return;
 const auto unique=frame.requestedAllocations.count;
 LOG("[NativeAllocation148] frame=%llu requests=%llu unique=%llu omitted_owner_appends=%llu completion_graph_entries=%llu",
  (unsigned long long)frameIndex,(unsigned long long)frame.allocationRequests148,(unsigned long long)unique,
  (unsigned long long)(frame.allocationRequests148-unique),(unsigned long long)frame.retained.count);
#endif
}
}

void RuntimeLease::SubmitAndDefer(Clock::time_point preparationStart, uint64_t frameIndex,
 id<MTLDrawable> drawable, std::function<void(const FrameMetrics&)> retire)
{
 FinishResourceCommit();
 // N+1 is already prepared and recorded. Complete N's event, feedback,
 // allocator, producer receipts and read checks at the original boundary;
 // retain only its completed owner graph until N+1 has been submitted.
 // Early preparation drains still release immediately for pool reuse.
 std::shared_ptr<RuntimePendingSubmission> completedPredecessor186;
 const bool independentQueue=InterventionTwoInFlight()&&frame->priorReadChecks.empty();
 if(independentQueue){
  // Bound GPU occupancy; do not invoke producer checks before their retirement.
  while(runtime.pendingSubmissions.size()>=2)runtime.DrainOldestLocked(&completedPredecessor186);
  // Scene receipts are delivered only for genuinely completed frames.
  if(frame->previousRetired){auto receipt=std::exchange(frame->previousRetired,{});receipt();}
 }else RetirePreviousSubmission(&completedPredecessor186);
 Require(frame->recording && !frame->pending && (!runtime.pendingSubmission||independentQueue) && bool(retire) && drawable!=nil,
  "Deferred Metal4 submission requires one recording presentation and an owned retirement");
 Require(runtime.nextEventValue != std::numeric_limits<uint64_t>::max(), "Metal4 completion event exhausted its monotonic value range");
 [frame->WritableRetained() addObject:drawable];
 ReportAllocationPreparation148(*frame,frameIndex);
 auto pending=std::make_shared<RuntimePendingSubmission>(frame);
#ifdef RECOIL_METAL4_LIVE_SCENE
 static std::atomic<bool> reported175{false};
 if (frameIndex>=300 && !reported175.load(std::memory_order_relaxed) &&
     frame->requestedAllocations.count>=256 && !reported175.exchange(true,std::memory_order_relaxed)) {
  LOG("[NativeSubmission175] index=%llu request_entries=%llu request_table_shared=%u graph_entries=%llu deferred_graph_shared=%u",
   (unsigned long long)frameIndex,(unsigned long long)frame->requestedAllocations.count,
   unsigned(frame->requestedAllocations==frame->slot->residentAllocations),
   (unsigned long long)pending->resources.retained.count,unsigned(pending->resources.retained==frame->retained));
 }
#endif
 pending->retire=std::move(retire);pending->metrics.frameIndex=frameIndex;
 pending->probe=BeginFrameOverlapProbe(frameIndex);
 if constexpr(NativeCPUTimingInstrumentation)pending->metrics.cpuCommandPrepMs=Milliseconds(preparationStart);
 MTL4CommitOptions* options=[MTL4CommitOptions new];
 [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback){
  (void)pending->resources.retained.count;
  pending->error=feedback.error;
  if(pending->probe){pending->probe->gpuStart=feedback.GPUStartTime;pending->probe->gpuEnd=feedback.GPUEndTime;}
  dispatch_semaphore_signal(pending->feedback);
 }];
 [frame->slot->command useResidencySet:frame->slot->residency];
 [frame->slot->command endCommandBuffer];frame->recording=false;
 pending->eventValue=++runtime.nextEventValue;
 pending->uniformSources220=TerrainUniformSources220(pending->resources.retained);
 pending->uniformReceipt220=StampTerrainUniform220(runtime.queue,pending->uniformSources220,pending->eventValue);
 if(ResidencyReleaseEnabled220()||ResidencyReleaseCounters220()){
  // Receipt pins exactly the table bound to this slot, never a requested-only table.
  if(frame->requestedAllocations==frame->slot->residentAllocations){pending->retirementTable220=frame->slot->residentAllocations;StampResidencyTable220(runtime.queue,pending->retirementTable220,pending->eventValue);}
  else {InvalidateResidencyTable220(frame->slot->residentAllocations);InvalidateResidencyTable220(frame->requestedAllocations);}
 }
 // Publish the quarantine owner before committing anything to the GPU.
 runtime.pendingSubmissions.push_back(pending);runtime.pendingSubmission=pending;frame->pending=true;
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(InterventionTwoInFlight()){
  static bool reportedTwo=false;
  if(runtime.pendingSubmissions.size()==2&&!reportedTwo){reportedTwo=true;
   LOG("[NativeInterventionTwoInFlight] queued_tickets=2 slots=3 prior_read_checks=0 oldest_event_and_feedback_retirement=1");}
 }
#endif
 id<MTL4CommandBuffer> commands[]={frame->slot->command};
 const auto submitted=Clock::now();
 if(pending->probe)pending->probe->submit=FrameOverlapHostTime();
 [runtime.queue waitForDrawable:drawable];
 [runtime.queue commit:commands count:1 options:options];
 [runtime.queue signalDrawable:drawable];[drawable present];
 [runtime.queue signalEvent:runtime.completionEvent value:pending->eventValue];
 if constexpr(NativeCPUTimingInstrumentation)pending->metrics.cpuSubmissionMs=Milliseconds(submitted);
 // Release on this same serialized caller, before another preparation can
 // acquire pool slots. No extra pending GPU frame or deferred cleanup queue.
#ifdef RECOIL_METAL4_LIVE_SCENE
 static std::atomic<bool> reported186{false};
 const bool report186=!InterventionTwoInFlight() && completedPredecessor186 && frameIndex>=600 &&
  !reported186.exchange(true,std::memory_order_relaxed);
 const auto graphEntries186=report186?completedPredecessor186->resources.retained.count:0;
 const auto ownerReferences186=report186?completedPredecessor186.use_count():0;
 const auto releaseStart186=report186?SubmissionTraceNow():0;
#endif
 // Capture only proven pure-CPU leaves immediately before the original
 // caller-side graph destruction, after the successor has been committed.
 TerrainUniformAttempt220 uniformAttempt220;
 __strong M4TerrainUniformJob220* uniformJob220=nil;
 if(completedPredecessor186)uniformJob220=PrepareTerrainUniformRelease220(runtime.queue,
  completedPredecessor186->uniformSources220,completedPredecessor186->uniformReceipt220,
  completedPredecessor186->eventValue,frameIndex,uniformAttempt220);
 completedPredecessor186.reset();
 DispatchTerrainUniformRelease220(runtime.queue,uniformJob220,uniformAttempt220);
 uniformJob220=nil;
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(report186)LOG("[NativeRetirement186] submission=%llu completed_graph_entries=%llu owner_references_at_release=%ld release_ns=%llu release_after_submit=1 same_thread=1 pending_gpu_limit=1 early_drains_unchanged=1 gain=unknown",
  (unsigned long long)frameIndex,(unsigned long long)graphEntries186,
  ownerReferences186,(unsigned long long)(SubmissionTraceNow()-releaseStart186));
#endif
 if(ResidencyReleaseEnabled220()||ResidencyReleaseCounters220())ReleaseCompletedResidencyOwners220(frame->retained,runtime.queue,frameIndex);
 else ReleaseCompletedResidencyOwners(frame->retained);
 // Only this successful transfer permits a pending lease to end without poison.
 frame->completionTransferred=true;
 if(pending->probe)pending->probe->returned=FrameOverlapHostTime();
 // The existing synchronous trace cannot represent a CPU-update gap as a wait.
 EndSubmissionTraceLease(this);
}

void Runtime::DrainPendingLocked()
{
 DrainPendingLocked(nullptr);
}
void Runtime::DrainPendingLocked(std::shared_ptr<RuntimePendingSubmission>* completedOwner)
{
 while(!pendingSubmissions.empty())DrainOldestLocked(completedOwner);
}
bool Runtime::SlotPending(const std::shared_ptr<RuntimeFrameSlot>& slot) const
{
 for(const auto& pending:pendingSubmissions)if(pending->resources.owner->slot==slot)return true;
 return false;
}
void Runtime::DrainOldestLocked(std::shared_ptr<RuntimePendingSubmission>* completedOwner)
{
 if(pendingSubmissions.empty())return;
 Require(!poisoned,"Cannot retire a poisoned deferred Metal4 request");
 auto pending=pendingSubmissions.front();auto frame=pending->resources.owner;
 try {
  const auto start=Clock::now();
  if(pending->probe){pending->probe->drain=FrameOverlapHostTime();pending->probe->ready=completionEvent.signaledValue>=pending->eventValue;}
  Require([completionEvent waitUntilSignaledValue:pending->eventValue timeoutMS:30000],"GPU completion shared-event timeout (30 seconds)");
  Require(dispatch_semaphore_wait(pending->feedback,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC))==0,"MTL4 commit feedback timeout (30 seconds)");
  if constexpr(NativeCPUTimingInstrumentation)pending->metrics.cpuCompletionWaitMs=Milliseconds(start);
  frame->pending=false;
  if(pending->probe){pending->probe->done=FrameOverlapHostTime();pending->probe->complete.store(true,std::memory_order_release);}
  if(pending->error!=nil) {
   NSString* details=pending->error.userInfo.description;
   if(details.length>4096)details=[details substringToIndex:4096];
   throw std::runtime_error("GPU reported deferred failure: "+Error(pending->error)+"; userInfo="+(details.UTF8String?:""));
  }
  [frame->slot->allocator reset];
  // Same completion order as the synchronous renderer; no acknowledgment on
  // submission, event timeout, feedback timeout, or GPU execution failure.
  pending->retire(pending->metrics);
  if(!frame->keepResidency){[frame->slot->residency removeAllAllocations];[frame->slot->residency commit];frame->slot->residentAllocations=nil;}
  // Publish only after every successful retirement action. On failure the
  // original pending owner remains quarantined, with no completion handoff.
  CompleteResidencyTable220(queue,pending->retirementTable220,pending->eventValue);
  CompleteTerrainUniform220(queue,pending->uniformReceipt220,pending->eventValue);
  if(completedOwner)*completedOwner=pending;
  pendingSubmissions.pop_front();
  pendingSubmission=pendingSubmissions.empty()?nullptr:pendingSubmissions.back();
 } catch(...) {
  poisoned=true;failedFrame=frame;unsafePendingWork=frame->pending;
  for(const auto& ticket:pendingSubmissions)unsafePendingWork=unsafePendingWork||ticket->resources.owner->pending;
  // Keep pendingSubmission and its renderer owners quarantined as well.
  throw;
 }
}

FrameMetrics RuntimeLease::SubmitAndWait(Clock::time_point preparationStart, uint64_t frameIndex, id<MTLDrawable> drawable)
{
	FinishResourceCommit();
	RetirePreviousSubmission();
	Require(frame->recording && !frame->pending && !runtime.pendingSubmission && !runtime.poisoned, "Metal4 submission requires one recording command buffer and retired predecessor");
	Require(runtime.nextEventValue != std::numeric_limits<uint64_t>::max(), "Metal4 completion event exhausted its monotonic value range");
	const auto trace=BeginSubmissionTraceSubmit(this);
	[frame->slot->command useResidencySet:frame->slot->residency];
 [frame->slot->command endCommandBuffer];
	frame->recording = false;
	FrameMetrics metrics;
	metrics.frameIndex = frameIndex;
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuCommandPrepMs = Milliseconds(preparationStart);
	struct Feedback {
		explicit Feedback(const std::shared_ptr<RuntimeFrameResources>& owner): resources(owner) {}
		RuntimeSubmissionResources resources;
		__strong NSError* error = nil;
		double start = 0;
		double end = 0;
	};
	if (drawable != nil) [frame->WritableRetained() addObject:drawable];
	ReportAllocationPreparation148(*frame,frameIndex);
	const auto feedback = std::make_shared<Feedback>(frame);
	dispatch_semaphore_t feedbackSemaphore = dispatch_semaphore_create(0);
	MTL4CommitOptions* options = [MTL4CommitOptions new];
	// Capture only a deadline in the asynchronous block, not all CPU timestamps.
	const auto traceGPUDeadline=trace.active?trace.deadline:0;
	[options addFeedbackHandler:^(id<MTL4CommitFeedback> result) {
		// The callback owns both native objects and the C++ pool write-exclusion
		// pins, even if a timeout unwinds the submitting lease.
		(void)feedback->resources.retained.count;
		feedback->error = result.error;
		if constexpr (NativeGPUTimingInstrumentation) {
			feedback->start = result.GPUStartTime;
			feedback->end = result.GPUEndTime;
		} else if (SubmissionTraceWantsGPU({traceGPUDeadline!=0,traceGPUDeadline})) {
			feedback->start = result.GPUStartTime;
			feedback->end = result.GPUEndTime;
		}
		dispatch_semaphore_signal(feedbackSemaphore);
	}];
	InvalidateResidencyTable220(frame->slot->residentAllocations);
	InvalidateResidencyTable220(frame->requestedAllocations);
	const uint64_t eventValue = ++runtime.nextEventValue;
	frame->pending = true;
	const auto submissionStart = Clock::now();
	id<MTL4CommandBuffer> commands[] = {frame->slot->command};
	if (drawable != nil) [runtime.queue waitForDrawable:drawable];
	[runtime.queue commit:commands count:1 options:options];
	if (drawable != nil) {
		[runtime.queue signalDrawable:drawable];
		[drawable present];
	}
	[runtime.queue signalEvent:runtime.completionEvent value:eventValue];
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuSubmissionMs = Milliseconds(submissionStart);
	const auto traceSubmitted=SubmissionTraceStamp(trace);
	// Only the already-completed previous request loses owners here. The
	// current request keeps its next-table and immutable feedback graph.
	const auto releaseStart=StartPreparationScope();
	ReleaseCompletedResidencyOwners(frame->retained);
	EndPreparationScope(26,releaseStart);
	const auto traceCleanupDone=SubmissionTraceStamp(trace);
	const auto waitStart = Clock::now();
	Require([runtime.completionEvent waitUntilSignaledValue:eventValue timeoutMS:30000], "GPU completion shared-event timeout (30 seconds)");
	const auto traceEventDone=SubmissionTraceStamp(trace);
	Require(dispatch_semaphore_wait(feedbackSemaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC)) == 0,
		"MTL4 commit feedback timeout (30 seconds)");
	const auto traceFeedbackDone=SubmissionTraceStamp(trace);
	if constexpr (NativeCPUTimingInstrumentation) metrics.cpuCompletionWaitMs = Milliseconds(waitStart);
	// Both completion mechanisms have retired the submission, even when its
	// feedback reports an error. Reuse is poisoned on error; teardown is safe.
	frame->pending = false;
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
			"; command=" + text(frame->slot->command.label) +
			"; domain=" + text(feedback->error.domain) +
			"; code=" + std::to_string(static_cast<long long>(feedback->error.code)) +
			"; userInfo=" + text(details));
	}
	if constexpr (NativeGPUTimingInstrumentation) {
		if (std::isfinite(feedback->start) && std::isfinite(feedback->end) && feedback->start > 0 && feedback->end >= feedback->start)
			metrics.gpuExecutionMs = (feedback->end - feedback->start) * 1000.0;
	}
	// Both the queue event and feedback completed before command memory reuse.
	[frame->slot->allocator reset];
	FinishSubmissionTrace(trace,traceSubmitted,traceEventDone,traceFeedbackDone,eventValue,frameIndex,
		drawable!=nil,feedback->start,feedback->end,traceCleanupDone);
	RestartSubmissionTraceRequest(this);
	return metrics;
}

} // namespace rendering::metal4

namespace rendering {
DeviceCapabilities QueryMetal4Capabilities()
{
	@autoreleasepool { return metal4::SharedRuntime().Capabilities(); }
}
} // namespace rendering
