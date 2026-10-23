/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Outline.h"
#include "Metal4NativeEffectState.h"
#include "NativeOutlineShaders.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include "Rendering/Backend/NativeOutlinePrograms.h"
#include "Rendering/Backend/NativeOutlineShaderABI.h"

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
using outlineProgram::DrawUniforms;
struct LinkedProgram {
	NativeOutlineProgramSource source;
	__strong id<MTLLibrary> library = nil;
	mutable std::map<std::vector<uint64_t>, id<MTLRenderPipelineState>> pipelines;
};
std::string SourceText(const NativeOutlineProgramSource& source)
{
	std::ostringstream text;
	text << std::setprecision(std::numeric_limits<float>::max_digits10);
	for (const auto& [name, value]: source.defines) {
		text << "#define " << name << ' ';
		std::visit([&](const auto& scalar) { text << scalar; }, value);
		text << '\n';
	}
	text << outline::SHADER;
	return text.str();
}
id<MTLBuffer> Allocate(RuntimeLease& lease, size_t bytes)
{
	Require(bytes && bytes <= lease.Device().maxBufferLength, "Original outline buffer exceeds actual device capacity");
	auto buffer = [lease.Device() newBufferWithLength:bytes options:MTLResourceStorageModeShared];
	Require(buffer != nil, "Cannot allocate original outline buffer");
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
	throw std::runtime_error("Unknown original outline blend equation");
}
float Scalar(const std::map<std::string, std::vector<float>>& values, const char* name, float fallback)
{
	const auto found = values.find(name);
	if (found == values.end()) return fallback; // Original GLSL link-time default.
	Require(found->second.size() == 1, "Original outline uniform scalar arity differs");
	return found->second.front();
}
} // namespace

struct PreparedOutline::Impl {
	struct Operation {
		uint64_t sequence = 0;
		__strong id<MTLRenderPipelineState> pipeline = nil;
		__strong id<MTLDepthStencilState> depth = nil;
		__strong id<MTL4ArgumentTable> arguments = nil;
		NSUInteger count = 0, first = 0, instances = 0, baseInstance = 0;
		NativeWorldDrawState world;
		effectstate::Raster raster;
		std::vector<CUSSourceImage> images;
		bool clear = false, noop = false, encoded = false;
	};
	std::shared_ptr<const NativeOutlineFrame> source;
	std::vector<Operation> operations;
	bool dependenciesEncoded = false, completed = false;
};
PreparedOutline::PreparedOutline(): impl(std::make_unique<Impl>()) {}
PreparedOutline::~PreparedOutline() = default;
size_t PreparedOutline::OperationCount() const { return impl->operations.size(); }
uint64_t PreparedOutline::SequenceAt(size_t index) const { return impl->operations.at(index).sequence; }
void PreparedOutline::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(command && !impl->dependenciesEncoded, "Original outline dependencies repeated or command absent");
	std::set<const void*> seen;
	for (const auto& draw: impl->operations) for (const auto& image: draw.images)
		if (seen.insert(image.owner.get()).second && image.encodeDependencies) image.encodeDependencies(command);
	impl->dependenciesEncoded = true;
}
void PreparedOutline::EncodeOperation(size_t index, id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder && impl->dependenciesEncoded && !impl->completed, "Original outline draw lacks its dependency stage");
	auto& draw = impl->operations.at(index);
	Require(!draw.encoded, "Original outline draw encoded twice");
	if (draw.noop) { draw.encoded = true; return; }
	for (const auto& image: draw.images) image.requireReadable();
	effectstate::RasterState(encoder, draw.world, draw.raster);
	[encoder setDepthClipMode:draw.world.depthClamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
	[encoder setCullMode:!draw.world.cullEnabled ? MTLCullModeNone : draw.world.cull == NativeWorldCull::Front ? MTLCullModeFront : MTLCullModeBack];
	[encoder setTriangleFillMode:draw.world.frontPolygonMode == NativePolygonMode::Line ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
	[encoder setRenderPipelineState:draw.pipeline];
	[encoder setDepthStencilState:draw.depth];
	if (draw.arguments) [encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex | MTLRenderStageFragment];
	[encoder drawPrimitives:draw.clear ? MTLPrimitiveTypeTriangle : MTLPrimitiveTypeTriangleStrip
		vertexStart:draw.first vertexCount:draw.count instanceCount:draw.instances baseInstance:draw.baseInstance];
	draw.encoded = true;
}
void PreparedOutline::DidComplete()
{
	Require(!impl->completed && impl->dependenciesEncoded &&
		std::all_of(impl->operations.begin(), impl->operations.end(), [](const auto& draw) { return draw.encoded; }),
		"Original outline cannot retire an unencoded or repeated submission");
	std::set<const void*> seen;
	for (const auto& draw: impl->operations) for (const auto& image: draw.images)
		if (seen.insert(image.owner.get()).second && image.didComplete) image.didComplete();
	impl->completed = true;
}

struct OutlineGPU::Impl {
	struct Buffer { std::weak_ptr<const void> owner; __strong id<MTLBuffer> value = nil; };
	std::unordered_map<const void*, Buffer> buffers, meshes;
	std::unordered_map<const void*, std::weak_ptr<const LinkedProgram>> programs;
	template<class T, class Fill> id<MTLBuffer> Cached(RuntimeLease& lease, std::shared_ptr<const T> owner, size_t bytes, Fill fill)
	{
		Require(bool(owner), "Original outline buffer has no immutable owner");
		auto& item = buffers[owner.get()];
		if (item.owner.expired()) {
			auto buffer = Allocate(lease, bytes);
			fill(static_cast<uint8_t*>(buffer.contents));
			item = {owner, buffer};
		} else {
			Require(item.value.length == bytes, "Original outline immutable buffer extent changed");
			lease.AddAllocation(item.value);
		}
		return item.value;
	}
	id<MTLBuffer> Published(RuntimeLease& lease, const std::shared_ptr<const NativeCUSPublishedBuffer>& source)
	{
		Require(source && source->capacityElements && source->stride &&
			source->capacityElements <= std::numeric_limits<size_t>::max() / source->stride,
			"Original outline published buffer extent differs");
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
		Require(field != source->layout.attributes.end() && field->scalarType == NativeScalarType::Float32 && field->byteSize == 8 && !field->normalized,
			"Original outline mesh position is not its actual float2 upload");
		auto& item = meshes[source.get()];
		if (item.owner.expired()) {
			auto buffer = Allocate(lease, source->layout.elements * 8);
			for (size_t i = 0; i < source->layout.elements; ++i)
				std::memcpy(static_cast<uint8_t*>(buffer.contents) + i * 8, source->bytes.data() + i * source->layout.stride + field->offset, 8);
			item = {source, buffer};
		} else lease.AddAllocation(item.value);
		return item.value;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease, const LinkedProgram& program,
		const OutlineTarget& target, const ScreenBlendState& blend, const std::array<bool, 4>& mask, bool clear, bool validation, double& milliseconds, bool coverage)
	{
		std::vector<uint64_t> key{uint64_t(target.color), uint64_t(target.depth), uint64_t(target.stencil), uint64_t(validation), uint64_t(clear),
			uint64_t(blend.enabled), uint64_t(blend.sourceColor), uint64_t(blend.destinationColor), uint64_t(blend.sourceAlpha),
			uint64_t(blend.destinationAlpha), uint64_t(blend.colorEquation), uint64_t(blend.alphaEquation)};
		for (bool channel: mask) key.push_back(channel);
		key.push_back(lease.SampleShadingKey(target.samples)); key.push_back(coverage);
		if (const auto found = program.pipelines.find(key); found != program.pipelines.end()) return found->second;
		const auto start = Clock::now();
		auto descriptor = [MTL4RenderPipelineDescriptor new];
		descriptor.rasterSampleCount = target.samples;
		descriptor.alphaToCoverageState = coverage ? MTL4AlphaToCoverageStateEnabled : MTL4AlphaToCoverageStateDisabled;
		auto vertex = [MTL4LibraryFunctionDescriptor new], fragment = [MTL4LibraryFunctionDescriptor new];
		vertex.library = fragment.library = program.library;
		vertex.name = clear ? @"OutlineClearVertex" : @"OutlineVertex";
		fragment.name = clear ? @"OutlineClearFragment" : @"OutlineFragment";
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
		Require(pipeline != nil, "Cannot compile original outline target pipeline: " + Error(error));
		program.pipelines.emplace(std::move(key), pipeline);
		milliseconds += Milliseconds(start);
		return pipeline;
	}
};
OutlineGPU::OutlineGPU(): impl(std::make_unique<Impl>()) {}
OutlineGPU::~OutlineGPU() = default;
void OutlineGPU::ClearResources() { impl->buffers.clear(); impl->meshes.clear(); impl->programs.clear(); }
std::shared_ptr<const NativeOutlineProgramConsumer> OutlineGPU::LinkOriginalStages(
	RuntimeLease& lease, const NativeOutlineProgramSource& source, double& milliseconds)
{
	ValidateNativeOutlineProgramSource(source);
	const auto start = Clock::now();
	auto program = std::make_shared<LinkedProgram>();
	program->source = source;
	auto descriptor = [MTL4LibraryDescriptor new];
	descriptor.name = @"Original31657 Outline Shader GL4";
	descriptor.source = String(SourceText(source));
	auto options = [MTLCompileOptions new];
	options.mathMode = MTLMathModeSafe; options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
	descriptor.options = options;
	NSError* error = nil;
	program->library = lease.NewLibrary(descriptor,&error);
	Require(program->library != nil, "Cannot compile original outline stages: " + Error(error));
	for (NSString* name in @[@"OutlineVertex", @"OutlineFragment", @"OutlineClearVertex", @"OutlineClearFragment"])
		Require([program->library newFunctionWithName:name] != nil, "Original outline compiled entry is missing");
	auto consumer = std::make_shared<NativeOutlineProgramConsumer>();
	consumer->pipelineLifetime = program;
	consumer->perUnit = NativeOutlineDefine(source, "PER_UNIT") == 1;
	consumer->useQuaternions = NativeOutlineDefine(source, "USEQUATERNIONS") == 1;
	consumer->vertexSHA512 = source.vertexSHA512; consumer->fragmentSHA512 = source.fragmentSHA512;
	for (const auto& uniform: outlineProgram::UniformMetadata) {
		if (uniform.perUnit >= 0 && bool(uniform.perUnit) != consumer->perUnit) continue;
		consumer->uniforms.push_back({std::string(uniform.name), uniform.glType, 1, uniform.integer});
		(uniform.integer ? consumer->defaultInt : consumer->defaultFloat)[std::string(uniform.name)] = {uniform.sourceDefault};
	}
	impl->programs[program.get()] = program;
	milliseconds += Milliseconds(start);
	return consumer;
}

std::unique_ptr<PreparedOutline> OutlineGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeOutlineFrame> source, const OutlineTarget& target,
	const ImageResolver& resolve, bool validation, double& milliseconds)
{
	Require(source && resolve && target.width && target.height && target.logicalWidth && target.logicalHeight,
		"Original Outline lacks an actual frame, image resolver or target");
	std::erase_if(impl->buffers, [](const auto& entry) { return entry.second.owner.expired(); });
	std::erase_if(impl->meshes, [](const auto& entry) { return entry.second.owner.expired(); });
	std::erase_if(impl->programs, [](const auto& entry) { return entry.second.expired(); });
	auto result = std::unique_ptr<PreparedOutline>(new PreparedOutline());
	result->impl->source = std::move(source);
	std::shared_ptr<const LinkedProgram> previousProgram;
	uint64_t previousSequence = 0;
	for (const auto& operation: result->impl->source->operations) {
		const uint64_t sequence = std::visit([](const auto& input) { return input.sequence; }, operation);
		Require(sequence > previousSequence, "Original Outline world operation ordinals are not strictly ordered");
		previousSequence = sequence;
		PreparedOutline::Impl::Operation draw;
		draw.sequence = sequence;
		if (const auto* clear = std::get_if<NativeOutlineClear>(&operation)) {
			ValidateNativeOutlineClear(*clear);
			Require(previousProgram && NativeOutlineDefine(previousProgram->source, "PER_UNIT") == 0,
				"Original Outline stencil clear lacks its preceding all-units program");
			draw.clear = true; draw.count = 3; draw.instances = 1;
			WorldCamera wholeFramebuffer;
			wholeFramebuffer.viewport = {0, 0, int32_t(target.logicalWidth), int32_t(target.logicalHeight)};
			auto raster = effectstate::Viewport(wholeFramebuffer, target.width, target.height,
				target.logicalWidth, target.logicalHeight, clear->scissor);
			draw.noop = !raster || target.stencil == MTLPixelFormatInvalid || !(clear->frontStencilWriteMask & 255u);
			if (!draw.noop) {
				draw.raster = *raster;
				draw.world.depthTest = false; draw.world.depthWrite = false; draw.world.stencilTest = true;
				draw.world.colorWrite = {false, false, false, false};
				for (auto* face: {&draw.world.frontStencil, &draw.world.backStencil}) {
					face->compare = ScreenAlphaCompare::Always;
					face->reference = clear->values.stencil & 255;
					face->writeMask = clear->frontStencilWriteMask;
					face->readMask = 255; face->pass = NativeWorldStencilOp::Replace;
				}
				ScreenBlendState blend; blend.enabled = false;
				draw.pipeline = impl->Pipeline(lease, *previousProgram, target, blend, draw.world.colorWrite, true, validation, milliseconds, false);
				draw.depth = effectstate::Depth(lease, draw.world, target.stencil);
				lease.Retain(draw.pipeline); lease.Retain(previousProgram->library);
			}
			result->impl->operations.push_back(std::move(draw));
			continue;
		}
		const auto& input = std::get<NativeOutlineDraw>(operation);
		ValidateNativeOutlineDraw(input);
		const auto linked = impl->programs.find(input.program->pipelineLifetime.get());
		Require(linked != impl->programs.end(), "Original Outline program belongs to another native lifetime");
		auto program = linked->second.lock();
		Require(bool(program), "Original Outline compiled owner expired");
		previousProgram = program;
		Require(input.world.frontPolygonMode == input.world.backPolygonMode && input.world.frontPolygonMode != NativePolygonMode::Point,
			"Original Outline polygon mode needs its exact consumer");
		auto camera = target.camera;
		if (input.surface.viewport) {
			const auto& viewport = *input.surface.viewport;
			camera.viewport = {viewport.x, viewport.y, int32_t(viewport.width), int32_t(viewport.height)};
		}
		auto raster = effectstate::Viewport(camera, target.width, target.height, target.logicalWidth, target.logicalHeight, input.surface.scissor);
		draw.noop = !raster || (input.world.cullEnabled && input.world.cull == NativeWorldCull::FrontAndBack);
		if (draw.noop) { result->impl->operations.push_back(std::move(draw)); continue; }
		draw.world = input.world; draw.raster = *raster;
		draw.count = input.count; draw.first = input.firstVertex;
		draw.instances = input.instanceCount; draw.baseInstance = input.firstInstance;
		auto table = [MTL4ArgumentTableDescriptor new];
		table.maxBufferBindCount = 7; table.maxTextureBindCount = 3; table.maxSamplerStateBindCount = 3; table.initializeBindings = YES;
		NSError* error = nil;
		draw.arguments = [lease.Device() newArgumentTableWithDescriptor:table error:&error];
		Require(draw.arguments != nil, "Cannot allocate original Outline arguments: " + Error(error));
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
		auto transforms = impl->Published(lease, input.transforms);
		auto models = impl->Published(lease, input.modelUniforms);
		[draw.arguments setAddress:mesh.gpuAddress atIndex:0]; [draw.arguments setAddress:instances.gpuAddress atIndex:1];
		[draw.arguments setAddress:matrices.gpuAddress atIndex:2]; [draw.arguments setAddress:params.gpuAddress atIndex:3];
		[draw.arguments setAddress:transforms.gpuAddress atIndex:4]; [draw.arguments setAddress:models.gpuAddress atIndex:5];
		DrawUniforms uniforms;
		uniforms.outlineWidth = Scalar(input.floatUniforms, "outlineWidth", 0);
		uniforms.quadPadding = Scalar(input.floatUniforms, "quadPadding", 0);
		uniforms.zoomScale = Scalar(input.floatUniforms, "zoomScale", 0);
		uniforms.resolutionScale = Scalar(input.floatUniforms, "resolutionScale", 0);
		uint32_t renderCaps = 0;
		std::memcpy(&renderCaps, engine.params->bytes.data() + 12, sizeof(renderCaps));
		uniforms.transport[1] = !(renderCaps & 1u);
		// Exact original fragment pixels, including source viewport origin, feed
		// texelFetch and gather. Raster viewport mapping alone uses logical sizes.
		uniforms.framebuffer = {1.f, 1.f, float(target.height), 0};
		uniforms.alpha = {float(input.surface.alphaTest.enabled), input.surface.alphaTest.reference, float(input.surface.alphaTest.compare), 0};
		std::array<CUSOutlineImage, 3> images;
		for (unsigned slot = 0; slot < images.size(); ++slot) {
			const auto found = input.intUniforms.find(std::string(outlineProgram::SamplerNames[slot]));
			Require(found != input.intUniforms.end() && found->second.size() == 1 && found->second.front() == float(slot),
				"Original Outline sampler mapping differs");
			images[slot] = resolve(lease, input.textures[slot], slot);
		}
		ValidateCUSOutlineImages(images);
		for (unsigned slot = 0; slot < images.size(); ++slot) {
			auto& resolved = images[slot]; auto& image = resolved.image;
			Require(image.texture && image.sampler && image.owner && image.requireReadable && image.texture.textureType == MTLTextureType2D,
				"Original Outline image lacks its retained actual sampler or producer");
			if (image.flipV) uniforms.transport[0] |= 1u << slot;
			uniforms.imageSwizzle[slot] = resolved.sampling.swizzle;
			uniforms.depthMode[slot] = resolved.sampling.depthMode;
			uniforms.imageSize[slot] = {uint32_t(image.texture.width), uint32_t(image.texture.height), uint32_t(image.texture.depth), uint32_t(image.texture.mipmapLevelCount)};
			lease.AddAllocation(image.texture); lease.Retain(image.sampler);
			[draw.arguments setTexture:image.texture.gpuResourceID atIndex:slot];
			[draw.arguments setSamplerState:image.sampler.gpuResourceID atIndex:slot];
			draw.images.push_back(std::move(image));
		}
		auto uniformBuffer = Upload(lease, &uniforms, sizeof(uniforms));
		[draw.arguments setAddress:uniformBuffer.gpuAddress atIndex:6];
		draw.pipeline = impl->Pipeline(lease, *program, target, input.surface.blend, input.world.colorWrite, false, validation, milliseconds, input.world.alphaToCoverage);
		draw.depth = effectstate::Depth(lease, input.world, target.stencil);
		lease.Retain(draw.pipeline); lease.Retain(program->library);
		result->impl->operations.push_back(std::move(draw));
	}
	return result;
}
} // namespace rendering::metal4
