/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4VolumetricClouds.h"
#include "Metal4VolumetricCloudShader.h"
#include "Rendering/Backend/NativePostprocessData.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <simd/simd.h>

namespace rendering::metal4 {
namespace {
struct GPUUniforms {
	simd_float4x4 inverseViewProjection;
	simd_float4 eye, offset, sunDirection, sunColor;
	simd_float4 scaleHeightBottomFade, fogColorOpacity, mapSunTime;
	simd_uint4 mode;
};
static_assert(sizeof(GPUUniforms) == 192);
simd_float4 Vector(const std::array<float,3>& values) { return {values[0],values[1],values[2],0}; }
id<MTLBuffer> UniformBuffer(RuntimeLease& lease, const NativeVolumetricCloudDraw& draw)
{
	const auto& p = draw.program; const auto& input = draw.uniforms;
	GPUUniforms u{};
	std::memcpy(&u.inverseViewProjection, input.viewProjectionInverse.data(), 64);
	u.eye = Vector(input.eye); u.offset = Vector(input.offset);
	u.sunDirection = Vector(input.sunDirection); u.sunColor = Vector(input.sunColor);
	u.scaleHeightBottomFade = {p.noiseScale,p.height,p.bottom,p.fadeAltitude};
	u.fogColorOpacity = {p.color[0],p.color[1],p.color[2],p.opacity};
	u.mapSunTime = {p.mapX,p.mapZ,p.sunPenetration,input.time};
	u.mode.x = p.clampToMap;
	auto buffer = [lease.Device() newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];
	Require(buffer != nil, "Cannot allocate actual volumetric-cloud uniforms");
	lease.AddAllocation(buffer); return buffer;
}
}
struct PreparedVolumetricClouds::Impl {
	struct Draw {
		std::uint64_t sequence = 0;
		std::shared_ptr<const NativePostprocessVersion> output;
		__strong MTL4RenderPassDescriptor* pass = nil;
		__strong id<MTL4ArgumentTable> arguments = nil;
		__strong id<MTLTexture> volume = nil;
	};
	std::shared_ptr<const NativeVolumetricCloudFrame> owner;
	std::vector<Draw> draws;
	std::vector<id<MTLTexture>> generateMips;
	std::vector<std::shared_ptr<bool>> mipCompletions;
	__strong id<MTLRenderPipelineState> pipeline = nil;
	__strong id<MTLDepthStencilState> depthState = nil;
};
PreparedVolumetricClouds::PreparedVolumetricClouds(): impl(std::make_unique<Impl>()) {}
PreparedVolumetricClouds::~PreparedVolumetricClouds() = default;
void PreparedVolumetricClouds::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	for (auto texture: impl->generateMips) {
		auto encoder = [command computeCommandEncoder];
		Require(encoder != nil, "Cannot create original Worley mip-generation encoder");
		[encoder generateMipmapsForTexture:texture];
		[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder endEncoding];
	}
}
std::size_t PreparedVolumetricClouds::DrawCount() const { return impl->draws.size(); }
std::uint64_t PreparedVolumetricClouds::SequenceAt(std::size_t index) const
{
	Require(index < impl->draws.size(), "Cloud draw index is outside its retained frame"); return impl->draws[index].sequence;
}
std::shared_ptr<const NativePostprocessVersion> PreparedVolumetricClouds::OutputAt(std::size_t index) const
{
	Require(index < impl->draws.size(), "Cloud output index is outside its retained frame"); return impl->draws[index].output;
}
id<MTLTexture> PreparedVolumetricClouds::OriginalVolumeAt(std::size_t index) const
{
	Require(index < impl->draws.size(), "Cloud volume index is outside its retained frame"); return impl->draws[index].volume;
}
void PreparedVolumetricClouds::EncodeDraw(std::size_t index, id<MTL4CommandBuffer> command) const
{
	Require(index < impl->draws.size(), "Cloud draw index is outside its retained frame");
	const auto& draw = impl->draws[index];
	auto encoder = [command renderCommandEncoderWithDescriptor:draw.pass];
	Require(encoder != nil, "Cannot create original cloud render-to-texture encoder");
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder setRenderPipelineState:impl->pipeline];
	[encoder setViewport:MTLViewport{0,0,double(draw.pass.renderTargetWidth),double(draw.pass.renderTargetHeight),0,1}];
	[encoder setScissorRect:MTLScissorRect{0,0,draw.pass.renderTargetWidth,draw.pass.renderTargetHeight}];
	[encoder setCullMode:MTLCullModeNone]; [encoder setTriangleFillMode:MTLTriangleFillModeFill];
	[encoder setDepthClipMode:MTLDepthClipModeClip]; [encoder setDepthBias:0 slopeScale:0 clamp:0];
	[encoder setStencilReferenceValue:0]; [encoder setDepthStencilState:impl->depthState];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder endEncoding];
}
void PreparedVolumetricClouds::DidComplete() const
{
	for (const auto& complete: impl->mipCompletions) *complete = true;
}
struct VolumetricCloudsGPU::Impl {
	struct Volume {
		std::weak_ptr<const NativeVolumetricCloudVolume> owner;
		__strong id<MTLTexture> texture = nil;
		std::shared_ptr<bool> complete = std::make_shared<bool>(false);
	};
	std::map<const NativeVolumetricCloudVolume*, Volume> volumes;
	__strong id<MTLRenderPipelineState> pipeline = nil;
	__strong id<MTLSamplerState> depthSampler = nil, volumeSampler = nil;
	__strong id<MTLDepthStencilState> depthState = nil;
	bool validation = false;
	id<MTLTexture> VolumeFor(RuntimeLease& lease, const std::shared_ptr<const NativeVolumetricCloudVolume>& owner,
		PreparedVolumetricClouds::Impl& prepared)
	{
		Require(owner != nullptr, "Missing actual original cloud Worley resource");
		auto found = volumes.find(owner.get());
		if (found != volumes.end() && found->second.owner.lock() != owner) { volumes.erase(found); found = volumes.end(); }
		if (found == volumes.end()) {
			Require(owner->width == 128 && owner->height == 128 && owner->depth == 128 &&
				owner->bytesPerRow == 512 && owner->bytesPerImage == 65536 && owner->bgraBytes.size() == 8388608,
				"Actual Worley volume extent differs from validated original bytes");
			auto descriptor = [MTLTextureDescriptor new];
			descriptor.textureType = MTLTextureType3D; descriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			descriptor.width = descriptor.height = descriptor.depth = 128; descriptor.mipmapLevelCount = 8;
			descriptor.storageMode = MTLStorageModeShared; descriptor.usage = MTLTextureUsageShaderRead;
			auto texture = [lease.Device() newTextureWithDescriptor:descriptor];
			Require(texture != nil, "Cannot allocate actual original 128 cubed Worley volume");
			[texture replaceRegion:MTLRegionMake3D(0,0,0,128,128,128) mipmapLevel:0 slice:0
				withBytes:owner->bgraBytes.data() bytesPerRow:owner->bytesPerRow bytesPerImage:owner->bytesPerImage];
			texture.label = String(owner->sourcePath);
			Volume candidate; candidate.owner = owner; candidate.texture = texture;
			found = volumes.emplace(owner.get(),std::move(candidate)).first;
		}
		auto& volume = found->second;
		lease.AddAllocation(volume.texture);
		if (!*volume.complete && std::find(prepared.generateMips.begin(),prepared.generateMips.end(),volume.texture) == prepared.generateMips.end()) {
			prepared.generateMips.push_back(volume.texture); prepared.mipCompletions.push_back(volume.complete);
		}
		return volume.texture;
	}
};
VolumetricCloudsGPU::VolumetricCloudsGPU(): impl(std::make_unique<Impl>()) {}
VolumetricCloudsGPU::~VolumetricCloudsGPU() { SetNativeVolumetricCloudConsumerAvailable(false); }
void VolumetricCloudsGPU::Initialize(RuntimeLease& lease, bool validation, double& compileMs)
{
	const auto start = Clock::now(); NSError* error = nil;
	auto candidate = std::make_unique<Impl>(); candidate->validation = validation;
	auto descriptor = [MTL4LibraryDescriptor new]; descriptor.source = String(NATIVE_VOLUMETRIC_CLOUD_SHADER);
	auto library = lease.NewLibrary(descriptor,&error);
	Require(library != nil, "Cannot compile original cloud shader: " + Error(error));
	auto pipelineDescriptor = [MTL4RenderPipelineDescriptor new];
	auto vertex = [MTL4LibraryFunctionDescriptor new]; vertex.library = library; vertex.name = @"CloudVertex";
	auto fragment = [MTL4LibraryFunctionDescriptor new]; fragment.library = library; fragment.name = @"CloudFragment";
	pipelineDescriptor.vertexFunctionDescriptor = vertex; pipelineDescriptor.fragmentFunctionDescriptor = fragment;
	pipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
	pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	pipelineDescriptor.colorAttachments[0].blendingState = MTL4BlendStateDisabled;
	auto options = [MTL4PipelineOptions new]; options.shaderValidation = ShaderValidationMode(validation);
	pipelineDescriptor.options = options;
	candidate->pipeline = lease.NewRenderPipeline(pipelineDescriptor,&error);
	Require(candidate->pipeline != nil, "Cannot compile original cloud FBO pipeline: " + Error(error));
	auto sampler = [MTLSamplerDescriptor new]; sampler.supportArgumentBuffers = YES;
	sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterNearest; sampler.mipFilter = MTLSamplerMipFilterNotMipmapped;
	sampler.sAddressMode = sampler.tAddressMode = MTLSamplerAddressModeRepeat;
	candidate->depthSampler = [lease.Device() newSamplerStateWithDescriptor:sampler];
	sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterLinear; sampler.mipFilter = MTLSamplerMipFilterLinear;
	sampler.rAddressMode = MTLSamplerAddressModeRepeat;
	candidate->volumeSampler = [lease.Device() newSamplerStateWithDescriptor:sampler];
	Require(candidate->depthSampler && candidate->volumeSampler, "Cannot create original cloud sampling states");
	auto depth = [MTLDepthStencilDescriptor new]; depth.depthCompareFunction = MTLCompareFunctionAlways; depth.depthWriteEnabled = NO;
	candidate->depthState = [lease.Device() newDepthStencilStateWithDescriptor:depth];
	Require(candidate->depthState != nil, "Cannot create cloud FBO depth-off state");
	// No half-ready publication on a failed compiler/descriptor retry.
	candidate->volumes = std::move(impl->volumes); impl = std::move(candidate);
	compileMs += Milliseconds(start);
	// The owning world renderer publishes family availability only after the
	// shared Depth24 copy, this real raymarch PSO and generic image compositor
	// are all initialized. This private PSO alone is not the complete consumer.
}
void VolumetricCloudsGPU::ClearResources() { impl->volumes.clear(); }
std::unique_ptr<PreparedVolumetricClouds> VolumetricCloudsGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeVolumetricCloudFrame> frame, std::span<const VolumetricCloudImages> images,
	bool validation, double& compileMs)
{
	Require(frame && frame->draws.size() == images.size(), "Cloud packet/image ownership extent differs");
	if (!impl->pipeline || impl->validation != validation) Initialize(lease,validation,compileMs);
	for (auto i = impl->volumes.begin(); i != impl->volumes.end();) {
		if (i->second.owner.expired()) i = impl->volumes.erase(i); else ++i;
	}
	auto result = std::unique_ptr<PreparedVolumetricClouds>(new PreparedVolumetricClouds);
	result->impl->owner = frame; result->impl->pipeline = impl->pipeline; result->impl->depthState = impl->depthState;
	lease.Retain(impl->pipeline); lease.Retain(impl->depthState); lease.Retain(impl->depthSampler); lease.Retain(impl->volumeSampler);
	std::uint64_t previousSequence = 0;
	for (std::size_t index = 0; index < frame->draws.size(); ++index) {
		const auto& draw = frame->draws[index]; const auto& native = images[index];
		ValidateNativeVolumetricCloudProgram(draw.program); ValidateNativeVolumetricCloudUniforms(draw.uniforms);
		Require(draw.sequence > previousSequence, "Cloud producers are not in actual original PreUnit order"); previousSequence = draw.sequence;
		Require(draw.depth && draw.output && draw.depth == native.depthVersion && draw.output == native.outputVersion &&
			draw.depth->image && draw.output->image, "Cloud producer immutable version identities differ");
		const auto& input = *draw.depth->image; const auto& output = *draw.output->image;
		Require(input.format == NativePostprocessFormat::Depth24Unorm && output.format == NativePostprocessFormat::RGBA8 && output.renderToTexture,
			"Cloud requires actual Depth24 copy and original RGBA8 FBO");
		Require(input.sampler.minFilter == NativePostprocessFilter::Nearest && input.sampler.magFilter == NativePostprocessFilter::Nearest &&
			input.sampler.wrapS == NativePostprocessWrap::Repeat && input.sampler.wrapT == NativePostprocessWrap::Repeat,
			"Cloud source depth sampler differs from the original descriptor");
		Require(native.depth && native.output && native.depth != native.output &&
			native.depth.pixelFormat == MTLPixelFormatDepth32Float && native.output.pixelFormat == MTLPixelFormatRGBA8Unorm &&
			native.depth.sampleCount == 1 && native.output.sampleCount == 1 &&
			native.depth.width == input.width && native.depth.height == input.height &&
			native.output.width == output.width && native.output.height == output.height,
			"Cloud actual shared images differ from their immutable descriptors");
		Require((native.depth.usage & MTLTextureUsageShaderRead) && (native.output.usage & MTLTextureUsageRenderTarget),
			"Cloud actual images lack original shader/attachment usage");
		lease.AddAllocation(native.depth); lease.AddAllocation(native.output);
		auto volume = impl->VolumeFor(lease,draw.volume,*result->impl);
		auto uniforms = UniformBuffer(lease,draw);
		auto tableDescriptor = [MTL4ArgumentTableDescriptor new];
		tableDescriptor.maxBufferBindCount = 1; tableDescriptor.maxTextureBindCount = 2; tableDescriptor.maxSamplerStateBindCount = 2;
		tableDescriptor.initializeBindings = YES;
		auto arguments = [lease.Device() newArgumentTableWithDescriptor:tableDescriptor error:nil];
		Require(arguments != nil, "Cannot allocate original cloud argument bindings"); lease.Retain(arguments);
		[arguments setAddress:uniforms.gpuAddress atIndex:0]; [arguments setTexture:native.depth.gpuResourceID atIndex:0];
		[arguments setTexture:volume.gpuResourceID atIndex:1]; [arguments setSamplerState:impl->depthSampler.gpuResourceID atIndex:0];
		[arguments setSamplerState:impl->volumeSampler.gpuResourceID atIndex:1];
		PreparedVolumetricClouds::Impl::Draw prepared;
		prepared.sequence = draw.sequence; prepared.output = draw.output; prepared.arguments = arguments; prepared.volume = volume;
		prepared.pass = [MTL4RenderPassDescriptor new];
		prepared.pass.colorAttachments[0].texture = native.output;
		prepared.pass.colorAttachments[0].loadAction = MTLLoadActionDontCare; // Full original shader quad writes every texel.
		prepared.pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		prepared.pass.renderTargetWidth = output.width; prepared.pass.renderTargetHeight = output.height;
		result->impl->draws.push_back(std::move(prepared));
	}
	return result;
}
} // namespace rendering::metal4
