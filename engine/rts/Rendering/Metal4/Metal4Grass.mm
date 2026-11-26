/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Grass.h"
#include "GrassZeroScaleCompaction.h"
#include "GrassDenseScaleCompaction.h"
#include "System/Log/ILog.h"
#include "GrassBendShortlist.h"
#include <cstdlib>
#include "Metal4GrassBoundaryPolicyS01.h"
#include "Metal4TerrainOwnerRoster.h"
#include "Metal4InterventionMath.h"
#include "NativeGrassShaders.h"
#include <simd/simd.h>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
namespace rendering::metal4 {
namespace {
constexpr unsigned GrassSpanSelected=2;
constexpr std::array<GrassSpanPolicy,7> GrassSpanPolicies={{
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {256, 1, 1, 2},
    {0, 0, 0, 0},
    {256, 1, 1, 2},
    {256, 1, 0, 2},
    {256, 1, 0, 2},
}};
struct Uniform {
	simd_float4x4 viewProj,viewInv,shadowView;
	simd_float4 mapSize,fogColor,wind,night;
	std::array<float,18> defines;float distanceMult;int32_t unitBendCount;
	simd_uint4 options,imageFlags[6],swizzle[6];simd_float4 imageLod[6],alpha;
};
static_assert(sizeof(Uniform)==656);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,size_t n){Require(n&&n<=lease.Device().maxBufferLength,"Original Grass immutable buffer exceeds actual device bounds");auto b=[lease.Device()newBufferWithBytes:bytes length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original Grass source buffer");lease.AddAllocation(b);return b;}
std::string GrassShaderSource(unsigned variant)
{
	std::string source="#define GRASS_SHADOWS "+std::to_string(variant&1u)+"\n#define GRASS_BEND "+std::to_string((variant>>1)&1u)+"\n"+grassprogram::Source;
	if(!(variant&4u))return source;
	constexpr std::string_view instance=" GrassOut out{};float4 instance=instances[iid];\n";
	constexpr std::string_view scale="||instance.w<=0.f";
	const auto entry=source.find(instance);
	const auto predicate=source.find(scale);
	Require(entry!=std::string::npos&&predicate!=std::string::npos&&source.find(instance,entry+instance.size())==std::string::npos&&
		source.find(scale,predicate+scale.size())==std::string::npos&&entry<predicate,"Original Grass scale guard source contract changed");
	source.erase(predicate,scale.size());
	source.insert(entry+instance.size()," const float instanceScale=instance.w;if(instanceScale<=0.f){out.position=float4(2,2,2,1);return out;}\n");
	return source;
}
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Original Grass blend equation differs");}
uint64_t Bits(const ScreenBlendState& b,const std::array<bool,4>& m){return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;}
}
struct PreparedGrass::Impl {
	struct Draw {
		uint64_t sequence=0;std::shared_ptr<const NativeGrassInput> source;NativeWorldDrawState state;effectstate::Raster raster;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
		std::vector<CUSSourceImage> images;bool visible=false,encoded=false;
		GrassInstanceRange instances;
		GrassSpanPlan splitInstances;
		__strong id<MTLRenderPipelineState> parityGuardPipeline=nil;
		void Encode(id<MTL4RenderCommandEncoder>,std::span<const GrassInstanceRange>,id<MTLRenderPipelineState> alternate=nil) const;
		void Encode(id<MTL4RenderCommandEncoder> encoder,GrassInstanceRange range) const
		{ Encode(encoder,std::span{&range,std::size_t{1}}); }
  std::array<std::function<bool()>,6> checkpointQueries;
  std::array<id<MTLTexture>,6> vertexImages{};
  std::array<id<MTLBuffer>,5> immutableBuffers{}; // vertex, instance, uniform, bend, index

	};
	std::vector<Draw> draws;TerrainOwnerRoster firstOwners;bool dependencies=false,completed=false;
	std::vector<GrassInstanceRange> parityTrimmedRanges;
	std::vector<std::array<GrassSpanPlan,GrassSpanPolicies.size()>> paritySpanPlans;
	ZeroScaleStats parityStats;
 uint64_t checkpointSubmission=0;
 mutable GrassReadDiagnosticS01 boundaryDiagnostic;
 bool recordBoundaryDiagnostic=false;
 std::array<id<MTLTexture>,3> checkpointAttachments{};
 bool InspectBoundaryS01(const std::array<id<MTLTexture>,3>& attachments) const;

};
PreparedGrass::PreparedGrass():impl(std::make_unique<Impl>()){}
PreparedGrass::~PreparedGrass()=default;
size_t PreparedGrass::DrawCount()const{return impl->draws.size();}
uint64_t PreparedGrass::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedGrass::EncodeDependencies(id<MTL4CommandBuffer> command){
	Require(command&&!impl->dependencies&&!impl->completed,"Original Grass dependency stage repeated or absent");
	for(const auto& [draw,imageIndex]:impl->firstOwners){const auto& image=impl->draws[draw].images[imageIndex];if(image.encodeDependencies)image.encodeDependencies(command);}
	impl->dependencies=true;
}
void PreparedGrass::Impl::Draw::Encode(id<MTL4RenderCommandEncoder> encoder,
	std::span<const GrassInstanceRange> ranges,id<MTLRenderPipelineState> alternate) const
{
	if (!visible) return;
	for (const auto& image: images) image.requireReadable();
	effectstate::RasterState(encoder,state,raster);
	[encoder setDepthClipMode:state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[encoder setCullMode:!state.cullEnabled?MTLCullModeNone:state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
	[encoder setRenderPipelineState:alternate?:pipeline];[encoder setDepthStencilState:depth];
	[encoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	// A raster-visible run leaves its state in the encoder even with no surviving instances.
	const size_t offset=size_t(source->indexOffset)*2;
	for (const auto range : ranges) {
		if (!range.count) continue;
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:source->count indexType:MTLIndexTypeUInt16
			indexBuffer:indices.gpuAddress+offset indexBufferLength:indices.length-offset
			instanceCount:range.count baseVertex:source->baseVertex baseInstance:range.base];
	}
}
void PreparedGrass::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder&&impl->dependencies&&!impl->completed,"Original Grass draw lacks its actual dependency stage");
	auto& d=impl->draws.at(n);
	Require(!d.encoded,"Original Grass cull run encoded twice");
	d.encoded=true;
	if (d.splitInstances.count)
		d.Encode(encoder,d.splitInstances.Ranges());
	else
		d.Encode(encoder,d.instances);
}
PreparedGrass::ZeroScaleStats PreparedGrass::GetZeroScaleStats() const
{
	return impl->parityStats;
}
PreparedGrass::ZeroScaleDrawStats PreparedGrass::GetZeroScaleDrawStats(size_t n) const
{
	const auto& d=impl->draws.at(n);
	const auto compact=impl->parityTrimmedRanges.at(n);
	return {d.sequence,d.source->baseInstance,d.source->instanceCount,compact.base,compact.count,d.source->count,d.visible};
}
void PreparedGrass::EncodeSameInputDraw(size_t n,id<MTL4RenderCommandEncoder> encoder,bool compact) const
{
	Require(encoder&&impl->dependencies&&!impl->completed,"Grass parity needs the original encoded dependencies");
	const auto& d=impl->draws.at(n);
	const auto range=compact?impl->parityTrimmedRanges.at(n):GrassInstanceRange{d.source->baseInstance,d.source->instanceCount};
	d.Encode(encoder,range);
}
PreparedGrass::SpanRanges PreparedGrass::GetSameInputSpanRanges(size_t n,unsigned mode) const
{
	const auto& d=impl->draws.at(n);
	const auto& plan=impl->paritySpanPlans.at(n).at(mode);
	SpanRanges result;
	if(!d.visible) return result;
	for(const auto range:plan.Ranges())
		if(range.count) result.ranges[result.count++]={range.base,range.count};
	const auto originalEnd=d.source->baseInstance+d.source->instanceCount;
	std::size_t previous=d.source->baseInstance;
	for(const auto& range:plan.Ranges()) {
		Require(range.base>=previous&&range.base<=originalEnd&&range.count<=originalEnd-range.base,
			"Grass split changed absolute instance order or range bounds");
		previous=range.base+range.count;
	}
	const auto& bytes=d.source->instances->bytes;
	std::size_t segment=0;
	for(std::size_t row=d.source->baseInstance;row<originalEnd;++row) {
		std::uint32_t scaleBits;
		std::memcpy(&scaleBits,bytes.data()+row*sizeof(GrassInstanceRow)+12,sizeof(scaleBits));
		if((scaleBits&0x7fff'ffffu)==0)continue;
		while(segment<plan.count&&row>=plan.spans[segment].base+plan.spans[segment].count)++segment;
		Require(segment<plan.count&&row>=plan.spans[segment].base,
			"Grass split dropped an original nonzero instance slot");
		++result.liveRows;
	}
	return result;
}
void PreparedGrass::EncodeSameInputSpanDraw(size_t n,id<MTL4RenderCommandEncoder> encoder,unsigned mode) const
{
	Require(encoder&&impl->dependencies&&!impl->completed,"Grass parity needs its original encoded dependencies");
	const auto& d=impl->draws.at(n);
	d.Encode(encoder,impl->paritySpanPlans.at(n).at(mode).Ranges(), (mode==3||mode==4||mode==6)?d.parityGuardPipeline:nil);
}
void PreparedGrass::DidComplete(){
	Require(impl->dependencies&&!impl->completed&&std::all_of(impl->draws.begin(),impl->draws.end(),[](const auto& d){return d.encoded;}),"Original Grass retirement preceded its actual cull runs");
	for(const auto& [draw,imageIndex]:impl->firstOwners){const auto& image=impl->draws[draw].images[imageIndex];if(image.didComplete)image.didComplete();}impl->completed=true;
}
namespace {
GrassStorageS01 GrassStorage(id<MTLTexture> image) {
 if(!image)return {};
 auto root=SourceTextureRoot160(image);
 // Independently allocated, retained resources cannot overlap each other.
 // Reject imported, heap/aliasable and buffer-backed storage instead of
 // inferring allocation identity from the texture/view object's address.
 const bool standalone=root&&!root.parentTexture&&!root.heap&&!root.buffer&&
  !root.iosurface&&!root.shareable&&![root isAliasable]&&root.storageMode!=MTLStorageModeMemoryless;
 return {reinterpret_cast<uintptr_t>((__bridge void*)root),standalone};
}
}
bool PreparedGrass::Impl::InspectBoundaryS01(const std::array<id<MTLTexture>,3>& attachments) const {
 if(recordBoundaryDiagnostic)boundaryDiagnostic={0,0,0};
 const auto reject=[&](uint32_t reason){if(recordBoundaryDiagnostic)boundaryDiagnostic.reasonBits=reason;return false;};
 if(!dependencies||completed)return reject(GrassReadLifecycleS01);
 std::array<GrassStorageS01,3> writes;
 for(size_t n=0;n<writes.size();++n)writes[n]=GrassStorage(attachments[n]);
 size_t visible=0;
 for(size_t drawIndex=0;drawIndex<draws.size();++drawIndex){
  const auto& d=draws[drawIndex];if(recordBoundaryDiagnostic)boundaryDiagnostic.drawIndex=drawIndex;
  if(!d.visible)continue;
  if(d.encoded)return reject(GrassReadAlreadyEncodedS01);
  ++visible;
  for(unsigned slot=1;slot<6;++slot){
   if(slot==3&&d.source->defines[9]!=1)continue;
   if(recordBoundaryDiagnostic)boundaryDiagnostic.slot=slot;
   auto texture=d.vertexImages[slot];
   if(!texture||texture.device!=attachments[0].device||texture.textureType!=MTLTextureType2D||
    texture.sampleCount!=1)return reject(GrassReadTextureS01);
   if(!GrassReadDisjointS01(GrassStorage(texture),writes))return reject(GrassReadStorageS01);
   if(!d.checkpointQueries[slot])return reject(GrassReadUnknownProviderS01);
   if(!d.checkpointQueries[slot]())return reject(GrassReadProducerS01);
  }
  // All five buffers originate from immutable NativeBufferSnapshot uploads
  // or this draw's 656-byte constants, never a GPU writable source. The source
  // snapshots, buffer objects and full ranges survive the common GPU ticket.
  for(unsigned n=0;n<d.immutableBuffers.size();++n){
   if(n==3&&d.source->defines[13]!=1)continue;
   if(recordBoundaryDiagnostic)boundaryDiagnostic.slot=n;auto buffer=d.immutableBuffers[n];
   if(!buffer||buffer.device!=attachments[0].device||buffer.heap||[buffer isAliasable]||
    !buffer.gpuAddress||!buffer.length||buffer.gpuAddress>UINT64_MAX-buffer.length)return reject(GrassReadBufferS01);
  }
 }
 if(!visible)return reject(GrassReadNoVisibleS01);
 if(recordBoundaryDiagnostic)boundaryDiagnostic={0,0,0};return true;
}
void PreparedGrass::EnableBoundaryDiagnosticsS01() noexcept {impl->recordBoundaryDiagnostic=true;}
GrassReadDiagnosticS01 PreparedGrass::BoundaryDiagnosticS01() const noexcept {return impl->boundaryDiagnostic;}
size_t PreparedGrass::VisibleDrawCountS01() const noexcept {
 return std::count_if(impl->draws.begin(),impl->draws.end(),[](const auto& draw){return draw.visible;});
}
bool PreparedGrass::CaptureBoundaryCheckpointS01(uint64_t submission,
 const std::array<id<MTLTexture>,3>& attachments) noexcept {
 if(!submission||impl->checkpointSubmission){if(impl->recordBoundaryDiagnostic)impl->boundaryDiagnostic.reasonBits=GrassReadSubmissionS01;return false;}
 try{
  if(!impl->InspectBoundaryS01(attachments))return false;
  impl->checkpointSubmission=submission;impl->checkpointAttachments=attachments;return true;
 }catch(...){if(impl->recordBoundaryDiagnostic)impl->boundaryDiagnostic.reasonBits=GrassReadExceptionS01;return false;} // Optional proof failure retains the original draw.
}
bool PreparedGrass::CanSplitBoundaryS01(uint64_t submission,
 const std::array<id<MTLTexture>,3>& attachments) const noexcept {
 if(!submission||impl->checkpointSubmission!=submission){if(impl->recordBoundaryDiagnostic)impl->boundaryDiagnostic.reasonBits=GrassReadSubmissionS01;return false;}
 if(impl->checkpointAttachments!=attachments){if(impl->recordBoundaryDiagnostic)impl->boundaryDiagnostic.reasonBits=GrassReadAttachmentsS01;return false;}
 try{return impl->InspectBoundaryS01(attachments);}catch(...){if(impl->recordBoundaryDiagnostic)impl->boundaryDiagnostic.reasonBits=GrassReadExceptionS01;return false;}
}
struct GrassGPU::Impl {
	struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;__strong id<MTLBuffer> buffer=nil;};
	std::map<const NativeBufferSnapshot*,Cached> buffers;
	struct ZeroScaleCache {
		std::weak_ptr<const NativeBufferSnapshot> source;
		GrassZeroScaleIndex index;
		std::map<std::pair<std::size_t,std::size_t>,GrassSpanPlan> spanPlans;
	};
	std::map<const NativeBufferSnapshot*,ZeroScaleCache> zeroScaleIndices;
	struct CompactCache {
		std::weak_ptr<const NativeBufferSnapshot> source;
		GrassDenseScaleIndex index;
		__strong id<MTLBuffer> buffer=nil;
	};
	std::map<const NativeBufferSnapshot*,CompactCache> compactBuffers;
	struct BendBoundsCache {
		std::weak_ptr<const NativeBufferSnapshot> source;
		std::map<std::pair<std::size_t,std::size_t>,GrassBendBounds> ranges;
	};
	std::map<const NativeBufferSnapshot*,BendBoundsCache> bendBounds;
	const GrassBendBounds& BendBounds(const NativeBufferSnapshotRef& source,GrassInstanceRange range)
	{
		auto& entry=bendBounds[source.get()];
		if(entry.source.expired()){
			entry.source=source;
			entry.ranges.clear();
		}
		auto [found,inserted]=entry.ranges.try_emplace(std::pair{range.base,range.count});
		if(inserted)found->second=GrassBendInstanceBounds(source->bytes,range);
		return found->second;
	}

	ZeroScaleCache& ZeroScaleMetadata(const NativeBufferSnapshotRef& source)
	{
		auto& cached=zeroScaleIndices[source.get()];
		if(cached.source.expired()){
			cached.index.Build(source->bytes);
			cached.spanPlans.clear();
			cached.source=source;
		}
		return cached;
	}
	const CompactCache& Compacted(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		auto& cached=compactBuffers[source.get()];
		if (cached.source.expired()) {
			CompactCache replacement;
			const auto packed=replacement.index.Build(source->bytes);
			if (!packed.empty()) replacement.buffer=Upload(lease,packed.data(),packed.size()*sizeof(GrassInstanceRow));
			replacement.source=source;
			cached=std::move(replacement);
		} else if (cached.buffer) {
			lease.AddAllocation(cached.buffer);
		}
		return cached;
	}
	std::map<unsigned,id<MTLLibrary>> libraries;
	using Key=std::tuple<unsigned,unsigned,bool,uint64_t,unsigned,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source){
		auto& b=buffers[source.get()];if(b.source.expired()||b.buffer==nil){b.buffer=Upload(lease,source->bytes.data(),source->bytes.size());b.source=source;}else lease.AddAllocation(b.buffer);return b.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,unsigned variant,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,unsigned samples,bool coverage){
		const Key key{unsigned(color),variant,validation,Bits(blend,mask),lease.SampleShadingKey(samples),coverage};if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;auto& library=libraries[variant];
		if(library==nil){auto d=[MTL4LibraryDescriptor new];d.source=String(GrassShaderSource(variant));d.name=@"Original31657 indexed Grass";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(o);d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original Grass native source: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=@"GrassVertex";fs.name=@"GrassFragment";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile actual original Grass pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
	}
};
GrassGPU::GrassGPU():impl(std::make_unique<Impl>()){}
GrassGPU::~GrassGPU(){SetNativeGrassConsumerAvailable(false);}
void GrassGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,bool validation,double& ms,unsigned samples){
	ScreenBlendState blend;for(unsigned variant=0;variant<4;++variant)impl->Pipeline(lease,color,variant,validation,blend,{true,true,true,true},ms,samples,false);SetNativeGrassConsumerAvailable(true);
}
void GrassGPU::ClearResources(){impl->buffers.clear();impl->zeroScaleIndices.clear();impl->compactBuffers.clear();impl->bendBounds.clear();}
std::shared_ptr<PreparedGrass> GrassGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawSelection167& selected,uint64_t sourceDrawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const GrassSourceImageProviderS01& images,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	return Prepare(lease,selected,sourceDrawFrame,view,width,height,logicalWidth,logicalHeight,
		images,color,depth,stencil,validation,ms,samples,false);
}
std::shared_ptr<PreparedGrass> GrassGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawSelection167& selected,uint64_t sourceDrawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const GrassSourceImageProviderS01& images,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples,bool sameInput){
	selected.RequireFamily(NativeWorldFamily167::Grass,sourceDrawFrame);if(selected.empty())return {};
	Require(images&&(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8),"Original Grass has no shared scene/image consumer");std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});
	static const bool trimTrailingEnabled=[] {
		const char* value=std::getenv("BAR_NATIVE_GRASS_ZERO_SCALE_COMPACT");
		return value&&std::strcmp(value,"1")==0;
	}();
	static const bool spanSkipRequested=[] {
		const char* value=std::getenv("BAR_NATIVE_GRASS_SKIP_EMPTY_SPANS");
		return value&&std::strcmp(value,"1")==0&&!InterventionFastMathEnabled();
	}();
	static const bool denseRequested=[] {
        const char* v=std::getenv("BAR_NATIVE_GRASS_DENSE_REFERENCE");
        return v&&std::strcmp(v,"1")==0&&!InterventionFastMathEnabled();
    }();
    const bool denseEnabled=trimTrailingEnabled&&denseRequested&&samples==1&&!sameInput;
    if(denseEnabled)std::erase_if(impl->compactBuffers,[](const auto& entry){return entry.second.source.expired();});
    const bool spanSkipEnabled=trimTrailingEnabled&&spanSkipRequested&&samples==1;
	static const bool earlyGuardRequested=[] {
		const char* value=std::getenv("BAR_NATIVE_GRASS_EARLY_SCALE_GUARD");
		return value&&std::strcmp(value,"1")==0&&!InterventionFastMathEnabled();
	}();
	const bool useEarlyGuard=earlyGuardRequested&&samples==1&&!sameInput;
	const bool prepareParityGuard=sameInput&&samples==1&&!InterventionFastMathEnabled();
	if (trimTrailingEnabled||sameInput) std::erase_if(impl->zeroScaleIndices,[](const auto& entry){return entry.second.source.expired();});
	auto out=std::shared_ptr<PreparedGrass>(new PreparedGrass);
	static const bool shortlistEnabled=[] {
		const char* value=std::getenv("BAR_NATIVE_GRASS_BEND_SHORTLIST");
		return value&&std::strcmp(value,"1")==0&&!InterventionFastMathEnabled();
	}();
	const bool shortlistBends=shortlistEnabled&&samples==1;
	if(shortlistBends)std::erase_if(impl->bendBounds,[](const auto& entry){return entry.second.source.expired();});

	std::set<const NativeBufferSnapshot*> paritySnapshots;
	for(const auto& c:selected){ValidateNativeWorldDrawCommand(c);const auto& s=*c.grass;
		Require(c.stage==NativeWorldDrawStage::PreUnit&&s.sourceDrawFrame==sourceDrawFrame,"Original Grass cull run belongs to another source frame/stage");
		Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Original Grass inherited polygon/clip state requires its actual shader profile");
		PreparedGrass::Impl::Draw d;d.sequence=c.sequence;d.source=c.grass;d.state=c.world;auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		const GrassInstanceRange originalRange{s.baseInstance,s.instanceCount};
		d.instances=originalRange;
		if (sameInput) {
			out->impl->parityTrimmedRanges.push_back(originalRange);
			out->impl->paritySpanPlans.emplace_back();
		}
		auto scissor=c.surface.scissor;if(scissor&&!scissor->windowCoordinates){scissor->x+=view.viewport[0];scissor->y+=view.viewport[1];scissor->windowCoordinates=true;}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,scissor);
		if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack)){out->impl->draws.push_back(std::move(d));continue;}d.raster=*raster;d.visible=true;
        auto compactRange=originalRange;
        const bool useDense=denseEnabled&&s.defines[12]==1.f&&s.count%3==0;
        const Impl::CompactCache* compact=nullptr;
        if(useDense){
            compact=&impl->Compacted(lease,s.instances);
            compactRange=compact->index.Remap(originalRange.base,originalRange.count);
            if(compactRange.count<originalRange.count){
                static const bool reported=[&]{LOG("[PrecisionRevisit] Grass dense: %zu -> %zu",originalRange.count,compactRange.count);return true;}();
                (void)reported;
            }
        } else if(trimTrailingEnabled||sameInput) {
			auto& metadata=impl->ZeroScaleMetadata(s.instances);
			compactRange=metadata.index.TrimTrailing(originalRange);
			const bool supportsSpans=(spanSkipEnabled||sameInput)&&s.defines[12]==1.f&&s.count%3==0&&samples==1&&!InterventionFastMathEnabled();
			if(spanSkipEnabled&&supportsSpans&&compactRange.count) {
				const auto key=std::pair{originalRange.base,originalRange.count};
				auto [it,inserted]=metadata.spanPlans.try_emplace(key);
				if(inserted)
					it->second=metadata.index.PlanEmptySpans(originalRange,GrassSpanPolicies[GrassSpanSelected]);
				const auto& plan=it->second;
				if(plan.count>1||plan.spans.front().base!=compactRange.base)
					d.splitInstances=plan;
			}
			if(sameInput) {
				auto& plans=out->impl->paritySpanPlans.back();
				plans[0]={{originalRange},1};
				plans[1]={{compactRange},1};
				for(size_t mode=2;mode<plans.size();++mode)
					plans[mode]=supportsSpans?metadata.index.PlanEmptySpans(originalRange,GrassSpanPolicies[mode]):plans[1];
			}
		}
		if (trimTrailingEnabled) d.instances=compactRange;
		if (sameInput) {
			out->impl->parityTrimmedRanges.back()=compactRange;
			auto& stats=out->impl->parityStats;
			++stats.draws;
			stats.originalInstances+=originalRange.count;
			stats.compactInstances+=compactRange.count;
			stats.removedInstances+=originalRange.count-compactRange.count;
			stats.originalIndexInstances+=s.count*originalRange.count;
			stats.compactIndexInstances+=s.count*compactRange.count;
			stats.distinctSnapshots+=paritySnapshots.insert(s.instances.get()).second;
		}
		Uniform u{};std::memcpy(&u.viewProj,s.original->matrices->bytes.data()+grass::ViewProjectionByte,64);std::memcpy(&u.viewInv,s.original->matrices->bytes.data()+grass::ViewInverseByte,64);std::memcpy(&u.shadowView,s.original->matrices->bytes.data()+grass::ShadowViewByte,64);
		std::memcpy(&u.mapSize,s.original->params->bytes.data()+grass::MapSizeByte,16);std::memcpy(&u.fogColor,s.original->params->bytes.data()+grass::FogColorByte,16);std::memcpy(&u.wind,s.uniforms.wind.data(),16);std::memcpy(&u.night,s.uniforms.night.data(),16);u.defines=s.defines;u.distanceMult=s.uniforms.distanceMult;u.unitBendCount=s.uniforms.unitBendCount;
		uint32_t caps;std::memcpy(&caps,s.original->params->bytes.data()+12,4);u.options=simd_make_uint4(caps&1u,0,unsigned(c.surface.alphaTest.enabled),unsigned(c.surface.alphaTest.compare));u.alpha=simd_make_float4(c.surface.alphaTest.reference,0,0,0);
		auto desc=[MTL4ArgumentTableDescriptor new];
		desc.maxBufferBindCount=4;desc.maxTextureBindCount=6;desc.maxSamplerStateBindCount=6;desc.initializeBindings=YES;
		NSError* error=nil;
		d.arguments=[lease.Device() newArgumentTableWithDescriptor:desc error:&error];
		Require(d.arguments!=nil,"Cannot allocate original Grass arguments: "+Error(error));
		lease.Retain(d.arguments);
        const auto instances=compact&&compact->index.Repacked()&&compact->buffer
            ?compact->buffer:impl->Buffer(lease,s.instances);
		const auto vertices=impl->Buffer(lease,s.vertices);
		d.indices=impl->Buffer(lease,s.indices);
		d.immutableBuffers[0]=vertices;d.immutableBuffers[1]=instances;d.immutableBuffers[4]=d.indices;
		[d.arguments setAddress:vertices.gpuAddress atIndex:0];
		[d.arguments setAddress:instances.gpuAddress atIndex:1];
		if (s.defines[13]==1) {
			auto bend=impl->Buffer(lease,s.bend.snapshot);
			auto offset=s.bend.offset;
			if(shortlistBends&&u.unitBendCount>0){
				const auto count=std::size_t(u.unitBendCount);
				const auto sourceRows=std::span(s.bend.snapshot->bytes).subspan(offset,count*sizeof(GrassInstanceRow));
				const auto selected=GrassBendSelect(impl->BendBounds(s.instances,useDense?originalRange:d.instances),sourceRows);
				if(selected.supported&&selected.count<count){
					u.unitBendCount=static_cast<int32_t>(selected.count);
					if(selected.count){
						const auto first=selected.ordinals[0];
						const auto last=selected.ordinals[selected.count-1];
						if(std::size_t(last-first)+1==selected.count)
							offset+=std::size_t(first)*sizeof(GrassInstanceRow);
						else{
							std::array<GrassInstanceRow,GrassBendMaximum> packed;
							for(std::size_t i=0;i<selected.count;++i)
								std::memcpy(packed[i].data(),sourceRows.data()+selected.ordinals[i]*sizeof(GrassInstanceRow),
									sizeof(GrassInstanceRow));
							bend=Upload(lease,packed.data(),selected.count*sizeof(GrassInstanceRow));
							offset=0;
						}
					}
				}
			}
			d.immutableBuffers[3]=bend;
			[d.arguments setAddress:bend.gpuAddress+offset atIndex:3];
		}
		for(unsigned slot=0;slot<6;++slot){if(slot==3&&s.defines[9]!=1)continue;const auto& binding=s.images[slot];const auto& sampling=binding.object->sampling;auto resolved=images(lease,binding,slot);auto image=std::move(resolved.image);
			Require(image.texture&&image.sampler&&image.owner&&image.requireReadable,"Original Grass active image lacks its actual storage/sampling/producer");Require(slot==3?image.depthImage:!image.depthImage,"Original Grass physical texture type differs from its shader sampler");
			for(unsigned k=0;k<4;++k){const unsigned sw=sampling.swizzle[k];Require(sw==0||sw==1||(sw>=0x1903&&sw<=0x1906),"Original Grass texture swizzle differs");u.swizzle[slot][k]=sw;}
			if(slot==3)Require(sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original Grass comparison sampler requires its original identity swizzle");
			u.imageFlags[slot]=simd_make_uint4(image.flipV,sampling.depthMode,0,image.clampDepthCompare);const bool mips=sampling.minFilter>=0x2700&&sampling.minFilter<=0x2703;
			u.imageLod[slot]=simd_make_float4(mips?sampling.minLod:0.f,mips?sampling.maxLod:0.f,sampling.lodBias,0);
			[d.arguments setTexture:image.texture.gpuResourceID atIndex:slot];[d.arguments setSamplerState:image.sampler.gpuResourceID atIndex:slot];lease.AddAllocation(image.texture);lease.Retain(image.sampler);
   if(slot){d.vertexImages[slot]=image.texture;d.checkpointQueries[slot]=std::move(resolved.beforeCheckpoint);}
   d.images.push_back(std::move(image));
		}
		auto constants=Upload(lease,&u,sizeof(u));d.immutableBuffers[2]=constants;[d.arguments setAddress:constants.gpuAddress atIndex:2];
		const unsigned variant=unsigned(s.defines[9]==1)|(unsigned(s.defines[13]==1)<<1)|(unsigned(useEarlyGuard)<<2);
		d.pipeline=impl->Pipeline(lease,color,variant,validation,c.surface.blend,c.world.colorWrite,ms,samples,c.world.alphaToCoverage);
		if(prepareParityGuard){
			d.parityGuardPipeline=impl->Pipeline(lease,color,variant|4u,validation,c.surface.blend,c.world.colorWrite,ms,samples,c.world.alphaToCoverage);
			lease.Retain(d.parityGuardPipeline);lease.Retain(impl->libraries.at(variant|4u));
		}
		d.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(d.pipeline);lease.Retain(impl->libraries.at(variant));out->impl->draws.push_back(std::move(d));
	}
	out->impl->firstOwners=BuildTerrainOwnerRoster(out->impl->draws);return out;
}
}
