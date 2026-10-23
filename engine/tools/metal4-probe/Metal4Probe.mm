/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "AssetData.h"
#include "../../rts/Rendering/Metal4/Metal4AssetRenderer.h"

#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using metal4probe::ModelData;
using metal4probe::TextureData;

struct Options {
	std::filesystem::path assets;
	std::filesystem::path png;
	std::filesystem::path report;
	std::filesystem::path capture;
	std::filesystem::path materialContract = std::filesystem::path(__FILE__).parent_path().parent_path() / "metal4-contracts/BarMaterialSemantics.metal";
	uint32_t width = 1024;
	uint32_t height = 1024;
	uint32_t frames = 16;
	uint32_t mode = 0;
	std::string modeName = "material";
	std::array<float, 3> teamColor = {0.18f, 0.45f, 0.95f};
	bool alphaClip = false;
	bool inspectOnly = false;
	bool decodeDDS = false;
	bool shaderValidation = true;
};

double Milliseconds(Clock::time_point start, Clock::time_point end = Clock::now())
{
	return std::chrono::duration<double, std::milli>(end - start).count();
}

NSString* String(const std::string& value)
{
	NSString* result = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
	if (result == nil)
		throw std::runtime_error("Text is not valid UTF-8");
	return result;
}

std::string Error(NSError* error)
{
	return error == nil ? "Metal returned nil without an error" : std::string(error.localizedDescription.UTF8String);
}

void Require(bool condition, const std::string& message)
{
	if (!condition)
		throw std::runtime_error(message);
}

uint32_t ParseInteger(const char* text, uint32_t minimum, uint32_t maximum)
{
	char* end = nullptr;
	const auto value = std::strtoul(text, &end, 10);
	Require(text[0] != '\0' && end[0] == '\0' && value >= minimum && value <= maximum, "Integer argument outside allowed range");
	return uint32_t(value);
}

Options ParseOptions(int argc, char** argv)
{
	Options options;
	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];
		auto next = [&]() -> const char* {
			Require(i + 1 < argc, "Missing value for " + argument);
			return argv[++i];
		};
		if (argument == "--assets") options.assets = next();
		else if (argument == "--png") options.png = next();
		else if (argument == "--report") options.report = next();
		else if (argument == "--capture") options.capture = next();
		else if (argument == "--material-contract") options.materialContract = next();
		else if (argument == "--size") options.width = options.height = ParseInteger(next(), 64, 4096);
		else if (argument == "--width") options.width = ParseInteger(next(), 64, 4096);
		else if (argument == "--height") options.height = ParseInteger(next(), 64, 4096);
		else if (argument == "--frames") options.frames = ParseInteger(next(), 1, 300);
		else if (argument == "--alpha-clip") options.alphaClip = true;
		else if (argument == "--inspect-assets") options.inspectOnly = true;
		else if (argument == "--decode-dds") options.decodeDDS = true;
		else if (argument == "--shader-validation") {
			const std::string setting = next();
			Require(setting == "on" || setting == "off", "--shader-validation requires on or off");
			options.shaderValidation = setting == "on";
		}
		else if (argument == "--team") {
			const std::string color = next();
			float red = 0, green = 0, blue = 0;
			char trailing = 0;
			Require(std::sscanf(color.c_str(), "%f,%f,%f%c", &red, &green, &blue, &trailing) == 3,
				"--team requires r,g,b in [0,1]");
			Require(std::isfinite(red) && std::isfinite(green) && std::isfinite(blue) &&
				red >= 0 && red <= 1 && green >= 0 && green <= 1 && blue >= 0 && blue <= 1,
				"Team colour outside [0,1]");
			options.teamColor = {red, green, blue};
		} else if (argument == "--mode") {
			options.modeName = next();
			const std::vector<std::string> modes = {"material", "diffuse", "team-mask", "emission", "metalness", "roughness", "alpha"};
			const auto found = std::find(modes.begin(), modes.end(), options.modeName);
			Require(found != modes.end(), "Unknown material diagnostic mode");
			options.mode = uint32_t(found - modes.begin());
		} else {
			throw std::runtime_error("Unknown argument: " + argument);
		}
	}
	Require(!options.assets.empty() && !options.report.empty(), "Usage: --assets DIR --report FILE [--png FILE] [--inspect-assets] [--decode-dds] [--shader-validation on|off] [--mode material|diffuse|team-mask|emission|metalness|roughness|alpha] [--team r,g,b] [--alpha-clip] [--size 64..4096 | --width PIXELS --height PIXELS] [--frames 1..300] [--capture FILE.gputrace]");
	Require(options.inspectOnly || !options.png.empty(), "GPU diagnostic requires --png output");
	return options;
}

void SaveReport(NSDictionary* report, const std::filesystem::path& path)
{
	if (!path.parent_path().empty())
		std::filesystem::create_directories(path.parent_path());
	NSError* error = nil;
	NSData* data = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:&error];
	Require(data != nil, "Cannot serialize probe report: " + Error(error));
	Require([data writeToFile:String(path.string()) options:NSDataWritingAtomic error:&error], "Cannot write probe report: " + Error(error));
}

NSArray* FloatArray(const std::array<float, 3>& value)
{
	return @[@(value[0]), @(value[1]), @(value[2])];
}

NSDictionary* AssetReport(const ModelData& model, const TextureData& color, const TextureData& other)
{
	NSMutableArray* pieces = [NSMutableArray array];
	for (const auto& piece: model.pieces) {
		[pieces addObject:@{@"name": String(piece.name), @"parent": @(piece.parent), @"local_offset": FloatArray(piece.offset),
			@"global_offset": FloatArray(piece.globalOffset), @"first_vertex": @(piece.firstVertex), @"vertices": @(piece.vertexCount),
			@"first_index": @(piece.firstIndex), @"indices": @(piece.indexCount), @"source_primitive": @(piece.sourcePrimitive)}];
	}
	auto texture = [](const TextureData& data) -> NSDictionary* {
		NSMutableArray* mips = [NSMutableArray array];
		for (const auto& mip: data.mips)
			[mips addObject:@{@"width": @(mip.width), @"height": @(mip.height), @"offset": @(mip.offset), @"bytes": @(mip.byteCount)}];
		return @{@"format": String(data.format), @"width": @(data.width), @"height": @(data.height), @"file_bytes": @(data.ddsBytes.size()), @"mips": mips};
	};
	NSMutableArray* warnings = [NSMutableArray array];
	for (const auto& warning: model.warnings)
		[warnings addObject:String(warning)];
	return @{@"texture1_name": String(model.texture1), @"texture2_name": String(model.texture2), @"source_radius": @(model.sourceRadius),
		@"source_height": @(model.sourceHeight), @"source_midpoint": FloatArray(model.sourceMidpoint), @"vertices": @(model.vertices.size()),
		@"indices": @(model.indices.size()), @"triangles": @(model.indices.size() / 3), @"pieces": pieces,
		@"minimum": FloatArray(model.minimum), @"maximum": FloatArray(model.maximum),
		@"strip_zero_prefix_triangles": @(model.stripZeroPrefixTriangles), @"degenerate_triangles": @(model.degenerateTriangles),
		@"zero_normals": @(model.zeroNormals), @"warnings": warnings, @"color_dds": texture(color), @"other_dds": texture(other),
		@"orientation": @"Original DDS bytes; S3O UV sampled at (u,1-v), equivalent to pinned unittexture flipDDS",
		@"pose": @"All original pieces; accumulated rest translations; no animation, rotation or simulation",
		@"tangents": @"Not computed; no normal mapping in this diagnostic",
		@"cpu_decoder": @"Shared engine s3o::Decode and s3o::Triangulate from rts/Rendering/Models/S3OData",
		@"cpu_equivalence": @"Raw decode and primitive conversion are shared with the engine. Diagnostic normal preparation uses exact sqrt instead of engine approximate isqrt, rejects invalid normals, and omits tangents. Linux engine prepared-output comparison pending."};
}

void SavePNG(const std::vector<uint8_t>& bytes, uint32_t width, uint32_t height, const std::filesystem::path& path)
{
	if (!path.parent_path().empty())
		std::filesystem::create_directories(path.parent_path());
	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, bytes.data(), bytes.size(), nullptr);
	CGImageRef image = CGImageCreate(width, height, 8, 32, size_t(width) * 4, colorSpace,
		CGBitmapInfo(kCGBitmapByteOrder32Big) | CGBitmapInfo(kCGImageAlphaLast), provider, nullptr, false, kCGRenderingIntentDefault);
	NSURL* url = [NSURL fileURLWithPath:String(path.string())];
	CGImageDestinationRef destination = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
	const bool created = image != nullptr && destination != nullptr;
	if (created)
		CGImageDestinationAddImage(destination, image, nullptr);
	const bool saved = created && CGImageDestinationFinalize(destination);
	if (destination) CFRelease(destination);
	if (image) CGImageRelease(image);
	if (provider) CGDataProviderRelease(provider);
	if (colorSpace) CGColorSpaceRelease(colorSpace);
	Require(saved, "Cannot write PNG output");
}

NSDictionary* FrameReport(const rendering::FrameMetrics& frame)
{
	return @{@"frame_index": @(frame.frameIndex), @"warmup": @(frame.frameIndex == 0), @"cpu_simulation_ms": [NSNull null],
		@"cpu_command_prep_ms": @(frame.cpuCommandPrepMs), @"cpu_submission_ms": @(frame.cpuSubmissionMs),
		@"gpu_execution_ms": frame.gpuExecutionMs ? (id)@(*frame.gpuExecutionMs) : (id)[NSNull null],
		@"cpu_completion_wait_ms": @(frame.cpuCompletionWaitMs), @"cpu_readback_ms": @(frame.cpuReadbackMs)};
}

struct PreparedTexture {
	rendering::AssetTextureFormat format = rendering::AssetTextureFormat::BC3RGBA;
	std::vector<rendering::AssetTextureMip> mips;
	std::vector<std::vector<uint8_t>> decodedStorage;
};

PreparedTexture PrepareTexture(const TextureData& data, bool nativeBC)
{
	PreparedTexture prepared;
	prepared.format = nativeBC ? rendering::AssetTextureFormat::BC3RGBA : rendering::AssetTextureFormat::RGBA8Unorm;
	prepared.mips.reserve(data.mips.size());
	prepared.decodedStorage.reserve(nativeBC ? 0 : data.mips.size());
	for (size_t level = 0; level < data.mips.size(); ++level) {
		const auto& source = data.mips[level];
		rendering::AssetTextureMip mip;
		mip.width = source.width;
		mip.height = source.height;
		if (nativeBC) {
			mip.bytes = std::span<const uint8_t>(data.ddsBytes).subspan(source.offset, source.byteCount);
			mip.bytesPerRow = size_t((source.width + 3) / 4) * 16;
		} else {
			prepared.decodedStorage.push_back(metal4probe::DecodeDXT5Mip(data, level));
			mip.bytes = prepared.decodedStorage.back();
			mip.bytesPerRow = size_t(source.width) * 4;
		}
		prepared.mips.push_back(mip);
	}
	return prepared;
}

std::string MaterialContractSource(const Options& options)
{
	const auto size = std::filesystem::file_size(options.materialContract);
	Require(size > 0 && size < 64 * 1024, "Material shader contract outside size bounds");
	std::ifstream file(options.materialContract, std::ios::binary);
	std::string source(size, '\0');
	Require(bool(file.read(source.data(), source.size())), "Cannot read material shader contract");
	return source;
}

void RunGPU(const Options& options, const ModelData& model, const TextureData& color, const TextureData& other, NSMutableDictionary* report)
{
	const auto capabilities = rendering::QueryMetal4Capabilities();
	report[@"device"] = @{@"name": String(capabilities.deviceName), @"supports_family_metal4": @(capabilities.directMetal4),
		@"supports_bc_texture_compression": @(capabilities.bcTextureCompression), @"unified_memory": @(capabilities.unifiedMemory)};
	Require(capabilities.directMetal4, "Device does not support MTLGPUFamilyMetal4; no renderer fallback exists");
	const bool nativeBC = capabilities.bcTextureCompression && !options.decodeDDS;
	const auto preparationStart = Clock::now();
	const auto vertices = metal4probe::BuildRestPoseVertices(model);
	const std::array<PreparedTexture, 2> textures = {PrepareTexture(color, nativeBC), PrepareTexture(other, nativeBC)};
	report[@"cpu_texture_and_pose_prepare_ms"] = @(Milliseconds(preparationStart));
	rendering::AssetDiagnosticRequest request;
	request.vertices = vertices;
	request.indices = model.indices;
	for (size_t i = 0; i < textures.size(); ++i) {
		request.textures[i].format = textures[i].format;
		request.textures[i].mips = textures[i].mips;
	}
	request.minimum = model.minimum;
	request.maximum = model.maximum;
	request.teamColor = options.teamColor;
	request.mode = rendering::AssetDiagnosticMode(options.mode);
	request.width = options.width;
	request.height = options.height;
	request.measuredFrames = options.frames;
	request.alphaClip = options.alphaClip;
	request.shaderValidation = options.shaderValidation;
	request.capturePath = options.capture.string();
	request.materialSemanticsSource = MaterialContractSource(options);
	const auto result = rendering::RenderMetal4Asset(request);
	report[@"resource_setup_ms"] = @(result.resourceSetupMs);
	report[@"shader_compile_ms"] = @(result.shaderCompileMs);
	report[@"cpu_readback_ms"] = @(result.readbackMs);
	report[@"resources"] = @{@"residency_allocations": @(result.residencyAllocationCount), @"resident_bytes": @(result.residentBytes),
		@"texture_path": nativeBC ? @"Native original BC3 blocks, all declared mips" : @"Bounded CPU DXT5 decode to RGBA8, all declared mips",
		@"forced_cpu_decode": @(options.decodeDDS), @"native_vs_decode_equivalence": @"GPU comparison pending; integer RGBA decode may differ from native BC interpolation/quantization",
		@"color_space": @"Linear UNORM texture channels; material gamma default 1.0; no automatic sRGB sampling"};
	report[@"direct_api"] = @{@"backend": @"rts/Rendering/Metal4/Metal4AssetRenderer",
		@"queue_protocol": @"MTL4CommandQueue", @"queue_class": String(result.queueClass),
		@"buffer_protocol": @"MTL4CommandBuffer", @"buffer_class": String(result.commandBufferClass),
		@"encoder_protocol": @"MTL4RenderCommandEncoder", @"encoder_class": String(result.renderEncoderClass),
		@"command_label": @"BAR original commander direct MTL4 diagnostic", @"encoder_label": @"Original commander indexed rest-pose draw",
		@"draw": @"Engine-owned MTL4 indexed triangle draw; GPU-address index buffer; MTL4 argument table"};
	NSMutableArray* frameReports = [NSMutableArray array];
	std::vector<double> measuredGPU;
	std::vector<double> measuredCPU;
	for (const auto& frame: result.frames) {
		[frameReports addObject:FrameReport(frame)];
		if (frame.frameIndex > 0) {
			if (frame.gpuExecutionMs) measuredGPU.push_back(*frame.gpuExecutionMs);
			measuredCPU.push_back(frame.cpuCommandPrepMs);
		}
	}
	report[@"frames"] = frameReports;
	size_t changedPixels = 0;
	for (size_t i = 0; i < result.rgba.size(); i += 4) {
		const bool differs = std::abs(int(result.rgba[i]) - 9) > 2 || std::abs(int(result.rgba[i + 1]) - 11) > 2 || std::abs(int(result.rgba[i + 2]) - 17) > 2;
		changedPixels += differs;
	}
	report[@"pixel_check"] = @{@"pixels_different_from_clear": @(changedPixels), @"total_pixels": @(size_t(result.width) * result.height), @"visible_geometry": @(changedPixels > 100)};
	Require(changedPixels > 100, "Offscreen output has insufficient visible geometry");
	const auto pngStart = Clock::now();
	SavePNG(result.rgba, result.width, result.height, options.png);
	report[@"png_write_ms"] = @(Milliseconds(pngStart));
	auto summary = [](std::vector<double> samples) -> id {
		if (samples.empty()) return [NSNull null];
		std::sort(samples.begin(), samples.end());
		double total = 0;
		for (double sample: samples) total += sample;
		return @{@"samples": @(samples.size()), @"mean_ms": @(total / samples.size()), @"median_ms": @(samples[samples.size() / 2]),
			@"p95_ms": @(samples[std::min(samples.size() - 1, size_t(std::ceil(samples.size() * 0.95) - 1))]), @"minimum_ms": @(samples.front()), @"maximum_ms": @(samples.back())};
	};
	report[@"timing_summary"] = @{@"cpu_command_prep": summary(measuredCPU), @"gpu_execution": summary(measuredGPU),
		@"simulation": [NSNull null], @"warmup_frames": @1, @"measurement": @"Serialized engine-owned offscreen diagnostic; GPU commit feedback duration; CPU preparation/submission/wait independent. No game FPS or simulation cost claim."};
	report[@"gpu_feedback_error"] = [NSNull null];
	report[@"gpu_timestamps_available"] = @(result.capabilities.gpuTimestamps);
	report[@"png"] = String(options.png.string());
	if (!result.capturePath.empty()) report[@"capture"] = String(result.capturePath);
}
} // namespace

int main(int argc, char** argv)
{
	@autoreleasepool {
		Options options;
		NSMutableDictionary* report = [@{@"schema": @"bar-metal4-asset-probe-v1", @"status": @"started",
			@"source_commit": @"de69361239d8c8b1012dba3f5aa3122954ea4da3", @"simulation_present": @NO,
			@"scope": @"Direct Metal 4 original-asset bootstrap diagnostic; full engine and BAR visual parity pending",
			@"integration": @"Shared engine S3O decoder and engine-owned Metal4AssetRenderer; diagnostic normal/texture preparation; engine controller/gameplay renderer pending",
			@"ai_assistance": @"Codex assisted source implementation; HQ runtime and human acceptance recorded separately",
			@"missing": @[@"Terrain", @"Unit animation", @"Gameplay simulation", @"Lua graphics", @"UI", @"Shadows", @"BAR IBL and BRDF/LUT", @"Normal and wreck maps", @"BAR exposure/tone mapping", @"Window/resize lifecycle", @"Linux CPU-output equivalence comparison"],
			@"os": [NSProcessInfo processInfo].operatingSystemVersionString} mutableCopy];
		try {
			options = ParseOptions(argc, argv);
			report[@"arguments"] = @{@"assets": String(options.assets.string()), @"width": @(options.width), @"height": @(options.height),
				@"size": options.width == options.height ? (id)@(options.width) : (id)[NSNull null], @"measured_frames": @(options.frames),
				@"resolution_policy": @"Explicit pixel dimensions; no implicit Retina/backing-scale multiplication",
				@"mode": String(options.modeName), @"team": FloatArray(options.teamColor),
				@"alpha_clip_specialization": @(options.alphaClip), @"forced_dds_decode": @(options.decodeDDS), @"material_contract": String(options.materialContract.string())};
			auto environment = [](const char* key) -> NSString* { const char* value = std::getenv(key); return value ? String(value) : @"unset"; };
			report[@"validation"] = @{@"metal_debug_layer_environment": environment("MTL_DEBUG_LAYER"), @"shader_validation_environment": environment("MTL_SHADER_VALIDATION"),
				@"capture_environment": environment("METAL_CAPTURE_ENABLED"), @"pipeline_shader_validation": options.shaderValidation ? @"Enabled explicitly on MTL4 pipeline" : @"Disabled explicitly on MTL4 pipeline",
				@"log_review": @"HQ must inspect bounded runtime stderr and GPU trace; shader-validation run and capture run are separate on this runtime"};
			const auto assetStart = Clock::now();
			const auto model = metal4probe::LoadS3O(options.assets / "armcom.s3o");
			const auto color = metal4probe::LoadDXT5DDS(options.assets / "Arm_color.dds");
			const auto other = metal4probe::LoadDXT5DDS(options.assets / "Arm_other.dds");
			Require(model.texture1 == "Arm_color.dds" && model.texture2 == "Arm_other.dds", "Commander texture references differ from staged original asset names");
			report[@"cpu_asset_load_ms"] = @(Milliseconds(assetStart));
			report[@"assets"] = AssetReport(model, color, other);
			if (options.inspectOnly) {
				report[@"status"] = @"cpu_assets_passed_gpu_not_run";
			} else {
				if (@available(macOS 26.0, *)) {
					RunGPU(options, model, color, other, report);
					report[@"status"] = @"direct_metal4_diagnostic_passed";
				} else {
					throw std::runtime_error("Direct Metal 4 diagnostic requires macOS 26 or newer");
				}
			}
			SaveReport(report, options.report);
			std::cout << "Probe complete: " << options.report << '\n';
			return 0;
		} catch (const std::exception& error) {
			report[@"status"] = @"failed";
			report[@"error"] = String(error.what());
			if (!options.report.empty()) {
				try { SaveReport(report, options.report); }
				catch (const std::exception& reportError) { std::cerr << reportError.what() << '\n'; }
			}
			std::cerr << "Metal 4 diagnostic failed: " << error.what() << '\n';
			return 1;
		}
	}
}
