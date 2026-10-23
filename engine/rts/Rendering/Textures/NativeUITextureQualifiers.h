/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rendering::ui {
struct NativeUITextureQualifiers {
	std::string name;
	bool nearest=false,linear=false,mipNear=false,clamped=false,greyed=false;
	std::optional<std::pair<std::uint32_t,std::uint32_t>> resize;
	std::optional<std::array<float,3>> tint;
	bool anisotropic=false,inverted=false,whiteBorder=false;
};
inline NativeUITextureQualifiers ParseNativeUITextureQualifiers(std::string_view qualified)
{
	NativeUITextureQualifiers result;
	if (qualified.empty() || qualified.front()!=':') { result.name=qualified; return result; }
	const auto end=qualified.find(':',1);
	if (end==std::string_view::npos) throw std::runtime_error("unterminated texture qualifier");
	// Original NamedTextures uses strtod followed by a float conversion.
	// Keep its sign/exponent/hex-float grammar, bounded by this terminator.
	const std::string prefix(qualified.substr(0,end));
	const auto number=[&](std::size_t& offset) {
		std::uint32_t value=0;
		const char* start=qualified.data()+offset;
		const auto parsed=std::from_chars(start,qualified.data()+end,value);
		if (parsed.ec!=std::errc{} || parsed.ptr==start || value>16384)
			throw std::runtime_error("texture resize dimension invalid or outside bounds");
		offset=parsed.ptr-qualified.data(); return std::max(1u,value);
	};
	for (std::size_t p=1;p<end;++p) {
		switch (qualified[p]) {
			case 'n': result.nearest=true; break;
			case 'l': result.linear=true; break;
			case 'm': result.mipNear=true; break;
			case 'c': result.clamped=true; break;
			case 'g': result.greyed=true; break;
			case 'a': result.anisotropic=true; break;
			case 'i': result.inverted=true; break;
			case 'b': result.whiteBorder=true; break;
			case 't': {
				std::array<float,3> tint{}; ++p;
				for (unsigned component=0;component<3;++component) {
					const char* start=prefix.c_str()+p; char* next=nullptr;
					const double parsed=std::strtod(start,&next);
					if (next==start || !std::isfinite(parsed) || parsed<0 || parsed>std::numeric_limits<float>::max())
						throw std::runtime_error("texture tint requires three finite nonnegative factors");
					tint[component]=static_cast<float>(parsed);
					// Byte Tint converts the product to uint32 before clamping.
					// Reject its undefined negative/overflow conversion domain.
					if (!std::isfinite(tint[component]*255.f) || double(tint[component]*255.f)>=4294967296.0)
						throw std::runtime_error("texture tint exceeds original byte conversion bounds");
					p=static_cast<std::size_t>(next-prefix.c_str());
					if (component<2) {
						if (p>=end || prefix[p]!=',') throw std::runtime_error("texture tint requires red,green,blue");
						++p;
					}
				}
				result.tint=tint; --p; break;
			}
			case 'r': {
				++p; const auto width=number(p);
				if (p>=end || qualified[p]!=',') throw std::runtime_error("texture resize requires width,height");
				++p; const auto height=number(p); result.resize={{width,height}}; --p; break;
			}
			default: throw std::runtime_error("texture qualifier is not implemented by the native screen path");
		}
	}
	result.name=qualified.substr(end+1); return result;
}
} // namespace rendering::ui
