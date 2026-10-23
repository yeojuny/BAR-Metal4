/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <array>
#include <optional>
#include <string_view>
namespace rendering {
// These are the two current31657 EXACT_CIRCLES programs. No old R8 mask pass.
enum class NativeSensor31657CircleKind: std::uint8_t { Radar, Sonar };
struct NativeSensor31657CircleDefines {
 NativeSensor31657CircleKind kind = NativeSensor31657CircleKind::Radar;
 std::string originalText, originalEngineDefinitions;
 float viewWidth = 0, viewHeight = 0;
 std::uint32_t circleSegments = 0, circleSSBOBinding = 5;
 bool visibilityCulling = false;
 std::optional<float> maxHeight;
 float stippleRate = 1;
};
struct NativeSensor31657CircleProgramSnapshot {
 std::shared_ptr<const NativeSensor31657CircleDefines> defines;
 std::array<float,4> rangeColor{};
 float teamColorMix = 1; // original default; selected widgets set zero
 std::int32_t heightUnit = 0, circleCount = 0;
};
class NativeSensor31657CircleProgramState {
public:
 NativeSensor31657CircleProgramState(std::string_view vertex, std::string_view fragment);
 int Location(std::string_view) const;
 bool Floats(int, std::span<const float>);
 bool Integers(int, std::span<const std::int32_t>);
 NativeSensor31657CircleProgramSnapshot Capture() const { return state; }
 std::vector<std::string_view> ActiveUniforms() const;
private: NativeSensor31657CircleProgramSnapshot state;
};
struct NativeSensor31657CircleDraw {
 std::uint64_t sequence = 0, sourceDrawFrame = 0;
 NativeSensor31657CircleProgramSnapshot program;
 // Actual compiled family generation, retained through the submitted draw.
 std::shared_ptr<const void> consumerLifetime;
 NativeBufferSnapshotRef vertices, instances;
 // Exact binding5 object/revision/offset/size, independently retained from VAO.
 // Its snapshot contains capacity bytes; only circleCount records are consumed.
 NativeBufferRange circles;
 std::uint32_t first = 0, count = 0, firstInstance = 0, instanceCount = 1;
 std::shared_ptr<const NativeCUSUniformPublication> original;
 std::shared_ptr<const NativeCUSPublishedBuffer> models;
 NativeCUSTextureBinding heightImage;
 std::shared_ptr<const ScreenTexture> heightDependency;
 ScreenDrawState surface;
 NativeWorldDrawState raster;
 std::uint32_t viewportWidth = 0, viewportHeight = 0;
};
struct NativeSensor31657CircleFrame {
 std::uint64_t sourceDrawFrame = 0;
 std::vector<NativeSensor31657CircleDraw> draws;
};
// Original SSBO1 rows are read by their actual instData.y. Heading remains w.
// Separate lists are necessary: binding5 is not assumed to be the VAO snapshot.
struct NativeSensor31657CircleModelPositions {
 std::vector<std::array<float,4>> instances, circles;
};
struct NativeSensor31657CircleOriginalUniforms {
 std::array<float,16> cameraViewProjection{}, cameraViewInverse{}, cameraViewProjectionInverse{};
 std::array<float,4> timeInfo{}, viewGeometry{}, mapSize{};
 std::uint32_t renderCaps = 0;
};
void ValidateNativeSensor31657CircleDraw(const NativeSensor31657CircleDraw&);
NativeSensor31657CircleOriginalUniforms ReadNativeSensor31657CircleUniforms(const NativeSensor31657CircleDraw&);
NativeSensor31657CircleModelPositions ReadNativeSensor31657CircleModelPositions(const NativeSensor31657CircleDraw&);
std::uint64_t NativeSensor31657SourceHash(std::string_view) noexcept;
void ValidateNativeSensor31657EngineDefinitions(std::string_view);
}
