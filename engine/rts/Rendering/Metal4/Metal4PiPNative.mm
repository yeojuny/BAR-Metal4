/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PiPNative.h"
#include "Metal4MiniMapNative.h"
#include "NativePiPShaders.h"
#include "NativeSourceLineRasterShaders130.h"
#include "NativePiPSourceLineShaders130.h"
#include "Rendering/Backend/NativePiPSourceLines130.h"
#include "Rendering/Backend/NativeExactPiPLines.h"
#include "NativeExactWorldLineShaders.h"
#include "NativeExactPixelLineShaders.h"
#include "NativeExactPiPLineShaders.h"
#include "Metal4SharedUploadPool.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>

// Both the prepared draw and the lease retain the pool token, including errors
// after encoding. A slot cannot be rewritten before actual submission retirement.
@interface M4PiPLineLifetime123 : NSObject {
@public
 std::shared_ptr<rendering::metal4::ScreenUploadStorage> storage;
}
@end
@implementation M4PiPLineLifetime123
@end

namespace rendering::metal4 {
namespace {
MTLBlendFactor Factor(ScreenBlendFactor value)
{
	switch (value) {
		case ScreenBlendFactor::Zero: return MTLBlendFactorZero;
		case ScreenBlendFactor::One: return MTLBlendFactorOne;
		case ScreenBlendFactor::SourceAlpha: return MTLBlendFactorSourceAlpha;
		case ScreenBlendFactor::OneMinusSourceAlpha: return MTLBlendFactorOneMinusSourceAlpha;
		case ScreenBlendFactor::DestinationAlpha: return MTLBlendFactorDestinationAlpha;
		case ScreenBlendFactor::OneMinusDestinationAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
		case ScreenBlendFactor::SourceColor: return MTLBlendFactorSourceColor;
		case ScreenBlendFactor::OneMinusSourceColor: return MTLBlendFactorOneMinusSourceColor;
		case ScreenBlendFactor::DestinationColor: return MTLBlendFactorDestinationColor;
		case ScreenBlendFactor::OneMinusDestinationColor: return MTLBlendFactorOneMinusDestinationColor;
		case ScreenBlendFactor::SourceAlphaSaturate: return MTLBlendFactorSourceAlphaSaturated;
	}
	throw std::runtime_error("Invalid original PiP blend factor");
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch (value) {
		case ScreenBlendEquation::Add: return MTLBlendOperationAdd;
		case ScreenBlendEquation::Subtract: return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract: return MTLBlendOperationReverseSubtract;
		case ScreenBlendEquation::Minimum: return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum: return MTLBlendOperationMax;
	}
	throw std::runtime_error("Invalid original PiP blend equation");
}
std::uint32_t BlendKey(const ScreenBlendState& s)
{
	Factor(s.sourceColor); Factor(s.destinationColor); Factor(s.sourceAlpha); Factor(s.destinationAlpha);
	Equation(s.colorEquation); Equation(s.alphaEquation);
	return std::uint32_t(s.enabled) | (std::uint32_t(s.sourceColor) << 1) | (std::uint32_t(s.destinationColor) << 5) |
		(std::uint32_t(s.sourceAlpha) << 9) | (std::uint32_t(s.destinationAlpha) << 13) |
		(std::uint32_t(s.colorEquation) << 17) | (std::uint32_t(s.alphaEquation) << 20);
}
MTLCompareFunction Compare(ScreenAlphaCompare value)
{
	switch (value) {
		case ScreenAlphaCompare::Never: return MTLCompareFunctionNever;
		case ScreenAlphaCompare::Less: return MTLCompareFunctionLess;
		case ScreenAlphaCompare::Equal: return MTLCompareFunctionEqual;
		case ScreenAlphaCompare::LessEqual: return MTLCompareFunctionLessEqual;
		case ScreenAlphaCompare::Greater: return MTLCompareFunctionGreater;
		case ScreenAlphaCompare::NotEqual: return MTLCompareFunctionNotEqual;
		case ScreenAlphaCompare::GreaterEqual: return MTLCompareFunctionGreaterEqual;
		case ScreenAlphaCompare::Always: return MTLCompareFunctionAlways;
	}
	throw std::runtime_error("Invalid original PiP compare");
}
MTLStencilOperation Operation(NativeWorldStencilOp value)
{
	switch (value) {
		case NativeWorldStencilOp::Keep: return MTLStencilOperationKeep;
		case NativeWorldStencilOp::Zero: return MTLStencilOperationZero;
		case NativeWorldStencilOp::Replace: return MTLStencilOperationReplace;
		case NativeWorldStencilOp::IncrementClamp: return MTLStencilOperationIncrementClamp;
		case NativeWorldStencilOp::DecrementClamp: return MTLStencilOperationDecrementClamp;
		case NativeWorldStencilOp::Invert: return MTLStencilOperationInvert;
		case NativeWorldStencilOp::IncrementWrap: return MTLStencilOperationIncrementWrap;
		case NativeWorldStencilOp::DecrementWrap: return MTLStencilOperationDecrementWrap;
	}
	throw std::runtime_error("Invalid original PiP stencil operation");
}
id<MTLBuffer> Upload(RuntimeLease& lease, const void* bytes, std::size_t length)
{
	Require(length && length <= lease.Device().maxBufferLength, "Original PiP buffer exceeds the actual device limit");
	auto result = [lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];
	Require(result != nil, "Cannot allocate original PiP immutable buffer");
	lease.AddAllocation(result);
	return result;
}
#include "Metal4PiPSourceLines130.inc"
MTLSamplerAddressMode Wrap(ScreenWrap value)
{
	switch (value) {
		case ScreenWrap::ClampEdge: return MTLSamplerAddressModeClampToEdge;
		case ScreenWrap::TransparentBorder: return MTLSamplerAddressModeClampToBorderColor;
		case ScreenWrap::Repeat: return MTLSamplerAddressModeRepeat;
		case ScreenWrap::LegacyClamp: break;
	}
	throw std::runtime_error("PiP explicit shader GL_CLAMP needs its derivative-preserving coordinate operation");
}
struct PiPLineProbe123 {
 const char* path=std::getenv("BAR_NATIVE_PIP_LINE_PROBE_FILE");
 uint64_t draws=0,segments=0,vertices=0,bytes=0,cpuDraws=0,multisample=0,sourceDraws=0,sourceSegments=0,sourceBytes=0,sourceMSAA=0;
 ~PiPLineProbe123(){if(path&&*path)if(auto f=std::fopen(path,"w")){std::fprintf(f,"gpuDraws,gpuSegments,gpuVertices,sharedPacketBytes,cpuDraws,multisampleCpuDraws,sourceGPUDraws,sourceGPUSegments,sourceInputBytes,sourceGPUMultisampleDraws\n%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)draws,(unsigned long long)segments,(unsigned long long)vertices,(unsigned long long)bytes,(unsigned long long)cpuDraws,(unsigned long long)multisample,(unsigned long long)sourceDraws,(unsigned long long)sourceSegments,(unsigned long long)sourceBytes,(unsigned long long)sourceMSAA);std::fclose(f);}}
};
inline PiPLineProbe123& LineProbe123(){static PiPLineProbe123 probe;return probe;}
struct PiPLineJob123 {
 __strong id<MTLComputePipelineState> colors=nil,geometry=nil;
 __strong id<MTL4ArgumentTable> colorArgs=nil,geometryArgs=nil;
 __strong M4PiPLineLifetime123* lifetime=nil;
 uint32_t segments=0,quads=0;
 mutable bool encoded=false;
 void Encode(id<MTL4CommandBuffer> command) const {
  if(encoded)return;
  auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot encode exact PiP lines");
  [encoder setComputePipelineState:colors];[encoder setArgumentTable:colorArgs];
  [encoder dispatchThreads:MTLSizeMake(segments,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
  [encoder barrierAfterEncoderStages:MTLStageDispatch beforeEncoderStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
  [encoder setComputePipelineState:geometry];[encoder setArgumentTable:geometryArgs];
  [encoder dispatchThreads:MTLSizeMake(quads,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
  [encoder barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
  [encoder endEncoding];encoded=true;
 }
};
struct ImageDependencies {
	std::shared_ptr<PreparedMiniMap> images;
	// Immutable earlier producer prerequisites. A later role never changes a
	// plan already held by a draw, encoded, or awaiting completion.
	std::vector<std::shared_ptr<ImageDependencies>> preceding;
	mutable bool encoded = false;
	mutable bool completed = false;
	void Encode(id<MTL4CommandBuffer> command) const
	{
		if (encoded) return;
		for(const auto& producer:preceding) producer->Encode(command);
		images->EncodeDependencies(command);
		encoded = true;
	}
	void Complete() const
	{
		Require(encoded, "PiP sampled engine image has no encoded producer");
		if(completed) return;
		for(const auto& producer:preceding) producer->Complete();
		images->DidComplete();
		completed=true;
	}
};
struct FrameImageDependencies {
	NativeMiniMapImageMask available=0;
	std::array<std::shared_ptr<ImageDependencies>,4> roles;
};
} // namespace

struct PreparedPiPDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline = nil;
	__strong id<MTLDepthStencilState> depthStencil = nil;
	__strong id<MTL4ArgumentTable> arguments = nil;
	__strong id<MTLBuffer> indices = nil;
	std::vector<std::shared_ptr<ImageDependencies>> dependencies;
 std::unique_ptr<PiPLineJob123> line;
	std::vector<PreparedPiPSourceImage> sourceImages;
	std::vector<PreparedScreenExternalImage> retainedImages;
	NativeWorldDrawState raster;
	MTLViewport viewport{};
	MTLScissorRect scissor{};
	std::uint32_t count = 0, first = 0, instances = 0, firstInstance = 0;
	bool indexed = false;
 bool sourceLine130=false;
};
PreparedPiPDraw::PreparedPiPDraw(): impl(std::make_unique<Impl>()) {}
PreparedPiPDraw::~PreparedPiPDraw() = default;
bool PreparedPiPDraw::Empty() const { return impl->count == 0; }
void PreparedPiPDraw::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	if(impl->line)impl->line->Encode(command);
	for (const auto& dependency: impl->dependencies) dependency->Encode(command);
	for(const auto& image:impl->sourceImages)if(image.encodeDependencies)image.encodeDependencies(command);
}
void PreparedPiPDraw::DidComplete() const
{
	if(impl->line)Require(impl->line->encoded,"PiP line completion has no producer");
	for (const auto& dependency: impl->dependencies) dependency->Complete();
	for(const auto& image:impl->sourceImages)if(image.didComplete)image.didComplete();
}
void PreparedPiPDraw::Encode(id<MTL4RenderCommandEncoder> encoder) const
{
	if (Empty()) return;
 if(impl->line)Require(impl->line->encoded,"PiP line draw precedes its producer");
	for(const auto& image:impl->sourceImages)if(image.requireReadable)image.requireReadable();
	for(const auto& image:impl->retainedImages)if(image.requireReadable)image.requireReadable();
	const auto& s = impl->raster;
	if (s.cullEnabled && s.cull == NativeWorldCull::FrontAndBack) return;
	[encoder setRenderPipelineState:impl->pipeline];
	[encoder setDepthStencilState:impl->depthStencil];
	[encoder setDepthClipMode:MTLDepthClipModeClip];
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setCullMode:s.cullEnabled && s.cull != NativeWorldCull::None ?
		(s.cull == NativeWorldCull::Front ? MTLCullModeFront : MTLCullModeBack) : MTLCullModeNone];
	[encoder setTriangleFillMode:MTLTriangleFillModeFill];
	[encoder setDepthBias:s.polygonOffset ? s.offsetUnits : 0 slopeScale:s.polygonOffset ? s.offsetFactor : 0 clamp:0];
	[encoder setStencilFrontReferenceValue:std::uint32_t(std::clamp(s.frontStencil.reference,0,255))
		backReferenceValue:std::uint32_t(std::clamp(s.backStencil.reference,0,255))];
	[encoder setViewport:impl->viewport];
	[encoder setScissorRect:impl->scissor];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex | MTLRenderStageFragment];
	if (impl->indexed) {
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:impl->count indexType:MTLIndexTypeUInt32
			indexBuffer:impl->indices.gpuAddress indexBufferLength:impl->indices.length];
	} else {
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:impl->first vertexCount:impl->count
			instanceCount:impl->instances baseInstance:impl->firstInstance];
	}
}

struct PiPGPU::Impl {
 ScreenUploadPool lineUploads;
 __strong id<MTLLibrary> lineLibrary=nil;
 __strong id<MTLComputePipelineState> lineColors=nil,lineGeometry=nil;
 bool lineValidation=false;
 void CompileLines(RuntimeLease& lease,bool validate,double& shaderMs){
  if(lineLibrary&&lineValidation==validate)return;
  const auto start=Clock::now();NSError* error=nil;
  auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  auto library=lease.NewLibraryWithSource(String(std::string(ExactWorldLineShader)+ExactPixelLineShader+ExactPiPLineShader),options,&error);
  Require(library,"Cannot compile exact PiP line arithmetic: "+Error(error));
  const auto compile=[&](NSString* name){
   auto function=[MTL4LibraryFunctionDescriptor new];function.library=library;function.name=name;
   auto descriptor=[MTL4ComputePipelineDescriptor new];descriptor.computeFunctionDescriptor=function;descriptor.options=[MTL4PipelineOptions new];descriptor.options.shaderValidation=ShaderValidationMode(validate);
   auto pipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];
   Require(pipeline,"Cannot compile exact PiP line pipeline: "+Error(error));return pipeline;
  };
  auto colors=compile(@"PiPLineColors123"),geometry=compile(@"PiPLineGeometry123");
  lineLibrary=library;lineColors=colors;lineGeometry=geometry;lineValidation=validate;shaderMs+=Milliseconds(start);
 }
 void PrepareLine(RuntimeLease& lease,PreparedPiPDraw::Impl& out,const exactWorldLine::Packet& packet,
  const NativePiPDraw& source,bool validate,double& shaderMs){
  CompileLines(lease,validate,shaderMs);lease.Retain(lineLibrary);lease.Retain(lineColors);lease.Retain(lineGeometry);
  lineUploads.BeginBatch(lease.Device());
  const auto align=[](size_t n){return (n+255)&~size_t(255);};
  const size_t segmentBytes=packet.segments.size()*sizeof(exactWorldLine::Line120Segment);
  const size_t spansOffset=align(segmentBytes),countOffset=align(spansOffset+packet.spans.size()*sizeof(exactWorldLine::Line120Span));
  const size_t uniformOffset=align(countOffset+sizeof(uint32_t)),packetBytes=uniformOffset+sizeof(source.program.uniforms);
  auto job=std::make_unique<PiPLineJob123>();job->lifetime=[M4PiPLineLifetime123 new];lease.Retain(job->lifetime);
  job->lifetime->storage=lineUploads.Acquire(lease.Device(),packetBytes);
  auto upload=job->lifetime->storage?job->lifetime->storage->buffer:[lease.Device() newBufferWithLength:packetBytes options:MTLResourceStorageModeShared];
  Require(upload&&upload.contents,"Cannot allocate exact PiP shared packet");lease.AddAllocation(upload);
  const uint32_t spanCount=packet.spans.size();auto bytes=static_cast<uint8_t*>(upload.contents);
  std::memcpy(bytes,packet.segments.data(),segmentBytes);std::memcpy(bytes+spansOffset,packet.spans.data(),packet.spans.size()*sizeof(exactWorldLine::Line120Span));
  std::memcpy(bytes+countOffset,&spanCount,sizeof(spanCount));std::memcpy(bytes+uniformOffset,&source.program.uniforms,sizeof(source.program.uniforms));
  auto segments=[lease.Device() newBufferWithLength:segmentBytes options:MTLResourceStorageModePrivate];
  auto vertices=[lease.Device() newBufferWithLength:size_t(packet.quads)*4*sizeof(NativePiPImmediateVertex) options:MTLResourceStorageModePrivate];
  auto indices=[lease.Device() newBufferWithLength:size_t(packet.quads)*6*sizeof(uint32_t) options:MTLResourceStorageModePrivate];
  Require(segments&&vertices&&indices,"Cannot allocate exact PiP generated geometry");
  lease.AddAllocation(segments);lease.AddAllocation(vertices);lease.AddAllocation(indices);
  auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=5;descriptor.initializeBindings=YES;NSError* error=nil;
  job->colorArgs=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];job->geometryArgs=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
  Require(job->colorArgs&&job->geometryArgs,"Cannot allocate exact PiP compute arguments: "+Error(error));lease.Retain(job->colorArgs);lease.Retain(job->geometryArgs);
  [job->colorArgs setAddress:upload.gpuAddress atIndex:0];[job->colorArgs setAddress:segments.gpuAddress atIndex:1];[job->colorArgs setAddress:upload.gpuAddress+uniformOffset atIndex:2];
  [job->geometryArgs setAddress:segments.gpuAddress atIndex:0];[job->geometryArgs setAddress:upload.gpuAddress+spansOffset atIndex:1];[job->geometryArgs setAddress:vertices.gpuAddress atIndex:2];[job->geometryArgs setAddress:upload.gpuAddress+countOffset atIndex:3];[job->geometryArgs setAddress:indices.gpuAddress atIndex:4];
  job->colors=lineColors;job->geometry=lineGeometry;job->segments=packet.segments.size();job->quads=packet.quads;
  [out.arguments setAddress:vertices.gpuAddress atIndex:0];[out.arguments setAddress:upload.gpuAddress+uniformOffset atIndex:3];
  out.indices=indices;out.count=packet.quads*6;out.indexed=true;out.line=std::move(job);
  auto& probe=LineProbe123();if(probe.path&&*probe.path){++probe.draws;probe.segments+=packet.segments.size();probe.vertices+=uint64_t(packet.quads)*4;probe.bytes+=packetBytes;}
 }

	__strong id<MTLLibrary> library = nil;
	bool validation = false;
	using PipelineKey = std::tuple<unsigned,NSUInteger,NSUInteger,NSUInteger,std::uint32_t,unsigned,std::uint32_t,bool>;
	std::map<PipelineKey, id<MTLRenderPipelineState>> pipelines;
	using SamplerKey = std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,bool>;
	std::map<SamplerKey, id<MTLSamplerState>> samplers;
	struct Buffer { std::weak_ptr<const NativeBufferSnapshot> owner; __strong id<MTLBuffer> value = nil; };
	std::unordered_map<const NativeBufferSnapshot*,Buffer> buffers;
	MiniMapGPU map;
	std::map<std::shared_ptr<const NativeMiniMapBackground>,FrameImageDependencies,
		std::owner_less<std::shared_ptr<const NativeMiniMapBackground>>> frameMaps;
	std::uint64_t imageOwners180=0,losOutputs180=0,radarOutputs180=0;
	~Impl() {
#ifdef RECOIL_METAL4_LIVE_SCENE
		if(imageOwners180) LOG("[NativePiPImageDemand180] owners=%llu los_outputs=%llu radar_outputs=%llu raw_inputs=%llu omitted_los=%llu omitted_radar=%llu omitted_compute=%llu omitted_mipmap=%llu scope=owner_lifetime",
			(unsigned long long)imageOwners180,(unsigned long long)losOutputs180,(unsigned long long)radarOutputs180,
			(unsigned long long)(losOutputs180+2*radarOutputs180),(unsigned long long)(imageOwners180-losOutputs180),
			(unsigned long long)(imageOwners180-radarOutputs180),(unsigned long long)(2*imageOwners180-losOutputs180-radarOutputs180),
			(unsigned long long)(2*imageOwners180-losOutputs180-radarOutputs180));
#endif
	}
 PiPSourceVertexCache130 sourceLineVertices130;
 __strong id<MTLLibrary> sourceLineLibrary130=nil;
 bool sourceLineValidation130=false;
 void SourceLines130(RuntimeLease& lease,bool validate,double& shaderMs){
  if(sourceLineLibrary130&&sourceLineValidation130==validate)return;
  const auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Original PiP source GPU line130";descriptor.source=String(std::string(NativePiPMetalPrograms)+SourceLineRasterShader130+PiPSourceLinesShader130);
  auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;NSError* error=nil;
  auto next=lease.NewLibrary(descriptor,&error);Require(next,"Cannot compile original PiP source line130: "+Error(error));sourceLineLibrary130=next;sourceLineValidation130=validate;shaderMs+=Milliseconds(start);
 }


	void Library(RuntimeLease& lease, bool validate, double& shaderMs)
	{
		if (library != nil && validation == validate) return;
		const auto start = Clock::now();
		auto descriptor = [MTL4LibraryDescriptor new];
		descriptor.name = @"Exact original BAR current31657 PiP programs";
		descriptor.source = String(NativePiPMetalPrograms);
		auto options = [MTLCompileOptions new];
		options.mathMode = MTLMathModeSafe;
		options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
		descriptor.options = options;
		NSError* error = nil;
		auto next = lease.NewLibrary(descriptor,&error);
		Require(next != nil, "Cannot compile exact original PiP programs: " + Error(error));
		library = next; validation = validate; pipelines.clear();
		shaderMs += Milliseconds(start);
	}
	id<MTLBuffer> VertexBuffer(RuntimeLease& lease, const NativeBufferSnapshotRef& source)
	{
		auto& entry = buffers[source.get()];
		if (entry.owner.expired()) { entry.owner = source; entry.value = Upload(lease,source->bytes.data(),source->bytes.size()); }
		else lease.AddAllocation(entry.value);
		return entry.value;
	}
	id<MTLSamplerState> Sampler(RuntimeLease& lease, const ScreenSampler& source)
	{
		ValidateScreenSampler(source);
		const auto mag = source.magnificationFilter.value_or(source.filter);
		const SamplerKey key{unsigned(source.filter),unsigned(mag),unsigned(source.wrapS),unsigned(source.wrapT),
			unsigned(source.mipFilter),source.anisotropy,source.whiteBorder};
		auto& result = samplers[key];
		if (result == nil) {
			auto d = [MTLSamplerDescriptor new];
			d.supportArgumentBuffers = YES;
			d.minFilter = source.filter == ScreenFilter::Linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
			d.magFilter = mag == ScreenFilter::Linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
			d.mipFilter = source.mipFilter == ScreenMipFilter::None ? MTLSamplerMipFilterNotMipmapped :
				(source.mipFilter == ScreenMipFilter::Nearest ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear);
			d.sAddressMode = Wrap(source.wrapS); d.tAddressMode = Wrap(source.wrapT);
			d.maxAnisotropy = source.anisotropy;
			d.borderColor = source.whiteBorder ? MTLSamplerBorderColorOpaqueWhite : MTLSamplerBorderColorTransparentBlack;
			result = [lease.Device() newSamplerStateWithDescriptor:d];
			Require(result != nil, "Cannot create original PiP sampler");
		}
		lease.Retain(result);
		return result;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease, const NativePiPDraw& source,
		MTLPixelFormat colorFormat, MTLPixelFormat depthFormat, MTLPixelFormat stencilFormat, bool validate, double& shaderMs,std::uint32_t samples,bool sourceLine130=false)
	{
		Library(lease,validate,shaderMs);
  if(sourceLine130)SourceLines130(lease,validate,shaderMs);
		unsigned mask = 0;
		for (unsigned i=0; i<4; ++i) if (source.surface.colorWrite[i]) mask |= 1u << i;
		const PipelineKey key{unsigned(source.program.program)+(sourceLine130?16u:0u),colorFormat,depthFormat,stencilFormat,BlendKey(source.surface.blend),mask,lease.SampleShadingKey(samples),source.raster.alphaToCoverage};
		auto& result = pipelines[key];
		if (result == nil) {
			const auto start = Clock::now();
			const auto& spec = GetNativePiPProgramSpec(source.program.program);
			auto d = [MTL4RenderPipelineDescriptor new];
			d.rasterSampleCount=samples;
			d.alphaToCoverageState=source.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
			auto vertex = [MTL4LibraryFunctionDescriptor new]; vertex.library = sourceLine130?sourceLineLibrary130:library;
			vertex.name = sourceLine130?(samples>1?@"PiPSourceVertexMSAA130":@"PiPSourceVertex130"):(source.program.program == NativePiPProgram::Line ? @"pipLineCoverageVertex" : String("pip" + std::string(spec.name) + "Vertex"));
			auto fragment = [MTL4LibraryFunctionDescriptor new]; fragment.library = sourceLine130&&samples==1?sourceLineLibrary130:library; fragment.name = sourceLine130&&samples==1?@"PiPSourceFragment130":String("pip" + std::string(spec.name) + "Fragment");
			d.vertexFunctionDescriptor = vertex; d.fragmentFunctionDescriptor = fragment;
			d.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
			// Metal 4 binds depth/stencil attachment formats at the render pass.
			// They remain in the key so attachment absence cannot reuse wrong state.
			auto options = [MTL4PipelineOptions new]; options.shaderValidation = ShaderValidationMode(validate); d.options = options;
			const auto& blend = source.surface.blend;
			auto color = d.colorAttachments[0]; color.pixelFormat = colorFormat;
			color.blendingState = blend.enabled ? MTL4BlendStateEnabled : MTL4BlendStateDisabled;
			color.sourceRGBBlendFactor = Factor(blend.sourceColor); color.destinationRGBBlendFactor = Factor(blend.destinationColor);
			color.sourceAlphaBlendFactor = Factor(blend.sourceAlpha); color.destinationAlphaBlendFactor = Factor(blend.destinationAlpha);
			color.rgbBlendOperation = Equation(blend.colorEquation); color.alphaBlendOperation = Equation(blend.alphaEquation);
			color.writeMask = MTLColorWriteMaskNone;
			if (mask & 1) color.writeMask |= MTLColorWriteMaskRed;
			if (mask & 2) color.writeMask |= MTLColorWriteMaskGreen;
			if (mask & 4) color.writeMask |= MTLColorWriteMaskBlue;
			if (mask & 8) color.writeMask |= MTLColorWriteMaskAlpha;
			NSError* error = nil;
			result = lease.NewRenderPipeline(d,&error);
			Require(result != nil, "Cannot compile original PiP render state: " + Error(error));
			shaderMs += Milliseconds(start);
		}
		lease.Retain(library); lease.Retain(result);if(sourceLine130)lease.Retain(sourceLineLibrary130);
		return result;
	}
};

PiPGPU::PiPGPU(): impl(std::make_unique<Impl>()) {}
PiPGPU::~PiPGPU() = default;
void PiPGPU::BeginFrame()
{
	impl->frameMaps.clear();
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});impl->sourceLineVertices130.BeginFrame();
}
void PiPGPU::ClearResources()
{
	BeginFrame(); impl->sourceLineVertices130.Clear(); impl->buffers.clear(); impl->map.ClearResources();impl->lineUploads=ScreenUploadPool{};
}
std::unique_ptr<PreparedPiPDraw> PiPGPU::Prepare(RuntimeLease& lease, const NativePiPDraw& source,
	ScreenGPU& sharedImages, const PreparedScreenFrame* producers, MTLViewport viewport, MTLScissorRect scissor,
	MTLPixelFormat colorFormat, MTLPixelFormat depthFormat, MTLPixelFormat stencilFormat, bool validation, double& shaderMs,
	std::optional<NativePostprocessConsumer> consumer,const PiPSourceImageProvider& sourceImages,std::uint32_t samples,std::uint32_t targetWidth,std::uint32_t targetHeight)
{
	ValidateNativePiPDraw(source);
	Require(!source.surface.alphaTest.enabled, "Original PiP active shader alpha-test branch is not paired");
	Require(source.raster.frontPolygonMode == NativePolygonMode::Fill && source.raster.backPolygonMode == NativePolygonMode::Fill,
		"Original PiP program requires fill polygon state");
	auto result = std::unique_ptr<PreparedPiPDraw>(new PreparedPiPDraw());
	auto& out = *result->impl;
	out.raster = source.raster; out.viewport = viewport; out.scissor = scissor;
	const auto geometry = GetNativePiPProgramSpec(source.program.program).geometry;
	if (geometry == NativePiPGeometry::Instanced && source.instanceCount == 0) return result;
	if (geometry == NativePiPGeometry::Line && source.vertexCount == 0) return result;
	auto sourceRaster130=geometry==NativePiPGeometry::Line?pipSource130::Admit(source,{viewport.originX,viewport.originY,viewport.width,viewport.height},targetWidth,targetHeight,samples):std::optional<pipSource130::RasterUniform>{};
 if(sourceRaster130){out.sourceLine130=true;out.raster.cullEnabled=false;out.raster.polygonOffset=false;out.raster.backStencil=out.raster.frontStencil;}
 out.pipeline = impl->Pipeline(lease,source,colorFormat,depthFormat,stencilFormat,validation,shaderMs,samples,bool(sourceRaster130));
	// A color-only original FBO has no depth buffer: enabled depth tests pass
	// without writes. Never invent a depth attachment from the state alone.
	const auto& state = out.raster;
	auto depth = [MTLDepthStencilDescriptor new];
	depth.depthCompareFunction = depthFormat != MTLPixelFormatInvalid && state.depthTest ? Compare(state.depthCompare) : MTLCompareFunctionAlways;
	depth.depthWriteEnabled = depthFormat != MTLPixelFormatInvalid && state.depthTest && state.depthWrite;
	if (state.stencilTest && stencilFormat != MTLPixelFormatInvalid) for (bool front: {true,false}) {
		const auto& original = front ? state.frontStencil : state.backStencil;
		auto face = [MTLStencilDescriptor new]; face.stencilCompareFunction = Compare(original.compare);
		face.stencilFailureOperation = Operation(original.fail); face.depthFailureOperation = Operation(original.depthFail);
		face.depthStencilPassOperation = Operation(original.pass); face.readMask = original.readMask; face.writeMask = original.writeMask;
		if (front) depth.frontFaceStencil = face; else depth.backFaceStencil = face;
	}
	out.depthStencil = [lease.Device() newDepthStencilStateWithDescriptor:depth];
	Require(out.depthStencil != nil, "Cannot create original PiP depth/stencil state"); lease.Retain(out.depthStencil);
	auto descriptor = [MTL4ArgumentTableDescriptor new]; descriptor.maxBufferBindCount = sourceRaster130?5:4;
	descriptor.maxTextureBindCount = 3; descriptor.maxSamplerStateBindCount = 3; descriptor.initializeBindings = YES;
	NSError* error = nil; out.arguments = [lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
	Require(out.arguments != nil, "Cannot create original PiP argument table: " + Error(error)); lease.Retain(out.arguments);
	std::optional<exactWorldLine::Packet> exactLines;
 if(!sourceRaster130&&geometry==NativePiPGeometry::Line&&samples==1&&viewport.width>=1&&viewport.height>=1&&viewport.width<=16384&&viewport.height<=16384&&std::floor(viewport.width)==viewport.width&&std::floor(viewport.height)==viewport.height)
  exactLines=exactPiPLine::TryBuild(source,unsigned(viewport.width),unsigned(viewport.height),samples);
 if(sourceRaster130){
  auto vertices=impl->sourceLineVertices130.Acquire(lease,source);auto packet=PreparePiPSourceInputs130(lease,source,*sourceRaster130);
  [out.arguments setAddress:vertices.gpuAddress atIndex:1];[out.arguments setAddress:packet.buffer.gpuAddress atIndex:3];[out.arguments setAddress:packet.buffer.gpuAddress+PiPSourceInputs130::RasterOffset atIndex:4];
  out.count=4;out.instances=source.vertexCount/2;out.viewport={0,0,double(targetWidth),double(targetHeight),0,1};
  auto& probe=LineProbe123();if(probe.path&&*probe.path){++probe.draws;probe.segments+=out.instances;probe.vertices+=uint64_t(out.instances)*4;probe.bytes+=PiPSourceInputs130::Bytes;++probe.sourceDraws;probe.sourceSegments+=out.instances;probe.sourceBytes+=PiPSourceInputs130::Bytes;if(samples>1)++probe.sourceMSAA;}
 }else if(exactLines){
  impl->PrepareLine(lease,out,*exactLines,source,validation,shaderMs);
 }else if (geometry == NativePiPGeometry::Immediate || geometry == NativePiPGeometry::Line) {
		NativePiPLineGeometry expanded;
		if (geometry == NativePiPGeometry::Line) {
   auto& probe=LineProbe123();if(probe.path&&*probe.path){++probe.cpuDraws;if(samples>1)++probe.multisample;}
			// GL line width is measured in actual framebuffer pixels, independent
			// of the widget's logical coordinate basis and Retina presentation scale.
			if(samples>1){
				expanded=BuildNativePiPLineGeometry(source,samples,{viewport.originX,viewport.originY,viewport.width,viewport.height},{targetWidth,targetHeight});
				if(expanded.fullViewport)out.viewport={0,0,double(targetWidth),double(targetHeight),0,1};
			}else{
				Require(viewport.width >= 1 && viewport.height >= 1 && viewport.width <= 16384 && viewport.height <= 16384 &&
					std::floor(viewport.width) == viewport.width && std::floor(viewport.height) == viewport.height,
					"Original PiP 1x line requires its integer framebuffer viewport extent");
				auto rasterSource=source;rasterSource.viewportWidth=std::uint32_t(viewport.width);rasterSource.viewportHeight=std::uint32_t(viewport.height);
				expanded=BuildNativePiPLineGeometry(rasterSource,1);
			}
		}
		const auto& vertices = geometry == NativePiPGeometry::Line ? expanded.vertices : source.immediate;
		const auto& indices = geometry == NativePiPGeometry::Line ? expanded.indices : source.indices;
		if (indices.empty()) return result;
		auto buffer = Upload(lease,vertices.data(),vertices.size()*sizeof(NativePiPImmediateVertex));
		out.indices = Upload(lease,indices.data(),indices.size()*sizeof(std::uint32_t));
		[out.arguments setAddress:buffer.gpuAddress atIndex:0]; out.count = std::uint32_t(indices.size()); out.indexed = true;
	} else {
		auto vertices = impl->VertexBuffer(lease,source.vertices), instances = impl->VertexBuffer(lease,source.instances);
		[out.arguments setAddress:vertices.gpuAddress atIndex:1]; [out.arguments setAddress:instances.gpuAddress atIndex:2];
		out.count = source.vertexCount; out.first = source.firstVertex; out.instances = source.instanceCount; out.firstInstance = source.firstInstance;
	}
	if(!out.line&&!out.sourceLine130){auto uniform = Upload(lease,&source.program.uniforms,sizeof(source.program.uniforms));
	[out.arguments setAddress:uniform.gpuAddress atIndex:3];}
	for (const auto& parameter: GetNativePiPProgramSpec(source.program.program).uniforms) {
		if (!parameter.sampler || !parameter.active) continue;
		const auto& sampled = source.sampledImages[parameter.slot];
		id<MTLTexture> image = nil; id<MTLSamplerState> sampler = nil;
		if(sampled.original.object) {
			Require(bool(sourceImages),"PiP $info lost its actual shared typed producer service");
			auto prepared=sourceImages(lease,sampled.original,sampled.sampler,validation,shaderMs);
			Require(prepared.texture && prepared.sampler && prepared.storageOwner && prepared.encodeDependencies && prepared.requireReadable && prepared.didComplete &&
				unsigned(prepared.flipV)==source.program.uniforms.flipV[parameter.slot],"PiP $info provider substituted its actual producer/origin");
			image=prepared.texture;sampler=prepared.sampler;out.sourceImages.push_back(std::move(prepared));
		} else if (sampled.engineImage != NativePiPEngineImage::None) {
			const auto requested=NativePiPImageDemand(source.program,[&](int,unsigned slot) {
				const auto& input=source.sampledImages[slot];
				return !input.original.object && input.engineSource==sampled.engineSource?input.engineImage:NativePiPEngineImage::None;
			});
			auto& frame=impl->frameMaps[sampled.engineSource];
			const NativeMiniMapImageMask missing=requested & ~frame.available;
			if(missing) {
				auto next=std::make_shared<ImageDependencies>();
				id<MTLTexture> ground=nil;
				if((missing & MiniMapImage::Radar) && (frame.available & MiniMapImage::GroundLOS)) {
					next->preceding.push_back(frame.roles[2]);ground=frame.roles[2]->images->GroundLOS();
				}
				next->images=impl->map.PrepareIndividualImages(lease,sampled.engineSource,missing,validation,shaderMs,ground);
				if(!frame.available) ++impl->imageOwners180;
				if(missing & MiniMapImage::GroundLOS) ++impl->losOutputs180;
				if(missing & MiniMapImage::Radar) ++impl->radarOutputs180;
				for(unsigned role=0;role<frame.roles.size();++role) if(missing & (1u<<role)) frame.roles[role]=next;
				frame.available |= missing;
			}
			const auto& dependency=frame.roles[unsigned(sampled.engineImage)-1];
			Require(bool(dependency),"PiP engine image demand has no prepared role");
			if (std::find(out.dependencies.begin(),out.dependencies.end(),dependency) == out.dependencies.end()) out.dependencies.push_back(dependency);
			switch (sampled.engineImage) {
				case NativePiPEngineImage::Height: image = dependency->images->Height(); break;
				case NativePiPEngineImage::Shading: image = dependency->images->Shading(); break;
				case NativePiPEngineImage::GroundLOS: image = dependency->images->GroundLOS(); break;
				case NativePiPEngineImage::Radar: image = dependency->images->Radar(); break;
				case NativePiPEngineImage::None: break;
			}
			sampler = impl->Sampler(lease,sampled.sampler);
		} else {
			const auto prepared = sharedImages.PrepareImage(lease,sampled.image,sampled.sampler,producers,consumer,
				[&out](PreparedScreenExternalImage image){out.retainedImages.push_back(std::move(image));});
			image = prepared.texture; sampler = prepared.sampler;
		}
		Require(image != nil && sampler != nil, "PiP program has no paired actual sampled image");
		[out.arguments setTexture:image.gpuResourceID atIndex:parameter.slot];
		[out.arguments setSamplerState:sampler.gpuResourceID atIndex:parameter.slot];
	}
	return result;
}
} // namespace rendering::metal4
