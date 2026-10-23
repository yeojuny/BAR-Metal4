/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace rendering::ui {

enum class NativeDDSFormat { BC1, BC2, BC3, BGRA8, BGR8, BC4, BC5, BC7, BC7SRGB };
struct NativeDDSMip {
	std::uint32_t width, height;
	std::size_t offset, bytesPerRow, byteCount;
};
struct NativeDDSLayout {
	NativeDDSFormat format;
	std::vector<NativeDDSMip> mips; // Includes base level; original file order.
	std::uint32_t bitsPerPixel = 0; // Zero for block compression.
	std::array<std::uint32_t, 4> channelMasks{}; // Original R/G/B/A masks.
};

// Validate the whole immutable VFS file before nv_dds performs allocations.
// Flat BGR/BGRA layouts deliberately match nv_dds::size_rgb and its GL_BGR /
// GL_BGRA upload interpretation. The original loader does not remove row
// padding or interpret arbitrary channel masks, so do not silently accept them.
inline NativeDDSLayout ValidateNativeUIDDS(std::span<const std::uint8_t> bytes)
{
	const auto require = [](bool value, const char* reason) {
		if (!value) throw std::runtime_error(reason);
	};
	require(bytes.size() >= 128 && bytes.size() <= 384ull * 1024 * 1024, "Native UI DDS file size outside bounds");
	const auto u32 = [&](std::size_t offset) -> std::uint32_t {
		require(offset <= bytes.size() && bytes.size()-offset >= 4, "Native UI DDS truncated header");
		return std::uint32_t(bytes[offset]) | std::uint32_t(bytes[offset+1]) << 8 |
			std::uint32_t(bytes[offset+2]) << 16 | std::uint32_t(bytes[offset+3]) << 24;
	};
	require(u32(0)==0x20534444 && u32(4)==124 && u32(76)==32, "Native UI invalid DDS header");
	require((u32(8)&0x1007)==0x1007 && (u32(108)&0x1000), "Native UI DDS missing 2D texture flags");
	require(!(u32(8)&0x800000) && u32(24)<=1 && u32(112)==0, "Native UI DDS volume/cubemap is unsupported");
	std::uint32_t width=u32(16), height=u32(12);
	require(width>0 && height>0 && width<=16384 && height<=16384, "Native UI DDS dimensions outside bounds");
	NativeDDSLayout result;
	std::size_t offset=128, blockBytes=16, components=0;
	if (u32(80)&4) switch (u32(84)) {
		case 0x31545844: result.format=NativeDDSFormat::BC1; blockBytes=8; break; // DXT1
		case 0x33545844: result.format=NativeDDSFormat::BC2; break; // DXT3
		case 0x35545844: result.format=NativeDDSFormat::BC3; break; // DXT5
		case 0x31495441: case 0x55344342: result.format=NativeDDSFormat::BC4; blockBytes=8; break;
		case 0x32495441: case 0x55354342: result.format=NativeDDSFormat::BC5; break;
		case 0x30315844: // DX10 formats supported by the original nv_dds loader.
			require(bytes.size()>=148, "Native UI DDS truncated DX10 header");
			require(u32(132)==3 && !(u32(136)&4) && u32(140)==1, "Native UI DDS DX10 requires one flat 2D texture");
			switch (u32(128)) {
				case 71: result.format=NativeDDSFormat::BC1; blockBytes=8; break;
				case 74: result.format=NativeDDSFormat::BC2; break;
				case 77: result.format=NativeDDSFormat::BC3; break;
				case 80: result.format=NativeDDSFormat::BC4; blockBytes=8; break;
				case 83: result.format=NativeDDSFormat::BC5; break;
				case 98: result.format=NativeDDSFormat::BC7; break;
				case 99: result.format=NativeDDSFormat::BC7SRGB; break;
				default: throw std::runtime_error("Native UI DDS DX10 format is not implemented");
			}
			offset=148; break;
		default: throw std::runtime_error("Native UI DDS compression is not implemented");
	} else {
		const auto flags=u32(80), bits=u32(88);
		require(u32(84)==0, "Native UI DDS unexpected uncompressed FourCC");
		require((flags==0x41 || flags==0x40) && (bits==32 || (flags==0x40 && bits==24)),
			"Native UI DDS uncompressed format is not implemented");
		result.bitsPerPixel=bits;
		result.channelMasks={u32(92),u32(96),u32(100),u32(104)};
		require(result.channelMasks[0]==0x00ff0000 && result.channelMasks[1]==0x0000ff00 && result.channelMasks[2]==0x000000ff,
			"Native UI DDS uncompressed channel masks do not match original BGR/BGRA loader");
		require((flags==0x41 && result.channelMasks[3]==0xff000000) ||
			(flags==0x40 && (result.channelMasks[3]==0 || (bits==32 && result.channelMasks[3]==0xff000000))),
			"Native UI DDS unsupported uncompressed alpha mask");
		components=bits/8;
		result.format=(bits==32)? NativeDDSFormat::BGRA8 : NativeDDSFormat::BGR8;
		// RGB32 still uploads four components in the original nv_dds path;
		// retain its fourth byte rather than inventing opaque alpha.
		if (u32(8)&8)
			require(u32(20)==std::size_t(width)*components, "Native UI DDS row pitch differs from original tight loader");
		if (u32(8)&0x80000)
			require(u32(20)==std::size_t(width)*height*components, "Native UI DDS linear size differs from tight base mip");
	}
	const std::uint32_t levels=std::max(1u,u32(28));
	std::uint32_t maximumLevels=1;
	for (auto extent=std::max(width,height); extent>1; extent>>=1) ++maximumLevels;
	require(levels<=maximumLevels, "Native UI DDS mip count exceeds dimensions");
	result.mips.reserve(levels);
	for (std::uint32_t level=0; level<levels; ++level) {
		const std::size_t row=components? std::size_t(width)*components : std::size_t((width+3)/4)*blockBytes;
		const std::size_t count=row*(components? height : (height+3)/4);
		require(offset<=bytes.size() && count<=bytes.size()-offset, "Native UI DDS truncated mip payload");
		result.mips.push_back({width,height,offset,row,count}); offset+=count;
		width=std::max(1u,width/2); height=std::max(1u,height/2);
	}
	require(offset==bytes.size(), "Native UI DDS payload does not exactly match declared mips");
	return result;
}

// Input is one already-oriented nv_dds BGR8 surface, without a DDS header.
// GL's three-component BGR upload supplies alpha 1. Preserve row/mip order;
// the caller uses the same original nv_dds flip decision before conversion.
inline std::vector<std::uint8_t> ExpandNativeUIBGR8(std::span<const std::uint8_t> pixels,
	std::uint32_t width, std::uint32_t height)
{
	if (!width || !height || width>16384 || height>16384 ||
		pixels.size()>384ull*1024*1024 || pixels.size()!=std::size_t(width)*height*3)
		throw std::runtime_error("Native UI BGR surface is not a bounded tight mip");
	std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
	for (std::size_t src=0,dst=0; src<pixels.size(); src+=3,dst+=4) {
		rgba[dst]=pixels[src+2]; rgba[dst+1]=pixels[src+1]; rgba[dst+2]=pixels[src]; rgba[dst+3]=255;
	}
	return rgba;
}

} // namespace rendering::ui
