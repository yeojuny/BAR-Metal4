/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/WorldFrameData.h"
#include "lib/squish/squish.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Upstream decoder is retained verbatim with its MIT license and commit pin.
// Static implementation keeps this upload-only codec out of engine ABI.
#define BCDEC_STATIC
#define BCDEC_BC4BC5_PRECISE
#define BCDEC_IMPLEMENTATION
#include "lib/bcdec/bcdec.h"
#undef BCDEC_IMPLEMENTATION
#undef BCDEC_BC4BC5_PRECISE
#undef BCDEC_STATIC
#undef BCDECDEF
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>

namespace rendering::metal4 {

// Decode the already-oriented original base into upload-only RGBA8 storage.
// Do not rewrite the immutable WorldTexture, flip rows, change alpha, or create
// CPU mip levels. The existing GPU producer still owns every generated level.
inline WorldTextureMip DecodeWorldBCBase(uint32_t imageWidth, uint32_t imageHeight,
	size_t bytesPerRow, std::span<const uint8_t> bytes,
	WorldTextureFormat format, uint32_t maxDimension)
{
	int flags = 0;
	size_t blockBytes = 16;
	switch (format) {
		case WorldTextureFormat::BC1RGBA: flags = squish::kDxt1; blockBytes = 8; break;
		case WorldTextureFormat::BC2RGBA: flags = squish::kDxt3; break;
		case WorldTextureFormat::BC3RGBA: flags = squish::kDxt5; break;
		case WorldTextureFormat::BC4Unorm: blockBytes = 8; break;
		case WorldTextureFormat::BC5Unorm:
		case WorldTextureFormat::BC7RGBA:
		case WorldTextureFormat::BC7RGBASRGB: break;
		default: throw std::invalid_argument("World BC base decode requires original compressed bytes");
	}
	const size_t width = imageWidth, height = imageHeight;
	if (!width || !height || width > maxDimension || height > maxDimension ||
		// Squish uses int byte offsets inside a strip of at most four rows.
		width > size_t(std::numeric_limits<int>::max() / 16) ||
		width > std::numeric_limits<size_t>::max() / 4 / height ||
		width > (128 * 1024) / 4)
		throw std::invalid_argument("World BC base extent exceeds device, decoder or upload bounds");
	const size_t blockColumns = (width + 3) / 4;
	const size_t blockRows = (height + 3) / 4;
	const size_t minimumRowBytes = blockColumns * blockBytes;
	if (bytesPerRow < minimumRowBytes || bytesPerRow > 128 * 1024 ||
		bytesPerRow > std::numeric_limits<size_t>::max() / blockRows ||
		bytes.size() < bytesPerRow * blockRows)
		throw std::invalid_argument("World BC base compressed row or payload is incomplete");
	WorldTextureMip decoded;
	decoded.width = imageWidth;
	decoded.height = imageHeight;
	decoded.bytesPerRow = width * 4;
	const size_t byteCount = decoded.bytesPerRow * height;
	if (byteCount > decoded.bytes.max_size())
		throw std::invalid_argument("World BC base decoded allocation exceeds addressable storage");
	decoded.bytes.resize(byteCount);
	for (size_t blockRow = 0; blockRow < blockRows; ++blockRow) {
		const size_t y = blockRow * 4;
		if (flags) {
			// Preserve the existing BC1/2/3 decoder and rounding.
			squish::DecompressImage(decoded.bytes.data() + y * decoded.bytesPerRow,
				int(width), int(std::min(size_t(4), height - y)),
				bytes.data() + blockRow * bytesPerRow, flags);
			continue;
		}
		for (size_t column = 0; column < blockColumns; ++column) {
			const auto* block = bytes.data() + blockRow * bytesPerRow + column * blockBytes;
			alignas(8) uint8_t rgba[64]{};
			if (format == WorldTextureFormat::BC4Unorm || format == WorldTextureFormat::BC5Unorm) {
				uint8_t channels[32]{};
				const size_t count = format == WorldTextureFormat::BC4Unorm ? 1 : 2;
				if (count == 1) bcdec_bc4(block, channels, 4, 0);
				else bcdec_bc5(block, channels, 8, 0);
				for (size_t pixel = 0; pixel < 16; ++pixel) {
					rgba[pixel * 4] = channels[pixel * count];
					rgba[pixel * 4 + 1] = count == 2 ? channels[pixel * count + 1] : 0;
					rgba[pixel * 4 + 3] = 255;
				}
			} else {
				// BC7 channels remain encoded bytes. sRGB transfer belongs to
				// the destination texture format, never to this decompressor.
				bcdec_bc7(block, rgba, 16);
			}
			const size_t x = column * 4;
			for (size_t row = 0; row < std::min(size_t(4), height - y); ++row)
				std::memcpy(decoded.bytes.data() + (y + row) * decoded.bytesPerRow + x * 4,
					rgba + row * 16, std::min(size_t(4), width - x) * 4);
		}
	}
	return decoded;
}

// Keep model callers on the same validation and decoder without copying the
// original compressed payload. Screen uploads borrow their immutable bytes too.
inline WorldTextureMip DecodeWorldBCBase(const WorldTextureMip& mip,
	WorldTextureFormat format, uint32_t maxDimension)
{
	return DecodeWorldBCBase(mip.width, mip.height, mip.bytesPerRow,
		std::span<const uint8_t>(mip.bytes), format, maxDimension);
}

// generateMipmaps is the retained original upload request, independent of a
// later sampler bind. A present embedded chain must never be regenerated here.
inline std::optional<WorldTextureMip> DecodeWorldBCBaseForMipmaps(
	const WorldTexture& source, uint32_t maxDimension)
{
	if (source.mips.empty())
		throw std::invalid_argument("World BC mip transport has no original base");
	if (!source.generateMipmaps || source.format == WorldTextureFormat::RGBA8Unorm ||
		source.format == WorldTextureFormat::BGRA8Unorm)
		return std::nullopt;
	if (source.mips.size() != 1)
		throw std::invalid_argument("World BC mip generation would overwrite an original embedded chain");
	const auto& base = source.mips.front();
	if (!base.width || !base.height || base.width > maxDimension || base.height > maxDimension)
		throw std::invalid_argument("World BC mip transport extent exceeds actual device bounds");
	if (base.width == 1 && base.height == 1)
		return std::nullopt; // No generated levels; retain actual BC storage.
	// The upload consumer preserves sRGB storage for BC7 sRGB. Generated
	// levels are still produced by the original ordered GPU mip operation.
	return DecodeWorldBCBase(base, source.format, maxDimension);
}

} // namespace rendering::metal4
