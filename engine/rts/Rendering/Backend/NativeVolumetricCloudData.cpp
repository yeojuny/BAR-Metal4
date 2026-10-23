/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeVolumetricCloudData.h"
#include "NativeVolumetricCloudSource.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
void Check(bool valid, const char* reason)
{
	if (!valid) throw std::runtime_error(std::string("Original volumetric clouds: ") + reason);
}
template<std::size_t N> bool Finite(const std::array<float, N>& values)
{
	return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
}
}
NativeVolumetricCloudProgram ParseNativeVolumetricCloudProgram(std::string_view vertex, std::string_view fragment)
{
	Check(vertex == ORIGINAL_CLOUD_VERTEX, "vertex program differs from its verified source");
	std::array<float, 11> values{};
	std::size_t field = 0, sourceOffset = 0, inputOffset = 0;
	bool depth01 = false, clamp = false;
	// Preserve every source byte outside the actual original format/gsub holes.
	while (sourceOffset < ORIGINAL_CLOUD_FRAGMENT.size()) {
		const auto remaining = ORIGINAL_CLOUD_FRAGMENT.substr(sourceOffset);
		if (remaining.starts_with("%f")) {
			Check(field < values.size(), "source constant field count is invalid");
			const auto* first = fragment.data() + inputOffset;
			const auto* end = fragment.data() + fragment.size();
			float value = 0;
			// Spring's bare Lua %f uses spring_lua_ftoa(precision=-1),
			// including integers and exponent notation. Consume that actual
			// finite literal while preserving every byte outside this hole.
			const auto parsed = std::from_chars(first, end, value, std::chars_format::general);
			Check(parsed.ec == std::errc{} && parsed.ptr != first && std::isfinite(value), "source constant is not a finite decimal");
			values[field++] = value;
			inputOffset = std::size_t(parsed.ptr - fragment.data());
			sourceOffset += 2;
			continue;
		}
		constexpr std::string_view depthMarker = "###DEPTH_CLIP01###";
		constexpr std::string_view clampMarker = "###CLAMP_TO_MAP###";
		constexpr std::string_view stepsMarker = "###RAYMARCH_STEPS###";
		if(remaining.starts_with(stepsMarker)) {
			// The complete pinned31657 widget sets cloudRaymarchSteps=10 and
			// substitutes tostring(max(2,floor(...))). The paired MSL uses ten
			// original samples; do not accept another count without its consumer.
			Check(fragment.substr(inputOffset).starts_with("10"),"original raymarch step count differs from its paired consumer");
			inputOffset+=2;sourceOffset+=stepsMarker.size();continue;
		}
		const auto marker = remaining.starts_with(depthMarker) ? depthMarker : remaining.starts_with(clampMarker) ? clampMarker : std::string_view{};
		if (!marker.empty()) {
			Check(inputOffset < fragment.size() && (fragment[inputOffset] == '0' || fragment[inputOffset] == '1'), "source boolean macro is invalid");
			(marker == depthMarker ? depth01 : clamp) = (fragment[inputOffset++] == '1');
			sourceOffset += marker.size();
			continue;
		}
		Check(inputOffset < fragment.size() && fragment[inputOffset] == ORIGINAL_CLOUD_FRAGMENT[sourceOffset], "fragment program differs from its verified source");
		++inputOffset;
		++sourceOffset;
	}
	Check(inputOffset == fragment.size() && field == values.size(), "source extent or constant count differs");
	NativeVolumetricCloudProgram out;
	out.noiseScale = values[0]; out.height = values[1]; out.bottom = values[2];
	std::copy_n(values.begin() + 3, 3, out.color.begin());
	out.mapX = values[6]; out.mapZ = values[7]; out.fadeAltitude = values[8];
	out.opacity = values[9]; out.sunPenetration = values[10];
	out.depthClip01Define = depth01; out.clampToMap = clamp;
	ValidateNativeVolumetricCloudProgram(out);
	return out;
}
void ValidateNativeVolumetricCloudProgram(const NativeVolumetricCloudProgram& p)
{
	Check(Finite(p.color) && std::isfinite(p.noiseScale) && std::isfinite(p.height) && std::isfinite(p.bottom) &&
		std::isfinite(p.mapX) && std::isfinite(p.mapZ) && std::isfinite(p.fadeAltitude) && std::isfinite(p.opacity) &&
		std::isfinite(p.sunPenetration), "program constants contain nonfinite values");
	Check(p.noiseScale != 0 && p.height != p.bottom && p.height != p.fadeAltitude, "source program has an undefined scale/height denominator");
	Check(p.mapX > 0 && p.mapZ > 0, "map extents must be positive");
}
void ValidateNativeVolumetricCloudUniforms(const NativeVolumetricCloudUniforms& u)
{
	Check(Finite(u.viewProjectionInverse) && Finite(u.eye) && Finite(u.offset) && Finite(u.sunDirection) && Finite(u.sunColor) && std::isfinite(u.time),
		"uniforms contain nonfinite values");
}
std::array<float,16> NativeVolumetricCloudDepth01Inverse(std::array<float,16> inverse, bool engineZeroToOne)
{
	Check(Finite(inverse), "engine inverse matrix contains nonfinite values");
	if (!engineZeroToOne) {
		// inverse(P_GL) * (x,y,2*z01-w,w), column-major. Pair the actual
		// hardware depth with its projection instead of advertising GL clip
		// control or applying a second conversion in the raymarch shader.
		for (unsigned row = 0; row < 4; ++row) {
			const float column = inverse[8 + row];
			inverse[8 + row] = 2 * column; inverse[12 + row] -= column;
		}
	}
	Check(Finite(inverse), "transported inverse matrix overflows");
	return inverse;
}
std::uint32_t NativeVolumetricCloudTextureExtent(double value, std::uint32_t limit)
{
	Check(std::isfinite(value) && value >= 1.0 && value <= double(std::numeric_limits<std::int32_t>::max()), "texture extent is not a representable positive GLsizei");
	const auto extent = static_cast<std::uint32_t>(value);
	Check(limit != 0 && extent <= limit, "texture extent exceeds the real device limit");
	return extent;
}
NativeVolumetricCloudVolume DecodeNativeVolumetricCloudDDS(std::span<const std::uint8_t> bytes, std::string sourcePath)
{
	Check(bytes.size() >= 128, "Worley DDS header is truncated");
	auto word = [&](std::size_t offset) {
		return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
			(std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
	};
	Check(word(0) == 0x20534444u && word(4) == 124 && word(76) == 32, "Worley DDS header is invalid");
	Check(word(12) == 128 && word(16) == 128 && word(24) == 128, "Worley DDS requires its original 128 cubed extent");
	Check((word(112) & 0x200000u) && !(word(112) & 0xfe00u), "Worley DDS requires original volume storage");
	Check(word(28) <= 1 && word(80) == 0x41 && word(84) == 0 && word(88) == 32, "Worley DDS requires original single-level 32-bit storage");
	Check(word(92) == 0xff && word(96) == 0xff00 && word(100) == 0xff0000 && word(104) == 0xff000000u, "Worley DDS channel masks differ");
	constexpr std::size_t row = 128 * 4, image = row * 128, payload = image * 128;
	Check(word(20) == 0 || word(20) == row, "Worley DDS pitch contradicts original tight rows");
	Check(bytes.size() == 128 + payload, "Worley DDS payload extent differs");
	NativeVolumetricCloudVolume out;
	out.sourcePath = std::move(sourcePath); out.width = out.height = out.depth = 128;
	out.bytesPerRow = row; out.bytesPerImage = image; out.bgraBytes.resize(payload);
	for (std::size_t z = 0; z < 128; ++z) {
		for (std::size_t y = 0; y < 128; ++y) {
			std::copy_n(bytes.data() + 128 + z * image + (127 - y) * row, row, out.bgraBytes.data() + z * image + y * row);
		}
	}
	return out;
}
} // namespace rendering
