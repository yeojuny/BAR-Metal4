/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4ScreenRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using namespace rendering;
constexpr std::uint32_t WIDTH = 32;
constexpr std::uint32_t HEIGHT = 32;
constexpr int TOLERANCE = 2; // RGBA8 conversion/rounding, not a reference-renderer tolerance.
std::uint32_t checks = 0;
std::uint32_t renders = 0;
std::string queueClass;
std::string commandClass;
std::string encoderClass;
std::string deviceName;
void Check(bool value, const std::string& message);

template<typename F>
void Throws(F&& action, const std::string& label)
{
	bool rejected = false;
	try { action(); } catch (const std::exception&) { rejected = true; }
	Check(rejected, label);
}

void Check(bool value, const std::string& message)
{
	++checks;
	if (!value) throw std::runtime_error(message);
}

int Byte(float value)
{
	return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

float Quantized(float value)
{
	return static_cast<float>(Byte(value)) / 255.0f;
}

void Pixel(const ScreenRenderResult& image, std::uint32_t x, std::uint32_t y,
	ScreenColor expected, const std::string& label)
{
	Check(image.width == WIDTH && image.height == HEIGHT, label + ": exact render extent");
	Check(image.rgba.size() == static_cast<std::size_t>(WIDTH) * HEIGHT * 4, label + ": exact RGBA extent");
	const std::size_t offset = (static_cast<std::size_t>(HEIGHT - 1 - y) * WIDTH + x) * 4;
	for (std::size_t c = 0; c < 4; ++c) {
		const int actual = image.rgba[offset + c];
		const int wanted = Byte(expected[c]);
		Check(std::abs(actual - wanted) <= TOLERANCE, label + ": channel " + std::to_string(c)
			+ " expected " + std::to_string(wanted) + " actual " + std::to_string(actual));
	}
}

void EveryPixel(const ScreenRenderResult& image, ScreenColor expected, const std::string& label)
{
	for (std::uint32_t y = 0; y < HEIGHT; ++y)
		for (std::uint32_t x = 0; x < WIDTH; ++x)
			Pixel(image, x, y, expected, label);
}

std::shared_ptr<ScreenTexture> Image(ScreenTextureFormat format, std::uint32_t width,
	std::uint32_t height, std::vector<std::uint8_t> bytes)
{
	auto result = std::make_shared<ScreenTexture>();
	result->sourceName = "bounded original-UI GPU contract fixture";
	result->generation = renders + 1;
	result->format = format;
	result->width = width;
	result->height = height;
	result->bytesPerRow = static_cast<std::size_t>(width) * (format == ScreenTextureFormat::R8Unorm ? 1 : 4);
	result->bytes = std::move(bytes);
	return result;
}

void Quad(ScreenDrawList& list, ScreenColor color, float u0 = 0, float v0 = 0,
	float u1 = 1, float v1 = 1)
{
	list.Color(color);
	list.BeginQuads();
	list.TexCoord(u0, v0); list.Vertex(0, 0);
	list.TexCoord(u1, v0); list.Vertex(static_cast<float>(WIDTH), 0);
	list.TexCoord(u1, v1); list.Vertex(static_cast<float>(WIDTH), static_cast<float>(HEIGHT));
	list.TexCoord(u0, v1); list.Vertex(0, static_cast<float>(HEIGHT));
	list.End();
}

ScreenRenderResult Render(const ScreenDrawList& list, ScreenColor clear = {0, 0, 0, 0})
{
	ScreenRenderRequest request;
	request.drawList = &list;
	request.width = WIDTH;
	request.height = HEIGHT;
	request.clearColor = clear;
	request.measuredFrames = 1;
	request.shaderValidation = true;
	auto result = RenderMetal4Screen(request);
	++renders;
	Check(result.capabilities.directMetal4, "actual device supports Metal4");
	Check(result.frames.size() == 2, "one warmup and one measured frame");
	Check(!result.frames.back().cpuSimulationMs, "fixture contains no simulation");
	queueClass = result.queueClass;
	commandClass = result.commandBufferClass;
	encoderClass = result.renderEncoderClass;
	deviceName = result.capabilities.deviceName;
	return result;
}

ScreenBlendState DisabledBlend()
{
	ScreenBlendState result;
	result.enabled = false;
	return result;
}

void TextureOrientationAndModulation()
{
	const auto image = Image(ScreenTextureFormat::RGBA8Unorm, 2, 2,
		{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255});
	ScreenDrawList list;
	list.SetBlend(DisabledBlend());
	list.SetTexture(image, ScreenMaterial::RGBA, ScreenUVMode::Normalized,
		{ScreenFilter::Nearest, ScreenWrap::ClampEdge, ScreenWrap::ClampEdge});
	Quad(list, {1, 1, 1, 1});
	const auto rendered = Render(list);
	Pixel(rendered, 4, 4, {1, 0, 0, 1}, "RGBA row0 bottom left");
	Pixel(rendered, 26, 4, {0, 1, 0, 1}, "RGBA row0 bottom right");
	Pixel(rendered, 4, 26, {0, 0, 1, 1}, "RGBA row1 top left");
	Pixel(rendered, 26, 26, {1, 1, 0, 1}, "RGBA row1 top right");

	// A new immutable generation is published; no retained snapshot is mutated.
	auto flipped = std::make_shared<ScreenTexture>(*image);
	flipped->generation += 100;
	flipped->flipV = true;
	list.ClearGeometry();
	list.SetTexture(flipped, ScreenMaterial::RGBA, ScreenUVMode::Normalized,
		{ScreenFilter::Nearest, ScreenWrap::ClampEdge, ScreenWrap::ClampEdge});
	Quad(list, {0.5f, 0.25f, 1, 0.8f});
	const auto modulated = Render(list);
	Pixel(modulated, 4, 4, {0, 0, 1, 0.8f}, "explicit flipV with RGB and alpha modulation");
	Pixel(modulated, 26, 4, {0.5f, 0.25f, 0, 0.8f}, "RGBA componentwise multiplication");
	Pixel(modulated, 4, 26, {0.5f, 0, 0, 0.8f}, "explicit flipV top row");

	ScreenDrawList bgra;
	bgra.SetBlend(DisabledBlend());
	bgra.SetTexture(Image(ScreenTextureFormat::BGRA8Unorm, 1, 1, {32, 64, 192, 255}));
	Quad(bgra, {1, 1, 1, 1});
	Pixel(Render(bgra), 8, 8, {192.0f / 255, 64.0f / 255, 32.0f / 255, 1}, "actual BGRA atlas format");
}

void FontCoverageAndAlpha()
{
	const ScreenColor source{0.8f, 0.4f, 0.2f, 0.6f};
	const ScreenColor clear{0.1f, 0.2f, 0.3f, 0.4f};
	ScreenDrawList font;
	font.SetTexture(Image(ScreenTextureFormat::R8Unorm, 1, 1, {128}), ScreenMaterial::FontCoverage,
		ScreenUVMode::Texel, {ScreenFilter::Nearest, ScreenWrap::ClampEdge, ScreenWrap::ClampEdge});
	Quad(font, source);
	const float alpha = source[3] * (128.0f / 255);
	ScreenColor expected{};
	for (std::size_t c = 0; c < 3; ++c)
		expected[c] = source[c] * alpha + Quantized(clear[c]) * (1 - alpha);
	expected[3] = alpha * alpha + Quantized(clear[3]) * (1 - alpha);
	Pixel(Render(font, clear), 9, 9, expected, "original font R8 coverage only changes alpha before blend");

	ScreenDrawList threshold;
	threshold.SetBlend(DisabledBlend());
	threshold.SetTexture(Image(ScreenTextureFormat::R8Unorm, 2, 1, {127, 128}), ScreenMaterial::FontCoverage,
		ScreenUVMode::Texel, {ScreenFilter::Nearest, ScreenWrap::ClampEdge, ScreenWrap::ClampEdge});
	threshold.SetAlphaTest({true, ScreenAlphaCompare::Greater, 0.5f});
	Quad(threshold, {0.7f, 0.3f, 0.1f, 1}, 0, 0, 2, 1);
	const auto tested = Render(threshold, clear);
	Pixel(tested, 4, 4, clear, "alpha test discards coverage127");
	Pixel(tested, 26, 4, {0.7f, 0.3f, 0.1f, 128.0f / 255}, "alpha test retains coverage128 and raw font RGB");

	ScreenDrawList border;
	border.SetTexture(Image(ScreenTextureFormat::R8Unorm, 1, 1, {255}), ScreenMaterial::FontCoverage,
		ScreenUVMode::Normalized, {ScreenFilter::Nearest, ScreenWrap::TransparentBorder, ScreenWrap::TransparentBorder});
	Quad(border, source, -1, -1, -1, -1);
	EveryPixel(Render(border, clear), clear, "transparent font atlas border");
}

void BlendAndScissors()
{
	const ScreenColor source{0.1f, 0.2f, 0.05f, 0.25f};
	const ScreenColor clear{0.2f, 0.1f, 0.3f, 0.4f};
	ScreenDrawList mixed;
	ScreenBlendState blend;
	blend.sourceColor = ScreenBlendFactor::One;
	blend.destinationColor = ScreenBlendFactor::OneMinusSourceAlpha;
	blend.sourceAlpha = ScreenBlendFactor::SourceAlpha;
	blend.destinationAlpha = ScreenBlendFactor::Zero;
	mixed.SetBlend(blend);
	Quad(mixed, source);
	ScreenColor expected{};
	for (std::size_t c = 0; c < 3; ++c)
		expected[c] = source[c] + Quantized(clear[c]) * (1 - source[3]);
	expected[3] = source[3] * source[3];
	Pixel(Render(mixed, clear), 13, 9, expected, "independent color and alpha blend factors");

	ScreenDrawList clipped;
	clipped.SetBlend(DisabledBlend());
	clipped.SetScissor(ScreenScissor{-4, 6, 12, 12});
	Quad(clipped, source);
	const auto scissors = Render(clipped, clear);
	Pixel(scissors, 4, 10, source, "signed bottom-left scissor clipped interior");
	Pixel(scissors, 12, 10, clear, "signed scissor right boundary");
	Pixel(scissors, 4, 4, clear, "signed scissor bottom boundary");
	Pixel(scissors, 4, 20, clear, "signed scissor top boundary");
	clipped.ClearGeometry();
	clipped.SetScissor(ScreenScissor{0, 0, 0, HEIGHT});
	Quad(clipped, source);
	EveryPixel(Render(clipped, clear), clear, "zero-width scissor skips draw");
	clipped.ClearGeometry();
	clipped.SetScissor(ScreenScissor{-100, -100, 4, 4});
	Quad(clipped, source);
	EveryPixel(Render(clipped, clear), clear, "offscreen scissor skips draw");
}

std::uint32_t Read32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
	Check(offset <= bytes.size() && bytes.size() - offset >= 4, "fixture DDS field extent");
	return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
		| (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}

ScreenColor DecodeBCPixel(const ScreenTextureMip& mip, ScreenTextureFormat format,
	std::uint32_t x, std::uint32_t y)
{
	// Independent BC format arithmetic used only as this fixture's pixel oracle.
	// The production renderer uploads the supplied compressed bytes unchanged.
	const std::size_t blockBytes = format == ScreenTextureFormat::BC1RGBA ? 8 : 16;
	const std::size_t offset = std::size_t(y / 4) * mip.bytesPerRow + std::size_t(x / 4) * blockBytes;
	const auto* block = mip.bytes.data() + offset;
	const auto* colors = block + (blockBytes == 8 ? 0 : 8);
	const auto endpoint = [&](std::size_t pos) {
		const std::uint16_t bits = std::uint16_t(std::uint16_t(colors[pos]) | (std::uint16_t(colors[pos + 1]) << 8));
		return ScreenColor{float((bits >> 11) & 31) / 31, float((bits >> 5) & 63) / 63, float(bits & 31) / 31, 1};
	};
	std::array<ScreenColor, 4> palette{endpoint(0), endpoint(2), {}, {}};
	const std::uint16_t first = std::uint16_t(std::uint16_t(colors[0]) | (std::uint16_t(colors[1]) << 8));
	const std::uint16_t second = std::uint16_t(std::uint16_t(colors[2]) | (std::uint16_t(colors[3]) << 8));
	for (std::size_t c = 0; c < 3; ++c) {
		if (format != ScreenTextureFormat::BC1RGBA || first > second) {
			palette[2][c] = (2 * palette[0][c] + palette[1][c]) / 3;
			palette[3][c] = (palette[0][c] + 2 * palette[1][c]) / 3;
		} else {
			palette[2][c] = (palette[0][c] + palette[1][c]) / 2;
			palette[3][c] = 0;
		}
	}
	palette[2][3] = 1;
	palette[3][3] = format == ScreenTextureFormat::BC1RGBA && first <= second ? 0 : 1;
	const unsigned local = (y % 4) * 4 + x % 4;
	const unsigned code = (colors[4 + local / 4] >> ((local % 4) * 2)) & 3;
	auto result = palette[code];
	if (format == ScreenTextureFormat::BC2RGBA) {
		result[3] = float((block[local / 2] >> ((local % 2) * 4)) & 15) / 15;
	} else if (format == ScreenTextureFormat::BC3RGBA) {
		std::uint64_t selectors = 0;
		for (unsigned i = 0; i < 6; ++i) selectors |= std::uint64_t(block[2 + i]) << (8 * i);
		const unsigned index = unsigned((selectors >> (3 * local)) & 7);
		float alpha;
		if (index < 2) alpha = block[index];
		else if (block[0] > block[1]) alpha = (float(8 - index) * block[0] + float(index - 1) * block[1]) / 7;
		else if (index < 6) alpha = (float(6 - index) * block[0] + float(index - 1) * block[1]) / 5;
		else alpha = index == 6 ? 0 : 255;
		result[3] = alpha / 255;
	}
	return result;
}

std::shared_ptr<ScreenTexture> SolidBC(ScreenTextureFormat format, std::uint32_t side,
	const std::vector<std::uint16_t>& levels, std::uint8_t alpha)
{
	auto result = std::make_shared<ScreenTexture>();
	result->format = format;
	result->sourceName = "independent BC block and mip-selection fixture";
	result->width = result->height = side;
	for (std::uint16_t color : levels) {
		const std::size_t blockBytes = format == ScreenTextureFormat::BC1RGBA ? 8 : 16;
		const std::size_t blocks = (std::size_t(side) + 3) / 4;
		ScreenTextureMip mip{side, side, blocks * blockBytes, std::vector<std::uint8_t>(blocks * blocks * blockBytes, 0)};
		for (std::size_t offset = 0; offset < mip.bytes.size(); offset += blockBytes) {
			const std::size_t colorOffset = offset + (blockBytes == 8 ? 0 : 8);
			mip.bytes[colorOffset] = std::uint8_t(color & 255);
			mip.bytes[colorOffset + 1] = std::uint8_t(color >> 8);
			if (format == ScreenTextureFormat::BC3RGBA) mip.bytes[offset] = alpha;
			if (format == ScreenTextureFormat::BC2RGBA)
				for (std::size_t i = 0; i < 8; ++i) mip.bytes[offset + i] = std::uint8_t((alpha & 15) * 17);
		}
		result->mips.push_back(std::move(mip));
		side = std::max(1u, side / 2);
	}
	return result;
}

void BCFormatsAndMips()
{
	for (ScreenTextureFormat format : {ScreenTextureFormat::BC1RGBA, ScreenTextureFormat::BC2RGBA, ScreenTextureFormat::BC3RGBA}) {
		ScreenDrawList list;
		list.SetBlend(DisabledBlend());
		list.SetTexture(SolidBC(format, 4, {0xf800}, format == ScreenTextureFormat::BC2RGBA ? 8 : 173),
			ScreenMaterial::RGBA, ScreenUVMode::Normalized,
			{ScreenFilter::Nearest, ScreenWrap::ClampEdge, ScreenWrap::ClampEdge, ScreenMipFilter::None});
		Quad(list, {1, 1, 1, 1});
		const float alpha = format == ScreenTextureFormat::BC1RGBA ? 1 : (format == ScreenTextureFormat::BC2RGBA ? 8.0f / 15 : 173.0f / 255);
		EveryPixel(Render(list), {1, 0, 0, alpha}, "native BC1/2/3 format and original alpha");
	}
	const auto chain = SolidBC(ScreenTextureFormat::BC3RGBA, 32, {0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f, 0x07ff}, 255);
	const std::array<ScreenColor, 6> expected{{{1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1}, {1,0,1,1}, {0,1,1,1}}};
	for (std::size_t level = 0; level < chain->mips.size(); ++level) {
		ScreenDrawList list;
		list.SetBlend(DisabledBlend());
		list.SetTexture(chain, ScreenMaterial::RGBA, ScreenUVMode::Normalized,
			{ScreenFilter::Nearest, ScreenWrap::Repeat, ScreenWrap::Repeat, ScreenMipFilter::Nearest});
		const float scale = float(std::uint32_t(1) << level);
		Quad(list, {1,1,1,1}, 0, 0, scale, scale);
		EveryPixel(Render(list), expected[level], "actual minification selects every original mip including2x2and1x1");
	}
	ScreenDrawList invalid;
	invalid.SetBlend(DisabledBlend());
	auto bad = std::make_shared<ScreenTexture>(*chain);
	bad->mips[1].height = 17;
	invalid.SetTexture(bad); Quad(invalid, {1,1,1,1});
	Throws([&] { Render(invalid); }, "mismatched original mip dimensions rejected before recording");
	bad = std::make_shared<ScreenTexture>(*chain); bad->mips.back().bytes.clear();
	invalid.ClearGeometry(); invalid.SetTexture(bad); Quad(invalid, {1,1,1,1});
	Throws([&] { Render(invalid); }, "truncated original mip rejected before recording");
	bad = std::make_shared<ScreenTexture>(*chain); bad->bytes = {1};
	invalid.ClearGeometry(); invalid.SetTexture(bad); Quad(invalid, {1,1,1,1});
	Throws([&] { Render(invalid); }, "competing legacy and original mip descriptions rejected");
}

std::shared_ptr<ScreenTexture> LoadFixtureDDS(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	Check(bool(file), "original DDS fixture is readable");
	const auto length = file.tellg();
	Check(length >= 128 && length <= 32 * 1024 * 1024, "bounded original DDS fixture length");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
	file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	Check(bool(file), "original DDS fixture read completed");
	Check(Read32(bytes, 0) == 0x20534444 && Read32(bytes, 4) == 124 && Read32(bytes, 76) == 32,
		"original fixture DDS header");
	Check(Read32(bytes, 84) == 0x35545844, "original Ready fixture retains DXT5/BC3");
	auto result = std::make_shared<ScreenTexture>();
	result->sourceName = path; result->format = ScreenTextureFormat::BC3RGBA;
	result->width = Read32(bytes, 16); result->height = Read32(bytes, 12);
	const auto count = std::max(1u, Read32(bytes, 28));
	std::uint32_t width = result->width, height = result->height;
	std::size_t offset = 128;
	for (std::uint32_t i = 0; i < count; ++i) {
		const std::size_t pitch = ((std::size_t(width) + 3) / 4) * 16;
		const std::size_t size = pitch * ((std::size_t(height) + 3) / 4);
		Check(offset <= bytes.size() && size <= bytes.size() - offset, "original DDS mip payload extent");
		result->mips.push_back({width, height, pitch, {bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + size)}});
		offset += size; width = std::max(1u, width / 2); height = std::max(1u, height / 2);
	}
	Check(offset == bytes.size(), "no discarded original DDS data or mips");
	return result;
}

std::shared_ptr<ScreenTexture> OriginalFlippedBC3(const ScreenTexture& raw)
{
	// Fixture-only implementation of the original nv_dds full-block Y flip.
	// Expected pixels below are read independently from unflipped source bytes.
	auto result = std::make_shared<ScreenTexture>(raw);
	for (auto& mip : result->mips) {
		const auto source = mip.bytes;
		const std::uint32_t rows = mip.height / 4, columns = mip.width / 4;
		for (std::uint32_t y = 0; y < rows; ++y) for (std::uint32_t x = 0; x < columns; ++x) {
			const std::size_t target = std::size_t(y) * mip.bytesPerRow + std::size_t(x) * 16;
			const std::size_t from = std::size_t(rows - 1 - y) * mip.bytesPerRow + std::size_t(x) * 16;
			std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(from), 16, mip.bytes.begin() + static_cast<std::ptrdiff_t>(target));
			std::uint64_t bits = 0, flipped = 0;
			for (unsigned i = 0; i < 6; ++i) bits |= std::uint64_t(source[from + 2 + i]) << (8 * i);
			for (unsigned pixel = 0; pixel < 16; ++pixel)
				flipped |= ((bits >> (pixel * 3)) & 7) << (((3 - pixel / 4) * 4 + pixel % 4) * 3);
			for (unsigned i = 0; i < 6; ++i) mip.bytes[target + 2 + i] = std::uint8_t((flipped >> (8 * i)) & 255);
			for (unsigned row = 0; row < 4; ++row) mip.bytes[target + 12 + row] = source[from + 15 - row];
		}
	}
	return result;
}

ScreenColor OriginalSample(const ScreenTextureMip& mip, float u, float v, bool linear)
{
	const auto sample = [&](int x, int y) {
		x = (x % int(mip.width) + int(mip.width)) % int(mip.width);
		y = (y % int(mip.height) + int(mip.height)) % int(mip.height);
		return DecodeBCPixel(mip, ScreenTextureFormat::BC3RGBA, std::uint32_t(x), mip.height - 1 - std::uint32_t(y));
	};
	if (!linear) return sample(int(std::floor(u * float(mip.width))), int(std::floor(v * float(mip.height))));
	const float x = u * float(mip.width) - 0.5f, y = v * float(mip.height) - 0.5f;
	const int left = int(std::floor(x)), bottom = int(std::floor(y));
	const float sx = x - float(left), sy = y - float(bottom);
	const auto a = sample(left, bottom), b = sample(left + 1, bottom), c = sample(left, bottom + 1), d = sample(left + 1, bottom + 1);
	ScreenColor result{};
	for (std::size_t i = 0; i < 4; ++i) result[i] = (a[i] * (1 - sx) + b[i] * sx) * (1 - sy) + (c[i] * (1 - sx) + d[i] * sx) * sy;
	return result;
}

void ActualReadyDDS(const char* path, std::uint32_t expectedSide, bool linear)
{
	const auto raw = LoadFixtureDDS(path);
	Check(raw->width == expectedSide && raw->height == expectedSide && raw->mips.size() == 1,
		"exact original Ready DDS dimensions and full source mip count");
	const auto source = OriginalFlippedBC3(*raw);
	ScreenDrawList list;
	const ScreenColor clear{0.1f,0.2f,0.3f,0.4f};
	const ScreenColor modulation = linear ? ScreenColor{0.4f,0.6f,0.8f,0.7f} : ScreenColor{1,1,1,1};
	if (!linear) list.SetBlend(DisabledBlend());
	list.SetTexture(source, ScreenMaterial::RGBA, ScreenUVMode::Normalized,
		{linear ? ScreenFilter::Linear : ScreenFilter::Nearest, ScreenWrap::Repeat, ScreenWrap::Repeat,
			linear ? ScreenMipFilter::Linear : ScreenMipFilter::None});
	Quad(list, modulation);
	const auto image = Render(list, clear);
	for (std::uint32_t y = 0; y < HEIGHT; ++y) for (std::uint32_t x = 0; x < WIDTH; ++x) {
		auto expected = OriginalSample(raw->mips[0], (float(x) + 0.5f) / WIDTH, (float(y) + 0.5f) / HEIGHT, linear);
		for (std::size_t c = 0; c < 4; ++c) expected[c] *= modulation[c];
		if (linear) {
			const float alpha = expected[3];
			for (std::size_t c = 0; c < 3; ++c) expected[c] = expected[c] * alpha + Quantized(clear[c]) * (1 - alpha);
			expected[3] = alpha * alpha + Quantized(clear[3]) * (1 - alpha);
		}
		Pixel(image, x, y, expected, "actual original Ready DDS bytes orientation filtering modulation and alpha");
	}
}
} // namespace

int main(int argc, char** argv)
{
	try {
		TextureOrientationAndModulation();
			FontCoverageAndAlpha();
			BlendAndScissors();
			BCFormatsAndMips();
			Check(argc == 1 || argc == 3, "optional original indicator and glow DDS fixture paths");
			if (argc == 3) {
				ActualReadyDDS(argv[1], 48, false);
				ActualReadyDDS(argv[2], 32, true);
			}
		std::cout << "{\"status\":\"passed\",\"checks\":" << checks << ",\"renders\":" << renders
			<< ",\"rgba8Tolerance\":" << TOLERANCE << ",\"device\":\"" << deviceName
			<< "\",\"queueClass\":\"" << queueClass << "\",\"commandBufferClass\":\"" << commandClass
			<< "\",\"renderEncoderClass\":\"" << encoderClass
			<< "\",\"shaderValidation\":true,\"simulation\":false,\"originalWidgetAcceptance\":false}\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "FAIL after " << renders << " renders and " << checks << " checks: " << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
