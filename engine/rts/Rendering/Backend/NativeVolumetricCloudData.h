/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rendering {
struct NativePostprocessVersion;

// Values are the actual original string.format("%f") constants, rather than
// unrounded UI values. Reloading an already-formatted original source does not
// silently introduce new effective constants.
struct NativeVolumetricCloudProgram {
	float noiseScale = 0; // Original input scale, not its reciprocal.
	float height = 0, bottom = 0;
	std::array<float, 3> color{};
	float mapX = 0, mapZ = 0;
	float fadeAltitude = 0, opacity = 0, sunPenetration = 0;
	bool depthClip01Define = false, clampToMap = false;
	bool operator==(const NativeVolumetricCloudProgram&) const = default;
};

// Exact original unqualified NamedTextures DDS: BGRA bytes, vertical flip in
// every Z slice, unchanged Z order. The native producer generates all eight
// mip levels from this real base and uses repeat + linear/trilinear sampling.
struct NativeVolumetricCloudVolume {
	std::string sourcePath;
	std::uint32_t width = 0, height = 0, depth = 0;
	std::size_t bytesPerRow = 0, bytesPerImage = 0;
	std::vector<std::uint8_t> bgraBytes;
};
struct NativeVolumetricCloudUniforms {
	// Column-major zero-to-one projection inverse paired with the copied native
	// depth. If the engine camera is [-1,1], the caller transports the inverse
	// through the exact depth-range conversion; no fake clip-control capability.
	std::array<float, 16> viewProjectionInverse{};
	std::array<float, 3> eye{}, offset{}, sunDirection{}, sunColor{};
	float time = 0;
};
struct NativeVolumetricCloudDraw {
	// Actual original PreUnit TexRect call ordinal, after its ordered depth copy.
	std::uint64_t sequence = 0;
	std::shared_ptr<const NativePostprocessVersion> depth, output, previous;
	std::shared_ptr<const NativeVolumetricCloudVolume> volume;
	NativeVolumetricCloudProgram program;
	NativeVolumetricCloudUniforms uniforms;
};
struct NativeVolumetricCloudFrame {
	std::uint64_t sourceDrawFrame = 0;
	std::vector<NativeVolumetricCloudDraw> draws;
};

// Accept only the complete source-paired original program. Unknown source is
// rejected; this is not a generic GLSL compiler or a synthetic shader handle.
NativeVolumetricCloudProgram ParseNativeVolumetricCloudProgram(std::string_view vertex, std::string_view fragment);
void ValidateNativeVolumetricCloudProgram(const NativeVolumetricCloudProgram& program);
void ValidateNativeVolumetricCloudUniforms(const NativeVolumetricCloudUniforms& uniforms);
std::array<float,16> NativeVolumetricCloudDepth01Inverse(std::array<float,16> engineInverse, bool engineZeroToOne);
NativeVolumetricCloudVolume DecodeNativeVolumetricCloudDDS(std::span<const std::uint8_t> bytes, std::string sourcePath);
// GLsizei(luaL_checknumber) truncation used by original CreateTexture, with
// checked representability before conversion. No round/ceil/Retina multiplier.
std::uint32_t NativeVolumetricCloudTextureExtent(double originalValue, std::uint32_t deviceLimit);

inline std::atomic<bool> nativeVolumetricCloudConsumerAvailable{false};
inline bool IsNativeVolumetricCloudConsumerAvailable() noexcept { return nativeVolumetricCloudConsumerAvailable.load(std::memory_order_acquire); }
inline void SetNativeVolumetricCloudConsumerAvailable(bool value) noexcept { nativeVolumetricCloudConsumerAvailable.store(value, std::memory_order_release); }
} // namespace rendering
