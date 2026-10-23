/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeBufferData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include "ScreenDrawList.h"
#include <array>
#include <span>
#include <string_view>

namespace rendering {

enum class NativePiPProgram : std::uint8_t { LOS, DecalBlit, MinimapShading, Decal, Water, Icon, Circle, Quad, Line, NanoStream };
enum class NativePiPGeometry : std::uint8_t { Immediate, Instanced, Line };

struct NativePiPUniformSpec {
	std::string_view name;
	std::uint8_t components;
	std::uint8_t slot;
	bool sampler;
	bool active;
};
struct NativePiPProgramSpec {
	std::string_view name;
	std::uint64_t vertexHash, fragmentHash;
	NativePiPGeometry geometry;
	std::uint8_t instanceFields;
	std::span<const NativePiPUniformSpec> uniforms;
};

const NativePiPProgramSpec& GetNativePiPProgramSpec(NativePiPProgram program);
NativePiPProgram IdentifyNativePiPProgram(std::string_view vertex, std::string_view fragment, std::string_view geometry = {});

// Matches the actual MSL buffer layout. Values are kept per program instance;
// two original PiP widgets never share uniforms or resource retirement state.
struct alignas(16) NativePiPUniformBlock {
	std::array<float,16> modelViewProjection{};
	std::array<std::array<float,4>,32> values{};
	// xyz are per-sampler row origins. w is the original shader incomplete
	// texture bit mask; that GL sample is (0,0,0,1), independently of fixed
	// function texture-enable state. It is never an unavailable-engine-image flag.
	std::array<std::uint32_t,4> flipV{};
};
static_assert(sizeof(NativePiPUniformBlock) == 592);
static_assert(offsetof(NativePiPUniformBlock, values) == 64);
static_assert(offsetof(NativePiPUniformBlock, flipV) == 576);

struct NativePiPProgramSnapshot {
	NativePiPProgram program;
	NativePiPUniformBlock uniforms;
	// The Metal texture slots address logical shader samplers. These original
	// GL uniform values select which actual bound texture each slot consumes.
	std::array<int,3> samplerUnits{};
};

class NativePiPProgramState {
public:
	explicit NativePiPProgramState(NativePiPProgram program);
	NativePiPProgramState(std::string_view vertex, std::string_view fragment, std::string_view geometry = {});
	int GetUniformLocation(std::string_view name) const;
	// False records the original invalid-operation case. No existing uniform
	// value is modified on type/arity/location failure. Location -1 is ignored.
	bool SetFloats(int location, std::span<const float> values);
	bool SetIntegers(int location, std::span<const int> values);
	NativePiPProgramSnapshot Capture() const;
private:
	NativePiPProgramSnapshot state;
};

struct NativePiPImmediateVertex {
	std::array<float,4> position;
	std::array<float,4> color;
	std::array<float,4> texCoord;
};
static_assert(sizeof(NativePiPImmediateVertex) == 48);

enum class NativePiPEngineImage : std::uint8_t { None, Height, Shading, GroundLOS, Radar };

struct NativePiPSampledImage {
	std::shared_ptr<const ScreenTexture> image;
	ScreenSampler sampler;
	// Actual bound $info registry generation; its existing typed producer owns pixels.
	NativeCUSTextureBinding original;
	// Explicit original texture-zero/incomplete binding. A missing dependency
	// must not set this merely to make validation or sampling succeed.
	bool incomplete = false;
	// Actual immutable engine inputs for the shared minimap GPU producers.
	// This alternative never aliases a fabricated CPU placeholder image.
	NativePiPEngineImage engineImage = NativePiPEngineImage::None;
	std::shared_ptr<const NativeMiniMapBackground> engineSource;
};

// This packet is intentionally not activated by Lua until the complete
// source-ordered Screen/RTT consumer and all real engine images are connected.
// It does not advertise a globally usable GLSL API or silently drop stencil.
struct NativePiPDraw {
	NativePiPProgramSnapshot program;
	std::array<NativePiPSampledImage,3> sampledImages;
	ScreenDrawState surface;
	NativeWorldDrawState raster;
	std::uint32_t viewportWidth = 0, viewportHeight = 0;
	std::vector<NativePiPImmediateVertex> immediate;
	std::vector<std::uint32_t> indices;
	NativeBufferSnapshotRef vertices, instances;
	std::uint32_t firstVertex = 0, vertexCount = 0;
	std::uint32_t firstInstance = 0, instanceCount = 0;
};

void ValidateNativePiPDraw(const NativePiPDraw& draw);

// The original non-smooth GL.LINES raster contract uses diamond-exit pixels,
// including integer width rounding. Apply the exact original five-uniform
// vertex expression, then reuse the existing validated screen line rasterizer.
struct NativePiPLineGeometry {
	std::vector<NativePiPImmediateVertex> vertices;
	std::vector<std::uint32_t> indices;
	bool fullViewport=false;
};
NativePiPLineGeometry BuildNativePiPLineGeometry(const NativePiPDraw& draw,unsigned targetSamples=1,
	std::array<double,4> rasterViewport={},std::array<uint32_t,2> targetExtent={});

} // namespace rendering
