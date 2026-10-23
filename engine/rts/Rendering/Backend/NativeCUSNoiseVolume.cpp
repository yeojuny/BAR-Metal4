/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSNoiseVolume.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace rendering {
NativeCUSNoiseVolume DecodeNativeCUSNoiseDDS(std::span<const uint8_t> bytes, std::string sourcePath)
{
	auto require = [&](bool valid, const char* reason) {
		if (!valid) throw std::runtime_error(std::string("Original CUS noise volume ") + reason + ": " + sourcePath);
	};
	require(bytes.size() >= 128, "DDS header is truncated");
	auto word = [&](size_t offset) {
		return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
			(uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
	};
	require(word(0) == 0x20534444u && word(4) == 124 && word(76) == 32, "DDS signature/header is invalid");
	// This source-verified shader family requests the actual 64^3 noise DDS.
	// Larger/different resources require their own supported upload contract.
	require(word(12) == 64 && word(16) == 64 && word(24) == 64, "requires its original 64x64x64 extent");
	require((word(112) & 0x200000u) && !(word(112) & 0xfe00u), "requires a volume, not cube/flat storage");
	require(word(28) <= 1, "mip chain is unsupported; source contains one level");
	require(word(80) == 0x41 && word(84) == 0 && word(88) == 32, "requires the original uncompressed 32-bit storage");
	require(word(92) == 0xff && word(96) == 0xff00 && word(100) == 0xff0000 && word(104) == 0xff000000u,
		"channel masks differ from the original noise asset");
	constexpr size_t row = 64 * 4, image = row * 64, payload = image * 64;
	require(word(20) == 0 || word(20) == row, "row pitch contradicts the original tight payload");
	require(bytes.size() == 128 + payload, "payload extent differs from the validated dimensions");
	NativeCUSNoiseVolume out;
	out.sourcePath = std::move(sourcePath);
	out.width = out.height = out.depth = 64;
	out.bytesPerRow = row;
	out.bytesPerImage = image;
	out.bgraBytes.resize(payload);
	for (size_t z = 0; z < 64; ++z) {
		for (size_t y = 0; y < 64; ++y) {
			std::copy_n(bytes.data() + 128 + z * image + (63 - y) * row, row,
				out.bgraBytes.data() + z * image + y * row);
		}
	}
	return out;
}
} // namespace rendering
