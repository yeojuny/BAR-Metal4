/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "RenderTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rendering {

// Explicit upload layout; it does not alias the engine's full model vertex.
struct AssetDiagnosticVertex {
	std::array<float, 3> position;
	std::array<float, 3> normal;
	std::array<float, 2> uv;
};
static_assert(sizeof(AssetDiagnosticVertex) == 32);
static_assert(offsetof(AssetDiagnosticVertex, normal) == 12);
static_assert(offsetof(AssetDiagnosticVertex, uv) == 24);

enum class AssetTextureFormat { BC3RGBA, RGBA8Unorm };
struct AssetTextureMip {
	uint32_t width = 0;
	uint32_t height = 0;
	std::span<const uint8_t> bytes;
	size_t bytesPerRow = 0;
};
struct AssetTextureView {
	AssetTextureFormat format = AssetTextureFormat::BC3RGBA;
	std::span<const AssetTextureMip> mips;
};

enum class AssetDiagnosticMode : uint32_t {
	Material, Diffuse, TeamMask, Emission, Metalness, Roughness, Alpha,
};

struct AssetDiagnosticCamera {
	std::array<float, 16> viewProjection{}; // Column-major, Metal clip depth [0, 1].
	std::array<float, 3> position{};
};

// All input views remain valid until the synchronous render call returns.
// Render dimensions are explicit pixels: a window's Retina scale never changes
// them implicitly. A later surface adapter owns presentation scaling separately.
struct AssetDiagnosticRequest {
	std::span<const AssetDiagnosticVertex> vertices;
	std::span<const uint32_t> indices;
	std::array<AssetTextureView, 2> textures;
	std::array<float, 3> minimum{};
	std::array<float, 3> maximum{};
	std::array<float, 3> teamColor = {0.18f, 0.45f, 0.95f};
	std::optional<AssetDiagnosticCamera> camera;
	AssetDiagnosticMode mode = AssetDiagnosticMode::Material;
	uint32_t width = 1024;
	uint32_t height = 1024;
	uint32_t measuredFrames = 16;
	bool alphaClip = false;
	bool shaderValidation = true;
	std::string capturePath;
	std::string materialSemanticsSource;
};

using AssetDiagnosticResult = RenderReadbackResult;

} // namespace rendering
