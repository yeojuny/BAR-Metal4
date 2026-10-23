/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4LegacyWaterBump.h"
#include "NativeLegacyWaterBumpShaders.h"
#include "Rendering/Env/LegacyWaterBumpLayers.h"
#include <map>

namespace rendering::metal4 {
namespace {
struct RawResources {
	std::shared_ptr<const NativeLegacyWaterBumpMaterial> material;
	std::array<id<MTLTexture>, 3> textures{};
};
struct alignas(16) LayerUniform { std::array<float, 4> uv, fogColor, fog; };
static_assert(sizeof(LayerUniform) == 48);
}
struct PreparedLegacyWaterBump::Impl {
	std::shared_ptr<const NativeLegacyWaterBumpFrame> frame;
	std::shared_ptr<RawResources> resources;
	__strong id<MTLTexture> output = nil;
	__strong id<MTLRenderPipelineState> pipeline = nil;
	__strong id<MTLDepthStencilState> depth = nil;
	std::array<id<MTL4ArgumentTable>, 5> arguments{};
	bool encoded = false;
};
struct LegacyWaterBumpGPU::Impl {
	std::shared_ptr<RawResources> resources;
	std::map<bool, id<MTLRenderPipelineState>> pipelines;
	__strong id<MTLSamplerState> sampler = nil;
	__strong id<MTLDepthStencilState> depth = nil;
};
PreparedLegacyWaterBump::PreparedLegacyWaterBump(std::unique_ptr<Impl> value): impl(std::move(value)) {}
PreparedLegacyWaterBump::~PreparedLegacyWaterBump() = default;
id<MTLTexture> PreparedLegacyWaterBump::Texture() const { return impl->output; }
LegacyWaterBumpGPU::LegacyWaterBumpGPU(): impl(std::make_unique<Impl>()) {}
LegacyWaterBumpGPU::~LegacyWaterBumpGPU() = default;
void LegacyWaterBumpGPU::ClearResources() { impl->resources.reset(); }

void PreparedLegacyWaterBump::Encode(id<MTL4CommandBuffer> command)
{
	Require(command != nil && !impl->encoded, "Legacy water bump command absent or encoded twice");
	auto pass = [MTL4RenderPassDescriptor new];
	pass.colorAttachments[0].texture = impl->output;
	pass.colorAttachments[0].loadAction = MTLLoadActionClear;
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	pass.renderTargetWidth = pass.renderTargetHeight = 128;
	auto e = [command renderCommandEncoderWithDescriptor:pass];
	Require(e != nil, "Cannot encode legacy water bump dependency");
	[e setViewport:MTLViewport{0, 0, 128, 128, 0, 1}];
	[e setScissorRect:MTLScissorRect{0, 0, 128, 128}];
	const auto cull = impl->frame->cull;
	[e setCullMode:cull == LegacyWaterBumpCull::Front ? MTLCullModeFront : cull == LegacyWaterBumpCull::Back ? MTLCullModeBack : MTLCullModeNone];
	[e setFrontFacingWinding:impl->frame->frontFaceClockwise ? MTLWindingClockwise : MTLWindingCounterClockwise];
	[e setTriangleFillMode:impl->frame->wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
	[e setDepthStencilState:impl->depth];
	[e setDepthBias:0 slopeScale:0 clamp:0];
	[e setRenderPipelineState:impl->pipeline];
	for (auto arguments: impl->arguments) {
		if (cull == LegacyWaterBumpCull::FrontAndBack) break;
		[e setArgumentTable:arguments atStages:MTLRenderStageVertex | MTLRenderStageFragment];
		[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	}
	[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[e endEncoding];
	impl->encoded = true;
}

std::unique_ptr<PreparedLegacyWaterBump> LegacyWaterBumpGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeLegacyWaterBumpFrame> frame, bool validation, double& compileMs)
{
	Require(frame != nullptr, "Legacy water bump frame missing");
	ValidateNativeLegacyWaterBump(*frame);
	auto prepared = std::make_unique<PreparedLegacyWaterBump::Impl>();
	prepared->frame = frame;
	auto it = impl->pipelines.find(validation);
	if (it == impl->pipelines.end()) {
		const auto start = Clock::now();
		auto options = [MTLCompileOptions new];
		options.mathMode = MTLMathModeSafe;
		options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
		NSError* error = nil;
		auto library = lease.NewLibraryWithSource(String(NativeLegacyWaterBumpShaders), options, &error);
		Require(library != nil, "Cannot compile legacy water bump shaders: " + Error(error));
		auto d = [MTL4RenderPipelineDescriptor new];
		d.rasterSampleCount = 1;
		auto v = [MTL4LibraryFunctionDescriptor new], f = [MTL4LibraryFunctionDescriptor new];
		v.library = f.library = library;
		v.name = @"LegacyBumpVertex"; f.name = @"LegacyBumpFragment";
		d.vertexFunctionDescriptor = v; d.fragmentFunctionDescriptor = f;
		d.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
		auto a = d.colorAttachments[0];
		a.pixelFormat = MTLPixelFormatRGBA8Unorm;
		a.blendingState = MTL4BlendStateEnabled;
		a.rgbBlendOperation = a.alphaBlendOperation = MTLBlendOperationAdd;
		a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = MTLBlendFactorOne;
		a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = MTLBlendFactorOne;
		auto po = [MTL4PipelineOptions new]; po.shaderValidation = ShaderValidationMode(validation); d.options = po;
		auto pipeline = lease.NewRenderPipeline(d, &error);
		Require(pipeline != nil, "Cannot compile legacy water bump pipeline: " + Error(error));
		it = impl->pipelines.emplace(validation, pipeline).first;
		compileMs += Milliseconds(start);
	}
	prepared->pipeline = it->second; lease.Retain(prepared->pipeline);
	if (!impl->sampler) {
		auto d = [MTLSamplerDescriptor new]; d.supportArgumentBuffers = YES;
		d.minFilter = d.magFilter = MTLSamplerMinMagFilterLinear;
		d.mipFilter = MTLSamplerMipFilterNotMipmapped;
		d.sAddressMode = d.tAddressMode = MTLSamplerAddressModeRepeat;
		impl->sampler = [lease.Device() newSamplerStateWithDescriptor:d];
		Require(impl->sampler != nil, "Cannot allocate legacy water bump sampler");
	}
	lease.Retain(impl->sampler);
	if (!impl->depth) {
		auto d = [MTLDepthStencilDescriptor new]; d.depthCompareFunction = MTLCompareFunctionAlways; d.depthWriteEnabled = NO;
		impl->depth = [lease.Device() newDepthStencilStateWithDescriptor:d];
		Require(impl->depth != nil, "Cannot allocate legacy water bump depth state");
	}
	prepared->depth = impl->depth; lease.Retain(impl->depth);
	if (!impl->resources || impl->resources->material->generation != frame->material->generation) {
		auto r = std::make_shared<RawResources>(); r->material = frame->material;
		for (unsigned i = 0; i < r->textures.size(); ++i) {
			auto d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:64 height:64 mipmapped:NO];
			d.storageMode = MTLStorageModeShared; d.usage = MTLTextureUsageShaderRead;
			r->textures[i] = [lease.Device() newTextureWithDescriptor:d];
			Require(r->textures[i] != nil, "Cannot allocate source legacy water raw bump image");
			[r->textures[i] replaceRegion:MTLRegionMake2D(0, 0, 64, 64) mipmapLevel:0 withBytes:r->material->rgba[i].data() bytesPerRow:64 * 4];
		}
		impl->resources = std::move(r);
	}
	Require(impl->resources->material == frame->material, "Legacy water bump source mutated without a new generation");
	prepared->resources = impl->resources;
	for (auto image: impl->resources->textures) lease.AddAllocation(image);
	// The target is unique to this immutable frame; a later Prepare cannot
	// overwrite an earlier in-flight frame's bump surface.
	auto d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:128 height:128 mipmapped:NO];
	d.storageMode = MTLStorageModePrivate; d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	prepared->output = [lease.Device() newTextureWithDescriptor:d];
	Require(prepared->output != nil, "Cannot allocate legacy water bump target"); lease.AddAllocation(prepared->output);
	const auto layers = MakeLegacyWaterBumpLayers(frame->simulationFrame);
	for (unsigned i = 0; i < layers.size(); ++i) {
		const auto& layer = layers[i];
		const LayerUniform u{{layer.uScale, layer.vScale, layer.vOffset, 0}, frame->fogColor,
			{frame->fogParameters[1], frame->fogParameters[3], float(frame->fogEnabled), 0}};
		auto buffer = [lease.Device() newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];
		Require(buffer != nil, "Cannot allocate legacy water bump layer uniform"); lease.AddAllocation(buffer);
		auto ad = [MTL4ArgumentTableDescriptor new];
		ad.maxBufferBindCount = ad.maxTextureBindCount = ad.maxSamplerStateBindCount = 1; ad.initializeBindings = YES;
		auto args = [lease.Device() newArgumentTableWithDescriptor:ad error:nil];
		Require(args != nil, "Cannot allocate legacy water bump arguments"); lease.Retain(args);
		[args setAddress:buffer.gpuAddress atIndex:0];
		[args setTexture:impl->resources->textures[layer.rawTextureIndex].gpuResourceID atIndex:0];
		[args setSamplerState:impl->sampler.gpuResourceID atIndex:0];
		prepared->arguments[i] = args;
	}
	return std::unique_ptr<PreparedLegacyWaterBump>(new PreparedLegacyWaterBump(std::move(prepared)));
}
}
