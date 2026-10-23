/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4DeferredLights.h"
#include "Metal4DeferredColorNative.h"
#include "Metal4NativeEffectState.h"
#include "NativeDeferredLightShaders.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include "Rendering/Backend/NativeDeferredLightPrograms.h"
#include "Rendering/Backend/NativeDeferredLightShaderABI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
using deferredLightProgram::DrawUniforms;
struct LinkedProgram {
	NativeDeferredLightProgramSource source;
	__strong id<MTLLibrary> library = nil;
	mutable __strong id<MTLLibrary> attachedLibrary = nil;
	mutable std::map<std::vector<uint64_t>, id<MTLRenderPipelineState>> pipelines;
};
float Define(const NativeDeferredLightProgramSource& source, const char* name)
{
	const auto& value = source.defines.at(name);
	return std::holds_alternative<float>(value) ? std::get<float>(value) : std::stof(std::get<std::string>(value));
}
bool VolumeActive(const NativeDeferredLightProgramSource& source)
{
	return Define(source, "USE3DNOISE") == 1 && Define(source, "RAYMARCHSTEPS") >= 4;
}
bool UniformActive(const NativeDeferredLightProgramSource& source, std::string_view name)
{
	if (name == "mapExtra" || name == "modelExtra") return false;
	if (name == "screenSpaceShadows") return source.defines.contains("SCREENSPACESHADOWS");
	if (name == "noise3DCube" || name == "windX" || name == "windZ") return VolumeActive(source);
	return true;
}
std::string SourceText(const NativeDeferredLightProgramSource& source, bool attached = false)
{
	std::ostringstream text;
	text << std::setprecision(std::numeric_limits<float>::max_digits10);
	for (const auto& [name, value]: source.defines) {
		text << "#define " << name << ' ';
		std::visit([&](const auto& scalar) { text << scalar; }, value);
		text << '\n';
	}
	// The actual common deferred producers expose D32F depth textures.
	text << "#define NATIVE_MAP_DEPTH_COLOR 0\n#define NATIVE_MODEL_DEPTH_COLOR 0\n";
	text << "#define NATIVE_LIGHT_ATTACHED " << unsigned(attached) << '\n';
	text << "#define NATIVE_LIGHT_VOLUME " << unsigned(VolumeActive(source)) << '\n';
	text << deferredLights::SHADER;
	return text.str();
}
id<MTLBuffer> Allocate(RuntimeLease& lease, size_t bytes)
{
	Require(bytes && bytes <= lease.Device().maxBufferLength, "Original deferred-light buffer exceeds actual device capacity");
	auto buffer = [lease.Device() newBufferWithLength:bytes options:MTLResourceStorageModeShared];
	Require(buffer != nil, "Cannot allocate original deferred-light buffer");
	lease.AddAllocation(buffer);
	return buffer;
}
id<MTLBuffer> Upload(RuntimeLease& lease, const void* bytes, size_t count)
{
	auto buffer = Allocate(lease, count);
	std::memcpy(buffer.contents, bytes, count);
	return buffer;
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
	throw std::runtime_error("Unknown original deferred-light blend equation");
}
float Scalar(const std::map<std::string, std::vector<float>>& values, const char* name, float fallback)
{
	const auto found = values.find(name);
	if (found == values.end()) return fallback; // Original GLSL link-time default.
	Require(found->second.size() == 1, "Original deferred-light uniform scalar arity differs");
	return found->second.front();
}
void ValidateImage(const NativeCUSTextureBinding& binding, const DeferredLightImage& resolved, unsigned slot)
{
	const auto& image = resolved.image;
	Require(binding.object && image.texture && image.sampler && image.owner && image.requireReadable,
		"Original deferred light lacks its retained image, sampler or producer");
	Require(image.texture.textureType == (slot == 8 ? MTLTextureType3D : MTLTextureType2D),
		"Original deferred-light sampler target differs");
	if (slot == 0 || slot == 1) {
		const auto backing = std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);
		const auto role = slot == 0 ? ShieldDepthRole::TerrainOnly : ShieldDepthRole::ModelsOnly;
		Require(backing && backing->Kind() == (slot == 0 ? NativeTextureBackingKind::MapDepth : NativeTextureBackingKind::ModelDepth),
			"Original deferred light sampled another depth role");
		const auto& actual = GetShieldDepthBackingImage(*backing);
		Require(actual.texture == image.texture && actual.producer && actual.role == role &&
			actual.producer->role == role && image.texture.pixelFormat == MTLPixelFormatDepth32Float &&
			resolved.epoch.actualProducer == actual.producer &&
			resolved.epoch.allocatedUndefined == (actual.producer->operation == ShieldDepthOperation::AllocationOnly),
			"Original deferred light lost its independent actual depth producer");
	} else if (slot == 2 || slot == 3 || slot == 6 || slot == 7) {
		const auto backing = std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing);
		const auto role = (slot == 2 || slot == 6) ? NativeGeometryTextureRole::Map : NativeGeometryTextureRole::Models;
		const unsigned attachment = slot < 4 ? 0 : 1;
		Require(backing && backing->Metadata().role == role && backing->Metadata().attachment == attachment,
			"Original deferred light sampled another GBuffer attachment");
		const auto& actual = GetDeferredColorBackingImage(*backing);
		Require(actual.texture == image.texture && actual.producer && resolved.epoch.actualProducer == actual.producer &&
			resolved.epoch.allocatedUndefined == (actual.producer->operation == ShieldDepthOperation::AllocationOnly),
			"Original deferred light lost its actual GBuffer producer");
	}
}
} // namespace

struct PreparedDeferredLights::Impl {
	struct Draw {
		uint64_t sequence = 0;
		__strong id<MTLRenderPipelineState> pipeline = nil;
		__strong id<MTLDepthStencilState> depth = nil;
		__strong id<MTL4ArgumentTable> arguments = nil;
		__strong id<MTLBuffer> indices = nil;
		NSUInteger count = 0, first = 0, instances = 0, baseInstance = 0;
		NSInteger baseVertex = 0;
		MTLIndexType indexType = MTLIndexTypeUInt32;
		NativeWorldDrawState world;
		effectstate::Raster raster;
		std::vector<CUSSourceImage> images;
		bool encoded = false;
	};
	std::shared_ptr<const NativeDeferredLightFrame> source;
	std::vector<Draw> draws;
	bool dependenciesEncoded = false, completed = false;
};
PreparedDeferredLights::PreparedDeferredLights(): impl(std::make_unique<Impl>()) {}
PreparedDeferredLights::~PreparedDeferredLights() = default;
size_t PreparedDeferredLights::DrawCount() const { return impl->draws.size(); }
uint64_t PreparedDeferredLights::SequenceAt(size_t index) const { return impl->draws.at(index).sequence; }
void PreparedDeferredLights::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(command && !impl->dependenciesEncoded, "Original deferred-light dependencies repeated or command absent");
	std::set<const void*> seen;
	for (const auto& draw: impl->draws) for (const auto& image: draw.images)
		if (seen.insert(image.owner.get()).second && image.encodeDependencies) image.encodeDependencies(command);
	impl->dependenciesEncoded = true;
}
void PreparedDeferredLights::EncodeDraw(size_t index, id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder && impl->dependenciesEncoded && !impl->completed, "Original deferred-light draw lacks its dependency stage");
	auto& draw = impl->draws.at(index);
	Require(!draw.encoded, "Original deferred-light draw encoded twice");
	for (const auto& image: draw.images) image.requireReadable();
	effectstate::RasterState(encoder, draw.world, draw.raster);
	[encoder setDepthClipMode:draw.world.depthClamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
	[encoder setCullMode:!draw.world.cullEnabled ? MTLCullModeNone : draw.world.cull == NativeWorldCull::Front ? MTLCullModeFront : MTLCullModeBack];
	[encoder setTriangleFillMode:draw.world.frontPolygonMode == NativePolygonMode::Line ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
	[encoder setRenderPipelineState:draw.pipeline];
	[encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex | MTLRenderStageFragment];
	if (draw.indices) {
		const auto offset = draw.first * (draw.indexType == MTLIndexTypeUInt16 ? 2 : 4);
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:draw.indexType
			indexBuffer:draw.indices.gpuAddress + offset indexBufferLength:draw.indices.length - offset
			instanceCount:draw.instances baseVertex:draw.baseVertex baseInstance:draw.baseInstance];
	} else {
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:draw.first vertexCount:draw.count
			instanceCount:draw.instances baseInstance:draw.baseInstance];
	}
	draw.encoded = true;
}
void PreparedDeferredLights::DidComplete()
{
	Require(!impl->completed && impl->dependenciesEncoded &&
		std::all_of(impl->draws.begin(), impl->draws.end(), [](const auto& draw) { return draw.encoded; }),
		"Original deferred light cannot retire an unencoded or repeated submission");
	std::set<const void*> seen;
	for (const auto& draw: impl->draws) for (const auto& image: draw.images)
		if (seen.insert(image.owner.get()).second && image.didComplete) image.didComplete();
	impl->completed = true;
}

struct DeferredLightsGPU::Impl {
	struct Buffer { std::weak_ptr<const void> owner; __strong id<MTLBuffer> value = nil; };
	std::unordered_map<const void*, Buffer> buffers, meshes;
	std::unordered_map<const void*, std::weak_ptr<const LinkedProgram>> programs;
	template<class T, class Fill> id<MTLBuffer> Cached(RuntimeLease& lease, std::shared_ptr<const T> owner, size_t bytes, Fill fill)
	{
		Require(bool(owner), "Original deferred-light buffer has no immutable owner");
		auto& item = buffers[owner.get()];
		if (item.owner.expired()) {
			auto buffer = Allocate(lease, bytes);
			fill(static_cast<uint8_t*>(buffer.contents));
			item = {owner, buffer};
		} else {
			Require(item.value.length == bytes, "Original deferred-light immutable buffer extent changed");
			lease.AddAllocation(item.value);
		}
		return item.value;
	}
	id<MTLBuffer> Published(RuntimeLease& lease, const std::shared_ptr<const NativeCUSPublishedBuffer>& source)
	{
		Require(source && source->capacityElements && source->stride &&
			source->capacityElements <= std::numeric_limits<size_t>::max() / source->stride,
			"Original deferred-light published buffer extent differs");
		return Cached(lease, source, source->capacityElements * source->stride, [&](uint8_t* bytes) {
			for (const auto& range: NativeCUSObservedRanges(*source)) for (size_t offset = range.offset; offset < range.offset + range.size;) {
				const size_t page = offset / NativeCUSPublishedPage::Bytes, local = offset % NativeCUSPublishedPage::Bytes;
				const size_t count = std::min(range.offset + range.size - offset, NativeCUSPublishedPage::Bytes - local);
				std::memcpy(bytes + offset, source->pages.at(page)->bytes.data() + local, count);
				offset += count;
			}
		});
	}
	id<MTLBuffer> Mesh(RuntimeLease& lease, const NativeBufferSnapshotRef& source)
	{
		const auto field = std::find_if(source->layout.attributes.begin(), source->layout.attributes.end(), [](const auto& attribute) { return attribute.id == 0; });
		Require(field != source->layout.attributes.end() && field->scalarType == NativeScalarType::Float32 && field->byteSize == 16 && !field->normalized,
			"Original deferred-light mesh position is not its actual float4 upload");
		auto& item = meshes[source.get()];
		if (item.owner.expired()) {
			auto buffer = Allocate(lease, source->layout.elements * 16);
			for (size_t i = 0; i < source->layout.elements; ++i)
				std::memcpy(static_cast<uint8_t*>(buffer.contents) + i * 16, source->bytes.data() + i * source->layout.stride + field->offset, 16);
			item = {source, buffer};
		} else lease.AddAllocation(item.value);
		return item.value;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease, const LinkedProgram& program,
		const DeferredLightTarget& target, const ScreenBlendState& blend, const std::array<bool, 4>& mask, bool attached, bool validation, double& milliseconds, bool coverage)
	{
		std::vector<uint64_t> key{uint64_t(target.color), uint64_t(target.depth), uint64_t(target.stencil), uint64_t(validation), uint64_t(attached),
			uint64_t(blend.enabled), uint64_t(blend.sourceColor), uint64_t(blend.destinationColor), uint64_t(blend.sourceAlpha),
			uint64_t(blend.destinationAlpha), uint64_t(blend.colorEquation), uint64_t(blend.alphaEquation)};
		for (bool channel: mask) key.push_back(channel);
		key.push_back(lease.SampleShadingKey(target.samples)); key.push_back(coverage);
		if (const auto found = program.pipelines.find(key); found != program.pipelines.end()) {
			lease.Retain(attached ? program.attachedLibrary : program.library); return found->second;
		}
		const auto start = Clock::now();
		if (attached && !program.attachedLibrary) {
			auto libraryDescriptor = [MTL4LibraryDescriptor new];
			libraryDescriptor.name = @"Original31657 unit-attached Deferred rendering GL4";
			libraryDescriptor.source = String(SourceText(program.source, true));
			auto compileOptions = [MTLCompileOptions new];
			compileOptions.mathMode = MTLMathModeSafe; compileOptions.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
			libraryDescriptor.options = compileOptions;
			NSError* error = nil;
			program.attachedLibrary = lease.NewLibrary(libraryDescriptor,&error);
			Require(program.attachedLibrary != nil, "Cannot compile original unit-attached deferred-light stages: " + Error(error));
		}
		auto selectedLibrary = attached ? program.attachedLibrary : program.library;
		lease.Retain(selectedLibrary);
		auto descriptor = [MTL4RenderPipelineDescriptor new];
		descriptor.rasterSampleCount = target.samples;
		descriptor.alphaToCoverageState = coverage ? MTL4AlphaToCoverageStateEnabled : MTL4AlphaToCoverageStateDisabled;
		auto vertex = [MTL4LibraryFunctionDescriptor new], fragment = [MTL4LibraryFunctionDescriptor new];
		vertex.library = fragment.library = selectedLibrary;
		vertex.name = @"DeferredLightVertex"; fragment.name = @"DeferredLightFragment";
		descriptor.vertexFunctionDescriptor = vertex; descriptor.fragmentFunctionDescriptor = fragment;
		descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
		auto color = descriptor.colorAttachments[0];
		color.pixelFormat = target.color;
		color.blendingState = blend.enabled ? MTL4BlendStateEnabled : MTL4BlendStateDisabled;
		color.sourceRGBBlendFactor = effectstate::Factor(blend.sourceColor);
		color.destinationRGBBlendFactor = effectstate::Factor(blend.destinationColor);
		color.sourceAlphaBlendFactor = effectstate::Factor(blend.sourceAlpha);
		color.destinationAlphaBlendFactor = effectstate::Factor(blend.destinationAlpha);
		color.rgbBlendOperation = Equation(blend.colorEquation); color.alphaBlendOperation = Equation(blend.alphaEquation);
		color.writeMask = MTLColorWriteMask((mask[0] ? MTLColorWriteMaskRed : 0) | (mask[1] ? MTLColorWriteMaskGreen : 0) |
			(mask[2] ? MTLColorWriteMaskBlue : 0) | (mask[3] ? MTLColorWriteMaskAlpha : 0));
		auto options = [MTL4PipelineOptions new];
		options.shaderValidation = ShaderValidationMode(validation);
		descriptor.options = options;
		NSError* error = nil;
		auto pipeline = lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline != nil, "Cannot compile original deferred-light target pipeline: " + Error(error));
		program.pipelines.emplace(std::move(key), pipeline);
		milliseconds += Milliseconds(start);
		return pipeline;
	}
};
DeferredLightsGPU::DeferredLightsGPU(): impl(std::make_unique<Impl>()) {}
DeferredLightsGPU::~DeferredLightsGPU() = default;
void DeferredLightsGPU::ClearResources() { impl->buffers.clear(); impl->meshes.clear(); impl->programs.clear(); }
std::shared_ptr<const NativeDeferredLightProgramConsumer> DeferredLightsGPU::LinkOriginalStages(
	RuntimeLease& lease, const NativeDeferredLightProgramSource& source, double& milliseconds)
{
	ValidateNativeDeferredLightProgramSource(source);
	const auto start = Clock::now();
	auto program = std::make_shared<LinkedProgram>();
	program->source = source;
	auto descriptor = [MTL4LibraryDescriptor new];
	descriptor.name = @"Original31657 Deferred rendering GL4";
	descriptor.source = String(SourceText(source));
	auto options = [MTLCompileOptions new];
	options.mathMode = MTLMathModeSafe; options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
	descriptor.options = options;
	NSError* error = nil;
	program->library = lease.NewLibrary(descriptor,&error);
	Require(program->library != nil, "Cannot compile original deferred-light stages: " + Error(error));
	for (NSString* name in @[@"DeferredLightVertex", @"DeferredLightFragment"])
		Require([program->library newFunctionWithName:name] != nil, "Original deferred-light compiled entry is missing");
	auto consumer = std::make_shared<NativeDeferredLightProgramConsumer>();
	consumer->pipelineLifetime = program;
	consumer->vertexSHA512 = source.vertexSHA512; consumer->fragmentSHA512 = source.fragmentSHA512;
	for (const auto& uniform: deferredLightProgram::UniformMetadata) {
		if (!UniformActive(source, uniform.name)) continue;
		consumer->uniforms.push_back({std::string(uniform.name), uniform.glType, 1, uniform.integer});
		(uniform.integer ? consumer->defaultInt : consumer->defaultFloat)[std::string(uniform.name)] = {uniform.sourceDefault};
	}
	impl->programs[program.get()] = program;
	milliseconds += Milliseconds(start);
	return consumer;
}

std::unique_ptr<PreparedDeferredLights> DeferredLightsGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeDeferredLightFrame> source, const DeferredLightTarget& target,
	const ImageResolver& resolve, bool validation, double& milliseconds)
{
	Require(source && resolve && target.width && target.height && target.logicalWidth && target.logicalHeight,
		"Original deferred light lacks an actual frame, image resolver or target");
	std::erase_if(impl->buffers, [](const auto& entry) { return entry.second.owner.expired(); });
	std::erase_if(impl->meshes, [](const auto& entry) { return entry.second.owner.expired(); });
	std::erase_if(impl->programs, [](const auto& entry) { return entry.second.expired(); });
	auto result = std::unique_ptr<PreparedDeferredLights>(new PreparedDeferredLights());
	result->impl->source = std::move(source);
	for (const auto& input: result->impl->source->draws) {
		ValidateNativeDeferredLightDraw(input);
		const auto linked = impl->programs.find(input.program->pipelineLifetime.get());
		Require(linked != impl->programs.end(), "Original deferred-light program belongs to another native lifetime");
		auto program = linked->second.lock();
		Require(bool(program), "Original deferred-light compiled owner expired");
		if (!input.count || !input.instanceCount || (input.world.cullEnabled && input.world.cull == NativeWorldCull::FrontAndBack)) continue;
		Require(input.world.frontPolygonMode == input.world.backPolygonMode && input.world.frontPolygonMode != NativePolygonMode::Point,
			"Original deferred-light polygon mode needs its exact consumer");
		auto camera = target.camera;
		if (input.surface.viewport) {
			const auto& viewport = *input.surface.viewport;
			camera.viewport = {viewport.x, viewport.y, int32_t(viewport.width), int32_t(viewport.height)};
		}
		auto raster = effectstate::Viewport(camera, target.width, target.height, target.logicalWidth, target.logicalHeight, input.surface.scissor);
		if (!raster) continue;
		PreparedDeferredLights::Impl::Draw draw;
		draw.sequence = input.sequence; draw.world = input.world; draw.raster = *raster;
		draw.count = input.count; draw.first = input.indexed ? input.firstIndex : input.firstVertex;
		draw.instances = input.instanceCount; draw.baseVertex = input.baseVertex; draw.baseInstance = input.firstInstance;
		auto table = [MTL4ArgumentTableDescriptor new];
		table.maxBufferBindCount = 7; table.maxTextureBindCount = 10; table.maxSamplerStateBindCount = 10; table.initializeBindings = YES;
		NSError* error = nil;
		draw.arguments = [lease.Device() newArgumentTableWithDescriptor:table error:&error];
		Require(draw.arguments != nil, "Cannot allocate original deferred-light arguments: " + Error(error));
		lease.Retain(draw.arguments);
		auto mesh = impl->Mesh(lease, input.vertices);
		auto instances = impl->Cached(lease, input.instances, input.instances->bytes.size(), [&](uint8_t* bytes) {
			std::memcpy(bytes, input.instances->bytes.data(), input.instances->bytes.size());
		});
		const auto& engine = *input.engineUniforms;
		auto matrices = impl->Cached(lease, engine.matrices, engine.matrices->bytes.size(), [&](uint8_t* bytes) {
			std::memcpy(bytes, engine.matrices->bytes.data(), engine.matrices->bytes.size());
		});
		auto params = impl->Cached(lease, engine.params, engine.params->bytes.size(), [&](uint8_t* bytes) {
			std::memcpy(bytes, engine.params->bytes.data(), NativeCUSParamsPrefixBytes);
			for (size_t row = 0; row < NativeCUSTeamRows; ++row) if (engine.params->writtenTeamRows[row])
				std::memcpy(bytes + NativeCUSParamsPrefixBytes + row * 16, engine.params->bytes.data() + NativeCUSParamsPrefixBytes + row * 16, 16);
		});
		[draw.arguments setAddress:mesh.gpuAddress atIndex:0]; [draw.arguments setAddress:instances.gpuAddress atIndex:1];
		[draw.arguments setAddress:matrices.gpuAddress atIndex:2]; [draw.arguments setAddress:params.gpuAddress atIndex:3];
		if (input.transforms) {
			Require(input.transforms->stride == (Define(program->source, "USEQUATERNIONS") == 0 ? 64 : 32),
				"Original deferred-light transform selector differs from its actual published layout");
			auto transforms = impl->Published(lease, input.transforms);
			[draw.arguments setAddress:transforms.gpuAddress atIndex:4];
		}
		if (input.modelUniforms) {
			auto models = impl->Published(lease, input.modelUniforms);
			[draw.arguments setAddress:models.gpuAddress atIndex:5];
		}
		if (input.indexed) {
			draw.indices = impl->Cached(lease, input.indices, input.indices->bytes.size(), [&](uint8_t* bytes) {
				std::memcpy(bytes, input.indices->bytes.data(), input.indices->bytes.size());
			});
			draw.indexType = input.indexType == 0x1403 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
		}
		DrawUniforms uniforms;
		uniforms.pointbeamcone = Scalar(input.floatUniforms, "pointbeamcone", 0);
		uniforms.attachedtounitID = Scalar(input.floatUniforms, "attachedtounitID", 0);
		uniforms.windX = Scalar(input.floatUniforms, "windX", 0); uniforms.windZ = Scalar(input.floatUniforms, "windZ", 0);
		uniforms.radiusMultiplier = Scalar(input.floatUniforms, "radiusMultiplier", 1);
		uniforms.intensityMultiplier = Scalar(input.floatUniforms, "intensityMultiplier", 1);
		uniforms.nightFactor = Scalar(input.floatUniforms, "nightFactor", 1);
		uniforms.screenSpaceShadows = int32_t(Scalar(input.intUniforms, "screenSpaceShadows", 0));
		uint32_t renderCaps = 0;
		std::memcpy(&renderCaps, engine.params->bytes.data() + 12, sizeof(renderCaps));
		uniforms.transport[1] = !(renderCaps & 1u); uniforms.transport[2] = input.firstInstance;
		// Original gl_FragCoord uses raster pixels for quad lanes and the 64-pixel blue-noise period.
		// Logical extents are used only by viewport/scissor mapping, not fragment coordinates.
		uniforms.framebuffer = {1.0f, 1.0f, float(target.height), 0};
		uniforms.alpha = {float(input.surface.alphaTest.enabled), input.surface.alphaTest.reference, float(input.surface.alphaTest.compare), 0};
		std::array<NativeDeferredPostprocessEpoch, 3> modelEpochs;
		for (unsigned slot: {0u, 1u, 2u, 3u, 6u, 7u, 8u, 9u}) {
			if (slot == 8 && !VolumeActive(program->source)) continue;
			const auto name = deferredLightProgram::SamplerNames[slot];
			const auto found = input.intUniforms.find(std::string(name));
			Require(found != input.intUniforms.end() && found->second.size() == 1 && found->second[0] == float(slot),
				"Original deferred-light selected sampler mapping differs");
			const auto& binding = input.textures[slot];
			auto resolved = resolve(lease, binding, slot);
			ValidateImage(binding, resolved, slot);
			auto& image = resolved.image;
			if (slot == 1) modelEpochs[0] = resolved.epoch;
			if (slot == 3) modelEpochs[1] = resolved.epoch;
			if (slot == 7) modelEpochs[2] = resolved.epoch;
			if (image.flipV) { Require(slot != 8, "Original volume source orientation differs"); uniforms.transport[0] |= 1u << slot; }
			uniforms.imageSwizzle[slot] = binding.object->sampling.swizzle;
			uniforms.depthMode[slot / 4][slot % 4] = binding.object->sampling.depthMode;
			uniforms.imageSize[slot] = {uint32_t(image.texture.width), uint32_t(image.texture.height), uint32_t(image.texture.depth), uint32_t(image.texture.mipmapLevelCount)};
			lease.AddAllocation(image.texture); lease.Retain(image.sampler);
			[draw.arguments setTexture:image.texture.gpuResourceID atIndex:slot];
			[draw.arguments setSamplerState:image.sampler.gpuResourceID atIndex:slot];
			draw.images.push_back(std::move(image));
		}
		ValidateNativeDeferredModelEpochs(modelEpochs);
		auto uniformBuffer = Upload(lease, &uniforms, sizeof(uniforms));
		[draw.arguments setAddress:uniformBuffer.gpuAddress atIndex:6];
		draw.pipeline = impl->Pipeline(lease, *program, target, input.surface.blend, input.world.colorWrite, uniforms.attachedtounitID > 0, validation, milliseconds, input.world.alphaToCoverage);
		draw.depth = effectstate::Depth(lease, input.world, target.stencil);
		lease.Retain(draw.pipeline); lease.Retain(program->library);
		result->impl->draws.push_back(std::move(draw));
	}
	return result;
}
} // namespace rendering::metal4
