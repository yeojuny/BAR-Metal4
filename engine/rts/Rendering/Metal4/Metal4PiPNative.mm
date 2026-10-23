/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PiPNative.h"
#include "Metal4MiniMapNative.h"
#include "NativePiPShaders.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>

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
struct ImageDependencies {
	std::shared_ptr<PreparedMiniMap> images;
	mutable bool encoded = false;
	void Encode(id<MTL4CommandBuffer> command) const
	{
		if (encoded) return;
		images->EncodeDependencies(command);
		encoded = true;
	}
	void Complete() const
	{
		Require(encoded, "PiP sampled engine image has no encoded producer");
		images->DidComplete();
	}
};
} // namespace

struct PreparedPiPDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline = nil;
	__strong id<MTLDepthStencilState> depthStencil = nil;
	__strong id<MTL4ArgumentTable> arguments = nil;
	__strong id<MTLBuffer> indices = nil;
	std::vector<std::shared_ptr<ImageDependencies>> dependencies;
	std::vector<PreparedPiPSourceImage> sourceImages;
	std::vector<PreparedScreenExternalImage> retainedImages;
	NativeWorldDrawState raster;
	MTLViewport viewport{};
	MTLScissorRect scissor{};
	std::uint32_t count = 0, first = 0, instances = 0, firstInstance = 0;
	bool indexed = false;
};
PreparedPiPDraw::PreparedPiPDraw(): impl(std::make_unique<Impl>()) {}
PreparedPiPDraw::~PreparedPiPDraw() = default;
bool PreparedPiPDraw::Empty() const { return impl->count == 0; }
void PreparedPiPDraw::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	for (const auto& dependency: impl->dependencies) dependency->Encode(command);
	for(const auto& image:impl->sourceImages)if(image.encodeDependencies)image.encodeDependencies(command);
}
void PreparedPiPDraw::DidComplete() const
{
	for (const auto& dependency: impl->dependencies) dependency->Complete();
	for(const auto& image:impl->sourceImages)if(image.didComplete)image.didComplete();
}
void PreparedPiPDraw::Encode(id<MTL4RenderCommandEncoder> encoder) const
{
	if (Empty()) return;
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
	__strong id<MTLLibrary> library = nil;
	bool validation = false;
	using PipelineKey = std::tuple<unsigned,NSUInteger,NSUInteger,NSUInteger,std::uint32_t,unsigned,std::uint32_t,bool>;
	std::map<PipelineKey, id<MTLRenderPipelineState>> pipelines;
	using SamplerKey = std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,bool>;
	std::map<SamplerKey, id<MTLSamplerState>> samplers;
	struct Buffer { std::weak_ptr<const NativeBufferSnapshot> owner; __strong id<MTLBuffer> value = nil; };
	std::unordered_map<const NativeBufferSnapshot*,Buffer> buffers;
	MiniMapGPU map;
	std::map<std::shared_ptr<const NativeMiniMapBackground>,std::shared_ptr<ImageDependencies>,
		std::owner_less<std::shared_ptr<const NativeMiniMapBackground>>> frameMaps;

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
		MTLPixelFormat colorFormat, MTLPixelFormat depthFormat, MTLPixelFormat stencilFormat, bool validate, double& shaderMs,std::uint32_t samples)
	{
		Library(lease,validate,shaderMs);
		unsigned mask = 0;
		for (unsigned i=0; i<4; ++i) if (source.surface.colorWrite[i]) mask |= 1u << i;
		const PipelineKey key{unsigned(source.program.program),colorFormat,depthFormat,stencilFormat,BlendKey(source.surface.blend),mask,lease.SampleShadingKey(samples),source.raster.alphaToCoverage};
		auto& result = pipelines[key];
		if (result == nil) {
			const auto start = Clock::now();
			const auto& spec = GetNativePiPProgramSpec(source.program.program);
			auto d = [MTL4RenderPipelineDescriptor new];
			d.rasterSampleCount=samples;
			d.alphaToCoverageState=source.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
			auto vertex = [MTL4LibraryFunctionDescriptor new]; vertex.library = library;
			vertex.name = source.program.program == NativePiPProgram::Line ? @"pipLineCoverageVertex" : String("pip" + std::string(spec.name) + "Vertex");
			auto fragment = [MTL4LibraryFunctionDescriptor new]; fragment.library = library; fragment.name = String("pip" + std::string(spec.name) + "Fragment");
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
		lease.Retain(library); lease.Retain(result);
		return result;
	}
};

PiPGPU::PiPGPU(): impl(std::make_unique<Impl>()) {}
PiPGPU::~PiPGPU() = default;
void PiPGPU::BeginFrame()
{
	impl->frameMaps.clear();
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});
}
void PiPGPU::ClearResources()
{
	BeginFrame(); impl->buffers.clear(); impl->map.ClearResources();
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
	out.pipeline = impl->Pipeline(lease,source,colorFormat,depthFormat,stencilFormat,validation,shaderMs,samples);
	// A color-only original FBO has no depth buffer: enabled depth tests pass
	// without writes. Never invent a depth attachment from the state alone.
	const auto& state = source.raster;
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
	auto descriptor = [MTL4ArgumentTableDescriptor new]; descriptor.maxBufferBindCount = 4;
	descriptor.maxTextureBindCount = 3; descriptor.maxSamplerStateBindCount = 3; descriptor.initializeBindings = YES;
	NSError* error = nil; out.arguments = [lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
	Require(out.arguments != nil, "Cannot create original PiP argument table: " + Error(error)); lease.Retain(out.arguments);
	if (geometry == NativePiPGeometry::Immediate || geometry == NativePiPGeometry::Line) {
		NativePiPLineGeometry expanded;
		if (geometry == NativePiPGeometry::Line) {
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
	auto uniform = Upload(lease,&source.program.uniforms,sizeof(source.program.uniforms));
	[out.arguments setAddress:uniform.gpuAddress atIndex:3];
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
			auto& dependency = impl->frameMaps[sampled.engineSource];
			if (!dependency) {
				dependency = std::make_shared<ImageDependencies>();
				dependency->images = impl->map.Prepare(lease,sampled.engineSource,validation,shaderMs,true);
			}
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
