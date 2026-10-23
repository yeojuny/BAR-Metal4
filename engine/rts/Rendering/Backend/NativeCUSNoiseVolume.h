/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rendering {
// The exact CUS noise64 asset is a single-level, uncompressed volume. Pinned
// nv_dds interprets its bytes as GL_BGRA despite the file's RGBA masks. Each Z
// slice is flipped vertically by the original loader; Z ordering is unchanged.
// Original NamedTextures/CBitmap generates the complete seven-level GPU chain
// from this base; the selected CUS noise reads explicitly sample level0.
struct NativeCUSNoiseVolume {
	std::string sourcePath;
	uint32_t width = 0, height = 0, depth = 0;
	size_t bytesPerRow = 0, bytesPerImage = 0;
	std::vector<uint8_t> bgraBytes;
};
// Bounded reader for this actual original family, not a general DDS decoder.
// Unsupported dimensions/mips/formats fail before allocation or publication.
NativeCUSNoiseVolume DecodeNativeCUSNoiseDDS(std::span<const uint8_t> bytes, std::string sourcePath);
} // namespace rendering
