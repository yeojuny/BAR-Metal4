/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeCUSUniformData.h"
#include "Rendering/Backend/NativeCUSTextureHistory.h"
#include "Rendering/Backend/NativeWorldDrawData.h"

#include <array>
#include <optional>
#include <string_view>

namespace rendering {

enum class NativeStartBoxProgram : std::uint8_t { Polygon, Cone, SDFBake };
enum class NativeStartBoxStage : std::uint8_t { WorldPreUnit, World, MiniMap, TextureBake };

struct NativeStartBoxDefines {
	// Preserve the actual original float32-Lua formatting passed to LuaShader.
	// Reformatting these values from C++ changes the shader's decimal constants.
	std::string originalText;
	std::uint32_t boxes = 0, polygons = 0, points = 0;
	float alpha = 0.5f, minimumY = 0, maximumY = 0, maximumSteepness = 0;
	std::optional<std::int32_t> scavAllyTeam, raptorAllyTeam;
};

struct NativeStartBoxProgramSnapshot {
	NativeStartBoxProgram program = NativeStartBoxProgram::Polygon;
	std::shared_ptr<const NativeStartBoxDefines> defines;
	std::int32_t isMiniMap = 0, myAllyTeam = -1, rotationMiniMap = 0, waterSurfaceMode = 0;
	// Original cone uses a float isMinimap, with distinct spelling and type.
	float coneIsMiniMap = 0, startPosScale = 0.0005f, waterLevel = 0;
	std::array<float,4> pipVisibleArea{0,1,0,1};
	std::array<std::int32_t,6> samplerUnits{};
};

std::uint64_t NativeStartBoxSourceHash(std::string_view) noexcept;
void ValidateNativeStartBoxEngineDefinitions(std::string_view);
NativeStartBoxProgram IdentifyNativeStartBoxProgram(std::string_view vertex, std::string_view fragment);
NativeStartBoxDefines ParseNativeStartBoxDefines(NativeStartBoxProgram, std::string_view);

class NativeStartBoxProgramState {
public:
	NativeStartBoxProgramState(std::string_view vertex, std::string_view fragment, std::string_view defines);
	int GetUniformLocation(std::string_view name) const;
	// Matches GL update semantics: -1 is ignored; a wrong type/arity cannot
	// change the prior value. Original misspellings/inactive uniforms stay -1.
	bool SetFloats(int location, std::span<const float>);
	bool SetIntegers(int location, std::span<const std::int32_t>);
	NativeStartBoxProgramSnapshot Capture() const { return state; }
private:
	NativeStartBoxProgramSnapshot state;
};

struct alignas(16) NativeStartBoxUniformBlock {
	std::array<float,16> cameraViewProjection{}, cameraViewProjectionInverse{};
	std::array<float,4> timeInfo{}, mapSize{}, pipVisibleArea{};
	std::array<std::int32_t,4> parameters{};
	std::array<float,4> values{}; // startPosScale, cone isMinimap, alpha-test ref, waterLevel
	std::array<std::uint32_t,4> options{}; // Actual image origins set at Prepare.
	std::array<float,16> cameraViewInverse{}; // Exact retained UBO matrix7, for original water ray.
};
static_assert(sizeof(NativeStartBoxUniformBlock) == 288);
static_assert(offsetof(NativeStartBoxUniformBlock, parameters) == 176);
static_assert(offsetof(NativeStartBoxUniformBlock, options) == 208);
static_assert(offsetof(NativeStartBoxUniformBlock, cameraViewInverse) == 224);

struct NativeStartBoxDraw {
	std::uint64_t sequence = 0, sourceDrawFrame = 0;
	NativeStartBoxStage stage = NativeStartBoxStage::WorldPreUnit;
	NativeStartBoxProgramSnapshot program;
	std::shared_ptr<const NativeCUSUniformPublication> uniforms;
	NativeBufferSnapshotRef vertices, instances, indices; // Retain original fillVAOSlots attachments.
	NativeBufferRange polygons;
	std::uint32_t firstVertex = 0, vertexCount = 0, firstInstance = 0, instanceCount = 0;
	// Actual sampler bindings at this draw, including immutable content and
	// sampler versions; a string such as $map_gbuffer_zvaltex is not storage.
	std::array<NativeCUSTextureBinding,6> images;
	// Actual copy/RTT/external-depth descriptors retain source-order edges.
	std::array<std::shared_ptr<const ScreenTexture>,6> dependencyImages;
	// An actual map image whose existing producer has not completed yet must
	// keep cached minimap targets behind the source-frame world preparation.
	bool mapSourceDependent=false;
	ScreenDrawState surface;
	NativeWorldDrawState raster;
	std::uint32_t viewportWidth = 0, viewportHeight = 0;
};

struct NativeStartBoxFrame {
	std::uint64_t sourceDrawFrame=0;
	std::vector<NativeStartBoxDraw> draws;
};

std::array<bool,6> NativeStartBoxActiveSamplers(const NativeStartBoxProgramSnapshot&);
NativeStartBoxUniformBlock ReadNativeStartBoxUniforms(const NativeStartBoxDraw&);
void ValidateNativeStartBoxDraw(const NativeStartBoxDraw&);

} // namespace rendering
