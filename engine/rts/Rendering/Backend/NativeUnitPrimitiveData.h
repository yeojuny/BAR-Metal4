/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeCUSPublishedData.h"
#include "Rendering/Backend/NativeCUSUniformData.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <array>
#include <memory>
#include <string_view>
#include <vector>
namespace rendering {
enum class NativeUnitPrimitiveKind : std::uint8_t { BuildingAO, FeatureAO, TeamPlatter, SelectedGround, SelectedUnfinished, SelectedWater, AllySelected, EnergyIcon, FireStateIcon, GroupNumber, IdleBuilderIcon, RepeatIcon, WaitIcon, RankIcon, ResurrectionHalo, UnitTrackerDebug, EnemySpotter };
constexpr bool IsNativeUnitIcon(NativeUnitPrimitiveKind kind) noexcept
{
 return kind>=NativeUnitPrimitiveKind::EnergyIcon && kind<=NativeUnitPrimitiveKind::RankIcon;
}
constexpr bool IsNativeSelectedUnits(NativeUnitPrimitiveKind kind) noexcept
{
	return kind==NativeUnitPrimitiveKind::SelectedGround || kind==NativeUnitPrimitiveKind::SelectedUnfinished || kind==NativeUnitPrimitiveKind::SelectedWater;
}
// Exact DrawPrimitiveAtUnit GS input: four mixed float/uint attributes after
// the first vec4. Do not reinterpret the 18 Lua values as 18 float32s.
struct NativeUnitPrimitiveInstance {
	std::array<float,4> dimensions;
	std::uint32_t team,vertices;
	std::array<float,4> parameters,uv;
	std::array<std::uint32_t,4> instanceData;
};
static_assert(sizeof(NativeUnitPrimitiveInstance)==72);
struct NativeUnitPrimitiveUniforms {
	std::uint64_t sourceDrawFrame=0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
};
struct NativeUnitPrimitiveUniformValues {
	std::array<float,16> viewProjection,viewInverse;
	std::array<float,4> time;
	ClipDepthConvention clipDepth=ClipDepthConvention::Unsupported;
};
struct NativeUnitPrimitiveDraw {
	std::uint64_t sequence=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::PreUnit;
	NativeUnitPrimitiveKind kind=NativeUnitPrimitiveKind::BuildingAO;
	NativeBufferSnapshotRef vertices;
	std::uint32_t first=0,count=0;
	float addRadius=0,iconDistance=20000,opacity=1;
	// Captured actual selected-program definitions. whiteMix is the literal in
	// the original POST_SHADING, not a later read of mutable widget config.
	float initialSize=0,whiteMix=0;
	float lineOpacity=0;
	std::uint32_t maxVertices=64;
	bool screenEffects=false;
	// Original uploader publications, never live CPU model storage. Source
	// indices in instanceData remain addresses in these immutable SSBOs.
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms,modelUniforms;
	std::shared_ptr<const ScreenTexture> atlas;
	ScreenSampler sampler;
	ScreenDrawState surface;
	NativeWorldDrawState world;
	// Actual USE_QUADS absence in the selected 31657 TeamPlatter program.
	// Carry the compile variant with each draw; other original variants keep quads.
	bool disableQuads=false;
};
struct NativeUnitPrimitiveFrame {
	NativeUnitPrimitiveUniforms uniforms;
	std::vector<NativeUnitPrimitiveDraw> draws;
};
// Compact transport of the exact two uploaded root transforms, composite,
// source addresses and (only when consumed) the original team-color row.
struct NativeUnitPrimitiveObject {
	std::array<float,4> previousQuaternion,previousTranslationScale,currentQuaternion,currentTranslationScale;
	std::array<std::uint32_t,4> metadata; // composite, transform index, uniform index, reserved
	std::array<float,4> teamColor;
};
static_assert(sizeof(NativeUnitPrimitiveObject)==96);
std::uint64_t NativeUnitPrimitiveSourceHash(std::string_view) noexcept;
void ValidateNativeUnitPrimitiveEngineDefinitions(std::string_view);
NativeUnitPrimitiveUniformValues ReadNativeUnitPrimitiveUniforms(const NativeUnitPrimitiveUniforms&);
void ValidateNativeUnitPrimitiveDraw(const NativeUnitPrimitiveDraw&);
void ValidateNativeUnitIconFrame(const NativeUnitPrimitiveFrame&);
std::vector<NativeUnitPrimitiveObject> ReadNativeUnitPrimitiveObjects(const NativeUnitPrimitiveDraw&,const NativeUnitPrimitiveUniforms&);
class NativeUnitPrimitiveRecorder {
public:
	void Begin(const NativeUnitPrimitiveUniforms&);
	void Record(NativeUnitPrimitiveDraw);
	std::shared_ptr<const NativeUnitPrimitiveFrame> End();
	void Abort() noexcept;
	bool Active()const{return bool(frame);}
private:
	std::shared_ptr<NativeUnitPrimitiveFrame> frame;
};
void SetNativeUnitPrimitiveConsumerAvailable(bool) noexcept;
bool IsNativeUnitPrimitiveConsumerAvailable() noexcept;
}
