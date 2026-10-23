/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4WorldBCDecode.h"
#include <utility>
namespace rendering::metal4 {
// Decode already-oriented source faces; original GPU producer owns mipmaps.
inline void DecodeSkyCubeBase(const WorldTextureMip& mip, WorldTextureFormat format,
	uint32_t maxDimension, std::vector<uint8_t>& rgba)
{
	if (mip.width != mip.height)
		throw std::invalid_argument("Sky cube base is not square");
	auto decoded = DecodeWorldBCBase(mip, format, maxDimension);
	rgba = std::move(decoded.bytes);
}
} // namespace rendering::metal4
