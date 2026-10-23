/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include "NativePostprocessData.h"
#include "NativeWorldDrawData.h"
#include "NativeSensorPreviewUniforms.h"
#include <array>
#include <functional>
#include <string_view>
namespace rendering {
enum class NativeSensorPreviewProgramKind: std::uint8_t { Mip, Coverage, Smooth, Sheet, Minimap, JammerDisc, JammerSheet };
enum class NativeSensorPreviewCallin: std::uint8_t { World, Minimap };
struct NativeSensorPreviewProgramSource {
 NativeSensorPreviewProgramKind kind = NativeSensorPreviewProgramKind::Mip;
 std::string vertex,fragment,defines,engineDefinitions;
 std::string originalVertexSHA512,originalFragmentSHA512;
 std::uint64_t expandedVertexHash = 0,expandedFragmentHash = 0;
 bool modelDepthTest = false;
 bool jammer = false;
};
struct NativeSensorPreviewProgramConsumer {
 // Actual renderer-created program owner: compiled Metal libraries+entrypoints.
 std::shared_ptr<const void> pipelineLifetime;
 NativeSensorPreviewProgramSource source;
};
using NativeSensorPreviewCompileFunction = std::function<std::shared_ptr<const NativeSensorPreviewProgramConsumer>(const NativeSensorPreviewProgramSource&)>;
struct NativeSensorPreviewProgramSnapshot {
 std::shared_ptr<const NativeSensorPreviewProgramSource> source;
 std::shared_ptr<const NativeSensorPreviewProgramConsumer> consumer;
 NativeSensorPreviewUniformBlock uniforms{};
 std::uint32_t jammerCircleValues = 0;
 // Physical sampler-name slot -> actual original GL uniform unit.
 std::array<std::int32_t,7> samplerUnits{};
};
class NativeSensorPreviewProgramState {
public:
 NativeSensorPreviewProgramState(std::string_view vertex,std::string_view fragment,const NativeSensorPreviewCompileFunction& actualCompiler);
 int Location(std::string_view) const;
 bool Floats(int,std::span<const float>);
 bool Integers(int,std::span<const std::int32_t>);
 std::vector<std::string_view> ActiveUniforms() const;
 NativeSensorPreviewProgramSnapshot Capture() const { return state; }
private: NativeSensorPreviewProgramSnapshot state;
};
struct NativeMiniMapBackground;
struct NativeSensorPreviewImageInput {
 std::int32_t effectiveUnit = -1;
 // Foreign engine/Lua image: actual retained registry binding and dependency.
 NativeCUSTextureBinding binding;
 std::shared_ptr<const ScreenTexture> dependency;
 // Private RTT image: actual current postprocess generation, never an alias ID.
 std::shared_ptr<const NativePostprocessVersion> version;
 // Actual allocation owner for dimension-only reads before any produced version.
 std::shared_ptr<const NativePostprocessImage> allocation;
 // Actual viewer LOS/radar/jammer snapshot for the original $info:radar suffix.
 // It is consumed by the existing MiniMapGPU radar producer, never a stand-in image.
 std::shared_ptr<const NativeMiniMapBackground> radarSource;
};
struct NativeSensorPreviewDraw {
 std::uint64_t sequence = 0,sourceDrawFrame = 0;
 NativeSensorPreviewCallin callin = NativeSensorPreviewCallin::World;
 NativeSensorPreviewProgramSnapshot program;
 NativeBufferSnapshotRef vertices;
 std::uint32_t first = 0,count = 0;
 // Exact real binding5 range. RayData and AlliedCircles retain separate
 // objects/revisions/layouts, despite both shaders reading float4 entries.
 NativeBufferRange storage;
 // Actual uploaded prefix within that bound range; zero is an allocation-only
 // AlliedCircles list. Shader winner guards determine whether it is read.
 std::size_t storageDefinedBytes = 0;
 std::array<NativeSensorPreviewImageInput,7> images;
 std::shared_ptr<const NativeCUSUniformPublication> original;
 ScreenDrawState surface;
 NativeWorldDrawState raster;
 std::uint32_t viewportWidth = 0,viewportHeight = 0;
};
enum class NativeSensorPreviewCommandKind: std::uint8_t { Draw, Clear };
struct NativeSensorPreviewCommand {
 NativeSensorPreviewCommandKind kind = NativeSensorPreviewCommandKind::Draw;
 std::uint64_t sequence = 0,sourceDrawFrame = 0;
 NativeSensorPreviewCallin callin = NativeSensorPreviewCallin::World;
 // Both null for actual final World/Minimap attachment draws. For RTT these
 // are installed only by the genuine PublishExternalOutput transaction.
 std::shared_ptr<const NativePostprocessVersion> output,previous;
 std::shared_ptr<const NativeSensorPreviewDraw> draw;
 std::array<float,4> clearColor{};
 ScreenDrawState surface;
 NativeWorldDrawState raster;
};
struct NativeSensorPreviewFrame {
 std::uint64_t sourceDrawFrame = 0;
 std::vector<NativeSensorPreviewCommand> commands;
};
// Source/data invariants for the actual renderer, not an oracle or fixture.
void ValidateNativeSensorPreviewProgramSource(const NativeSensorPreviewProgramSource&);
std::uint32_t NativeSensorPreviewRequiredTextureMask(const NativeSensorPreviewProgramSnapshot&);
// textureSize(prevTex) still needs allocated storage when Smooth reset skips
// its contents. This separate mask prevents inventing a first-use clear.
std::uint32_t NativeSensorPreviewRequiredContentsMask(const NativeSensorPreviewProgramSnapshot&);
bool NativeSensorPreviewRequiresStorage(const NativeSensorPreviewProgramSnapshot&);
void ValidateNativeSensorPreviewDraw(const NativeSensorPreviewDraw&);
std::uint64_t NativeSensorPreviewSourceHash(std::string_view) noexcept;
}
