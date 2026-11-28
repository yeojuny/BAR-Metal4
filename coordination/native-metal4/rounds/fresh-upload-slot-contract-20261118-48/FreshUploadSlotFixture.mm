
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <dispatch/dispatch.h>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool b,const char* message) {
    if(!b) throw std::runtime_error(message);
}

// Only standalone injection. All actual buffers and argument tables are MTL4.
@interface FaultDevice : NSObject
@property(nonatomic,strong) id<MTLDevice> real;
@property(nonatomic) NSUInteger maxBufferLength;
@property(nonatomic) BOOL rejectSlab;
@property(nonatomic) unsigned slabAttempts;
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)size options:(MTLResourceOptions)options;
- (id<MTLBuffer>)newBufferWithBytes:(const void*)data length:(NSUInteger)size options:(MTLResourceOptions)options;
@end
@implementation FaultDevice
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)size options:(MTLResourceOptions)opts {
    ++_slabAttempts;
    if(_rejectSlab) return nil;
    return [_real newBufferWithLength:size options:opts];
}
- (id<MTLBuffer>)newBufferWithBytes:(const void*)data length:(NSUInteger)size options:(MTLResourceOptions)opts {
    return [_real newBufferWithBytes:data length:size options:opts];
}
@end
struct RuntimeLease {
    __strong FaultDevice* device;
    __strong id<MTLResidencySet> residency;
    std::vector<id<MTLBuffer>> allocations;
    FaultDevice* Device() {return device;}
    void AddAllocation(id<MTLBuffer> buffer) {
        Require(buffer!=nil,"Fixture must own an actual allocation");
        allocations.push_back(buffer);
        [residency addAllocation:buffer];
    }
};
struct NativeSensorLOSInstanceValues {std::vector<std::array<float,4>> positions,colors;};
struct alignas(16) CircleUniform {
	std::array<float,16> cameraViewProjection{},cameraViewInverse{},cameraViewProjectionInverse{};
	std::array<float,4> timeInfo{},viewGeometry{},mapSize{},rangeColor{},viewport{},fragmentToLogical{},values{},parameters{};
	std::array<uint32_t,4> options{},counts{};
	std::array<float,4> distanceFade{};
};
static_assert(sizeof(CircleUniform)==368);
id<MTLBuffer> UploadCircle(RuntimeLease& lease,const void* bytes,size_t size)
{
	Require(bytes&&size&&size<=lease.Device().maxBufferLength,"Current original sensor buffer exceeds actual device bounds");
	auto buffer=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate current original sensor immutable bytes");lease.AddAllocation(buffer);return buffer;
}
bool SensorLOSOutlineUploadSlabEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("BAR_NATIVE_SENSORLOS_OUTLINE_UPLOAD_SLAB");
		return value && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

bool TryUploadCircleOutline(RuntimeLease& lease, const NativeSensorLOSInstanceValues& instances,
	const CircleUniform& uniform, std::array<MTLGPUAddress,3>& addresses)
{
	// Follow the engine's 256-byte constant-block slice alignment.
	constexpr size_t alignment = 256;
	const std::array slices{
		std::as_bytes(std::span(instances.positions)),
		std::as_bytes(std::span(&uniform,1)),
		std::as_bytes(std::span(instances.colors)),
	};
	const size_t limit = lease.Device().maxBufferLength;
	std::array<size_t,slices.size()> offsets{};
	size_t length = 0;
	for (size_t i = 0; i < slices.size(); ++i) {
		const size_t padding = (alignment - length % alignment) % alignment;
		if (slices[i].empty() || padding > limit - length || slices[i].size() > limit - length - padding)
			return false;
		offsets[i] = length + padding;
		length = offsets[i] + slices[i].size();
	}
	auto slab = [lease.Device() newBufferWithLength:length options:MTLResourceStorageModeShared];
	const MTLGPUAddress base = slab.gpuAddress;
	auto* bytes = static_cast<std::byte*>(slab.contents);
	if (!bytes || slab.length != length || !base || base % alignment ||
		length > std::numeric_limits<MTLGPUAddress>::max() - base)
		return false;

	for (size_t i = 0; i < slices.size(); ++i) {
		std::memcpy(bytes + offsets[i], slices[i].data(), slices[i].size());
		addresses[i] = base + offsets[i];
	}
	// The lease strongly owns and makes the allocation resident until retirement.
	lease.AddAllocation(slab);
	return true;
}


using FreshUploadBytes = std::span<const std::byte>;

template <size_t Count>
struct FreshUploadLayout {
	std::array<size_t, Count> offsets{};
	size_t length = 0;
};

template <size_t Count>
constexpr std::optional<FreshUploadLayout<Count>> PlanFreshDrawUpload(
	const std::array<size_t,Count>& lengths, size_t limit)
{
	// The engine already aligns shared constant-block addresses to 256 bytes.
	constexpr size_t alignment = 256;
	FreshUploadLayout<Count> layout;
	for (size_t i = 0; i < Count; ++i) {
		const size_t padding = (alignment - layout.length % alignment) % alignment;
		if (!lengths[i] || padding > limit - layout.length ||
			lengths[i] > limit - layout.length - padding)
			return std::nullopt;

		layout.offsets[i] = layout.length + padding;
		layout.length = layout.offsets[i] + lengths[i];
	}
	return layout;
}

inline bool FreshDrawUploadBundleEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("BAR_NATIVE_FRESH_DRAW_UPLOAD_BUNDLE");
		return value && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

// The source bytes are read-only draw inputs. The runtime lease owns the new
// allocation through completion and adds it to the very same residency set.
template <size_t Count>
std::optional<std::array<MTLGPUAddress,Count>> TryPackFreshDrawUploads(
	RuntimeLease& lease, const std::array<FreshUploadBytes,Count>& slices)
{
	if (!FreshDrawUploadBundleEnabled())
		return std::nullopt;

	std::array<size_t,Count> lengths;
	for (size_t i = 0; i < Count; ++i)
		lengths[i] = slices[i].size();
	const auto layout = PlanFreshDrawUpload(lengths,lease.Device().maxBufferLength);
	if (!layout)
		return std::nullopt;

	auto slab = [lease.Device() newBufferWithLength:layout->length options:MTLResourceStorageModeShared];
	auto* bytes = static_cast<std::byte*>(slab.contents);
	const MTLGPUAddress base = slab.gpuAddress;
	if (!bytes || slab.length != layout->length || !base || base % 256 ||
		layout->length > std::numeric_limits<MTLGPUAddress>::max() - base)
		return std::nullopt;

	std::array<MTLGPUAddress,Count> addresses;
	for (size_t i = 0; i < Count; ++i) {
		std::memcpy(bytes + layout->offsets[i],slices[i].data(),slices[i].size());
		addresses[i] = base + layout->offsets[i];
	}
	lease.AddAllocation(slab);
	return addresses;
}



static constexpr unsigned positionsWords=660, uniformWords=92, totalWords=1412;
static const char* Shader=R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 SlotTestVertex(uint id [[vertex_id]]) {
    const float2 xy[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(xy[id],0,1);
}
inline uint Triplet(float4 p, device const uint* a,constant uint* b,device const uint* c) {
    uint i=uint(p.x);
    if(i<660) return a[i];
    if(i<752) return b[i-660];
    return c[i-752];
}
fragment uint SlotSensor(float4 p [[position]],device const uint* a [[buffer(2)]],
    constant uint* b [[buffer(3)]],device const uint* c [[buffer(4)]]) {return Triplet(p,a,b,c);}
fragment uint SlotCircle(float4 p [[position]],device const uint* a [[buffer(2)]],
    constant uint* b [[buffer(3)]],device const uint* c [[buffer(5)]]) {return Triplet(p,a,b,c);}
fragment uint SlotPair(float4 p [[position]],device const uint* a [[buffer(1)]],
    constant uint* b [[buffer(2)]]) {uint i=uint(p.x);return i<660?a[i]:b[i-660];}
)MSL";

struct Case {
    const char* name;
    unsigned kind;       // 0: SensorLOS, 1: three-slice common, 2: two-slice common
    bool nilSlab;
    bool limitSlab;
    bool fan;
    unsigned allocations;
    unsigned slabAttempts;
};

int main() { @autoreleasepool { try {
    const auto gpu=MTLCreateSystemDefaultDevice();
    Require(gpu&&[gpu supportsFamily:MTLGPUFamilyMetal4],"Metal4 device required");
    NSError* error=nil;
    auto compiler=[gpu newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];
    Require(compiler!=nil,"Metal4 compiler");
    auto source=[MTL4LibraryDescriptor new]; source.source=[NSString stringWithUTF8String:Shader];
    auto lib=[compiler newLibraryWithDescriptor:source error:&error];
    if(!lib)std::cerr<<error.description.UTF8String<<"\n";
    Require(lib!=nil,"Metal4 library");
    std::array<id<MTLRenderPipelineState>,3> pipelines{};
    const std::array<NSString*,3> names{@"SlotSensor",@"SlotCircle",@"SlotPair"};
    for(size_t n=0;n<names.size();++n) {
        auto d=[MTL4RenderPipelineDescriptor new];
        auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];
        v.library=f.library=lib;v.name=@"SlotTestVertex";f.name=names[n];
        d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;
        d.rasterSampleCount=1;d.colorAttachments[0].pixelFormat=MTLPixelFormatR32Uint;
        pipelines[n]=[compiler newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:&error];
        if(!pipelines[n])std::cerr<<error.description.UTF8String<<"\n";
        Require(pipelines[n]!=nil,"Metal4 test pipeline");
    }
    const std::array<Case,8> cases{{
        {"sensor_slab",0,false,false,false,1,1},
        {"sensor_nil",0,true,false,false,3,1},
        {"sensor_capacity",0,false,true,false,3,0},
        {"sensor_fan_bypass",0,false,false,true,3,0},
        {"common_circle_slab",1,false,false,false,1,1},
        {"common_circle_capacity",1,false,true,false,3,0},
        {"common_pair_slab",2,false,false,false,1,1},
        {"common_pair_nil",2,true,false,false,2,1},
    }};
    auto command=[gpu newCommandBuffer];auto queue=[gpu newMTL4CommandQueue];
    auto allocator=[gpu newCommandAllocator];auto event=[gpu newSharedEvent];
    auto residency=[gpu newResidencySetWithDescriptor:[MTLResidencySetDescriptor new] error:&error];
    Require(command&&queue&&allocator&&event&&residency,"Queue and residency");
    std::vector<RuntimeLease> leases;
    std::vector<id<MTLTexture>> targets;
    std::vector<id<MTL4ArgumentTable>> tables;
    std::vector<std::vector<uint32_t>> expected;
    [command beginCommandBufferWithAllocator:allocator];
    for(size_t n=0;n<cases.size();++n) {
        const auto& test=cases[n];
        const unsigned words=test.kind==2?positionsWords+uniformWords:totalWords;
        auto proxy=[FaultDevice new];proxy.real=gpu;proxy.rejectSlab=test.nilSlab;
        proxy.maxBufferLength=test.limitSlab?2640:gpu.maxBufferLength;
        leases.push_back(RuntimeLease{proxy,residency,{}});auto& lease=leases.back();
        std::vector<uint32_t> values(words);
        for(unsigned i=0;i<words;++i) values[i]=0x3f001000u+unsigned(n*0x10000u)+i;
        NativeSensorLOSInstanceValues positions;positions.positions.resize(165);positions.colors.resize(165);
        CircleUniform uniform;
        std::memcpy(positions.positions.data(),values.data(),2640);
        std::memcpy(&uniform,values.data()+positionsWords,sizeof(uniform));
        if(words==totalWords) std::memcpy(positions.colors.data(),values.data()+positionsWords+uniformWords,2640);

        auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=6;ad.initializeBindings=YES;
        auto table=[gpu newArgumentTableWithDescriptor:ad error:&error];Require(table!=nil,"Argument table");
        tables.push_back(table);
        if(test.kind==0) {
            struct {bool packedOutlineInputs=false;__strong id<MTL4ArgumentTable> arguments;} out{false,table};
            struct {bool stencilPass;} defines{test.fan};
	std::array<MTLGPUAddress,3> uploads{};
	out.packedOutlineInputs = !defines.stencilPass && SensorLOSOutlineUploadSlabEnabled() &&
		TryUploadCircleOutline(lease,positions,uniform,uploads);
	if (!out.packedOutlineInputs) {
		const std::array<id<MTLBuffer>,3> buffers{
			UploadCircle(lease,positions.positions.data(),positions.positions.size()*16),
			UploadCircle(lease,&uniform,sizeof(uniform)),
			UploadCircle(lease,positions.colors.data(),positions.colors.size()*16),
		};
		for (size_t i = 0; i < uploads.size(); ++i)
			uploads[i] = buffers[i].gpuAddress;
	}

	for (size_t i = 0; i < uploads.size(); ++i)
		[out.arguments setAddress:uploads[i] atIndex:i+2];
            Require(out.packedOutlineInputs==(test.allocations==1),"SensorLOS fallback selection");
        } else if(test.kind==1) {
            const auto packed=TryPackFreshDrawUploads(lease,std::array<FreshUploadBytes,3>{
                std::as_bytes(std::span(positions.positions)),std::as_bytes(std::span(&uniform,1)),
                std::as_bytes(std::span(positions.colors))});
            Require(packed.has_value()==(test.allocations==1),"Common three-slice selection");
            [table setAddress:packed ? (*packed)[0] : UploadCircle(lease,positions.positions.data(),2640).gpuAddress atIndex:2];
            [table setAddress:packed ? (*packed)[1] : UploadCircle(lease,&uniform,sizeof(uniform)).gpuAddress atIndex:3];
            [table setAddress:packed ? (*packed)[2] : UploadCircle(lease,positions.colors.data(),2640).gpuAddress atIndex:5];
        } else {
            const auto packed=TryPackFreshDrawUploads(lease,std::array<FreshUploadBytes,2>{
                std::as_bytes(std::span(positions.positions)),std::as_bytes(std::span(&uniform,1))});
            Require(packed.has_value()==(test.allocations==1),"Common two-slice selection");
            [table setAddress:packed ? (*packed)[0] : UploadCircle(lease,positions.positions.data(),2640).gpuAddress atIndex:1];
            [table setAddress:packed ? (*packed)[1] : UploadCircle(lease,&uniform,sizeof(uniform)).gpuAddress atIndex:2];
        }
        Require(lease.allocations.size()==test.allocations&&proxy.slabAttempts==test.slabAttempts,"Allocation count and forced fallback");
        if(test.allocations==1) {
            Require(lease.allocations[0].length==(test.kind==2?3184:5968),"Slab length including alignment");
            Require(lease.allocations[0].gpuAddress%256==0,"Aligned allocation base");
        }
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:words height:1 mipmapped:NO];
        td.usage=MTLTextureUsageRenderTarget;td.storageMode=MTLStorageModeShared;
        auto target=[gpu newTextureWithDescriptor:td];Require(target!=nil,"Readback target");
        targets.push_back(target);expected.push_back(std::move(values));[residency addAllocation:target];
        auto pass=[MTL4RenderPassDescriptor new];
        pass.renderTargetWidth=words;pass.renderTargetHeight=1;pass.defaultRasterSampleCount=1;
        pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Render encoder");
        [encoder setRenderPipelineState:pipelines[test.kind]];
        [encoder setArgumentTable:table atStages:MTLRenderStageFragment];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
    }
    [command endCommandBuffer];
    [residency commit];[queue addResidencySet:residency];
    auto done=dispatch_semaphore_create(0);__block NSError* failure=nil;auto commit=[MTL4CommitOptions new];
    [commit addFeedbackHandler:^(id<MTL4CommitFeedback> f) {failure=f.error;dispatch_semaphore_signal(done);}];
    id<MTL4CommandBuffer> list[]{command};[queue commit:list count:1 options:commit];[queue signalEvent:event value:1];
    Require([event waitUntilSignaledValue:1 timeoutMS:30000],"GPU event");
    Require(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC))==0,"GPU feedback");
    if(failure)std::cerr<<failure.description.UTF8String<<"\n";
    Require(!failure,"GPU execution failed");
    unsigned words=0;
    for(size_t n=0;n<cases.size();++n) {
        std::vector<uint32_t> observed(expected[n].size());
        [targets[n] getBytes:observed.data() bytesPerRow:observed.size()*sizeof(uint32_t)
                       fromRegion:MTLRegionMake2D(0,0,observed.size(),1) mipmapLevel:0];
        if(observed!=expected[n]){
            for(size_t i=0;i<observed.size();++i) if(observed[i]!=expected[n][i]) {
                std::cerr<<cases[n].name<<" mismatch word "<<i<<" actual "<<observed[i]<<" expected "<<expected[n][i]<<"\n"; break;
            }
            throw std::runtime_error("GPU word mismatch");
        }
        words+=observed.size();
        std::cout<<"PASS "<<cases[n].name<<" words="<<observed.size()<<" allocations="<<leases[n].allocations.size()<<"\n";
    }
    [allocator reset];[queue removeResidencySet:residency];
    std::cout<<"PASS cases="<<cases.size()<<" words="<<words<<" metal4_submissions=1\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;} }}
