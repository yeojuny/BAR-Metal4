#include <cstdio>
#include "Metal4GroundDecalDepthMapping.h"
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4GroundDecal.h"
#include "Metal4InterventionMath.h"
#include "NativeGroundDecalShaders.h"
#include "NativeGroundDecalExpiredTrackPrefix.h"
#include "Metal4GroundDecalIndexPolicyG01.h"
#include "System/Log/ILog.h"
#include <objc/runtime.h>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

namespace rendering::metal4 {
namespace {
id<MTLBuffer> Buffer(RuntimeLease& l,const void* p,size_t n,const char* label,bool optional=false){Require(p&&n,"Original decal buffer empty");auto b=[l.Device() newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];if(optional&&!b)return nil;Require(b!=nil,"Cannot allocate original decal buffer");b.label=String(label);l.AddAllocation(b);return b;}
MTLPixelFormat Format(WorldTextureFormat f){switch(f){case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;}throw std::invalid_argument("Original decal source format unknown");}
id<MTL4ArgumentTable> Arguments(RuntimeLease& l,uint32_t buffers,uint32_t textures,uint32_t samplers){auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=textures;d.maxSamplerStateBindCount=samplers;d.initializeBindings=YES;auto a=[l.Device() newArgumentTableWithDescriptor:d error:nil];Require(a!=nil,"Cannot allocate original decal argument table");l.Retain(a);return a;}
struct AtlasResources {
	std::shared_ptr<const NativeGroundDecalAtlas> source;
	__strong id<MTLTexture> texture=nil;
	std::vector<id<MTLTexture>> inputs;
	__strong id<MTLBuffer> vertices=nil;
	std::vector<NativeGroundDecalAtlasDraw> draws;
	bool ready=false;
};
struct InstanceResources {
	std::shared_ptr<const NativeGroundDecalInstances> source;
	GroundDecalExpiredTrackPrefix prefix;
 bool allTracks=false;
	__strong id<MTLBuffer> buffer=nil;

	size_t Count() const noexcept {return source->values.size()-prefix.rows;}
};
struct AtlasPassDraw {uint32_t page=0,level=0;__strong id<MTL4ArgumentTable> arguments=nil;};
struct alignas(16) AtlasUniforms {std::array<float,4> clamp{};std::array<uint32_t,4> flags{};};
const std::filesystem::path& CoverageDirectory()
{
    static const std::filesystem::path directory=[] {
        const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_COVERAGE_DIR");
        return value?std::filesystem::path(value):std::filesystem::path{};
    }();
    return directory;
}

constexpr unsigned coverageTypes=5;
constexpr unsigned coverageStages=2;
constexpr uint32_t coverageMagic=0x47444331u;
constexpr size_t trackPassCapacity=4194304;
constexpr std::array<const char*,3> diagnosticAttachments={"color","depth","stencil"};

bool DiscardRejected()
{
    static const bool enabled=[] {
        const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_DISCARD_REJECTED");
        return value && std::strcmp(value,"1")==0;
    }();
    return enabled;
}

bool TrackOnlyRequested()
{
 static const bool enabled=[] {const char* v=std::getenv("BAR_NATIVE_GROUND_DECAL_TRACK_ONLY");return !v||!*v||std::strcmp(v,"1")==0;}();
 return enabled&&!InterventionFastMathEnabled();
}
bool TrackOnlyCensus()
{
 static const bool diagnostic=std::getenv("BAR_NATIVE_GROUND_DECAL_PROJECTION_SAME_INPUT_DIR")!=nullptr;
 return TrackOnlyRequested()||diagnostic;
}
bool TrackMinimap(MTLPixelFormat f)
{
 return f==MTLPixelFormatBC1_RGBA||f==MTLPixelFormatRGBA8Unorm||f==MTLPixelFormatBGRA8Unorm;
}
bool SkipUnusedTerrainColor(uint32_t samples,MTLPixelFormat minimapFormat)
{
	static const bool enabled=[] {
		const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_SKIP_UNUSED_TERRAIN_COLOR");
		return value&&std::strcmp(value,"1")==0;
	}();
	return enabled&&samples==1&&!InterventionFastMathEnabled()&&
		(minimapFormat==MTLPixelFormatBC1_RGBA||minimapFormat==MTLPixelFormatRGBA8Unorm||
		 minimapFormat==MTLPixelFormatBGRA8Unorm);
}
bool TrimExpiredTrackPrefix(uint32_t samples)
{
	static const bool enabled=[] {
		const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_EXPIRED_TRACK_PREFIX");
		return value&&std::strcmp(value,"1")==0;
	}();
	return enabled&&samples==1&&!InterventionFastMathEnabled();
}

id<MTLTexture> WorldPositionsForDraw(RuntimeLease& lease,const NativeGroundDecalFrame& frame,
	const GroundDecalSceneImages& images,size_t instances)
{
	static const bool enabled=[] {
		const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_WORLD_POSITION_REUSE");
		return value && std::strcmp(value,"1")==0;
	}();
	if (!enabled || !instances || frame.sampleCount!=1 || frame.highQuality ||
	    InterventionFastMathEnabled() || !images.bottomAlignedTerrainSnapshotThisDraw)
		return nil;

	auto depth=images.terrainDepth.texture;
	const auto width=depth.width, height=depth.height;
	// Even origins and dimensions keep the raster's 2x2 derivative quads inside
	// the cache, including helper lanes at the outer attachment edges.
	const auto& map=images.fragmentToLogical;
	if ((width&1) || (height&1) || map[0]!=1 || map[1]!=-1 || map[2]!=0 || map[3]<float(height))
		return nil;

	// The terrain snapshot pool grants this image to one live draw at a time.
	// Its lease pins the storage until retirement, including failed submissions.
	// Associate only the allocation. Each draw regenerates every pixel from scratch.
	static char worldPositionsKey;
	id<MTLTexture> positions=objc_getAssociatedObject(depth,&worldPositionsKey);
	if (!positions) {
		auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Uint
			width:width height:height mipmapped:NO];
		td.storageMode=MTLStorageModePrivate;
		td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
		positions=[lease.Device() newTextureWithDescriptor:td];
		if (!positions) return nil;
		positions.label=@"GroundDecal same-draw world positions";
		objc_setAssociatedObject(depth,&worldPositionsKey,positions,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	}
	lease.AddAllocation(positions);
	return positions;
}
}
struct PreparedGroundDecal::Impl {
	std::shared_ptr<const NativeGroundDecalFrame> frame;
	std::shared_ptr<AtlasResources> atlas;
	std::shared_ptr<InstanceResources> instances;
	__strong id<MTLRenderPipelineState> surfacePipeline=nil,atlasPipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong id<MTLTexture> worldPositions=nil;
	__strong id<MTLComputePipelineState> worldPositionPipeline=nil;
	bool worldPositionsEncoded=false;
	// Immutable per-preparation upload, retained by this packet AND the lease.
	// Never mutated or cached; normal and failed-submission owners are unchanged.
	__strong id<MTLBuffer> indicesG01=nil;
    __strong id<MTLBuffer> coverageCounters=nil;
    __strong id<MTLBuffer> trackPasses=nil,trackPassCount=nil;
    std::array<id<MTLBuffer>,3> attachments{};
    std::array<uint32_t,2> attachmentExtent{};
    unsigned coverageTick=0;
    bool readbackEncoded=false;
    bool usedIndexedDraw=false;
	bool probeG01=false;
	unsigned probeReasonG01=0;
	std::vector<AtlasPassDraw> atlasDraws;
	bool dependenciesEncoded=false,encoded=false;
};
PreparedGroundDecal::PreparedGroundDecal(std::unique_ptr<Impl> v):impl(std::move(v)){}
PreparedGroundDecal::~PreparedGroundDecal()=default;
id<MTLTexture> PreparedGroundDecal::Atlas()const{return impl->atlas->texture;}
uint32_t PreparedGroundDecal::AtlasDrawCount()const{return uint32_t(impl->atlasDraws.size());}
void PreparedGroundDecal::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(!impl->dependenciesEncoded,"Original decal atlas encoded twice");
	id<MTL4RenderCommandEncoder> e=nil;uint32_t page=~0u,level=~0u;
	for(const auto& d:impl->atlasDraws){
		if(d.page!=page||d.level!=level){if(e){[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}page=d.page;level=d.level;
			auto pass=[MTL4RenderPassDescriptor new];auto a=pass.colorAttachments[0];a.texture=impl->atlas->texture;a.slice=page;a.level=level;a.loadAction=MTLLoadActionDontCare;a.storeAction=MTLStoreActionStore;
			e=[command renderCommandEncoderWithDescriptor:pass];Require(e!=nil,"Cannot encode original decal atlas page/mip");
			const auto w=std::max(1u,impl->atlas->source->width>>level),h=std::max(1u,impl->atlas->source->height>>level);
			[e setViewport:MTLViewport{0,0,double(w),double(h),0,1}];[e setScissorRect:MTLScissorRect{0,0,w,h}];
			[e setRenderPipelineState:impl->atlasPipeline];[e setDepthStencilState:impl->depth];[e setCullMode:MTLCullModeNone];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
		}
		[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
	}
	if(e){[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	impl->dependenciesEncoded=true;
}
void PreparedGroundDecal::EncodeWorldPositions(id<MTL4CommandBuffer> command)
{
	if (!impl->worldPositions) return;
	Require(impl->dependenciesEncoded && !impl->worldPositionsEncoded,"GroundDecal world position encode order differs");
	auto compute=[command computeCommandEncoder];
	Require(compute!=nil,"Cannot encode GroundDecal world positions");
	compute.label=@"GroundDecal same-draw world positions";
	[compute barrierAfterQueueStages:MTLStageBlit beforeStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
	[compute setComputePipelineState:impl->worldPositionPipeline];
	[compute setArgumentTable:impl->arguments];
	[compute dispatchThreads:MTLSizeMake(impl->worldPositions.width,impl->worldPositions.height,1)
		threadsPerThreadgroup:MTLSizeMake(8,8,1)];
	[compute barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[compute endEncoding];
	impl->worldPositionsEncoded=true;
}
void PreparedGroundDecal::Encode(id<MTL4RenderCommandEncoder> e)
{
	Require(impl->dependenciesEncoded&&!impl->encoded,"Original ground decal pass order differs");
	Require(!impl->worldPositions || impl->worldPositionsEncoded,"GroundDecal world positions are missing for this draw");
	[e setRenderPipelineState:impl->surfacePipeline];[e setDepthStencilState:impl->depth];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setCullMode:MTLCullModeBack];[e setDepthClipMode:MTLDepthClipModeClip];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
	[e setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	const auto count=impl->instances->Count();
	const bool indexed=impl->indicesG01&&[e respondsToSelector:@selector(drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferLength:instanceCount:baseVertex:baseInstance:)];
	if(count){
		if(indexed){
			// GPU address includes byte offset zero; count is in UInt16 elements,
			// length is in bytes. Preserve original vertex and instance origins.
			[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:GroundDecalIndicesG01.size() indexType:MTLIndexTypeUInt16 indexBuffer:impl->indicesG01.gpuAddress indexBufferLength:impl->indicesG01.length instanceCount:count baseVertex:0 baseInstance:0];
		}else{
			[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36 instanceCount:count];
		}
	}
	impl->usedIndexedDraw=indexed;
	impl->encoded=true;
	// At most one nonempty indexed and one nonempty fallback receipt. Clean
	// runs leave PROBE off: no counters, reason updates, or log formatting.
	if(impl->probeG01&&count){
		static std::atomic_flag reportedIndexed=ATOMIC_FLAG_INIT,reportedFallback=ATOMIC_FLAG_INIT;
		auto& reported=indexed?reportedIndexed:reportedFallback;
		if(!reported.test_and_set(std::memory_order_relaxed)){
			const unsigned reason=impl->indicesG01&&!indexed?5u:impl->probeReasonG01;
			LOG("[NativeGroundDecalIndexG01] source_frame=%llu indexed=%u index_count=%u unique_ids=%u index_bytes=%zu instances=%zu samples=%u reason=%u base_vertex=0 base_instance=0",
			 static_cast<unsigned long long>(impl->frame->sourceDrawFrame),unsigned(indexed),indexed?36u:0u,indexed?8u:36u,indexed?GroundDecalIndexBytesG01:0,count,impl->frame->sampleCount,reason);
		}
	}
}
bool PreparedGroundDecal::WantsDiagnosticReadback() const {return impl->coverageTick!=0;}
void PreparedGroundDecal::PrepareDiagnosticReadback(RuntimeLease& lease,size_t width,size_t height)
{
    Require(impl->coverageTick && !impl->attachmentExtent[0] && width && height,
        "GroundDecal diagnostic copy extent unavailable");
    impl->attachmentExtent={uint32_t(width),uint32_t(height)};
    for(size_t i=0;i<impl->attachments.size();++i) {
        const size_t rowBytes=width*(i==2?1u:4u);
        auto buffer=[lease.Device() newBufferWithLength:rowBytes*height options:MTLResourceStorageModeShared];
        Require(buffer!=nil,"Cannot allocate GroundDecal final output readback");
        impl->attachments[i]=buffer;
        lease.AddAllocation(buffer);
    }
}
void PreparedGroundDecal::EncodeDiagnosticReadback(id<MTL4CommandBuffer> command,
    id<MTLTexture> color,id<MTLTexture> depth,id<MTLTexture> stencil)
{
    Require(impl->encoded && !impl->readbackEncoded && impl->coverageTick,
            "GroundDecal diagnostic copy requires its actual draw");
    Require(color && depth && stencil && !color.framebufferOnly &&
        (color.pixelFormat==MTLPixelFormatBGRA8Unorm || color.pixelFormat==MTLPixelFormatBGRA8Unorm_sRGB ||
         color.pixelFormat==MTLPixelFormatRGBA8Unorm || color.pixelFormat==MTLPixelFormatRGBA8Unorm_sRGB) &&
        depth.pixelFormat==MTLPixelFormatDepth32Float && stencil.pixelFormat==MTLPixelFormatStencil8,
        "GroundDecal diagnostic attachment formats differ");
    auto copy=[command computeCommandEncoder];
    Require(copy!=nil,"Cannot encode GroundDecal final attachment copies");
    copy.label=@"GroundDecal final output diagnostic";
    [copy barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit
        beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
    Require(color.width==impl->attachmentExtent[0] && color.height==impl->attachmentExtent[1],
        "GroundDecal final color size differs from prepared readback");
    const std::array<id<MTLTexture>,3> textures={color,depth,stencil};
    for(size_t i=0;i<textures.size();++i) {
        const auto texture=textures[i];
        Require(texture.sampleCount==1 && texture.width==color.width && texture.height==color.height,
            "GroundDecal final attachment extent/sample count differs");
        const size_t rowBytes=texture.width*(i==2?1u:4u);
        auto buffer=impl->attachments[i];
        [copy copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(texture.width,texture.height,1) toBuffer:buffer destinationOffset:0
            destinationBytesPerRow:rowBytes destinationBytesPerImage:0];
    }
    [copy endEncoding];
    impl->readbackEncoded=true;
}
void PreparedGroundDecal::DidComplete()
{
    Require(impl->dependenciesEncoded&&impl->encoded,"Original ground decal cannot retire unencoded resources");
    impl->atlas->ready=true;
    if (!impl->coverageCounters) return;

    const auto& frame=*impl->frame;
    const auto draw=frame.sourceDrawFrame;
    const auto instances=std::span<const NativeGroundDecalInstance>(impl->instances->source->values)
        .subspan(impl->instances->prefix.rows);
    std::array<uint32_t,coverageTypes> instanceCounts{};
    for (const auto& instance:instances) {
        const uint32_t type=instance.createParams5[0]&0xFu;
        ++instanceCounts[type<coverageTypes?type:0];
    }
    const std::array<uint32_t,10> header={
        coverageMagic,uint32_t(frame.viewport[2]),uint32_t(frame.viewport[3]),impl->coverageTick,
        uint32_t(draw),uint32_t(draw>>32),uint32_t(instances.size()),uint32_t(frame.shadows),
        std::bit_cast<uint32_t>(frame.adjustedFrame),uint32_t(impl->usedIndexedDraw)
    };
    Require(impl->readbackEncoded,"GroundDecal final readback not encoded");
    const auto folder=CoverageDirectory();
    const auto suffix="-"+std::to_string(impl->coverageTick)+".bin";
    const auto writeBinary=[&](const std::string& name,const void* contents,size_t bytes) {
        std::ofstream stream(folder/(name+suffix),std::ios::binary|std::ios::trunc);
        stream.write(static_cast<const char*>(contents),bytes);
        stream.close();
        Require(stream.good(),"Cannot write GroundDecal diagnostic bytes");
    };
    const uint32_t passCount=*static_cast<const uint32_t*>(impl->trackPassCount.contents);
    if (impl->worldPositions) {
        uint64_t checked=0;
        const auto* counts=static_cast<const uint32_t*>(impl->coverageCounters.contents);
        for (size_t i=0;i<impl->coverageCounters.length/sizeof(*counts);i+=coverageStages)
            checked+=counts[i];
        const auto mismatches=static_cast<const uint32_t*>(impl->trackPassCount.contents)[1];
        LOG("[GroundDecalWorldPositionReuse] tick=%u source_draw=%llu width=%zu height=%zu dispatched=%u checked=%llu mismatches=%u",
            impl->coverageTick,static_cast<unsigned long long>(draw),size_t(impl->worldPositions.width),
            size_t(impl->worldPositions.height),unsigned(impl->worldPositionsEncoded),
            static_cast<unsigned long long>(checked),mismatches);
    }
    Require(passCount<=trackPassCapacity,"GroundDecal track pass record overflow");
    writeBinary("tracks",impl->trackPasses.contents,size_t(passCount)*sizeof(std::array<uint32_t,4>));
    for(size_t i=0;i<impl->attachments.size();++i)
        writeBinary(diagnosticAttachments[i],impl->attachments[i].contents,impl->attachments[i].length);
    writeBinary("extent",impl->attachmentExtent.data(),sizeof(impl->attachmentExtent));
    const auto path=folder/("coverage"+suffix);
    const auto temporary=path.string()+".tmp";
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
    file.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
    file.write(reinterpret_cast<const char*>(instanceCounts.data()),sizeof(instanceCounts));
    file.write(static_cast<const char*>(impl->coverageCounters.contents),impl->coverageCounters.length);
    file.close();
    Require(file.good(),"Cannot write ground decal coverage counter file");
    std::filesystem::rename(temporary,path);
    LOG("[GroundDecalCoverage] tick=%u source_draw=%llu adjusted_frame=%.6f instances=%zu indexed=%u",
        impl->coverageTick,static_cast<unsigned long long>(draw),frame.adjustedFrame,
        instances.size(),unsigned(impl->usedIndexedDraw));
}
struct alignas(16) GroundDecalPhysicalUniforms {
 NativeGroundDecalUniforms original;
 std::array<uint32_t,4> depthCrop;
 std::array<float,4> logicalOrigin;
};
static_assert(sizeof(GroundDecalPhysicalUniforms)==400);
struct GroundDecalGPU::Impl {
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,bool,uint32_t,bool,bool,bool,bool,bool,bool,bool,bool,bool,bool,bool>;
	using LibraryKey=std::tuple<bool,bool,bool,bool,bool,bool,bool,bool,bool,bool,bool,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	std::array<id<MTLComputePipelineState>,2> worldPositionPipelines{};
	std::map<LibraryKey,id<MTLLibrary>> libraries;
	std::array<id<MTLSamplerState>,6> samplers{};
	__strong id<MTLDepthStencilState> depth=nil;
	std::shared_ptr<AtlasResources> atlas;
	std::shared_ptr<InstanceResources> instances;
	float lastInstanceAdjustedFrame=0.0f;
	unsigned lastCoverageTick=0;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& l,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool atlas,bool high,uint32_t samples,bool validation,double& milliseconds,bool shadows=false,bool skipUnusedTerrainColor=false,bool countCoverage=false,bool reuseWorldPosition=false,
		std::optional<bool> coldAttenuationOverride=std::nullopt,bool trackOnly=false,bool physicalDepth=false){
        Require(!physicalDepth||(!atlas&&!reuseWorldPosition&&!countCoverage),"Ground decal physical depth mapping lost its distinct shader contract");
				static const bool coldGlow=[] {const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_EXACT_COLD_GLOW");return value && std::strcmp(value,"1")==0 && !InterventionFastMathEnabled();}();
		const bool exactColdGlow=coldGlow&&samples==1&&!atlas;
		static const bool coldAttenuationEnabled=[] {
			const char* value=std::getenv("BAR_NATIVE_GROUND_DECAL_EXACT_COLD_ATTENUATION");
			return value&&std::strcmp(value,"1")==0;
		}();
		const bool skipColdAttenuation=exactColdGlow&&!high&&coldAttenuationOverride.value_or(coldAttenuationEnabled);
		const bool discard=!atlas&&!high&&samples==1&&DiscardRejected();
		static const bool zeroAngleEnabled = [] {
			const char* value = std::getenv("BAR_NATIVE_GROUND_DECAL_ZERO_ANGLE");
			return value && std::strcmp(value, "1") == 0 && !InterventionFastMathEnabled();
		}();
		const bool zeroAngle = zeroAngleEnabled && !atlas && !high && samples == 1;
		static const bool trackEarlyRejectEnabled = [] {
			const char* value = std::getenv("BAR_NATIVE_GROUND_DECAL_TRACK_EARLY_REJECT");
			return value && std::strcmp(value, "1") == 0 && !InterventionFastMathEnabled();
		}();
		const bool trackEarlyReject = trackEarlyRejectEnabled && !atlas && !high && samples == 1;
		trackOnly=trackOnly&&!atlas&&!high&&samples==1&&!InterventionFastMathEnabled();
		const LibraryKey libraryKey{high,shadows,skipUnusedTerrainColor,exactColdGlow,countCoverage,discard,zeroAngle,reuseWorldPosition,skipColdAttenuation,trackEarlyReject,trackOnly,physicalDepth};
		const Key key{color,depthFormat,stencilFormat,atlas,high,l.SampleShadingKey(samples),validation,shadows,skipUnusedTerrainColor,exactColdGlow,countCoverage,zeroAngle,reuseWorldPosition,skipColdAttenuation,trackEarlyReject,trackOnly,physicalDepth};if(auto it=pipelines.find(key);it!=pipelines.end()){l.Retain(libraries.at(libraryKey));return it->second;}
		const auto start=Clock::now();auto it=libraries.find(libraryKey);NSError* error=nil;
		if(it==libraries.end()){auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(o);
			auto msl=NativeGroundDecalShaderSource(high,shadows,skipUnusedTerrainColor,zeroAngle,reuseWorldPosition);
            static const bool flatWrap = [] {
                const char* value = std::getenv("BAR_NATIVE_GROUND_DECAL_FLAT_WRAP");
                return value && std::strcmp(value, "1") == 0;
            }();
            if (flatWrap && !InterventionFastMathEnabled())
                msl.insert(0, "#define BAR_DIAG_GROUND_TRACK_WRAP_VERTEX 1\n");
			if(exactColdGlow)msl.insert(0,"#define BAR_GROUND_DECAL_EXACT_COLD_GLOW 1\n");
			if(skipColdAttenuation)msl.insert(0,"#define BAR_GROUND_DECAL_EXACT_COLD_ATTENUATION 1\n");
            if(discard)
                msl.insert(0,"#define BAR_GROUND_DECAL_DISCARD_REJECTED 1\n");
			if(countCoverage)msl.insert(0,"#define BAR_GROUND_DECAL_COVERAGE 1\n");
			if(trackOnly)msl.insert(0,"#define BAR_GROUND_DECAL_TRACK_ONLY 1\n");
			if(trackEarlyReject)msl.insert(0,"#define BAR_GROUND_DECAL_TRACK_EARLY_REJECT 1\n");
            if(physicalDepth)msl.insert(0,"#define BAR_GROUND_DECAL_PHYSICAL_DEPTH 1\n");
			auto lib=l.NewLibraryWithSource(String(msl),o,&error);Require(lib!=nil,"Cannot compile original ground decal family: "+Error(error));it=libraries.emplace(libraryKey,lib).first;
			if(trackEarlyReject)
				LOG("[GroundDecalTrack] early_reject=1 world_position_reuse=%u coverage=%u",
					unsigned(reuseWorldPosition),unsigned(countCoverage));
		}
		l.Retain(it->second);
		if (reuseWorldPosition && !worldPositionPipelines[validation]) {
			auto function=[MTL4LibraryFunctionDescriptor new];
			function.library=it->second;
			function.name=@"GroundDecalBuildWorldPositions";
			auto desc=[MTL4ComputePipelineDescriptor new];
			desc.computeFunctionDescriptor=function;
			desc.options=[MTL4PipelineOptions new];
			desc.options.shaderValidation=ShaderValidationMode(validation);
			auto compute=[l.Compiler() newComputePipelineStateWithDescriptor:desc compilerTaskOptions:nil error:&error];
			Require(compute!=nil,"Cannot compile GroundDecal world position pipeline: "+Error(error));
			worldPositionPipelines[validation]=compute;
		}
		auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=it->second;v.name=atlas?@"GroundDecalAtlasVertex":@"GroundDecalVertex";f.name=atlas?@"GroundDecalAtlasFragment":@"GroundDecalFragment";
		// Metal4 retains depth/stencil formats on the real render pass. The
		// cache still records the consumer contract; no legacy MTL descriptor.
		d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=atlas?MTL4BlendStateDisabled:MTL4BlendStateEnabled;
		a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto pipeline=l.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original ground decal pipeline: "+Error(error));pipelines[key]=pipeline;milliseconds+=Milliseconds(start);return pipeline;
	}
};
GroundDecalGPU::GroundDecalGPU():impl(std::make_unique<Impl>()){}
GroundDecalGPU::~GroundDecalGPU()=default;
void GroundDecalGPU::ClearResources(){impl->atlas.reset();impl->instances.reset();}
void GroundDecalGPU::Initialize(RuntimeLease& l,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,uint32_t samples,bool validation,double& milliseconds)
{
 Require(samples&&samples<=32&&(samples&(samples-1))==0&&[l.Device() supportsTextureSampleCount:samples],"Original ground decal initialization sample count invalid");
 impl->Pipeline(l,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,false,1,validation,milliseconds);
 impl->Pipeline(l,color,depthFormat,stencilFormat,false,false,samples,validation,milliseconds);
 if(samples>1)impl->Pipeline(l,color,depthFormat,stencilFormat,false,true,samples,validation,milliseconds);
}
std::unique_ptr<PreparedGroundDecal> GroundDecalGPU::Prepare(RuntimeLease& l,std::shared_ptr<const NativeGroundDecalFrame> frame,
	const GroundDecalSceneImages& images,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& milliseconds,GroundDecalTextureProvider textureProvider)
{
	Require(frame!=nullptr,"Original ground decal frame absent");ValidateNativeGroundDecalFrame(*frame);
	Require(images.sourceDrawFrame==frame->sourceDrawFrame,"Original ground decal framebuffer is from another draw");
	const auto& h=images.height;const auto& n=images.normals;const auto& m=images.minimap;const auto& d=images.terrainDepth;
	Require(h.texture&&h.texture.pixelFormat==MTLPixelFormatR32Float&&n.texture&&n.texture.pixelFormat==MTLPixelFormatRG16Float&&m.texture,"Original ground decal terrain images are incomplete");
 const auto depthMapping=MatchGroundDecalDepthMapping(frame->viewport,images.logicalExtent,images.renderExtent,images.depthCrop);
 if(!depthMapping||!d.texture||d.texture.pixelFormat!=MTLPixelFormatDepth32Float||d.texture.width!=images.depthCrop[2]||d.texture.height!=images.depthCrop[3]||depthMapping->fragmentToLogical!=images.fragmentToLogical){
  const auto& v=frame->viewport;const auto& c=images.depthCrop;
  char detail[448];std::snprintf(detail,sizeof(detail),"Original ground decal requires unresampled current physical depth crop: logicalViewport=%d,%d,%d,%d logical=%u,%u render=%u,%u crop=%u,%u,%u,%u texture=%llu,%llu draw=%llu",v[0],v[1],v[2],v[3],images.logicalExtent[0],images.logicalExtent[1],images.renderExtent[0],images.renderExtent[1],c[0],c[1],c[2],c[3],(unsigned long long)d.texture.width,(unsigned long long)d.texture.height,(unsigned long long)frame->sourceDrawFrame);
  Require(false,detail);
 }
 const bool physicalDepth=!depthMapping->legacyExact;
 static const bool witness=[] {const char* v=std::getenv("BAR_METAL4_GROUND_DECAL_DEPTH_WITNESS");return v&&*v=='1';}();
 if(witness){
  static thread_local std::array<uint32_t,8> previous{};
  const auto& c=images.depthCrop;const std::array<uint32_t,8> state{images.logicalExtent[0],images.logicalExtent[1],images.renderExtent[0],images.renderExtent[1],c[0],c[1],c[2],c[3]};
  if(state!=previous){previous=state;LOG("[GroundDecalDepthMapping] draw=%llu frame=%g logical=%ux%u render=%ux%u crop=%u,%u,%u,%u texture=%llux%llu samples=%llu instances=%zu physical=%u",(unsigned long long)frame->sourceDrawFrame,frame->adjustedFrame,state[0],state[1],state[2],state[3],c[0],c[1],c[2],c[3],(unsigned long long)d.texture.width,(unsigned long long)d.texture.height,(unsigned long long)d.texture.sampleCount,frame->instances->values.size(),unsigned(physicalDepth));}
 }
	Require(d.texture.sampleCount==(frame->highQuality?frame->sampleCount:1),"Original ground decal depth sampling differs from source");
	Require(!frame->shadows||(images.shadowDepth.texture&&images.shadowColor.texture),"Original ground decal shadow producer incomplete");
	auto p=std::make_unique<PreparedGroundDecal::Impl>();p->frame=frame;
    const auto& coverage=CoverageDirectory();
    if (!physicalDepth && !coverage.empty() && frame->sampleCount==1 && !frame->highQuality) {
        for (unsigned tick:{1200u,1590u}) {
            if (tick<=impl->lastCoverageTick || frame->adjustedFrame<float(tick) ||
                frame->adjustedFrame>=float(tick+1)) continue;
            std::ifstream request(coverage/("request-"+std::to_string(tick)));
            uint64_t minimumDraw=0;
            if (!(request>>minimumDraw) || frame->sourceDrawFrame<minimumDraw) continue;
            p->coverageTick=tick;
            impl->lastCoverageTick=tick;
            break;
        }
    }
	const bool skipUnusedTerrainColor=SkipUnusedTerrainColor(frame->sampleCount,m.texture.pixelFormat);
	if(!impl->depth){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionAlways;d.depthWriteEnabled=NO;impl->depth=[l.Device() newDepthStencilStateWithDescriptor:d];Require(impl->depth!=nil,"Cannot create original ground decal raster state");}p->depth=impl->depth;l.Retain(p->depth);
	for(size_t i=0;i<impl->samplers.size();++i){if(!impl->samplers[i]){auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;s.minFilter=(i<2)?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.magFilter=MTLSamplerMinMagFilterLinear;s.mipFilter=i==5?MTLSamplerMipFilterLinear:MTLSamplerMipFilterNotMipmapped;
			s.sAddressMode=s.tAddressMode=(i==3||i==4)?MTLSamplerAddressModeClampToBorderColor:MTLSamplerAddressModeClampToEdge;s.borderColor=MTLSamplerBorderColorOpaqueWhite;s.compareFunction=i==3?MTLCompareFunctionLessEqual:MTLCompareFunctionNever;
			impl->samplers[i]=[l.Device() newSamplerStateWithDescriptor:s];Require(impl->samplers[i]!=nil,"Cannot create original ground decal sampler");}l.Retain(impl->samplers[i]);}
	if(!impl->atlas||impl->atlas->source->generation!=frame->atlas->generation){ValidateNativeGroundDecalAtlas(*frame->atlas);auto a=std::make_shared<AtlasResources>();a->source=frame->atlas;a->draws=MakeNativeGroundDecalAtlasDraws(*a->source);
		auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:a->source->width height:a->source->height mipmapped:a->source->levels>1];td.textureType=MTLTextureType2DArray;td.arrayLength=a->source->pages;td.mipmapLevelCount=a->source->levels;td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
		a->texture=[l.Device() newTextureWithDescriptor:td];Require(a->texture!=nil,"Cannot allocate original multipage ground decal atlas");
		for(const auto& source:a->source->sources){id<MTLTexture> image=nil;if(textureProvider)image=textureProvider(l,source);else{const auto& b=source->mips.front();auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Format(source->format) width:b.width height:b.height mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;image=[l.Device() newTextureWithDescriptor:td];Require(image!=nil,"Cannot allocate original decal atlas source");[image replaceRegion:MTLRegionMake2D(0,0,b.width,b.height) mipmapLevel:0 withBytes:b.bytes.data() bytesPerRow:b.bytesPerRow];}
			Require(image!=nil,"Original decal shared image provider returned no image");a->inputs.push_back(image);}
		std::vector<NativeGroundDecalAtlasVertex> vertices;for(const auto& draw:a->draws)vertices.insert(vertices.end(),draw.vertices.begin(),draw.vertices.end());a->vertices=Buffer(l,vertices.data(),vertices.size()*sizeof(vertices[0]),"Original decal atlas snapped per-mip geometry");impl->atlas=a;
	}
	Require(impl->atlas->source==frame->atlas,"Original decal atlas changed within one generation");p->atlas=impl->atlas;l.AddAllocation(p->atlas->texture);l.AddAllocation(p->atlas->vertices);for(auto image:p->atlas->inputs)l.AddAllocation(image);
	if(!p->atlas->ready){p->atlasPipeline=impl->Pipeline(l,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,false,1,validation,milliseconds);l.Retain(p->atlasPipeline);
		for(size_t i=0;i<p->atlas->draws.size();++i){const auto& draw=p->atlas->draws[i];const auto& src=*p->atlas->source->sources[draw.source];AtlasUniforms u{draw.sourceClamp,{src.flipV,0,0,0}};
			auto b=Buffer(l,&u,sizeof(u),"Original decal atlas source clamp");auto args=Arguments(l,2,1,1);[args setAddress:p->atlas->vertices.gpuAddress+i*6*sizeof(NativeGroundDecalAtlasVertex) atIndex:0];[args setAddress:b.gpuAddress atIndex:1];[args setTexture:p->atlas->inputs[draw.source].gpuResourceID atIndex:0];[args setSamplerState:impl->samplers[2].gpuResourceID atIndex:0];p->atlasDraws.push_back({draw.page,draw.level,args});}
	}
	const bool trimTracks=TrimExpiredTrackPrefix(frame->sampleCount);
	const bool matchingUpload=impl->instances&&impl->instances->source==frame->instances;
	const bool rewound=matchingUpload&&frame->adjustedFrame<impl->lastInstanceAdjustedFrame;
	const bool validUpload=matchingUpload&&(impl->instances->prefix.rows==0||
		(trimTracks&&impl->instances->prefix.CanReuse(impl->lastInstanceAdjustedFrame,frame->adjustedFrame)));
	if(!validUpload){
		auto upload=std::make_shared<InstanceResources>();
		upload->source=frame->instances;
		const auto rows=std::span<const NativeGroundDecalInstance>(frame->instances->values);
if(TrackOnlyCensus()&&!rows.empty()){
   upload->allTracks=std::all_of(rows.begin(),rows.end(),[](const auto& row){return (row.createParams5[0]&15u)==3u;});
  }
  const NativeGroundDecalInstance empty{};
		const auto prefix=trimTracks&&!rewound?FindGroundDecalExpiredTrackPrefix(rows,frame->adjustedFrame):GroundDecalExpiredTrackPrefix{};
		if(prefix.rows){
			const auto suffix=rows.subspan(prefix.rows);
			upload->buffer=Buffer(l,suffix.empty()?&empty:suffix.data(),suffix.empty()?sizeof(empty):suffix.size_bytes(),
				"Original ground decal immutable instance revision",true);
			if(upload->buffer)upload->prefix=prefix;
		}
		if(!upload->buffer)
			upload->buffer=Buffer(l,rows.empty()?&empty:rows.data(),rows.empty()?sizeof(empty):rows.size_bytes(),
				"Original ground decal immutable instance revision");
		impl->instances=std::move(upload);
	}
	impl->lastInstanceAdjustedFrame=frame->adjustedFrame;
	p->instances=impl->instances;l.AddAllocation(p->instances->buffer);
	p->worldPositions=physicalDepth?nil:WorldPositionsForDraw(l,*frame,images,p->instances->Count());
	const bool trackOnlyEligible=p->instances->allTracks&&p->instances->Count()&&TrackMinimap(m.texture.pixelFormat)&&
    frame->sampleCount==1&&!frame->highQuality&&!InterventionFastMathEnabled();
 if(TrackOnlyRequested()&&trackOnlyEligible){
  static bool reported=false;if(!reported){reported=true;LOG("[GroundDecalTrackOnly] active instances=%zu",p->instances->Count());}
 }
 p->surfacePipeline=impl->Pipeline(l,color,depthFormat,stencilFormat,false,frame->highQuality,
		frame->sampleCount,validation,milliseconds,frame->shadows,skipUnusedTerrainColor,
		p->coverageTick!=0,p->worldPositions!=nil,std::nullopt,TrackOnlyRequested()&&trackOnlyEligible,physicalDepth);
	l.Retain(p->surfacePipeline);
	const std::array<GroundDecalSceneImage,6> sources={h,n,m,images.shadowDepth,images.shadowColor,d};uint32_t flips=0;
	for(size_t i=0;i<sources.size();++i){flips|=uint32_t(sources[i].flipV)<<i;if(sources[i].texture)l.AddAllocation(sources[i].texture);}
	const auto uniforms=MakeNativeGroundDecalUniforms(*frame,images.fragmentToLogical,flips);
 const GroundDecalPhysicalUniforms physical{uniforms,depthMapping->crop,depthMapping->logicalOrigin};
 auto ub=physicalDepth?Buffer(l,&physical,sizeof(physical),"Original ground decal matrix and physical depth crop"):Buffer(l,&uniforms,sizeof(uniforms),"Original ground decal matrices and material");p->arguments=Arguments(l,p->coverageTick?6:2,p->worldPositions?8:7,6);[p->arguments setAddress:p->instances->buffer.gpuAddress atIndex:0];[p->arguments setAddress:ub.gpuAddress atIndex:1];
	if (p->worldPositions) {
		p->worldPositionPipeline=impl->worldPositionPipelines[validation];
		l.Retain(p->worldPositionPipeline);
		[p->arguments setTexture:p->worldPositions.gpuResourceID atIndex:7];
	}
    if (p->coverageTick) {
        const std::array<uint32_t,2> dimensions={uint32_t(frame->viewport[2]),uint32_t(frame->viewport[3])};
        const size_t count=(size_t(dimensions[0])*dimensions[1]*coverageTypes+1)*coverageStages;
        p->coverageCounters=[l.Device() newBufferWithLength:count*sizeof(uint32_t) options:MTLResourceStorageModeShared];
        Require(p->coverageCounters!=nil,"Cannot allocate ground decal coverage counters");
        std::memset(p->coverageCounters.contents,0,p->coverageCounters.length);
        l.AddAllocation(p->coverageCounters);
        const auto size=Buffer(l,dimensions.data(),sizeof(dimensions),"Ground decal coverage viewport");
        [p->arguments setAddress:p->coverageCounters.gpuAddress atIndex:2];
        [p->arguments setAddress:size.gpuAddress atIndex:3];
        p->trackPasses=[l.Device() newBufferWithLength:trackPassCapacity*sizeof(std::array<uint32_t,4>)
            options:MTLResourceStorageModeShared];
        const std::array<uint32_t,2> zero{};
        p->trackPassCount=Buffer(l,zero.data(),p->worldPositions?sizeof(zero):sizeof(uint32_t),"GroundDecal ordered track pass count");
        Require(p->trackPasses!=nil,"Cannot allocate GroundDecal track pass records");
        l.AddAllocation(p->trackPasses);
        [p->arguments setAddress:p->trackPasses.gpuAddress atIndex:4];
        [p->arguments setAddress:p->trackPassCount.gpuAddress atIndex:5];
    }
	for(size_t i=0;i<sources.size();++i)if(sources[i].texture)[p->arguments setTexture:sources[i].texture.gpuResourceID atIndex:i];[p->arguments setTexture:p->atlas->texture.gpuResourceID atIndex:6];for(size_t i=0;i<impl->samplers.size();++i)[p->arguments setSamplerState:impl->samplers[i].gpuResourceID atIndex:i];
	static const bool indexG01=[](){const char* v=std::getenv("BAR_NATIVE_GROUND_DECAL_INDEX_G01");return v&&std::strcmp(v,"1")==0;}();
	static const bool probeG01=[](){const char* v=std::getenv("BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE");return v&&std::strcmp(v,"1")==0;}();
	p->probeG01=probeG01;
	if(probeG01)p->probeReasonG01=!indexG01?1u:frame->sampleCount!=1?2u:p->instances->Count()==0?3u:0u;
	if(GroundDecalIndexEligibleG01(indexG01,frame->sampleCount,p->instances->Count())){
		// Allocation nil is an optional optimization miss before encoding.
		// Lease/state failures still propagate: never hide a residency error.
		auto indices=Buffer(l,GroundDecalIndicesG01.data(),GroundDecalIndexBytesG01,"Ground decal G01 immutable cube indices",true);
		if(indices&&GroundDecalIndexBufferValidG01(indices.gpuAddress,indices.length))p->indicesG01=indices;
		else if(probeG01)p->probeReasonG01=4;
	}
	return std::unique_ptr<PreparedGroundDecal>(new PreparedGroundDecal(std::move(p)));
}
}
