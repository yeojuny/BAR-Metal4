/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "../../rts/Rendering/Metal4/Metal4ScreenRenderer.h"
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace rendering;
unsigned checks = 0;

void Check(bool condition, const std::string& message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}

NSString* String(const std::string& value)
{
	NSString* result = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
	Check(result != nil, "Text is not UTF-8");
	return result;
}

ScreenDrawList Rebuild(NSDictionary* snapshot)
{
	NSArray* rawVertices = snapshot[@"vertices"];
	NSArray* rawIndices = snapshot[@"indices"];
	NSArray* commands = snapshot[@"commands"];
	Check([rawVertices isKindOfClass:[NSArray class]] && [rawIndices isKindOfClass:[NSArray class]] && [commands isKindOfClass:[NSArray class]], "Fixture geometry fields must be arrays");
	Check(rawVertices.count <= 100'000 && rawIndices.count <= 600'000 && commands.count <= 1000, "Fixture arrays exceed bounds");
	std::vector<ScreenVertex> vertices;
	for (NSArray* values in rawVertices) {
		Check([values isKindOfClass:[NSArray class]] && values.count == 7, "Fixture vertex must have seven components");
		for (id value in values) Check([value isKindOfClass:[NSNumber class]], "Fixture vertex components must be numbers");
		vertices.push_back({[values[0] floatValue], [values[1] floatValue], [values[2] floatValue],
			{[values[3] floatValue], [values[4] floatValue], [values[5] floatValue], [values[6] floatValue]}});
	}
	std::vector<uint32_t> indices;
	for (NSNumber* value in rawIndices) { Check([value isKindOfClass:[NSNumber class]], "Fixture indices must be numbers"); indices.push_back(value.unsignedIntValue); }
	ScreenDrawList list;
	for (NSDictionary* command in commands) {
		Check([command isKindOfClass:[NSDictionary class]], "Fixture command must be a dictionary");
		const auto first = [command[@"first_index"] unsignedIntValue];
		const auto count = [command[@"index_count"] unsignedIntValue];
		Check(first == list.Indices().size() && count % 6 == 0 && first <= indices.size() && count <= indices.size() - first, "Fixture must preserve contiguous quad command ranges");
		NSDictionary* state = command[@"state"];
		NSDictionary* blend = state[@"blend"];
		Check([state isKindOfClass:[NSDictionary class]] && [blend isKindOfClass:[NSDictionary class]], "Fixture state/blend must be dictionaries");
		list.SetBlend({[blend[@"enabled"] boolValue], ScreenBlendFactor([blend[@"source_color"] unsignedIntValue]),
			ScreenBlendFactor([blend[@"destination_color"] unsignedIntValue]), ScreenBlendFactor([blend[@"source_alpha"] unsignedIntValue]),
			ScreenBlendFactor([blend[@"destination_alpha"] unsignedIntValue])});
		id scissor = state[@"scissor"];
		if (scissor == [NSNull null]) list.SetScissor(std::nullopt);
		else {
			Check([scissor isKindOfClass:[NSArray class]] && [scissor count] == 4, "Fixture scissor requires four components");
			list.SetScissor(ScreenScissor{[scissor[0] intValue], [scissor[1] intValue], [scissor[2] unsignedIntValue], [scissor[3] unsignedIntValue]});
		}
		list.BeginQuads();
		for (size_t offset = first; offset < size_t(first) + count; offset += 6) {
			const uint32_t base = indices[offset];
			Check(base < vertices.size() && vertices.size() - base >= 4 && indices[offset + 1] == base + 1 && indices[offset + 2] == base + 2 &&
				indices[offset + 3] == base && indices[offset + 4] == base + 2 && indices[offset + 5] == base + 3, "Fixture changed the quad diagonal/order");
			for (unsigned i = 0; i < 4; ++i) {
				const auto& vertex = vertices[base + i];
				list.Color(vertex.color); list.Vertex(vertex.x, vertex.y, vertex.z);
			}
		}
		list.End();
	}
	Check(list.Vertices().size() == vertices.size() && list.Indices().size() == indices.size() && list.Commands().size() == commands.count, "Fixture reconstruction altered geometry counts");
	Check(vertices.empty() || std::memcmp(list.Vertices().data(), vertices.data(), vertices.size() * sizeof(ScreenVertex)) == 0, "Fixture reconstruction altered exact vertex bits");
	Check(std::equal(list.Indices().begin(), list.Indices().end(), indices.begin()), "Fixture reconstruction altered index order/degeneracy");
	return list;
}

ScreenColor Quantized(ScreenColor color)
{
	for (auto& value: color) value = float(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)) / 255.0f;
	return color;
}

float BlendFactor(ScreenBlendFactor factor, float alpha)
{
	switch (factor) {
		case ScreenBlendFactor::Zero: return 0;
		case ScreenBlendFactor::One: return 1;
		case ScreenBlendFactor::SourceAlpha: return alpha;
		case ScreenBlendFactor::OneMinusSourceAlpha: return 1 - alpha;
	}
	throw std::runtime_error("Analytic blend factor unsupported");
}

// Independent scalar equations, not a CPU implementation of the GPU pipeline.
ScreenColor Blend(ScreenColor source, ScreenColor destination, ScreenBlendState state = {})
{
	if (!state.enabled) return source;
	for (size_t channel = 0; channel < 4; ++channel) {
		const auto sourceFactor = channel == 3 ? state.sourceAlpha : state.sourceColor;
		const auto destinationFactor = channel == 3 ? state.destinationAlpha : state.destinationColor;
		destination[channel] = source[channel] * BlendFactor(sourceFactor, source[3]) + destination[channel] * BlendFactor(destinationFactor, source[3]);
	}
	return destination;
}

void Pixel(const ScreenRenderResult& image, uint32_t x, uint32_t bottomY, ScreenColor expected, const std::string& label)
{
	Check(x < image.width && bottomY < image.height, "Analytic sample outside framebuffer");
	const size_t offset = (size_t(image.height - 1 - bottomY) * image.width + x) * 4;
	for (size_t channel = 0; channel < 4; ++channel) {
		const int golden = int(std::lround(std::clamp(expected[channel], 0.0f, 1.0f) * 255.0f));
		Check(std::abs(int(image.rgba[offset + channel]) - golden) <= 2,
			label + " analytic pixel mismatch at " + std::to_string(x) + "," + std::to_string(bottomY) + " channel " + std::to_string(channel));
	}
}

void Quad(ScreenDrawList& list, float x0, float y0, float x1, float y1, ScreenColor bottom, ScreenColor top)
{
	list.BeginQuads();
	list.Color(bottom); list.Vertex(x0, y0); list.Vertex(x1, y0);
	list.Color(top); list.Vertex(x1, y1); list.Vertex(x0, y1);
	list.End();
}

void PNG(const ScreenRenderResult& image, const std::filesystem::path& path)
{
	CGColorSpaceRef color = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, image.rgba.data(), image.rgba.size(), nullptr);
	CGImageRef bitmap = CGImageCreate(image.width, image.height, 8, 32, size_t(image.width) * 4, color,
		CGBitmapInfo(kCGBitmapByteOrder32Big) | CGBitmapInfo(kCGImageAlphaLast), provider, nullptr, false, kCGRenderingIntentDefault);
	NSURL* url = [NSURL fileURLWithPath:String(path.string())];
	CGImageDestinationRef destination = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
	const bool valid = bitmap != nullptr && destination != nullptr;
	if (valid) CGImageDestinationAddImage(destination, bitmap, nullptr);
	const bool saved = valid && CGImageDestinationFinalize(destination);
	if (destination) CFRelease(destination); if (bitmap) CGImageRelease(bitmap); if (provider) CGDataProviderRelease(provider); if (color) CGColorSpaceRelease(color);
	Check(saved, "Cannot export screen fixture PNG");
}

NSDictionary* Metadata(const ScreenRenderResult& result, const std::string& label)
{
	NSMutableArray* frames = [NSMutableArray array];
	for (const auto& frame: result.frames)
		[frames addObject:@{@"frame": @(frame.frameIndex), @"cpu_command_prep_ms": @(frame.cpuCommandPrepMs), @"cpu_submission_ms": @(frame.cpuSubmissionMs),
			@"cpu_completion_wait_ms": @(frame.cpuCompletionWaitMs), @"gpu_execution_ms": frame.gpuExecutionMs ? (id)@(*frame.gpuExecutionMs) : (id)[NSNull null], @"cpu_simulation_ms": [NSNull null]}];
	return @{@"label": String(label), @"width": @(result.width), @"height": @(result.height), @"queue_class": String(result.queueClass),
		@"buffer_class": String(result.commandBufferClass), @"encoder_class": String(result.renderEncoderClass), @"device": String(result.capabilities.deviceName),
		@"resource_setup_ms": @(result.resourceSetupMs), @"shader_compile_ms": @(result.shaderCompileMs), @"readback_ms": @(result.readbackMs),
		@"residency_allocations": @(result.residencyAllocationCount), @"frames": frames};
}

void FixturePixels(const ScreenRenderResult& image, const std::string& label)
{
	const ScreenColor constant{0.2f, 0.4f, 0.6f, 0.37f};
	if (label == "cut_corners" || label == "square_corners") {
		Pixel(image, 60, 50, Blend(constant, {}), label);
		Pixel(image, 5, 10, {}, label);
		Pixel(image, 11, 21, label == "cut_corners" ? ScreenColor{} : Blend(constant, {}), label);
	} else if (label == "gradient") {
		const float t = (50.5f - 20.0f) / 60.0f;
		const ScreenColor source{1 - t, 0, t, 0.2f + 0.6f * t};
		const ScreenBlendState additive{true, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One};
		Pixel(image, 60, 50, Blend(source, {}, additive), label);
		Pixel(image, 80, 50, {}, label); Pixel(image, 60, 24, {}, label); Pixel(image, 60, 65, {}, label);
	} else if (label == "inherited_state") {
		const ScreenBlendState additive{true, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One};
		Pixel(image, 60, 50, Blend({1.0f / 6.0f, 0, 5.0f / 6.0f, 0.7f}, {}, additive), label);
		Pixel(image, 80, 50, {}, label);
	} else if (label == "minimum_corner") {
		Pixel(image, 5, 4, Blend({1, 1, 1, 0.25f}, {}), label); Pixel(image, 20, 20, {}, label);
	} else if (label == "invalid_bounds" || label == "error_recovery") {
		Check(std::all_of(image.rgba.begin(), image.rgba.end(), [](uint8_t value) { return value == 0; }), "Empty fixture changed clear pixels");
	} else throw std::runtime_error("Unknown frozen fixture snapshot label: " + label);
}

void SaveJSON(NSMutableDictionary* report, const std::filesystem::path& path)
{
	if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
	NSError* error = nil;
	NSData* bytes = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:&error];
	Check(bytes != nil && [bytes writeToFile:String(path.string()) options:NSDataWritingAtomic error:&error], "Cannot save GPU analytic report");
}
} // namespace

int main(int argc, char** argv)
{
	@autoreleasepool {
		std::filesystem::path fixture, reportPath, outputDirectory;
		std::string capture;
		bool validation = true;
		NSMutableDictionary* report = [@{@"status": @"started", @"scope": @"Direct Metal4 planar untextured ScreenDrawList; analytic pixel checks; Linux visual parity pending", @"pixel_tolerance_rgba8": @2, @"simulation_present": @NO} mutableCopy];
		NSMutableArray* renders = [NSMutableArray array]; report[@"renders"] = renders;
		try {
			for (int i = 1; i < argc; ++i) {
				const std::string argument = argv[i]; Check(i + 1 < argc, "Missing GPU harness argument"); const char* value = argv[++i];
				if (argument == "--fixture") fixture = value;
				else if (argument == "--report") reportPath = value;
				else if (argument == "--output-dir") outputDirectory = value;
				else if (argument == "--capture") capture = value;
				else if (argument == "--shader-validation") { Check(std::string(value) == "on" || std::string(value) == "off", "Shader validation requires on/off"); validation = std::string(value) == "on"; }
				else throw std::runtime_error("Unknown GPU harness argument");
			}
			Check(!fixture.empty() && !reportPath.empty() && !outputDirectory.empty(), "Usage: --fixture FILE --report FILE --output-dir DIR [--shader-validation on|off] [--capture fresh.gputrace]");
			std::filesystem::create_directories(outputDirectory);
			Check(std::filesystem::file_size(fixture) < 4 * 1024 * 1024, "Fixture file outside size bound");
			NSData* bytes = [NSData dataWithContentsOfFile:String(fixture.string())]; NSError* error = nil;
			Check(bytes != nil, "Cannot read frozen fixture data");
			NSDictionary* root = [NSJSONSerialization JSONObjectWithData:bytes options:0 error:&error];
			Check([root isKindOfClass:[NSDictionary class]] && [root[@"schema_version"] intValue] == 1 && [root[@"coordinate_origin"] isEqual:@"bottom-left screen pixels"], "Wrong frozen fixture schema/origin");
			NSArray* snapshots = root[@"snapshots"]; Check([snapshots isKindOfClass:[NSArray class]] && snapshots.count == 7, "Frozen fixture snapshot count differs");
			for (NSDictionary* snapshot in snapshots) {
				Check([snapshot isKindOfClass:[NSDictionary class]] && [snapshot[@"label"] isKindOfClass:[NSString class]], "Fixture snapshot label must be text");
				const std::string label = [snapshot[@"label"] UTF8String];
				const auto list = Rebuild(snapshot);
				ScreenRenderRequest request; request.drawList = &list; request.shaderValidation = validation;
				if (label == "cut_corners") request.capturePath = capture;
				const auto image = RenderMetal4Screen(request);
				FixturePixels(image, label); PNG(image, outputDirectory / (label + ".png"));
				[renders addObject:Metadata(image, label)];
			}
			auto controlled = [&](const std::string& label, const ScreenDrawList& list, ScreenColor clear, auto verify) {
				ScreenRenderRequest request; request.drawList = &list; request.width = request.height = 64; request.clearColor = clear; request.shaderValidation = validation;
				const auto image = RenderMetal4Screen(request); verify(image); PNG(image, outputDirectory / (label + ".png")); [renders addObject:Metadata(image, label)];
			};
			const ScreenColor clear{0.1f, 0.2f, 0.3f, 0.4f}; const ScreenColor source{0.8f, 0.4f, 0.2f, 0.25f};
			ScreenDrawList standard; Quad(standard, 10, 10, 50, 50, source, source);
			controlled("standard_alpha", standard, clear, [&](const auto& image) { Pixel(image, 20, 20, Blend(source, Quantized(clear)), "standard_alpha"); Pixel(image, 0, 0, clear, "clear"); });
			const ScreenBlendState additive{true, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One, ScreenBlendFactor::SourceAlpha, ScreenBlendFactor::One};
			ScreenDrawList addition; addition.SetBlend(additive); Quad(addition, 10, 10, 50, 50, source, source);
			controlled("additive_alpha", addition, clear, [&](const auto& image) { Pixel(image, 20, 20, Blend(source, Quantized(clear), additive), "additive_alpha"); });
			const ScreenBlendState independent{true, ScreenBlendFactor::One, ScreenBlendFactor::Zero, ScreenBlendFactor::Zero, ScreenBlendFactor::One};
			ScreenDrawList mixed; mixed.SetBlend(independent); Quad(mixed, 10, 10, 50, 50, source, source);
			controlled("independent_alpha", mixed, clear, [&](const auto& image) { Pixel(image, 20, 20, Blend(source, Quantized(clear), independent), "independent_alpha"); });
			ScreenBlendState disabled; disabled.enabled = false;
			ScreenDrawList interpolation; interpolation.SetBlend(disabled); Quad(interpolation, 10, 10, 50, 50, {1, 0, 0, 0.2f}, {0, 0, 1, 0.8f});
			controlled("smooth_interpolation", interpolation, {}, [&](const auto& image) { const float t = 20.5f / 40; Pixel(image, 20, 30, {1 - t, 0, t, 0.2f + 0.6f * t}, "smooth_interpolation"); });
			ScreenDrawList scissors; scissors.SetBlend(disabled); const ScreenColor opaque{0.6f, 0.2f, 0.8f, 1};
			for (ScreenScissor scissor: {ScreenScissor{-5, -6, 10, 12}, ScreenScissor{100, 10, 5, 5}, ScreenScissor{-100, -100, 10, 10}, ScreenScissor{10, 10, 0, 10}}) {
				scissors.SetScissor(scissor); Quad(scissors, 0, 0, 64, 64, opaque, opaque);
			}
			scissors.SetScissor(std::nullopt); Quad(scissors, 20, 20, 30, 30, opaque, opaque);
			controlled("signed_empty_scissors", scissors, {}, [&](const auto& image) {
				Pixel(image, 2, 2, opaque, "negative-scissor"); Pixel(image, 5, 2, {}, "right-exclusive"); Pixel(image, 2, 6, {}, "top-exclusive");
				Pixel(image, 2, 61, {}, "bottom-left-origin"); Pixel(image, 25, 25, opaque, "disabled-scissor-after-empty"); Pixel(image, 15, 15, {}, "empty-scissors");
			});
			ScreenDrawList unsupportedZ; unsupportedZ.BeginQuads();
			unsupportedZ.Vertex(0, 0, 1); unsupportedZ.Vertex(10, 0, 1); unsupportedZ.Vertex(10, 10, 1); unsupportedZ.Vertex(0, 10, 1); unsupportedZ.End();
			bool zRejected = false;
			try { ScreenRenderRequest request; request.drawList = &unsupportedZ; (void)RenderMetal4Screen(request); }
			catch (const std::runtime_error& error) { zRejected = std::string(error.what()).find("z==0") != std::string::npos; }
			Check(zRejected, "Screen renderer silently accepted unsupported z projection");
			ScreenDrawList unfinished; unfinished.BeginQuads();
			bool recordingRejected = false;
			try { ScreenRenderRequest request; request.drawList = &unfinished; (void)RenderMetal4Screen(request); }
			catch (const std::runtime_error& error) { recordingRejected = std::string(error.what()).find("completed immutable") != std::string::npos; }
			Check(recordingRejected, "Screen renderer accepted an unfinished mutable batch");
			report[@"status"] = @"passed"; report[@"checks_passed"] = @(checks); report[@"fixture"] = String(fixture.string());
			report[@"pipeline_shader_validation"] = @(validation); report[@"runtime"] = @"Same serialized native context for all fixture/control renders; event values monotonic";
			report[@"unsupported_rejected"] = @[@"nonzero z projection", @"unfinished mutable batch"];
			SaveJSON(report, reportPath); std::cout << "Screen GPU analytic checks passed: " << checks << '\n'; return 0;
		} catch (const std::exception& error) {
			report[@"status"] = @"failed"; report[@"error"] = String(error.what());
			if (!reportPath.empty()) { try { SaveJSON(report, reportPath); } catch (...) {} }
			std::cerr << error.what() << '\n'; return 1;
		}
	}
}
