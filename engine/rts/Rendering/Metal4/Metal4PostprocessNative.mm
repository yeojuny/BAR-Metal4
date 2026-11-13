/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PostprocessNative.h"
#include "Metal4PostprocessTexturePool181.h"
#include "Rendering/Backend/NativeRetiredResourcePool.h"
#include "Rendering/Backend/NativePostprocessOverwrite157.h"
#include "System/Log/ILog.h"
#include "NativeSampleShadingShaders.h"
#include "Metal4DOFShaderSource.h"
#include "Metal4CASShaderSource.h"
#include "Metal4SepiaShaderSource.h"
#include "Rendering/Backend/NativeCUSUniformData.h"
#include "Rendering/Backend/NativeCASProgram.h"
#include "Metal4NativeEffectState.h"
#include "Metal4ScreenNative.h"
#include "Metal4WorldViewport.h"
#include "Metal4MainAttachments.h"
#include <tuple>
#include <bit>
#include <map>
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <simd/simd.h>

namespace rendering::metal4 {
struct PostprocessUniformStorage {
	__strong id<MTLBuffer> buffer=nil;
};
class PostprocessUniformPool {
public:
	static constexpr size_t ByteLimit=1024u*1024u;
	static constexpr size_t EntryLimit=128;
	void BeginBatch(id<MTLDevice> device) {
		if(owner!=device) { Clear(); owner=device; }
		for(auto& bucket:buckets)bucket.next=0;
	}
	std::shared_ptr<PostprocessUniformStorage> Acquire(id<MTLDevice> device,const void* data,size_t length) {
		Require(device==owner,"Postprocess uniform pool has no matching device batch");
		Require(length==112 || length==160 || length==288,"Postprocess uniform size differs from its three original layouts");
		auto& bucket=buckets[length==112?0:length==160?1:2];
		// Only a pool-owned slot is retired. Every prepared operation and its
		// completion/poison graph retain the same immutable shared owner.
		while(bucket.next<bucket.slots.size()) {
			auto& slot=bucket.slots[bucket.next++];
			if(slot.use_count()!=1)continue;
			auto address=slot->buffer.contents;
			if(address==nullptr)continue;
			std::memcpy(address,data,length);
			return slot;
		}
		// Keep the exact original fresh allocation and failure behavior when
		// owners are live or cache limits are reached; never restrict a draw.
		auto buffer=[device newBufferWithBytes:data length:length options:MTLResourceStorageModeShared];
		Require(buffer!=nil,"Cannot allocate actual DOF uniforms/quad");
		auto slot=std::make_shared<PostprocessUniformStorage>(PostprocessUniformStorage{buffer});
		const auto footprint=std::max(size_t(buffer.length),size_t(buffer.allocatedSize));
		if(buffer.contents!=nullptr && entries<EntryLimit && footprint<=ByteLimit-bytes) {
			bucket.slots.push_back(slot);bucket.next=bucket.slots.size();
			bytes+=footprint;++entries;
		}
		return slot;
	}
	void Clear() noexcept {
		for(auto& bucket:buckets) { bucket.slots.clear(); bucket.next=0; }
		bytes=0;entries=0;owner=nil;
	}
	size_t CachedBytes() const {return bytes;}
	size_t CachedCount() const {return entries;}
private:
	struct Bucket {std::vector<std::shared_ptr<PostprocessUniformStorage>> slots;size_t next=0;};
	// Three exact uniform sizes keep both resource bytes and lookup storage
	// bounded. No weak tokens can acquire ownership after the retirement test.
	std::array<Bucket,3> buckets;
	__strong id<MTLDevice> owner=nil;
	size_t bytes=0,entries=0;
};
} // namespace rendering::metal4

namespace rendering::metal4 {
struct PostprocessArgumentStorage {
	__strong id<MTL4ArgumentTable> table = nil;
};
using PostprocessArgumentLayout = std::array<NSUInteger,3>;
class PostprocessArgumentPool {
public:
	void BeginBatch(id<MTLDevice> device) {
		if (device != owner) {
			pool.Clear();
			owner = device;
		}
		pool.BeginBatch();
	}
	std::shared_ptr<PostprocessArgumentStorage> Acquire(id<MTLDevice> device,
		const PostprocessArgumentLayout& layout, const char* failure) {
		Require(device == owner, "Postprocess argument pool has no matching device batch");
		Require(layout==PostprocessArgumentLayout{2,1,1} || layout==PostprocessArgumentLayout{2,6,6},
			"Postprocess argument layout differs from its two actual descriptors");
		return pool.Acquire(layout, [&] {
			auto descriptor = [MTL4ArgumentTableDescriptor new];
			descriptor.maxBufferBindCount = layout[0];
			descriptor.maxTextureBindCount = layout[1];
			descriptor.maxSamplerStateBindCount = layout[2];
			descriptor.initializeBindings = YES;
			NSError* error = nil;
			auto table = [device newArgumentTableWithDescriptor:descriptor error:&error];
			if (table == nil) Require(false, std::string(failure) + Error(error));
			return PostprocessArgumentStorage{table};
		}, [&](PostprocessArgumentStorage& slot) {
			// Match initializeBindings=YES, including every unused slot. Never
			// mutate bindings while a prepared frame or completion graph owns it.
			for (NSUInteger i = 0; i < layout[0]; ++i) [slot.table setAddress:0 atIndex:i];
			for (NSUInteger i = 0; i < layout[1]; ++i) [slot.table setTexture:MTLResourceID{0} atIndex:i];
			for (NSUInteger i = 0; i < layout[2]; ++i) [slot.table setSamplerState:MTLResourceID{0} atIndex:i];
		});
	}
	void Clear() noexcept { pool.Clear(); owner=nil; }
	std::size_t CachedCount() const { return pool.CachedCount(); }
private:
	__strong id<MTLDevice> owner = nil;
	rendering::NativeRetiredResourcePool<PostprocessArgumentLayout,PostprocessArgumentStorage,256> pool;
};

} // namespace rendering::metal4

@interface M4PostprocessArgumentLifetime : NSObject {
@public
	std::vector<std::shared_ptr<rendering::metal4::PostprocessArgumentStorage>> slots;
	std::vector<std::shared_ptr<rendering::metal4::PostprocessUniformStorage>> uniformSlots;
}
@end
@implementation M4PostprocessArgumentLifetime
@end

namespace rendering::metal4 {
namespace {
constexpr const char* COPY_SHADER = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"COPYMSL(
fragment float4 DOFCopyColor(DOFVarying input [[stage_in]],texture2d<float> source [[texture(0)]],sampler sourceSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 return source.sample(sourceSampler,DOFFlip(input.uv));
}
struct DOFCopyDepthValue { float value [[depth(any)]]; };
fragment DOFCopyDepthValue DOFCopyDepth(DOFVarying input [[stage_in]],depth2d<float> source [[texture(0)]],sampler sourceSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 return {source.sample(sourceSampler,DOFFlip(input.uv))};
}
fragment DOFCopyDepthValue DOFCopyDepth24(DOFVarying input [[stage_in]],depth2d<float> source [[texture(0)]],sampler sourceSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const float d=clamp(source.sample(sourceSampler,DOFFlip(input.uv)),0.0f,1.0f);
 // Float multiplication followed by +0.5 already loses integer ties above
 // 2^23. Convert the exact IEEE input times (2^24-1) with integer arithmetic.
 const uint bits=as_type<uint>(d), exponent=(bits>>23)&255;
 uint q=0;
 if(d>=1.0f)q=0xffffff;
 else if(exponent>=102){const uint shift=150-exponent;const ulong product=ulong((bits&0x7fffff)|0x800000)*ulong(0xffffff);q=uint((product+(ulong(1)<<(shift-1)))>>shift);}
 // Correctly rounded UNORM24 -> float: q/2^24 is exact; for every nonzero
 // q the true q/(2^24-1) lies just beyond its next-half-ULP boundary.
 return {q?as_type<float>(as_type<uint>(float(q)*0x1p-24f)+1):0.0f};
}
)COPYMSL";
struct GPUUniforms { simd_float4x4 projection; simd_float4 eye,resolutionDistance,mouseFocus,autoParams; simd_int4 settings; simd_float4 target; };
static_assert(sizeof(GPUUniforms)==160);
struct CASGPUUniforms {simd_float4x4 cameraViewProjInv,cameraViewInv,cameraProj;simd_float4 viewGeometry;simd_int4 viewRect;float strength,groundY,debugFootprint,pad;simd_float4 target;simd_uint4 alpha;simd_float4 raster;};
static_assert(sizeof(CASGPUUniforms)==288);
struct SepiaGPUUniforms {simd_float4 rows[3],target;simd_int4 viewPos;simd_uint4 alpha;simd_float4 parameters;};
static_assert(sizeof(SepiaGPUUniforms)==112);
MTLPixelFormat Format(NativePostprocessFormat format)
{
	switch(format) {case NativePostprocessFormat::RGBA8:return MTLPixelFormatRGBA8Unorm;case NativePostprocessFormat::RGBA16Float:return MTLPixelFormatRGBA16Float;case NativePostprocessFormat::Depth32:case NativePostprocessFormat::Depth24Unorm:return MTLPixelFormatDepth32Float;case NativePostprocessFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case NativePostprocessFormat::R16Float:return MTLPixelFormatR16Float;case NativePostprocessFormat::R32Float:return MTLPixelFormatR32Float;case NativePostprocessFormat::RG8:return MTLPixelFormatRG8Unorm;case NativePostprocessFormat::R8Unorm:return MTLPixelFormatR8Unorm;}
	throw std::runtime_error("Unknown postprocess attachment format");
}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t length)
{
	auto buffer=[lease.Device()newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];Require(buffer!=nil,"Cannot allocate actual DOF uniforms/quad");lease.AddAllocation(buffer);return buffer;
}
// Current original pass/FBO contract; copy pipeline IDs are kept outside0..7.
constexpr int COPY_COLOR_PIPELINE=8,COPY_DEPTH_PIPELINE=9,COPY_DEPTH24_PIPELINE=10,CAS_PIPELINE=11,SEPIA_PIPELINE=19;
unsigned Outputs(const NativeDOFUniforms& u) {return u.pass==0?2:u.pass==1?3:u.pass==3?4:1;}
unsigned RequiredInputs(const NativeDOFUniforms& u) {
 switch(u.pass){case 0:return (1u<<0)|(1u<<1);case 1:case 3:return (1u<<0)|(1u<<5);
 case 2:return (1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<5);
 case 4:return 63u;case 5:return (1u<<0)|(1u<<1)|(u.quality?(1u<<2):0u);
 case 6:case 7:return 1u;default:throw std::runtime_error("Unknown original31657 DOF pass");}
}
MTLPixelFormat AttachmentFormat(int pass,unsigned index,MTLPixelFormat primary) {
 if(pass>=COPY_COLOR_PIPELINE)return primary;
 if(pass==0)return index==0?MTLPixelFormatRGBA16Float:MTLPixelFormatRG8Unorm;
 if(pass==1||pass==3)return MTLPixelFormatRGBA16Float;
 return pass==5?primary:MTLPixelFormatRGBA8Unorm;
}
std::uint64_t PipelineKey(int pass,int quality,MTLPixelFormat format) {return std::uint64_t(format)<<16|unsigned(pass)<<1|unsigned(quality);}
void CopyPrevious(id<MTL4CommandBuffer> command,id<MTLTexture> from,id<MTLTexture> to)
{
	if(!from)return;
	auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot allocate immutable postprocess previous-version copy encoder");
	[copy barrierAfterQueueStages:MTLStageVertex|MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	[copy copyFromTexture:from toTexture:to];
	[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
}
}
struct PreparedPostprocessFrame::Impl {
	struct Operation {
		std::uint32_t offset=0;
		NativePostprocessStage stage=NativePostprocessStage::Screen;
		std::size_t sourceIndex=0;
		std::shared_ptr<bool> encoded=std::make_shared<bool>(false);
		std::vector<std::shared_ptr<bool>> outputEncoded;
		__strong MTL4RenderPassDescriptor* pass=nil;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLDepthStencilState> depthState=nil;
		MTLViewport viewport{};
		MTLScissorRect scissor{};
		bool noDraw=false;unsigned vertexCount=6;
  std::shared_ptr<const NativeCASRaster> casRaster;
		std::vector<std::pair<std::shared_ptr<bool>,std::shared_ptr<bool>>> reads;
		bool screenOutput=false;
		unsigned resolveMain=0;
		std::vector<std::pair<id<MTLTexture>,id<MTLTexture>>> previous;
		std::vector<PreparedScreenExternalImage> priorScreen;
	};
	std::shared_ptr<const NativePostprocessFrame> owner;
	// The same immutable holder belongs to prepared operations and the runtime
	// completion/poison graph; it outlives all operation argument references.
	__strong M4PostprocessArgumentLifetime* argumentLifetime=nil;
	std::vector<Operation> operations;
	std::map<const NativePostprocessVersion*,id<MTLTexture>> producedImages;
	std::vector<std::shared_ptr<bool>> completed;
	std::map<const NativePostprocessVersion*,std::shared_ptr<bool>> externalEncoded;
	__strong id<MTLTexture> screenColor=nil, screenDepth=nil, screenStencil=nil;
	std::uint32_t screenCommands=0;
	std::shared_ptr<const MainFrameAttachments> main;
	// Borrowed identity, never a new owner. Set only for the caller's audited
	// providers and the validated single-sample main attachment construction.
	const PreparedScreenFrame* casTailScreen=nullptr;
	bool EncodeOperation(id<MTL4CommandBuffer> command,std::size_t operationIndex,
		const PreparedScreenFrame* finalScreenTail=nullptr) const;
};
PreparedPostprocessFrame::PreparedPostprocessFrame():impl(std::make_unique<Impl>()){}
PreparedPostprocessFrame::~PreparedPostprocessFrame()=default;
void PreparedPostprocessFrame::Encode(id<MTL4CommandBuffer> command,const PreparedScreenFrame& screen,std::uint32_t firstOriginalCommand) const
{
	Require(screen.CommandCount()==impl->screenCommands,"Original screen command stream changed after DOF preparation");
	Require(firstOriginalCommand<=impl->screenCommands,"Original screen suffix starts after its recorded prefix");
	std::uint32_t first=firstOriginalCommand;
	auto encodeScreen=[&](std::uint32_t end){
		Require(end>=first && end<=impl->screenCommands,"Source copy/effect would replay an already-consumed screen prefix");
		if(first==end)return;
		auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=impl->screenColor;pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
		// Copies/effects may split the root command stream between a stencil
		// mask and its consumer. Preserve the actual scene attachments across
		// these resumes; effect MRTs and source color-only RTTs stay separate.
		if(impl->screenDepth){pass.depthAttachment.texture=impl->screenDepth;pass.depthAttachment.loadAction=MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;}
		if(impl->screenStencil){pass.stencilAttachment.texture=impl->screenStencil;pass.stencilAttachment.loadAction=MTLLoadActionLoad;pass.stencilAttachment.storeAction=MTLStoreActionStore;}
		pass.renderTargetWidth=impl->screenColor.width;pass.renderTargetHeight=impl->screenColor.height;
		pass.defaultRasterSampleCount=impl->screenColor.sampleCount;
		auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot resume original ordered screen draws");
		[encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		screen.EncodeRange(encoder,first,end);[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];first=end;
	};
	struct Event {std::uint32_t offset;std::uint64_t sequence;bool target;std::size_t index;};std::vector<Event> events;
	for(const auto& op:impl->operations){
		if(op.stage!=NativePostprocessStage::Screen){Require(*op.encoded,"Actual world source copy was not encoded at its original sequence");continue;}
		events.push_back({op.offset,impl->owner->operations[op.sourceIndex].commandSequence,false,op.sourceIndex});
	}
	for(const auto& target:screen.DeferredTargets())events.push_back({target.offset,target.sequence,true,target.index});
	std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.offset!=b.offset?a.offset<b.offset:a.sequence<b.sequence;});
	for(const auto& event:events){
		encodeScreen(event.offset);
		if(event.target)screen.EncodeDeferredTarget(command,event.index);
		else if(impl->EncodeOperation(command,event.index,&event==&events.back()?&screen:nullptr))
			first=impl->screenCommands;
	}
	encodeScreen(impl->screenCommands);
}
void PreparedPostprocessFrame::EncodeOperation(id<MTL4CommandBuffer> command,std::size_t operationIndex) const
{
	impl->EncodeOperation(command,operationIndex);
}
bool PreparedPostprocessFrame::Impl::EncodeOperation(id<MTL4CommandBuffer> command,std::size_t operationIndex,
	const PreparedScreenFrame* finalScreenTail) const
{
	auto found=std::find_if(operations.begin(),operations.end(),[&](const auto& op){return op.sourceIndex==operationIndex;});
	Require(command!=nil && found!=operations.end(),"Missing actual source-copy/effect operation");const auto& op=*found;
	Require(!*op.encoded,"Source-copy/effect operation encoded more than once");
	for(const auto& read:op.reads)Require(*read.first||*read.second,"Original DOF read/copy precedes actual producer encode or retirement");
	for(const auto& image:op.priorScreen)image.requireReadable();
	// Source snapshots are refreshed at this operation, after preceding root/world writes.
	if(op.resolveMain)main->EncodeResolve(command,static_cast<MainReadImages>(op.resolveMain));
	for(const auto& pair:op.previous)CopyPrevious(command,pair.first,pair.second);
	if(op.noDraw){*op.encoded=true;for(const auto& flag:op.outputEncoded)*flag=true;return false;}
	// Only the final sorted event can offer a tail. CAS and encodeScreen use
	// the same private main C/D/S Load/Store construction and default pass
	// properties; re-audit this contract if either constructor changes. The
	// provider permission is tied to this exact screen, not just its count.
	const bool appendTail=finalScreenTail && finalScreenTail==casTailScreen &&
		op.stage==NativePostprocessStage::Screen && op.screenOutput && op.offset<screenCommands &&
		owner->operations[op.sourceIndex].kind==NativePostprocessOperationKind::CAS &&
		op.outputEncoded.empty() && op.previous.empty() && op.priorScreen.empty() && op.resolveMain==0;
	auto encoder=[command renderCommandEncoderWithDescriptor:op.pass];Require(encoder!=nil,"Cannot allocate original effect/copy MRT encoder");
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder setRenderPipelineState:op.pipeline];[encoder setViewport:op.viewport];[encoder setScissorRect:op.scissor];
	[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setStencilReferenceValue:0];[encoder setDepthStencilState:op.depthState];
 if(op.casRaster){
  const auto& state=op.casRaster->world;effectstate::RasterState(encoder,state,{op.viewport,op.scissor});
  [encoder setDepthClipMode:state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
  const auto cull=state.cullEnabled?state.cull:NativeWorldCull::None;
  [encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
 }

	[encoder setArgumentTable:op.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:op.vertexCount];
	if(appendTail){
		// CAS has no output versions. Preserve its logical encoded state even
		// if a later UI readability check fails; completion remains unchanged.
		*op.encoded=true;
		// This still waits on earlier encoders, including their vertex work.
		// Separate read-only inputs leave only ordinary ordered attachment
		// access between CAS and UI, not a new shader-memory dependency.
		[encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		try{finalScreenTail->EncodeRange(encoder,op.offset,screenCommands);}
		catch(...){[encoder endEncoding];throw;}
	}
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
	if(!appendTail){*op.encoded=true;for(const auto& flag:op.outputEncoded)*flag=true;}
	return appendTail;
}
void PreparedPostprocessFrame::MarkExternalEncoded(const std::shared_ptr<const NativePostprocessVersion>& version) const
{
	Require(version!=nullptr,"Missing actually encoded external output");auto found=impl->externalEncoded.find(version.get());
	Require(found!=impl->externalEncoded.end() && !*found->second,"External output is undeclared or encoded twice");*found->second=true;
}
id<MTLTexture> PreparedPostprocessFrame::ProducedImage(const std::shared_ptr<const NativePostprocessVersion>& version) const
{
	Require(version!=nullptr,"Missing requested postprocess version");auto found=impl->producedImages.find(version.get());
	Require(found!=impl->producedImages.end(),"Requested image is not produced by this owned postprocess frame");return found->second;
}
void PreparedPostprocessFrame::DidComplete() const {
	for(const auto& op:impl->operations)Require(*op.encoded,"Cannot retire an unexecuted source-copy/effect operation");
	for(const auto& [version,encoded]:impl->externalEncoded)Require(*encoded,"Cannot retire an unexecuted external image declaration");
	for(const auto& completed:impl->completed)*completed=true;
}
struct PostprocessGPU::Impl {
	bool reported157=false;
	std::unique_ptr<PostprocessTexturePool181> texturePool181=std::make_unique<PostprocessTexturePool181>();
	std::uint64_t freshIneligible181=0;
	std::uint64_t ownerRequests181=0,ownerRegistrations181=0;
	void PinTexture181(RuntimeLease& lease,const std::shared_ptr<PostprocessTextureStorage181>& storage) {
		++ownerRequests181;
		if(lease.RetainPostprocessTexture181(storage)) ++ownerRegistrations181;
	}
	~Impl() {
		if(texturePool181 && (texturePool181->requests || freshIneligible181)) {
			const auto& p=*texturePool181;
			LOG("[NativePostprocessTexture181] revision=2 eligible_versions=%llu reused=%llu created=%llu uncached=%llu fresh_ineligible_versions=%llu owner_requests=%llu owner_registrations=%llu holder_allocations=0 peak_cached_entries=%zu peak_cached_bytes=%zu byte_limit=%zu entry_limit=%zu scope=owner_lifetime",
				(unsigned long long)p.requests,(unsigned long long)p.reused,(unsigned long long)p.created,
				(unsigned long long)p.uncached,(unsigned long long)freshIneligible181,
				(unsigned long long)ownerRequests181,(unsigned long long)ownerRegistrations181,p.peakEntries,p.peakBytes,
				PostprocessTexturePool181::ByteLimit,PostprocessTexturePool181::EntryLimit);
		}
	}
	PostprocessArgumentPool argumentPool;
	PostprocessUniformPool uniformPool;
	id<MTLBuffer> UniformBuffer(RuntimeLease& lease,M4PostprocessArgumentLifetime* lifetime,const void* data,size_t length) {
		auto slot=uniformPool.Acquire(lease.Device(),data,length);
		auto buffer=slot->buffer;
		lifetime->uniformSlots.push_back(std::move(slot));
		lease.AddAllocation(buffer);
		return buffer;
	}
	std::map<std::array<uint32_t,8>,id<MTLBuffer>> immutableQuads;
	id<MTLBuffer> QuadBuffer(RuntimeLease& lease,const NativePostprocessQuad& quad) {
		static_assert(sizeof(NativePostprocessQuad)==sizeof(std::array<uint32_t,8>));
		const auto key=std::bit_cast<std::array<uint32_t,8>>(quad);
		const auto found=immutableQuads.find(key);
		if(found!=immutableQuads.end() && found->second.device==lease.Device()) {
			lease.AddAllocation(found->second);return found->second;
		}
		auto value=Buffer(lease,&quad,sizeof(quad));
		// Storage is never overwritten. The current lease retains evictees.
		if(immutableQuads.size()>=64)immutableQuads.erase(immutableQuads.begin());
		immutableQuads.insert_or_assign(key,value);return value;
	}
	struct Image { std::weak_ptr<const NativePostprocessVersion> owner;__strong id<MTLTexture> image=nil;std::shared_ptr<PostprocessTextureStorage181> storage181;std::shared_ptr<bool> completed=std::make_shared<bool>(false);std::shared_ptr<bool> encoded=std::make_shared<bool>(false); };
	std::map<const NativePostprocessVersion*,Image> images;
 // Allocation-only dimension queries belong to this SAME GPU resource owner.
 // They have no contents-version status and cannot satisfy Retired/ImageFor.
 struct Allocation {std::weak_ptr<const NativePostprocessImage> owner;__strong id<MTLTexture> image=nil;};
 std::map<std::uint64_t,Allocation> allocations;
 void PruneAllocations(){
  for(auto i=allocations.begin();i!=allocations.end();)if(i->second.owner.expired())i=allocations.erase(i);else ++i;
 }
 id<MTLTexture> AllocationFor(RuntimeLease& lease,std::shared_ptr<const NativePostprocessImage> actual){
  Require(actual!=nullptr,"Scalar textureSize requires its actual allocation owner");
  ValidateNativePostprocessImage(*actual);
  Require(actual->renderToTexture&&(actual->format==NativePostprocessFormat::R16Float||actual->format==NativePostprocessFormat::R32Float||actual->format==NativePostprocessFormat::R8Unorm),
   "Allocation-only textureSize requires an original scalar R8/R16F/R32F color target");
  Require(actual->width<=lease.Capabilities().maxTextureDimension2D&&actual->height<=lease.Capabilities().maxTextureDimension2D,
   "Scalar allocation exceeds the actual native device extent");
  PruneAllocations();
  if(auto found=allocations.find(actual->object);found!=allocations.end()){
   const auto native=found->second.image;
   Require(found->second.owner.lock()==actual&&native&&native.device==lease.Device()&&
    native.textureType==MTLTextureType2D&&native.sampleCount==1&&native.pixelFormat==Format(actual->format)&&
    native.width==actual->width&&native.height==actual->height,
    "Scalar allocation identity refers to another actual owner/device/storage");
   lease.AddAllocation(native);return native;
  }
  auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Format(actual->format)
   width:actual->width height:actual->height mipmapped:NO];
  descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
  auto native=[lease.Device()newTextureWithDescriptor:descriptor];
  Require(native!=nil,"Cannot allocate actual scalar RTT storage for textureSize");
  native.label=String("Original scalar allocation "+std::to_string(actual->object)+" undefined contents; dimensions only");
  Allocation entry;entry.owner=actual;entry.image=native;allocations.emplace(actual->object,std::move(entry));
  lease.AddAllocation(native);return native;
 }

	std::shared_ptr<const NativePostprocessFrame> declaredFrame;
	std::shared_ptr<const NativePostprocessPlan> declaredPlan;
	CPUImageProvider cpuImages;
	__strong id<MTLLibrary> library=nil;
	bool validation=false;
	MTLPixelFormat screenFormat=MTLPixelFormatInvalid;
	std::map<std::tuple<std::uint64_t,std::uint32_t,bool>,id<MTLRenderPipelineState>> pipelines;
	std::map<unsigned,id<MTLSamplerState>> samplers;
	__strong id<MTLDepthStencilState> copyDepth=nil;
	void Compile(RuntimeLease& lease,MTLPixelFormat format,bool verify,double& compileMs)
	{
		const auto start=Clock::now();NSError* error=nil;
		auto source=std::string(NATIVE_DOF_SHADER)+COPY_SHADER+NATIVE_CAS_SHADER+NATIVE_SEPIA_SHADER;auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(source);
		auto candidate=lease.NewLibrary(descriptor,&error);Require(candidate!=nil,"Original31657 eight-pass DOF shader compilation failed: "+Error(error));
		library=candidate;validation=verify;screenFormat=format;
		for(int pass=0;pass<=7;++pass)Pipeline(lease,pass,1,pass==5?format:pass==0||pass==1||pass==3?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA8Unorm);
		Pipeline(lease,5,0,format);
  for(int variant=0;variant<8;++variant)Pipeline(lease,CAS_PIPELINE+variant,0,format);
  Pipeline(lease,SEPIA_PIPELINE,0,format);
		Pipeline(lease,COPY_COLOR_PIPELINE,0,MTLPixelFormatRGBA8Unorm);Pipeline(lease,COPY_DEPTH_PIPELINE,0,MTLPixelFormatDepth32Float);Pipeline(lease,COPY_DEPTH24_PIPELINE,0,MTLPixelFormatDepth32Float);
		auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionAlways;depth.depthWriteEnabled=YES;copyDepth=[lease.Device()newDepthStencilStateWithDescriptor:depth];Require(copyDepth!=nil,"Cannot create original depth-copy write state");
		compileMs+=Milliseconds(start);
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,int pass,int quality,MTLPixelFormat format,const NativeCASRaster* raster=nullptr,std::uint32_t samples=1)
	{
  std::uint64_t stateBits=0;
  if(pass>=CAS_PIPELINE){const auto mask=raster?raster->world.colorWrite:std::array<bool,4>{true,true,true,true};
   for(unsigned i=0;i<4;++i)stateBits|=std::uint64_t(mask[i])<<(40+i);
   if(raster)stateBits|=std::uint64_t(raster->surface.blend.colorEquation)<<44|std::uint64_t(raster->surface.blend.alphaEquation)<<47;
  }
		const bool coverage=raster && raster->world.alphaToCoverage;
		const auto key=std::make_tuple(PipelineKey(pass,quality,format)|stateBits,lease.SampleShadingKey(samples),coverage);auto found=pipelines.find(key);if(found!=pipelines.end()){lease.Retain(found->second);return found->second;}
		Require(library!=nil,"DOF native library is not initialized");
		auto descriptor=[MTL4RenderPipelineDescriptor new];auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=pass==SEPIA_PIPELINE?@"SepiaVertex":pass>=CAS_PIPELINE?@"CASVertex":pass==0?@"DOFFilterVertex":@"DOFVertex";descriptor.vertexFunctionDescriptor=vertex;
		auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;
		if(pass==SEPIA_PIPELINE)fragment.name=@"SepiaFragment";
		else if(pass>=CAS_PIPELINE){Require(pass<CAS_PIPELINE+8,"Unknown CAS source variant");fragment.name=String("CASFragment"+std::to_string(pass-CAS_PIPELINE));}
  else if(pass==COPY_COLOR_PIPELINE)fragment.name=@"DOFCopyColor";else if(pass==COPY_DEPTH_PIPELINE)fragment.name=@"DOFCopyDepth";else if(pass==COPY_DEPTH24_PIPELINE)fragment.name=@"DOFCopyDepth24";
		else{Require(pass>=0&&pass<=7&&(quality==1||(pass==5&&quality==0)),"DOF pipeline differs from its original fixed-pass/quality program");fragment.name=String("DOFFragmentPass"+std::to_string(pass)+(quality?"HQ":"LQ"));}
		descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		descriptor.rasterSampleCount=samples;
		descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		if(pass==COPY_DEPTH_PIPELINE||pass==COPY_DEPTH24_PIPELINE)descriptor.colorAttachments[0].pixelFormat=MTLPixelFormatInvalid;
		else{NativeDOFUniforms u;u.pass=pass;u.quality=quality;const unsigned count=(pass==COPY_COLOR_PIPELINE || pass>=CAS_PIPELINE)?1:Outputs(u);for(unsigned i=0;i<count;++i){descriptor.colorAttachments[i].pixelFormat=AttachmentFormat(pass,i,format);descriptor.colorAttachments[i].blendingState=MTL4BlendStateDisabled;}}
  if(pass>=CAS_PIPELINE && pass<SEPIA_PIPELINE && ((pass-CAS_PIPELINE)&2)) {
   auto a=descriptor.colorAttachments[0];a.blendingState=MTL4BlendStateEnabled;
   a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorOne;
   a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorSource1Color;
   auto equation=[](ScreenBlendEquation value){switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown original CAS blend equation");};
   a.rgbBlendOperation=raster?equation(raster->surface.blend.colorEquation):MTLBlendOperationAdd;
   a.alphaBlendOperation=raster?equation(raster->surface.blend.alphaEquation):MTLBlendOperationAdd;
  }
  if(pass>=CAS_PIPELINE && raster){const auto& m=raster->world.colorWrite;descriptor.colorAttachments[0].writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));}

		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		NSError* error=nil;auto pipeline=lease.NewRenderPipeline(descriptor,&error);Require(pipeline!=nil,"Cannot compile actual original DOF pass: "+Error(error));pipelines.emplace(key,pipeline);lease.Retain(pipeline);return pipeline;
	}
	bool Retired(const NativePostprocessVersion& version) const
	{
		auto i=images.find(&version);return i!=images.end()&&!i->second.owner.expired()&&*i->second.completed;
	}
 // CPU preparation may pin an already encoded immutable input from N.
 // This is NOT retirement/readability publication: N's real event+feedback
 // still precede N+1 GPU submission, and original per-version read checks stay.
 // CPU encoding may refer to the pinned image; it never publishes completion.
 bool PreparationInput(const NativePostprocessVersion& version,RuntimeLease& lease) const
 {
  if(Retired(version))return true;
  if(!lease.PreparingWhilePending())return false;
  auto i=images.find(&version);if(i==images.end())return false;
  auto owner=i->second.owner.lock();
  if(owner.get()!=&version || !*i->second.encoded)return false;
  // Pin the exact logical version and its immutable allocation's completion
  // flag, not this mutable cache or a later lookup by reused raw address.
  // ImageFor separately pins the physical texture in this lease's GPU graph.
  // Encoding is preparation permission only; event+feedback retirement must
  // publish this exact producer completion before the consumer can submit.
  const auto completed=i->second.completed;
  const auto* key=owner.get();
  return lease.OrderAfterPreviousSubmission(key,[owner=std::move(owner),completed]{
   Require(bool(owner)&&*completed,"Pending postprocess input did not complete before consumer submission");
  });
 }
 struct PreparationContext {Impl* cache;RuntimeLease* lease;};
 static bool PreparationInputCallback(const NativePostprocessVersion& v,void* raw){
  const auto& state=*static_cast<PreparationContext*>(raw);return state.cache->PreparationInput(v,*state.lease);
 }
	id<MTLTexture> ImageFor(RuntimeLease& lease,const std::shared_ptr<const NativePostprocessVersion>& version,bool output,std::vector<std::shared_ptr<bool>>& completed,bool reusableCopy181=false)
	{
		Require(version&&version->image,"Missing actual postprocess image version");
		if(HasNativePostprocessCPUContents(*version)){
			Require(!output&&bool(cpuImages),"Immediate TexImage needs the existing native CPU image provider");
			auto native=cpuImages(lease,version->sourceImage);
			Require(native&&native.textureType==MTLTextureType2D&&native.sampleCount==1&&
				native.pixelFormat==Format(version->image->format)&&native.width==version->image->width&&native.height==version->image->height&&
				(native.usage&MTLTextureUsageShaderRead),"CPU TexImage provider returned another storage allocation");
			lease.AddAllocation(native);return native;
		}
		auto found=images.find(version.get());
		if(found!=images.end()){
			Require(found->second.owner.lock()==version,"Postprocess image address lifetime changed");
			Require(found->second.image.device==lease.Device(),"Postprocess image version belongs to another Metal device");
			Require(!output||!*found->second.completed,"Already-completed postprocess packet cannot be resubmitted");
			if(found->second.storage181)PinTexture181(lease,found->second.storage181);
			lease.AddAllocation(found->second.image);if(output)completed.push_back(found->second.completed);return found->second.image;
		}
		Require(output,"Missing producer for unretired postprocess image");const auto& image=*version->image;
		Require(image.width<=lease.Capabilities().maxTextureDimension2D&&image.height<=lease.Capabilities().maxTextureDimension2D,"Original DOF attachment exceeds actual device extent");
		const auto create=[&]{
			auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Format(image.format) width:image.width height:image.height mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
			auto native=[lease.Device()newTextureWithDescriptor:descriptor];Require(native!=nil,"Cannot allocate original DOF immutable attachment");return native;
		};
		Image entry;entry.owner=version;
		if(reusableCopy181) {
			entry.storage181=texturePool181->Acquire(lease.Device(),{Format(image.format),image.width,image.height},version,create);
			entry.image=entry.storage181->texture;PinTexture181(lease,entry.storage181);
		} else {entry.image=create();++freshIneligible181;}
		auto native=entry.image;native.label=String("Original DOF image "+std::to_string(image.object)+" generation "+std::to_string(version->generation));
		auto flag=entry.completed;images.emplace(version.get(),std::move(entry));lease.AddAllocation(native);completed.push_back(std::move(flag));return native;
	}
	id<MTLSamplerState> Sampler(RuntimeLease& lease,NativePostprocessSampler value)
	{
		Require(value.wrapS!=NativePostprocessWrap::LegacyClamp&&value.wrapT!=NativePostprocessWrap::LegacyClamp,"DOF family does not sample a GL_CLAMP image; generic clamp-border consumer is pending");
		const unsigned key=unsigned(value.minFilter)|(unsigned(value.magFilter)<<1)|(unsigned(value.wrapS)<<2)|(unsigned(value.wrapT)<<4);
		auto found=samplers.find(key);if(found!=samplers.end()){lease.Retain(found->second);return found->second;}
		auto d=[MTLSamplerDescriptor new];d.minFilter=value.minFilter==NativePostprocessFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.magFilter=value.magFilter==NativePostprocessFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
		d.mipFilter=MTLSamplerMipFilterNotMipmapped;d.sAddressMode=value.wrapS==NativePostprocessWrap::Repeat?MTLSamplerAddressModeRepeat:MTLSamplerAddressModeClampToEdge;d.tAddressMode=value.wrapT==NativePostprocessWrap::Repeat?MTLSamplerAddressModeRepeat:MTLSamplerAddressModeClampToEdge;d.supportArgumentBuffers=YES;
		auto native=[lease.Device()newSamplerStateWithDescriptor:d];Require(native!=nil,"Cannot create actual original DOF sampler");samplers.emplace(key,native);lease.Retain(native);return native;
	}
};
PostprocessGPU::PostprocessGPU():impl(std::make_unique<Impl>()){}
PostprocessGPU::~PostprocessGPU(){SetNativeSepiaConsumerAvailable(false);SetNativeCASConsumerAvailable(false);SetNativeDOFConsumerAvailable(false);SetNativeScreenCopyConsumerAvailable(false);}
void PostprocessGPU::Initialize(RuntimeLease& lease,MTLPixelFormat format,bool validation,double& compileMs)
{
	Require(format==MTLPixelFormatRGBA8Unorm||format==MTLPixelFormatBGRA8Unorm,"Original DOF currently requires actual non-sRGB RGBA8/BGRA8 screen storage");
	auto candidate=std::make_unique<Impl>();candidate->Compile(lease,format,validation,compileMs);
	// Changing validation/paired PSOs must not erase retired immutable image
	// generations that are still referenced by a later partial update.
	candidate->images=std::move(impl->images);candidate->texturePool181=std::move(impl->texturePool181);candidate->freshIneligible181=impl->freshIneligible181;candidate->ownerRequests181=impl->ownerRequests181;candidate->ownerRegistrations181=impl->ownerRegistrations181;candidate->allocations=std::move(impl->allocations);candidate->declaredFrame=std::move(impl->declaredFrame);candidate->declaredPlan=std::move(impl->declaredPlan);candidate->cpuImages=std::move(impl->cpuImages);impl=std::move(candidate);SetNativeDOFConsumerAvailable(true);SetNativeCASConsumerAvailable(true);SetNativeSepiaConsumerAvailable(true);
}
void PostprocessGPU::ClearResources(){impl->immutableQuads.clear();impl->argumentPool.Clear();impl->uniformPool.Clear();impl->declaredFrame.reset();impl->declaredPlan.reset();impl->images.clear();impl->texturePool181->Clear();impl->allocations.clear();}
void PostprocessGPU::SetCPUImageProvider(CPUImageProvider provider){impl->cpuImages=std::move(provider);}
void PostprocessGPU::PrepareFrameImages(RuntimeLease& lease,std::shared_ptr<const NativePostprocessFrame> frame,std::uint64_t sourceDrawFrame)
{
	Require(frame && frame->sourceDrawFrame==sourceDrawFrame,"Copied-image packet does not belong to the actual captured world draw");
	// This is also called before generic image preparation. Expired version
	// addresses may already have been reused by the current Lua frame, so prune
	// before either ordered validation or the first immutable-image allocation.
	for(auto i=impl->images.begin();i!=impl->images.end();)if(i->second.owner.expired())i=impl->images.erase(i);else ++i;
 impl->PruneAllocations();
	Impl::PreparationContext context{impl.get(),&lease};
 // Static ordering is scoped to this exact immutable owner, never serial alone.
 // Availability and lease registration below still run on every preparation.
 auto plan=impl->declaredPlan;
 if(!plan||!plan->MatchesRoot(frame))plan=BuildNativePostprocessPlan(frame);
 ValidateNativePostprocessFrame(*plan,Impl::PreparationInputCallback,&context);
	std::vector<std::shared_ptr<bool>> ignored;
	for(const auto& ref:plan->Order()){
		const auto& outputs=ref.external?frame->externalOutputs[ref.index].outputs:frame->operations[ref.index].outputs;
		// Only this existing fixed-copy proof guarantees every pixel is written
		// before the version's first read. DOF discard, external and partial writes
		// retain their original fresh storage; no clear or copy is added/removed.
		const bool reusableCopy181=!ref.external && NativePostprocessOverwrite157(frame->operations[ref.index],frame->logicalWidth,frame->logicalHeight);
		for(const auto& output:outputs)impl->ImageFor(lease,output,true,ignored,reusableCopy181);
	}
	impl->declaredFrame=std::move(frame);impl->declaredPlan=std::move(plan);
}
id<MTLTexture> PostprocessGPU::ResolveAllocation(RuntimeLease& lease,std::shared_ptr<const NativePostprocessImage> actualAllocation)
{
 return impl->AllocationFor(lease,std::move(actualAllocation));
}
id<MTLTexture> PostprocessGPU::ResolveImage(RuntimeLease& lease,const std::shared_ptr<const NativePostprocessVersion>& version,std::optional<NativePostprocessConsumer> consumer)
{
	Require(version!=nullptr,"Missing shared native image version");
	if(!HasNativePostprocessCPUContents(*version)&&!impl->PreparationInput(*version,lease)){
		Require(impl->declaredFrame&&impl->declaredPlan&&impl->declaredPlan->MatchesRoot(impl->declaredFrame),"Shared native image has no actual source frame");
		ValidateNativePostprocessConsumer(*impl->declaredPlan,*version,consumer,[](const NativePostprocessVersion& v,void* state){return static_cast<Impl*>(state)->Retired(v);},impl.get());
	}
	std::vector<std::shared_ptr<bool>> ignored;return impl->ImageFor(lease,version,false,ignored);
}
void PostprocessGPU::RequireReadable(const std::shared_ptr<const NativePostprocessVersion>& version) const
{
	Require(version&&version->image,"Missing actual postprocess read version");
	if(HasNativePostprocessCPUContents(*version))return;
	const auto found=impl->images.find(version.get());
	Require(found!=impl->images.end()&&found->second.owner.lock()==version&&
		(*found->second.completed||*found->second.encoded),"Original postprocess image read precedes its actual producer encoding/retirement");
}
std::unique_ptr<PreparedPostprocessFrame> PostprocessGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativePostprocessFrame> frame,
	id<MTLTexture> color,id<MTLTexture> depth,const PreparedScreenFrame& originalScreen,bool validation,double& compileMs,id<MTLTexture> actualStencil,std::shared_ptr<const MainFrameAttachments> main,bool independentRootShaderResources)
{
	Require(frame&&color&&depth,"DOF requires its actual world color/depth");
	if(main)Require(main->Color()==color && main->Depth()==depth && main->Stencil()==actualStencil &&
		main->Samples()==color.sampleCount,"DOF main owner differs from its actual output attachments");
	else Require(color.sampleCount==1&&depth.sampleCount==1,"Multisample DOF output requires its ordered main read owner");
	const auto readColor=main?main->ReadColor():color;
	const auto readDepth=main?main->ReadDepth():depth;
	Require(readColor && readDepth && readColor.sampleCount==1 && readDepth.sampleCount==1 &&
		readColor.width==color.width && readColor.height==color.height && readColor.pixelFormat==color.pixelFormat &&
		readDepth.width==depth.width && readDepth.height==depth.height && readDepth.pixelFormat==depth.pixelFormat,
		"DOF read images differ from their actual main output extent/format");
	const auto commandCount=originalScreen.CommandCount();const auto originalViewport=originalScreen.Viewport();
	Require(originalViewport.width>0&&originalViewport.height>0,"DOF requires the actual nonempty original screen viewport");
	Require((readColor.usage&MTLTextureUsageShaderRead)&&(readDepth.usage&MTLTextureUsageShaderRead),"Actual framebuffer must permit original color/depth screen copies");
	Require(depth.pixelFormat==MTLPixelFormatDepth32Float,"Original screen depth copy requires actual world depth32");
	if(!impl->library||impl->screenFormat!=color.pixelFormat||impl->validation!=validation)Initialize(lease,color.pixelFormat,validation,compileMs);
	PrepareFrameImages(lease,frame,frame->sourceDrawFrame);
	for(auto attachment:{depth,actualStencil})if(attachment)
		Require(attachment.width==color.width && attachment.height==color.height && attachment.sampleCount==color.sampleCount,
			"Root screen attachment extent/sample count differs from actual scene color");
	if(actualStencil)Require(actualStencil.pixelFormat==MTLPixelFormatStencil8,"Unsupported actual root stencil attachment format");
	auto prepared=std::make_unique<PreparedPostprocessFrame>();prepared->impl->main=main;prepared->impl->owner=frame;prepared->impl->screenColor=color;prepared->impl->screenDepth=depth;prepared->impl->screenStencil=actualStencil;prepared->impl->screenCommands=commandCount;lease.AddAllocation(color);lease.AddAllocation(depth);if(actualStencil)lease.AddAllocation(actualStencil);
	// The validated main owner fixes exact C/D/S identities and their private
	// descriptor construction. Multisample and generic/custom-provider paths
	// keep their existing separate encoders. This adds no resource traversal.
	if(independentRootShaderResources && main && main->Samples()==1)
		prepared->impl->casTailScreen=&originalScreen;
	impl->argumentPool.BeginBatch(lease.Device());
	impl->uniformPool.BeginBatch(lease.Device());
	size_t omittedCopies157=0,retainedCopies157=0;uint64_t omittedBytes157=0;
	for(std::size_t operationIndex=0;operationIndex<frame->operations.size();++operationIndex){
		const auto& operation=frame->operations[operationIndex];
		const bool overwrite157=NativePostprocessOverwrite157(operation,frame->logicalWidth,frame->logicalHeight);
		Require(operation.screenCommandOffset<=commandCount,"DOF insertion exceeds original screen stream");PreparedPostprocessFrame::Impl::Operation out;out.offset=operation.screenCommandOffset;out.screenOutput=operation.screenOutput;out.stage=operation.stage;out.sourceIndex=operationIndex;
		out.pass=[MTL4RenderPassDescriptor new];unsigned width=unsigned(color.width),height=unsigned(color.height);
		std::vector<id<MTLTexture>> outputs;
		for(std::size_t i=0;i<operation.outputs.size();++i){auto image=impl->ImageFor(lease,operation.outputs[i],true,prepared->impl->completed);outputs.push_back(image);prepared->impl->producedImages.emplace(operation.outputs[i].get(),image);out.outputEncoded.push_back(impl->images.at(operation.outputs[i].get()).encoded);width=unsigned(image.width);height=unsigned(image.height);
			// Source history remains owned and validated. Only the old-pixel GPU
			// dependency is absent when this fixed copy overwrites every pixel.
			const bool preserve=!overwrite157&&(operation.previous[i]||operation.priorScreen);
			if(operation.previous[i]&&!overwrite157){auto previous=impl->ImageFor(lease,operation.previous[i],false,prepared->impl->completed);out.previous.emplace_back(previous,image);}
			if(overwrite157&&(operation.previous[i]||operation.priorScreen)){++omittedCopies157;omittedBytes157+=uint64_t(width)*height*4;}
			if(preserve)++retainedCopies157;
			if(operation.kind==NativePostprocessOperationKind::CopyDepth){out.pass.depthAttachment.texture=image;out.pass.depthAttachment.loadAction=preserve?MTLLoadActionLoad:MTLLoadActionDontCare;out.pass.depthAttachment.storeAction=MTLStoreActionStore;}
			else{out.pass.colorAttachments[i].texture=image;out.pass.colorAttachments[i].loadAction=preserve?MTLLoadActionLoad:MTLLoadActionDontCare;out.pass.colorAttachments[i].storeAction=MTLStoreActionStore;}
		}
		if(operation.priorScreen&&!overwrite157) {
			auto prior=originalScreen.ResolvePreviousImage(lease,operation.priorScreen,
				NativePostprocessConsumer{operation.stage,operation.sequence,operation.screenCommandOffset,operation.commandSequence});
			Require(outputs.size()==1 && prior.texture.width==outputs[0].width && prior.texture.height==outputs[0].height &&
				prior.texture.pixelFormat==outputs[0].pixelFormat,"Partial panel copy changed the original prior allocation");
			out.previous.emplace_back(prior.texture,outputs[0]);out.priorScreen.push_back(std::move(prior));
		}
		if(operation.screenOutput){out.pass.defaultRasterSampleCount=color.sampleCount;out.pass.colorAttachments[0].texture=color;out.pass.colorAttachments[0].loadAction=MTLLoadActionLoad;out.pass.colorAttachments[0].storeAction=MTLStoreActionStore;}
		out.pass.renderTargetWidth=width;out.pass.renderTargetHeight=height;out.viewport=operation.screenOutput?originalViewport:MTLViewport{0,0,double(width),double(height),0,1};out.scissor={0,0,width,height};
		if(operation.kind==NativePostprocessOperationKind::DOF || operation.kind==NativePostprocessOperationKind::CAS || operation.kind==NativePostprocessOperationKind::Sepia){
		 Require(operation.viewport.has_value(),"Original31657 DOF requires its captured gl.Viewport");const auto& v=*operation.viewport;
		 if(!v.width||!v.height)out.noDraw=true;
		 else{Require(v.width<=INT32_MAX&&v.height<=INT32_MAX,"DOF viewport exceeds original GLsizei");auto mapped=MapWorldViewport({v.x,v.y,int32_t(v.width),int32_t(v.height)},operation.screenOutput?frame->logicalWidth:width,operation.screenOutput?frame->logicalHeight:height,width,height);
		  if(!mapped)out.noDraw=true;else{const auto& vp=mapped->viewport;const auto& sc=mapped->scissor;out.viewport={vp[0],vp[1],vp[2],vp[3],0,1};out.scissor={sc[0],sc[1],sc[2],sc[3]};}
		 }
		}
		if(!prepared->impl->argumentLifetime) {
			prepared->impl->argumentLifetime=[M4PostprocessArgumentLifetime new];
			lease.Retain(prepared->impl->argumentLifetime);
		}
		const NSUInteger textureSlots=operation.kind==NativePostprocessOperationKind::DOF?6:1;
		auto argumentOwner=impl->argumentPool.Acquire(lease.Device(),{2,textureSlots,textureSlots},
			"Cannot allocate original DOF binding table: ");
		auto args=argumentOwner->table;
		prepared->impl->argumentLifetime->slots.push_back(std::move(argumentOwner));
		lease.Retain(args);out.arguments=args;
		NativePostprocessQuad quad=operation.quad;
		if(operation.kind==NativePostprocessOperationKind::Sepia){
			const auto& u=operation.sepia;out.casRaster=u.raster;Require(bool(out.casRaster),"Sepia raster missing");
			const auto& state=u.raster->world;const auto& surface=u.raster->surface;
			WorldCamera camera{};const auto& v=*operation.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};
			auto raster=effectstate::Viewport(camera,width,height,frame->logicalWidth,frame->logicalHeight,surface.scissor);
			if(!raster||(state.cullEnabled&&state.cull==NativeWorldCull::FrontAndBack))out.noDraw=true;
			else{out.viewport=raster->viewport;out.scissor=raster->scissor;}
			out.pass.depthAttachment.texture=depth;out.pass.depthAttachment.loadAction=MTLLoadActionLoad;out.pass.depthAttachment.storeAction=MTLStoreActionStore;
			if(actualStencil){out.pass.stencilAttachment.texture=actualStencil;out.pass.stencilAttachment.loadAction=MTLLoadActionLoad;out.pass.stencilAttachment.storeAction=MTLStoreActionStore;}
			out.depthState=effectstate::Depth(lease,state,actualStencil?actualStencil.pixelFormat:MTLPixelFormatInvalid);
			SepiaGPUUniforms gpu{};for(unsigned i=0;i<3;++i)gpu.rows[i]=simd_make_float4(u.rows[i][0],u.rows[i][1],u.rows[i][2],0);
			gpu.target={float(width),float(height),float(frame->logicalWidth)/width,float(frame->logicalHeight)/height};
			gpu.viewPos=simd_make_int4(u.viewPos[0],u.viewPos[1],0,0);gpu.alpha=simd_make_uint4(surface.alphaTest.enabled,unsigned(surface.alphaTest.compare),0,0);
			ValidateNativeCUSUniformPublication(*u.engine);std::uint32_t caps=0;std::memcpy(&caps,u.engine->params->bytes.data()+12,4);
			gpu.parameters=simd_make_float4(u.gamma,surface.alphaTest.reference,(caps&1u)?0.f:.5f,0);
			auto uniform=impl->UniformBuffer(lease,prepared->impl->argumentLifetime,&gpu,sizeof(gpu));[args setAddress:uniform.gpuAddress atIndex:1];
			const auto& version=operation.inputs[0];auto image=impl->ImageFor(lease,version,false,prepared->impl->completed);
			const auto& entry=impl->images.at(version.get());out.reads.emplace_back(entry.completed,entry.encoded);[args setTexture:image.gpuResourceID atIndex:0];
			out.pipeline=impl->Pipeline(lease,SEPIA_PIPELINE,0,color.pixelFormat,out.casRaster.get(),std::uint32_t(color.sampleCount));out.vertexCount=3;
		}else if(operation.kind==NativePostprocessOperationKind::CAS){
   out.casRaster=operation.cas.raster;Require(bool(out.casRaster),"CAS captured raster missing");
   const auto& state=out.casRaster->world;const auto& surface=out.casRaster->surface;
   WorldCamera camera{};const auto& v=*operation.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};
   auto raster=effectstate::Viewport(camera,width,height,frame->logicalWidth,frame->logicalHeight,surface.scissor);
   if(!raster || (state.cullEnabled && state.cull==NativeWorldCull::FrontAndBack))out.noDraw=true;
   else {out.viewport=raster->viewport;out.scissor=raster->scissor;}
   out.pass.depthAttachment.texture=depth;out.pass.depthAttachment.loadAction=MTLLoadActionLoad;out.pass.depthAttachment.storeAction=MTLStoreActionStore;
   if(actualStencil){out.pass.stencilAttachment.texture=actualStencil;out.pass.stencilAttachment.loadAction=MTLLoadActionLoad;out.pass.stencilAttachment.storeAction=MTLStoreActionStore;}
   out.depthState=effectstate::Depth(lease,state,actualStencil?actualStencil.pixelFormat:MTLPixelFormatInvalid);
   const auto& u=operation.cas;Require(u.engine && operation.inputs[0],"CAS lost original engine uniforms or screen copy");
   ValidateNativeCUSUniformPublication(*u.engine);CASGPUUniforms gpu{};
   // Original UniformMatricesBuffer: cameraProj=4, cameraViewInv=7, cameraViewProjInv=9.
   // Original UniformParamsBuffer viewGeometry is vec4 row2; retain producer bytes.
   std::memcpy(&gpu.cameraProj,u.engine->matrices->bytes.data()+4*64,64);
   std::memcpy(&gpu.cameraViewInv,u.engine->matrices->bytes.data()+7*64,64);
   std::memcpy(&gpu.cameraViewProjInv,u.engine->matrices->bytes.data()+9*64,64);
   std::memcpy(&gpu.viewGeometry,u.engine->params->bytes.data()+2*16,16);
   std::memcpy(&gpu.viewRect,u.viewRect.data(),16);gpu.strength=u.strength;gpu.groundY=u.groundY;gpu.debugFootprint=u.debugFootprint;
   gpu.target={float(width),float(height),float(frame->logicalWidth)/width,float(frame->logicalHeight)/height};
   std::uint32_t caps=0;std::memcpy(&caps,u.engine->params->bytes.data()+12,4);
   gpu.alpha=simd_make_uint4(surface.alphaTest.enabled,unsigned(surface.alphaTest.compare),0,0);
   gpu.raster=simd_make_float4(surface.alphaTest.reference,(caps&1u)?0.f:.5f,0,0);
   auto uniform=impl->UniformBuffer(lease,prepared->impl->argumentLifetime,&gpu,sizeof(gpu));[args setAddress:uniform.gpuAddress atIndex:1];
   const auto& version=operation.inputs[0];auto image=impl->ImageFor(lease,version,false,prepared->impl->completed);
   Require(image.width==std::uint32_t(u.viewRect[2]+1) && image.height==std::uint32_t(u.viewRect[3]+1) && image.pixelFormat==MTLPixelFormatRGBA8Unorm,"CAS actual screen-copy image differs");
   const auto& entry=impl->images.at(version.get());out.reads.emplace_back(entry.completed,entry.encoded);
   [args setTexture:image.gpuResourceID atIndex:0];out.pipeline=impl->Pipeline(lease,CAS_PIPELINE+int(u.variant),0,color.pixelFormat,out.casRaster.get(),std::uint32_t(color.sampleCount));out.vertexCount=3;
  }else if(operation.kind!=NativePostprocessOperationKind::DOF){
			const auto& src=operation.source;const auto& dst=operation.destination;
			quad.clip={2.f*dst.x/width-1,2.f*dst.y/height-1,2.f*(dst.x+dst.width)/width-1,2.f*(dst.y+dst.height)/height-1};
			quad.uv={float(src.x)/frame->logicalWidth,float(src.y)/frame->logicalHeight,float(src.x+src.width)/frame->logicalWidth,float(src.y+src.height)/frame->logicalHeight};
			if(main)out.resolveMain=static_cast<unsigned>(operation.kind==NativePostprocessOperationKind::CopyDepth?MainReadImages::Depth:MainReadImages::Color);
			auto source=operation.kind==NativePostprocessOperationKind::CopyDepth?readDepth:readColor;[args setTexture:source.gpuResourceID atIndex:0];
			NativePostprocessSampler nearest;nearest.minFilter=nearest.magFilter=NativePostprocessFilter::Nearest;nearest.wrapS=nearest.wrapT=NativePostprocessWrap::ClampEdge;auto sampler=impl->Sampler(lease,nearest);[args setSamplerState:sampler.gpuResourceID atIndex:0];
			out.pipeline=impl->Pipeline(lease,operation.kind==NativePostprocessOperationKind::CopyDepth?(operation.outputs[0]->image->format==NativePostprocessFormat::Depth24Unorm?COPY_DEPTH24_PIPELINE:COPY_DEPTH_PIPELINE):COPY_COLOR_PIPELINE,0,Format(operation.outputs[0]->image->format));
			if(operation.kind==NativePostprocessOperationKind::CopyDepth){out.depthState=impl->copyDepth;lease.Retain(out.depthState);}
		}else{
			const auto& u=operation.uniforms;GPUUniforms gpu{};std::memcpy(&gpu.projection,u.projection.data(),sizeof(gpu.projection));gpu.eye={u.eyePosition[0],u.eyePosition[1],u.eyePosition[2],0};gpu.resolutionDistance={u.resolution[0],u.resolution[1],u.distanceLimits[0],u.distanceLimits[1]};gpu.mouseFocus={u.mouseDepthCoordinate[0],u.mouseDepthCoordinate[1],u.manualFocusDepth,u.fStop};gpu.autoParams={u.autofocusFudgeFactor,u.autofocusPower,u.autofocusFocalLength,float(u.zeroToOneDepth)};gpu.settings={u.autofocus,u.mousefocus,u.quality,u.pass};gpu.target={float(height),0,0,0};
			auto uniform=impl->UniformBuffer(lease,prepared->impl->argumentLifetime,&gpu,sizeof(gpu));[args setAddress:uniform.gpuAddress atIndex:1];
			const unsigned required=RequiredInputs(u);
			Require(u.quality==1||(u.pass==5&&u.quality==0),"Original31657 non-composition DOF program has HIGH_QUALITY1");
			Require(operation.screenOutput==(u.pass==5)&&operation.outputs.size()==(operation.screenOutput?0:Outputs(u)),"Original31657 DOF pass/FBO output contract differs");
			for(size_t i=0;i<operation.outputs.size();++i)Require(Format(operation.outputs[i]->image->format)==AttachmentFormat(u.pass,unsigned(i),operation.screenOutput?color.pixelFormat:Format(operation.outputs[0]->image->format)),"Original31657 DOF MRT format/quantization differs");
			const auto retainRead=[&](const std::shared_ptr<const NativePostprocessVersion>& version){if(HasNativePostprocessCPUContents(*version))return;const auto& entry=impl->images.at(version.get());out.reads.emplace_back(entry.completed,entry.encoded);};
			for(const auto& previous:operation.previous)if(previous)retainRead(previous);
			for(unsigned i=0;i<6;++i){
			 // Each actually sampled original slot requires its own real immutable
			 // source. Only inactive static declarations reuse real origTex storage.
			 const bool sampled=(required&(1u<<i))!=0;
			 if(sampled)Require(operation.inputEnabled[i]&&operation.inputs[i],"Original31657 DOF sampler has no actual enabled produced image");
			 const auto& version=sampled?operation.inputs[i]:operation.inputs[0];Require(version!=nullptr,"DOF has no genuine origTex for inactive declarations");
			 auto image=impl->ImageFor(lease,version,false,prepared->impl->completed);if(sampled){if(u.pass==0&&i==1){const auto& sampling=version->image->sampler;Require(image.pixelFormat==MTLPixelFormatDepth32Float&&sampling.minFilter==NativePostprocessFilter::Nearest&&sampling.magFilter==NativePostprocessFilter::Nearest&&sampling.wrapS==NativePostprocessWrap::Repeat&&sampling.wrapT==NativePostprocessWrap::Repeat,"DOF filter requires actual D32F NEAREST/REPEAT copied depth");}retainRead(version);}
			 auto sampler=impl->Sampler(lease,version->image->sampler);[args setTexture:image.gpuResourceID atIndex:i];[args setSamplerState:sampler.gpuResourceID atIndex:i];
			}
			out.pipeline=impl->Pipeline(lease,u.pass,u.quality,operation.screenOutput?color.pixelFormat:Format(operation.outputs[0]->image->format),nullptr,operation.screenOutput?std::uint32_t(color.sampleCount):1u);
		}
		auto vertex=impl->QuadBuffer(lease,quad);[args setAddress:vertex.gpuAddress atIndex:0];prepared->impl->operations.push_back(std::move(out));
	}
	for(const auto& producer:frame->externalOutputs)for(const auto& output:producer.outputs){
		auto image=impl->ImageFor(lease,output,true,prepared->impl->completed);auto& entry=impl->images.at(output.get());
		prepared->impl->producedImages.emplace(output.get(),image);prepared->impl->externalEncoded.emplace(output.get(),entry.encoded);
	}
	if(!impl->reported157&&frame->sourceDrawFrame>=512&&omittedCopies157){
		LOG("[NativePostprocess157] frame=%llu operations=%zu omitted_prior_copies=%zu omitted_logical_source_bytes=%llu omitted_prior_resolves=%zu retained_prior_copies=%zu framebuffer_snapshots_unchanged=1 shaders_unchanged=1 source_order_unchanged=1",
		 static_cast<unsigned long long>(frame->sourceDrawFrame),frame->operations.size(),omittedCopies157,
		 static_cast<unsigned long long>(omittedBytes157),omittedCopies157,retainedCopies157);
		impl->reported157=true;
	}
	return prepared;
}
} // namespace rendering::metal4
