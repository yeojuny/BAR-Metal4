#include "Metal4WeaponRangeViewport.h"
#include <cstdio>
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WeaponRangeNative.h"
#include "Metal4TransientUpload177.h"
#include "NativeWeaponRangeShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <mutex>
#include "System/Log/ILog.h"

namespace rendering::metal4 {
namespace {
// RANGE199_ALGORITHM_BEGIN
constexpr uint32_t MaxRangeLineVertices = 1024;

bool CanIndexRangeLine(bool composite, NativeDrawPrimitive primitive, uint32_t vertices, unsigned samples)
{
    return !composite && primitive == NativeDrawPrimitive::LineLoop && samples == 1 &&
           (vertices == 128 || vertices == MaxRangeLineVertices);
}

std::vector<uint16_t> MakeRangeLineIndices(uint32_t vertices)
{
    Require(vertices >= 3 && vertices <= MaxRangeLineVertices, "Invalid range line index extent");
    std::vector<uint16_t> indices;
    indices.reserve(6 * vertices);
    for (uint32_t segment = 0; segment < vertices; ++segment) {
        // The original line vertex shader maps IDs 0/3 and 2/4 to the same corners.
        // Reuse those IDs without changing either triangle or its provoking vertex value.
        const auto base = uint16_t(6 * segment);
        indices.insert(indices.end(), {base, uint16_t(base+1), uint16_t(base+2),
                                       base, uint16_t(base+2), uint16_t(base+5)});
    }
    return indices;
}
// RANGE199_ALGORITHM_END

id<MTLBuffer> Upload(RuntimeLease& lease,const void* source,size_t size)
{
	Require(source && size && size<=lease.Device().maxBufferLength,"Original weapon range buffer exceeds device capacity");
	auto result=[lease.Device()newBufferWithBytes:source length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original weapon range buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::logic_error("Unknown weapon range blend equation");
}

uint64_t RangeWarmNow() noexcept{return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
bool RangeWarmEnabled(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_WEAPON_RANGE_ASYNC_WARM");return p&&std::strcmp(p,"1")==0;}();return v;}
bool RangeWarmWitness(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_WEAPON_RANGE_WARM_WITNESS");return p&&std::strcmp(p,"1")==0;}();return v;}
struct RangeWarmResult {__strong id<MTLLibrary> library=nil;uint64_t finished=0;std::atomic<bool> ready{false};};
struct RangeWarmEntry {const Runtime* context=nullptr;__strong id<MTLDevice> device=nil;__strong id<MTL4Compiler> compiler=nil;__strong MTL4LibraryDescriptor* descriptor=nil;__strong id<MTL4CompilerTask> task=nil;std::shared_ptr<RangeWarmResult> result;uint64_t queued=0;};
MTL4LibraryDescriptor* RangeLibraryDescriptor(){
 auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 original Attack/Defense range coverage and composite";const char* division233=std::getenv("BAR_NATIVE_RANGE_DIVISION233");
 const bool useDivision233=division233&&std::strcmp(division233,"1")==0;
 const char* exactInterior=std::getenv("BAR_NATIVE_WEAPON_RANGE_EXACT_INTERIOR");
 const bool useExactInterior=!useDivision233&&exactInterior&&std::strcmp(exactInterior,"1")==0;
 std::string text=std::string("#define RECOIL_RANGE_DIVISION233 ")+(useDivision233?"1\n":"0\n")+(useExactInterior?"#define RECOIL_WEAPON_RANGE_EXACT_INTERIOR 1\n":"")+NativeWeaponRangeShaderSource;
 descriptor.source=String(text);auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;return descriptor;
}

std::shared_ptr<RangeWarmEntry> SharedRangeWarm(RuntimeLease& lease,bool queue,int64_t simulation) {
 static std::mutex lock;static std::weak_ptr<RangeWarmEntry> weak;
 std::shared_ptr<RangeWarmEntry> shared;
 std::lock_guard<std::mutex> guard(lock);
 shared=weak.lock();
 if(shared&&shared->context==lease.ContextIdentity()&&shared->device==lease.Device()&&shared->compiler==lease.Compiler())return shared;
 if(!queue)return {};
 auto result=std::make_shared<RangeWarmResult>();auto descriptor=RangeLibraryDescriptor();const auto queued=RangeWarmNow();
 auto task=[lease.Compiler() newLibraryWithDescriptor:descriptor completionHandler:^(id<MTLLibrary> library,NSError* error){
  @autoreleasepool{(void)error;result->library=library;result->finished=RangeWarmNow();result->ready.store(true,std::memory_order_release);}
 }];
 if(!task)return {};
 shared=std::make_shared<RangeWarmEntry>(RangeWarmEntry{lease.ContextIdentity(),lease.Device(),lease.Compiler(),descriptor,task,result,queued});
 weak=shared;
 if(RangeWarmWitness())LOG("[WeaponRangeWarm] event=queued sim=%lld source_len=%llu queued_ns=%llu",(long long)simulation,(unsigned long long)descriptor.source.length,(unsigned long long)queued);
 return shared;
}

uint64_t PipelineBits(const ScreenBlendState& b,const std::array<bool,4>& m)
{
	return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|
		uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;
}
}
struct PreparedWeaponRangeDraw::Impl {
 __strong M4TransientUploadLifetime177* uploads177=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	NativeWeaponRangeDraw source;
	PreparedStartBoxImage height,coverageMask;
	NativeWorldDrawState rasterState;
	effectstate::Raster raster;
	uint32_t count=0,instances=0;
    __strong id<MTLBuffer> lineIndices=nil;
};
PreparedWeaponRangeDraw::PreparedWeaponRangeDraw():impl(std::make_unique<Impl>()){}
PreparedWeaponRangeDraw::~PreparedWeaponRangeDraw()=default;
bool PreparedWeaponRangeDraw::Empty()const{return !impl->count || !impl->instances;}
void PreparedWeaponRangeDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(Empty())return;
	if(impl->height.requireReadable)impl->height.requireReadable();
	if(impl->coverageMask.requireReadable)impl->coverageMask.requireReadable();
	effectstate::RasterState(encoder,impl->rasterState,impl->raster);
	const auto cull=impl->rasterState.cullEnabled?impl->rasterState.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setDepthStencilState:impl->depth];[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
    // RANGE199_DRAW_BEGIN
    if (impl->lineIndices) {
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:impl->count indexType:MTLIndexTypeUInt16
                           indexBuffer:impl->lineIndices.gpuAddress indexBufferLength:impl->lineIndices.length
                         instanceCount:impl->instances];
    } else {
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:impl->count instanceCount:impl->instances];
    }
    // RANGE199_DRAW_END
}
struct WeaponRangeGPU::Impl {
 TransientUploadPool177 uploads177{"weapon_range"};
	using PipelineKey=std::tuple<unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	__strong id<MTLLibrary> library=nil;
 std::shared_ptr<RangeWarmEntry> warm;const Runtime* attemptedContext=nullptr;__weak id<MTL4Compiler> attemptedCompiler=nil;int64_t witnessSimulation=0;
 void Warm(RuntimeLease& lease,int64_t simulation) noexcept {
  if(RangeWarmWitness())witnessSimulation=simulation;
  if(!RangeWarmEnabled()||library)return;
  if(attemptedContext==lease.ContextIdentity()&&attemptedCompiler==lease.Compiler())return;
  warm.reset();attemptedContext=lease.ContextIdentity();attemptedCompiler=lease.Compiler();
  try{warm=SharedRangeWarm(lease,true,simulation);}catch(...){/* optional prewarm never alters original fallback */}
 }
 bool Adopt(RuntimeLease& lease){
  if(!RangeWarmEnabled())return false;
  if(!warm||warm->context!=lease.ContextIdentity()||warm->device!=lease.Device()||warm->compiler!=lease.Compiler())warm=SharedRangeWarm(lease,false,witnessSimulation);
  if(!warm||warm->context!=lease.ContextIdentity()||warm->device!=lease.Device()||warm->compiler!=lease.Compiler()||!warm->result->ready.load(std::memory_order_acquire)||!warm->result->library)return false;
  auto result=warm->result;auto candidate=result->library;
  lease.RegisterShaderSource(candidate,warm->descriptor);library=candidate;
  if(RangeWarmWitness())LOG("[WeaponRangeWarm] event=adopt sim=%lld source_len=%llu queued_ns=%llu finished_ns=%llu adopt_ns=%llu",(long long)witnessSimulation,(unsigned long long)warm->descriptor.source.length,(unsigned long long)warm->queued,(unsigned long long)result->finished,(unsigned long long)RangeWarmNow());
  // Keep this owner until GPU resource cleanup so the second renderer can adopt.
  return true;
 }

	std::map<PipelineKey,id<MTLRenderPipelineState>> pipelines;
    // RANGE199_CACHE_BEGIN
    const Runtime* lineIndexContext=nullptr;
    __strong id<MTLBuffer> lineIndexBuffer=nil;

    id<MTLBuffer> LineIndices(RuntimeLease& lease)
    {
        if (lineIndexContext!=lease.ContextIdentity() || !lineIndexBuffer || lineIndexBuffer.device!=lease.Device()) {
            const auto indices=MakeRangeLineIndices(MaxRangeLineVertices);
            lineIndexBuffer=[lease.Device() newBufferWithBytes:indices.data() length:indices.size()*sizeof(uint16_t)
                                                      options:MTLResourceStorageModeShared];
            lineIndexContext=lease.ContextIdentity();
        }
        if (lineIndexBuffer) lease.AddAllocation(lineIndexBuffer);
        return lineIndexBuffer;
    }
    // RANGE199_CACHE_END
	struct CachedBuffer {std::weak_ptr<const NativeBufferSnapshot> original;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		auto& entry=buffers[source.get()];
		if(entry.original.lock()!=source)entry={source,Upload(lease,source->bytes.data(),source->bytes.size())};
		lease.AddAllocation(entry.buffer);return entry.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeWeaponRangeDraw& draw,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms,bool rebaseFill)
	{
		const bool line=draw.primitive==NativeDrawPrimitive::LineLoop;
		const unsigned program=draw.program.defines->composite?5:draw.program.defines->maskPass?2:draw.program.maskClip>0.5f?(line?3:4):(line?1:0);
		const PipelineKey key{program|(rebaseFill?8u:0u),color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(draw.surface.blend,draw.raster.colorWrite),draw.raster.alphaToCoverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()){lease.Retain(found->second);return found->second;}
		const auto start=Clock::now();NSError* error=nil;
		if(!library&&!Adopt(lease)){
   auto descriptor=RangeLibraryDescriptor();const auto begin=RangeWarmWitness()?RangeWarmNow():0;
   library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original weapon range MSL: "+Error(error));
   if(RangeWarmWitness())LOG("[WeaponRangeWarm] event=sync sim=%lld source_len=%llu wall_ns=%llu",(long long)witnessSimulation,(unsigned long long)descriptor.source.length,(unsigned long long)(RangeWarmNow()-begin));
   // A late async completion can still be adopted by the other owner.
  }
		lease.Retain(library);auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		vertex.name=program==5?(rebaseFill?@"OriginalWeaponRangeCompositeRebased":@"OriginalWeaponRangeComposite"):line?@"OriginalWeaponRangeLine":(rebaseFill?@"OriginalWeaponRangeFillRebased":@"OriginalWeaponRangeFill");
		constexpr const char* fragments[]={"OriginalWeaponRangeFillFragment","OriginalWeaponRangeLineFragment","OriginalWeaponRangeMaskFragment","OriginalWeaponRangeMaskedLineFragment","OriginalWeaponRangeMaskedFillFragment","OriginalWeaponRangeCompositeFragment"};
		fragment.name=String(std::string(fragments[program])+((samples>1&&(program==1||program==3))?"MSAA":""));
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		const auto& b=draw.surface.blend;const auto& m=draw.raster.colorWrite;auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;
		attachment.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);
		attachment.rgbBlendOperation=Equation(b.colorEquation);attachment.alphaBlendOperation=Equation(b.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto result=lease.NewRenderPipeline(descriptor,&error);Require(result!=nil,"Cannot compile original weapon range pipeline: "+Error(error));
		pipelines.emplace(key,result);lease.Retain(result);ms+=Milliseconds(start);return result;
	}
};
WeaponRangeGPU::WeaponRangeGPU():impl(std::make_unique<Impl>()){}
WeaponRangeGPU::~WeaponRangeGPU()=default;
void WeaponRangeGPU::WarmLibrary(RuntimeLease& lease,int64_t simulation) noexcept {impl->Warm(lease,simulation);}
void WeaponRangeGPU::ClearResources(){impl->warm.reset();impl->attemptedContext=nullptr;impl->attemptedCompiler=nil;impl->uploads177.Clear();impl->buffers.clear();impl->lineIndexBuffer=nil;impl->lineIndexContext=nullptr;}
std::unique_ptr<PreparedWeaponRangeDraw> WeaponRangeGPU::Prepare(RuntimeLease& lease,const NativeWeaponRangeDraw& draw,
	const StartBoxImageResolver& resolver,MTLViewport viewport,MTLScissorRect scissor,std::array<std::uint32_t,2> targetExtent,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds,
	std::optional<NativePostprocessConsumer> consumer)
{
	ValidateNativeWeaponRangeDraw(draw);
	auto result=std::unique_ptr<PreparedWeaponRangeDraw>(new PreparedWeaponRangeDraw);
 impl->uploads177.BeginBatch(lease.Device());
 auto upload177=[&](const void* bytes,size_t length){
  return UploadTransient177(lease,impl->uploads177,result->impl->uploads177,bytes,length,
   [&]{return Upload(lease,bytes,length);});
 };

	const bool line=draw.primitive==NativeDrawPrimitive::LineLoop;
	if(!draw.instanceCount || !scissor.width || !scissor.height || (!line && draw.raster.cullEnabled && draw.raster.cull==NativeWorldCull::FrontAndBack))return result;
 // PiP deliberately describes the full virtual minimap viewport, which
 // can exceed the physical attachment by orders of magnitude. Keep its
 // coordinate transform unchanged; only the actual hardware viewport is
 // rebased for an extended filled/composite draw. Lines already encode in
 // full attachment coordinates and use the original virtual endpoints.
 const auto contract=ClassifyWeaponRangeViewport({viewport.originX,viewport.originY,viewport.width,viewport.height},line);
 if(!contract.valid || (scissor.x>targetExtent[0]||scissor.width>targetExtent[0]-scissor.x||scissor.y>targetExtent[1]||scissor.height>targetExtent[1]-scissor.y)){
  const auto v=draw.surface.viewport.value_or(ScreenViewport{});
  char detail[512];std::snprintf(detail,sizeof(detail),"Weapon range viewport exceeds validated raster bounds: raw=%d,%d,%u,%u mapped=%.9f,%.9f,%.9f,%.9f scissor=%llu,%llu,%llu,%llu target=%u,%u line=%u minimap=%g",v.x,v.y,v.width,v.height,viewport.originX,viewport.originY,viewport.width,viewport.height,(unsigned long long)scissor.x,(unsigned long long)scissor.y,(unsigned long long)scissor.width,(unsigned long long)scissor.height,targetExtent[0],targetExtent[1],unsigned(line),draw.program.inMiniMap);
  Require(false,detail);
 }
 static const bool witness=[] {const char* v=std::getenv("BAR_METAL4_WEAPON_RANGE_VIEWPORT_WITNESS");return v&&*v=='1';}();
 if(witness&&contract.extended){
  static std::array<std::atomic_bool,2> reported{};
  if(!reported[unsigned(line)].exchange(true)){
   const auto v=draw.surface.viewport.value_or(ScreenViewport{});const auto sc=draw.surface.scissor.value_or(ScreenScissor{});
   std::fprintf(stderr,"[WeaponRangeVirtualViewport] accepted line=%u rebaseFill=%u samples=%u raw=%d,%d,%u,%u rawScissor=%d,%d,%u,%u mapped=%.9f,%.9f,%.9f,%.9f scissor=%llu,%llu,%llu,%llu target=%u,%u\n",unsigned(line),unsigned(contract.rebaseFill),samples,v.x,v.y,v.width,v.height,sc.x,sc.y,sc.width,sc.height,viewport.originX,viewport.originY,viewport.width,viewport.height,(unsigned long long)scissor.x,(unsigned long long)scissor.y,(unsigned long long)scissor.width,(unsigned long long)scissor.height,targetExtent[0],targetExtent[1]);
  }
 }
	Require(viewport.width>0 && viewport.height>0 && viewport.znear==0 && viewport.zfar==1,"Weapon range requires original full clip-depth viewport");
	Require(targetExtent[0] && targetExtent[1] && targetExtent[0]<=16384 && targetExtent[1]<=16384,"Original weapon range target extent exceeds raster bounds");
	Require(samples&&[lease.Device() supportsTextureSampleCount:samples],"Weapon range output sample count unsupported");
	Require(depth==MTLPixelFormatInvalid || depth==MTLPixelFormatDepth16Unorm || depth==MTLPixelFormatDepth32Float || depth==MTLPixelFormatDepth32Float_Stencil8,"Original weapon range actual depth attachment format differs");
	Require(stencil==MTLPixelFormatInvalid || stencil==MTLPixelFormatStencil8 || stencil==MTLPixelFormatDepth32Float_Stencil8,"Original weapon range actual stencil attachment format differs");
 Require(bool(resolver),"Original31657 range images require actual shared owners");
 const bool composite=draw.program.defines->composite;
 if(draw.program.defines->maskPass)Require(samples==1 && color==MTLPixelFormatR8Unorm && depth==MTLPixelFormatDepth16Unorm && stencil==MTLPixelFormatInvalid,"Original31657 coverage requires its actual R8/D16 single-sample target");
 std::erase_if(impl->buffers,[](const auto& entry){return entry.second.original.expired();});
 auto& out=*result->impl;out.source=draw;out.rasterState=draw.raster;out.raster={viewport,scissor};
 if(!composite){
  auto image=resolver(lease,draw.height,consumer);
  Require(image.kind==StartBoxImageKind::Color && image.original==draw.height.object && image.texture && image.sampler && bool(image.storageOwner),"Weapon range resolver returned different height storage or sampler");
  const auto& allocation=image.original->allocation;
  Require(image.texture.textureType==MTLTextureType2D && image.texture.pixelFormat==MTLPixelFormatR32Float && image.texture.width==allocation.width && image.texture.height==allocation.height && image.texture.mipmapLevelCount==allocation.levels && (image.texture.usage&MTLTextureUsageShaderRead),"Original range requires real R32F height storage");
  Require(image.original->sampling.swizzle[0]==0x1903 && image.texture.swizzle.red==MTLTextureSwizzleRed,"Original range height red-channel mapping differs");
  out.height=std::move(image);
 }
 if(composite || draw.program.maskClip>0.5f){
  Require(draw.program.inMiniMap<0.5f || draw.surface.viewport.has_value(),"Original31657 minimap mask read requires its observed source viewport");
  auto image=resolver(lease,draw.coverageMask,consumer);
  Require(image.kind==StartBoxImageKind::Color && image.original==draw.coverageMask.object && image.texture && bool(image.storageOwner),"Original31657 mask resolver returned different image/version owner");
  Require(image.texture.textureType==MTLTextureType2D && image.texture.pixelFormat==MTLPixelFormatR8Unorm && image.texture.width==image.original->allocation.width && image.texture.height==image.original->allocation.height && image.texture.mipmapLevelCount==1 && (image.texture.usage&MTLTextureUsageShaderRead),"Original31657 coverage must sample its actual readable R8 image");
  Require(image.original->sampling.swizzle[0]==0x1903 && image.texture.swizzle.red==MTLTextureSwizzleRed,"Original31657 coverage red mapping differs");
  out.coverageMask=std::move(image);
 }
	// Original GL tests pass without writes when the actual FBO has no
	// corresponding attachment. Preserve the source capture and derive only
	// effective native state, as the common screen consumer already does.
	if(depth==MTLPixelFormatInvalid){out.rasterState.depthTest=false;out.rasterState.depthWrite=false;}
	if(stencil==MTLPixelFormatInvalid)out.rasterState.stencilTest=false;
	// GL face culling and polygon-offset fill do not apply to LINE_LOOP.
	if(line){out.rasterState.cullEnabled=false;out.rasterState.polygonOffset=false;
		// GL viewport maps and clips the center line, but does not scissor
		// its expanded width. Keep the captured scissor and rasterize the
		// coverage quads in the full attachment viewport.
		out.raster.viewport={0,0,double(targetExtent[0]),double(targetExtent[1]),0,1};}
 if(contract.rebaseFill)out.raster.viewport={0,0,double(targetExtent[0]),double(targetExtent[1]),0,1};
 auto u=ReadNativeWeaponRangeUniforms(draw);u.options[2]=unsigned(out.height.flipV);u.programOptions[3]=unsigned(out.coverageMask.flipV);
 u.viewport={float(viewport.originX),float(viewport.originY),float(viewport.width),float(viewport.height)};
 u.outputViewport={0,0,float(targetExtent[0]),float(targetExtent[1])};
 u.renderOptions={unsigned(line),unsigned(draw.surface.alphaTest.compare)|(unsigned(draw.surface.alphaTest.enabled)<<8),0,0};
 if(out.coverageMask.texture)Require(u.viewGeometry[0]>0 && u.viewGeometry[1]>0 && out.coverageMask.texture.width==u.viewGeometry[0] && out.coverageMask.texture.height==u.viewGeometry[1],"Original31657 mask extent differs from source world view");
 auto vertices=impl->Buffer(lease,draw.vertices),uniforms=upload177(&u,sizeof(u));
 auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=4;descriptor.maxTextureBindCount=2;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;
 NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(out.arguments!=nil,"Cannot allocate original31657 range arguments: "+Error(error));lease.Retain(out.arguments);
 [out.arguments setAddress:vertices.gpuAddress+size_t(draw.firstVertex)*16 atIndex:0];[out.arguments setAddress:uniforms.gpuAddress atIndex:3];
 if(!composite){
  auto instances=impl->Buffer(lease,draw.instances);auto positions=ReadNativeWeaponRangeModelPositions(draw);
  // Static source does not read model rows. This remains an unused ABI binding only.
  std::array<float,4> unused{};auto models=positions.empty()?upload177(unused.data(),sizeof(unused)):upload177(positions.data(),positions.size()*sizeof(positions.front()));
  [out.arguments setAddress:instances.gpuAddress+size_t(draw.firstInstance)*96 atIndex:1];[out.arguments setAddress:models.gpuAddress atIndex:2];
  lease.AddAllocation(out.height.texture);lease.Retain(out.height.sampler);
  [out.arguments setTexture:out.height.texture.gpuResourceID atIndex:0];[out.arguments setSamplerState:out.height.sampler.gpuResourceID atIndex:0];
 }
 if(out.coverageMask.texture){lease.AddAllocation(out.coverageMask.texture);[out.arguments setTexture:out.coverageMask.texture.gpuResourceID atIndex:1];}
    // The same buffer prefix works for both admitted circle sizes. Its GPU lease
    // owns submitted storage even after the private cache has been cleared.
    if (CanIndexRangeLine(composite,draw.primitive,draw.vertexCount,samples))
        out.lineIndices=impl->LineIndices(lease);
	out.pipeline=impl->Pipeline(lease,draw,color,depth,stencil,samples,validation,milliseconds,contract.rebaseFill);out.depth=effectstate::Depth(lease,out.rasterState,stencil);
	out.count=composite?draw.vertexCount:line?draw.vertexCount*6:(draw.vertexCount-2)*3;out.instances=draw.instanceCount;return result;
}
}
