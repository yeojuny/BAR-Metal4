/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4AssetRenderer.h"
#include "Metal4RuntimeNative.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace {
using namespace rendering::metal4;

void ValidateRequest(const rendering::AssetDiagnosticRequest& request)
{
	Require(request.width > 0 && request.width <= 4096 && request.height > 0 && request.height <= 4096,
		"Asset diagnostic dimensions must be explicit pixels in 1..4096");
	Require(request.measuredFrames > 0 && request.measuredFrames <= 300, "Asset diagnostic measured-frame count outside 1..300");
	Require(!request.vertices.empty() && request.vertices.size() <= 2'000'000 && !request.indices.empty() &&
		request.indices.size() <= 12'000'000 && request.indices.size() % 3 == 0, "Asset diagnostic mesh exceeds bounds or has incomplete triangles");
	Require(uint32_t(request.mode) <= uint32_t(rendering::AssetDiagnosticMode::Alpha), "Unknown diagnostic material mode");
	Require(!request.materialSemanticsSource.empty() && request.materialSemanticsSource.size() < 64 * 1024, "Material shader contract outside size bounds");
	for (uint32_t index: request.indices) Require(index < request.vertices.size(), "Asset diagnostic index exceeds vertex range");
	for (const auto& vertex: request.vertices) {
		for (float value: vertex.position) Require(std::isfinite(value), "Non-finite diagnostic vertex position");
		for (float value: vertex.normal) Require(std::isfinite(value), "Non-finite diagnostic vertex normal");
		for (float value: vertex.uv) Require(std::isfinite(value), "Non-finite diagnostic UV");
	}
	for (size_t axis = 0; axis < 3; ++axis) {
		Require(std::isfinite(request.minimum[axis]) && std::isfinite(request.maximum[axis]) && request.minimum[axis] <= request.maximum[axis], "Invalid diagnostic model bounds");
		Require(std::isfinite(request.teamColor[axis]) && request.teamColor[axis] >= 0 && request.teamColor[axis] <= 1, "Diagnostic team colour outside [0,1]");
	}
	if (request.camera) {
		for (float value: request.camera->viewProjection) Require(std::isfinite(value), "Non-finite diagnostic camera matrix");
		for (float value: request.camera->position) Require(std::isfinite(value), "Non-finite diagnostic camera position");
	}
}

id<MTLTexture> UploadTexture(id<MTLDevice> device, const rendering::AssetTextureView& data, NSString* label)
{
	const bool nativeBC = data.format == rendering::AssetTextureFormat::BC3RGBA;
	Require(nativeBC || data.format == rendering::AssetTextureFormat::RGBA8Unorm, "Unsupported diagnostic texture format");
	Require(!nativeBC || device.supportsBCTextureCompression, "Supplied BC3 texture is unsupported; caller must prepare RGBA8 texels");
	Require(!data.mips.empty() && data.mips.size() <= 14, "Diagnostic texture mip count outside bounds");
	const auto& base = data.mips.front();
	Require(base.width > 0 && base.width <= 8192 && base.height > 0 && base.height <= 8192, "Diagnostic texture dimensions outside bounds");
	uint32_t maximumLevels = 1;
	for (uint32_t dimension = std::max(base.width, base.height); dimension > 1; dimension >>= 1) ++maximumLevels;
	Require(data.mips.size() <= maximumLevels, "Diagnostic mip count exceeds texture dimensions");
	uint32_t width = base.width, height = base.height;
	for (const auto& mip: data.mips) {
		Require(mip.width == width && mip.height == height, "Diagnostic texture mip dimensions are inconsistent");
		const size_t rows = nativeBC ? (height + 3) / 4 : height;
		const size_t minimumRowBytes = nativeBC ? size_t((width + 3) / 4) * 16 : size_t(width) * 4;
		Require(mip.bytesPerRow >= minimumRowBytes && mip.bytesPerRow <= 128 * 1024 * 1024 &&
			mip.bytes.size() >= (rows - 1) * mip.bytesPerRow + minimumRowBytes, "Diagnostic mip data/stride range is incomplete");
		width = std::max(1U, width / 2); height = std::max(1U, height / 2);
	}
	MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:nativeBC ? MTLPixelFormatBC3_RGBA : MTLPixelFormatRGBA8Unorm width:base.width height:base.height mipmapped:data.mips.size() > 1];
	descriptor.mipmapLevelCount = data.mips.size();
	descriptor.storageMode = MTLStorageModeShared;
	descriptor.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
	Require(texture != nil, "Cannot allocate diagnostic original DDS texture");
	texture.label = label;
	for (size_t level = 0; level < data.mips.size(); ++level) {
		const auto& mip = data.mips[level];
		[texture replaceRegion:MTLRegionMake2D(0, 0, mip.width, mip.height) mipmapLevel:level withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];
	}
	return texture;
}

simd_float4x4 LookAt(simd_float3 eye, simd_float3 target)
{
	const auto backward = simd_normalize(eye - target);
	const auto right = simd_normalize(simd_cross(simd_make_float3(0, 1, 0), backward));
	const auto up = simd_cross(backward, right);
	return simd_matrix(simd_make_float4(right.x, up.x, backward.x, 0),
		simd_make_float4(right.y, up.y, backward.y, 0), simd_make_float4(right.z, up.z, backward.z, 0),
		simd_make_float4(-simd_dot(right, eye), -simd_dot(up, eye), -simd_dot(backward, eye), 1));
}

simd_float4x4 Perspective(float radians, float aspect, float nearPlane, float farPlane)
{
	const float y = 1.0f / std::tan(radians * 0.5f);
	const float z = farPlane / (nearPlane - farPlane);
	return simd_matrix(simd_make_float4(y / aspect, 0, 0, 0), simd_make_float4(0, y, 0, 0),
		simd_make_float4(0, 0, z, -1), simd_make_float4(0, 0, z * nearPlane, 0));
}

struct Uniforms {
	simd_float4x4 viewProjection;
	simd_float4 camera;
	simd_float4 sun;
	simd_float4 team;
	simd_uint4 options;
};
static_assert(sizeof(Uniforms) == 128);

const char* PROBE_SHADER = R"MSL(
struct ProbeVertex { packed_float3 position; packed_float3 normal; packed_float2 uv; };
struct ProbeUniforms { float4x4 viewProjection; float4 camera; float4 sun; float4 team; uint4 options; };
struct ProbeVaryings { float4 clip [[position]]; float3 world; float3 normal; float2 uv; };
vertex ProbeVaryings ProbeVertexMain(uint id [[vertex_id]], device const ProbeVertex* vertices [[buffer(0)]], constant ProbeUniforms& u [[buffer(1)]]) {
	ProbeVertex v = vertices[id];
	ProbeVaryings out;
	out.world = float3(v.position);
	out.clip = u.viewProjection * float4(out.world, 1.0f);
	out.normal = float3(v.normal);
	out.uv = float2(v.uv.x, 1.0f - v.uv.y);
	return out;
}
fragment float4 ProbeFragmentMain(ProbeVaryings in [[stage_in]], constant ProbeUniforms& u [[buffer(1)]], texture2d<float> color [[texture(0)]], texture2d<float> other [[texture(1)]]) {
	constexpr sampler s(coord::normalized, address::repeat, filter::linear, mip_filter::linear);
	float4 t1 = color.sample(s, in.uv);
	float4 t2 = other.sample(s, in.uv);
	BarMaterial material = BarDecodeMaterial(t1, t2, u.team.rgb);
	if (BarAlphaShadowDiscard(material, u.options.y != 0)) discard_fragment();
	if (u.options.x == 1) return float4(material.albedo, 1);
	if (u.options.x == 2) return float4(float3(t1.a), 1);
	if (u.options.x == 3) return float4(float3(material.emissiveness), 1);
	if (u.options.x == 4) return float4(float3(material.metalness), 1);
	if (u.options.x == 5) return float4(float3(material.roughness), 1);
	if (u.options.x == 6) return float4(float3(material.alpha), 1);
	float3 n = length_squared(in.normal) > 1e-12f ? normalize(in.normal) : float3(0,1,0);
	float3 l = normalize(u.sun.xyz);
	float3 v = normalize(u.camera.xyz - in.world);
	float3 h = normalize(l + v);
	float nl = clamp(dot(n,l), 0.0f, 1.0f);
	float nv = clamp(dot(n,v), 0.001f, 1.0f);
	float nh = clamp(dot(n,h), 0.0f, 1.0f);
	float vh = clamp(dot(v,h), 0.0f, 1.0f);
	float rough2 = material.roughness * material.roughness;
	float3 f0 = BarReflectanceF0(material);
	float3 fresnel = BarFresnelSchlick(f0, float3(BarReflectanceF90(f0)), vh);
	float visibility = BarVisibilityOcclusionFast(nl, nv, rough2);
	float distribution = BarMicrofacetDistribution(nh, rough2 * rough2);
	float3 diffuse = material.albedo * (1.0f - material.metalness) * (1.0f - fresnel) / M_PI_F;
	float3 direct = (diffuse + fresnel * visibility * distribution / M_PI_F) * nl * 2.0f;
	// This ambient/direct/emissive subset has no BAR IBL/LUT/normal maps/shadows/exposure/tone mapping.
	float3 radiance = material.albedo * (0.16f + material.emissiveness) + direct;
	return float4(clamp(radiance, 0.0f, 1.0f), material.alpha);
}
)MSL";

rendering::AssetDiagnosticResult RenderInternal(const rendering::AssetDiagnosticRequest& request)
{
	RuntimeLease lease;
	id<MTLDevice> device = lease.Device();
	rendering::AssetDiagnosticResult result;
	result.capabilities = lease.Capabilities();
	result.width = request.width;
	result.height = request.height;
	CaptureScope capture;
	capture.Start(device, request.capturePath);
	const auto setupStart = Clock::now();
	id<MTLBuffer> vertices = [device newBufferWithBytes:request.vertices.data() length:request.vertices.size_bytes() options:MTLResourceStorageModeShared];
	id<MTLBuffer> indices = [device newBufferWithBytes:request.indices.data() length:request.indices.size_bytes() options:MTLResourceStorageModeShared];
	Require(vertices != nil && indices != nil, "Cannot allocate S3O geometry buffers");
	vertices.label = @"Original S3O rest-pose vertices";
	indices.label = @"Pinned S3O triangulated indices";
	Uniforms uniform{};
	if (request.camera) {
		static_assert(sizeof(uniform.viewProjection) == sizeof(request.camera->viewProjection));
		std::memcpy(&uniform.viewProjection, request.camera->viewProjection.data(), sizeof(uniform.viewProjection));
		uniform.camera = simd_make_float4(request.camera->position[0], request.camera->position[1], request.camera->position[2], 1);
	} else {
		const simd_float3 minimum = {request.minimum[0], request.minimum[1], request.minimum[2]};
		const simd_float3 maximum = {request.maximum[0], request.maximum[1], request.maximum[2]};
		const simd_float3 center = (minimum + maximum) * 0.5f;
		const float radius = std::max(1.0f, simd_length(maximum - minimum) * 0.5f);
		const simd_float3 eye = center + simd_make_float3(1.8f, 1.0f, 2.2f) * radius;
		uniform.viewProjection = simd_mul(Perspective(0.72f, float(request.width) / float(request.height), radius * 0.02f, radius * 8.0f), LookAt(eye, center));
		uniform.camera = simd_make_float4(eye, 1);
	}
	uniform.sun = simd_make_float4(simd_normalize(simd_make_float3(-0.4f, 0.8f, 0.5f)), 0);
	uniform.team = simd_make_float4(request.teamColor[0], request.teamColor[1], request.teamColor[2], 1);
	uniform.options = simd_make_uint4(uint32_t(request.mode), request.alphaClip ? 1 : 0, 0, 0);
	for (size_t column = 0; column < 4; ++column)
		for (size_t row = 0; row < 4; ++row)
			Require(std::isfinite(uniform.viewProjection.columns[column][row]), "Diagnostic camera computation overflow");
	for (size_t axis = 0; axis < 3; ++axis)
		Require(std::isfinite(uniform.camera[axis]), "Diagnostic camera position overflow");
	id<MTLBuffer> uniforms = [device newBufferWithBytes:&uniform length:sizeof(uniform) options:MTLResourceStorageModeShared];
	id<MTLTexture> colorTexture = UploadTexture(device, request.textures[0], @"Original Arm_color.dds all mips");
	id<MTLTexture> otherTexture = UploadTexture(device, request.textures[1], @"Original Arm_other.dds all mips");
	MTLTextureDescriptor* targetDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:request.width height:request.height mipmapped:NO];
	targetDescriptor.usage = MTLTextureUsageRenderTarget;
	targetDescriptor.storageMode = MTLStorageModeShared;
	id<MTLTexture> target = [device newTextureWithDescriptor:targetDescriptor];
	MTLTextureDescriptor* depthDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:request.width height:request.height mipmapped:NO];
	depthDescriptor.usage = MTLTextureUsageRenderTarget;
	depthDescriptor.storageMode = MTLStorageModePrivate;
	id<MTLTexture> depth = [device newTextureWithDescriptor:depthDescriptor];
	Require(uniforms != nil && target != nil && depth != nil, "Cannot allocate diagnostic uniforms/attachments");
	target.label = @"Diagnostic RGBA8 offscreen output";
	depth.label = @"Diagnostic Depth32Float";
	MTLDepthStencilDescriptor* depthStateDescriptor = [MTLDepthStencilDescriptor new];
	depthStateDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	depthStateDescriptor.depthWriteEnabled = YES;
	id<MTLDepthStencilState> depthState = [device newDepthStencilStateWithDescriptor:depthStateDescriptor];
	NSError* error = nil;
	MTL4ArgumentTableDescriptor* argumentDescriptor = [MTL4ArgumentTableDescriptor new];
	argumentDescriptor.maxBufferBindCount = 2;
	argumentDescriptor.maxTextureBindCount = 2;
	argumentDescriptor.initializeBindings = YES;
	id<MTL4ArgumentTable> arguments = [device newArgumentTableWithDescriptor:argumentDescriptor error:&error];
	Require(arguments != nil, "Cannot create MTL4 argument table: " + Error(error));
	[arguments setAddress:vertices.gpuAddress atIndex:0];
	[arguments setAddress:uniforms.gpuAddress atIndex:1];
	[arguments setTexture:colorTexture.gpuResourceID atIndex:0];
	[arguments setTexture:otherTexture.gpuResourceID atIndex:1];
	id<MTLAllocation> allocations[] = {vertices, indices, uniforms, colorTexture, otherTexture, target, depth};
	for (id<MTLAllocation> allocation: allocations) lease.AddAllocation(allocation);
	lease.CommitResources();
	result.resourceSetupMs = Milliseconds(setupStart);
	result.residencyAllocationCount = lease.AllocationCount();
	result.residentBytes = lease.ResidentBytes();
	result.queueClass = lease.QueueClass();
	result.commandBufferClass = lease.CommandBufferClass();
	const auto shaderStart = Clock::now();
	id<MTL4Compiler> compiler = lease.Compiler();
	MTL4LibraryDescriptor* libraryDescriptor = [MTL4LibraryDescriptor new];
	libraryDescriptor.source = String(request.materialSemanticsSource + PROBE_SHADER);
	libraryDescriptor.name = @"BAR channel diagnostic library";
	id<MTLLibrary> library = [compiler newLibraryWithDescriptor:libraryDescriptor error:&error];
	Require(library != nil, "Cannot compile diagnostic MSL: " + Error(error));
	MTL4LibraryFunctionDescriptor* vertexFunction = [MTL4LibraryFunctionDescriptor new];
	vertexFunction.library = library;
	vertexFunction.name = @"ProbeVertexMain";
	MTL4LibraryFunctionDescriptor* fragmentFunction = [MTL4LibraryFunctionDescriptor new];
	fragmentFunction.library = library;
	fragmentFunction.name = @"ProbeFragmentMain";
	MTL4RenderPipelineDescriptor* pipelineDescriptor = [MTL4RenderPipelineDescriptor new];
	pipelineDescriptor.label = @"BAR channels diagnostic, incomplete PBR";
	pipelineDescriptor.vertexFunctionDescriptor = vertexFunction;
	pipelineDescriptor.fragmentFunctionDescriptor = fragmentFunction;
	pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	pipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
	MTL4PipelineOptions* pipelineOptions = [MTL4PipelineOptions new];
	pipelineOptions.shaderValidation = ShaderValidationMode(request.shaderValidation);
	pipelineDescriptor.options = pipelineOptions;
	id<MTLRenderPipelineState> pipeline = [compiler newRenderPipelineStateWithDescriptor:pipelineDescriptor compilerTaskOptions:nil error:&error];
	Require(pipeline != nil && depthState != nil, "Cannot create diagnostic pipeline: " + Error(error));
	result.shaderCompileMs = Milliseconds(shaderStart);
	MTL4RenderPassDescriptor* pass = [MTL4RenderPassDescriptor new];
	pass.colorAttachments[0].texture = target;
	pass.colorAttachments[0].loadAction = MTLLoadActionClear;
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.colorAttachments[0].clearColor = MTLClearColorMake(0.035, 0.045, 0.065, 1);
	pass.depthAttachment.texture = depth;
	pass.depthAttachment.loadAction = MTLLoadActionClear;
	pass.depthAttachment.storeAction = MTLStoreActionDontCare;
	pass.depthAttachment.clearDepth = 1;
	pass.renderTargetWidth = request.width;
	pass.renderTargetHeight = request.height;
	result.frames.reserve(size_t(request.measuredFrames) + 1);
	lease.Retain(depthState);
	lease.Retain(arguments);
	lease.Retain(pipeline);
	lease.Retain(library);
	for (uint32_t frameIndex = 0; frameIndex <= request.measuredFrames; ++frameIndex) {
		@autoreleasepool {
			const auto prepStart = Clock::now();
			id<MTL4CommandBuffer> command = lease.Begin("BAR original commander direct MTL4 diagnostic");
			id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
			Require(encoder != nil, "Cannot create MTL4 render encoder");
			encoder.label = @"Original commander indexed rest-pose draw";
			[encoder setRenderPipelineState:pipeline];
			[encoder setDepthStencilState:depthState];
			[encoder setCullMode:MTLCullModeNone];
			[encoder setViewport:MTLViewport{0, 0, double(request.width), double(request.height), 0, 1}];
			[encoder setArgumentTable:arguments atStages:MTLRenderStageVertex | MTLRenderStageFragment];
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:request.indices.size() indexType:MTLIndexTypeUInt32 indexBuffer:indices.gpuAddress indexBufferLength:indices.length];
			result.renderEncoderClass = NSStringFromClass([encoder class]).UTF8String;
			[encoder endEncoding];
			const auto metrics = lease.SubmitAndWait(prepStart, frameIndex);
			result.frames.push_back(metrics);
			if (frameIndex > 0 && metrics.gpuExecutionMs) result.capabilities.gpuTimestamps = true;
		}
	}
	result.rgba = lease.Readback(target, request.width, request.height, result.readbackMs);
	capture.Stop();
	if (!request.capturePath.empty()) {
		Require(std::filesystem::exists(request.capturePath), "Metal capture stopped without creating GPU trace");
		result.capturePath = request.capturePath;
	}
	return result;
}

} // namespace

namespace rendering {

AssetDiagnosticResult RenderMetal4Asset(const AssetDiagnosticRequest& request)
{
	@autoreleasepool {
		ValidateRequest(request);
		if (@available(macOS 26.0, *)) return RenderInternal(request);
		throw std::runtime_error("Direct Metal 4 asset diagnostic requires macOS 26 or newer");
	}
}

} // namespace rendering
