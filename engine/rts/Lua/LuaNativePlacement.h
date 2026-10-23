/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePlacementData.h"
#include <functional>
struct lua_State;struct S3DModel;struct BuildInfo;struct float3;struct SColor;
namespace LuaNativePlacement {
using ModelProvider=std::function<std::shared_ptr<const rendering::WorldModel>(const S3DModel*)>;
void SetModelProvider(ModelProvider);
void BeginWorldFrame();
std::shared_ptr<const rendering::NativePlacementFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
// A scope for original engine placement and range lines. It cannot capture
// arbitrary legacy immediate-mode rendering or affect Lua global capabilities.
class EngineScope {
public: EngineScope(float lineWidth,unsigned sourceBlend,unsigned destinationBlend,
		rendering::NativeWorldDrawStage stage=rendering::NativeWorldDrawStage::World,
		bool requirePlacementResources=true);~EngineScope();
	EngineScope(const EngineScope&)=delete;EngineScope& operator=(const EngineScope&)=delete;
};
bool CapturingEngine() noexcept;
void EngineDepthTest(bool);
void EngineBlend(unsigned source,unsigned destination);
void RecordEngineLineLoop(std::span<const float3>,const SColor&);
void RecordEngineLines(std::span<const float3>,rendering::ScreenColor);
void RecordEngineCulledTriangles(std::span<const float3>,rendering::ScreenColor,bool cullFront);
void RecordBuildSquare(const BuildInfo&,float height,bool canBuild,std::span<const uint8_t> statuses);
void EngineColor(rendering::ScreenColor);
void RecordQueuedBuildSites(std::span<const float> quads,std::span<const float> waterQuads,
	std::span<const float> lines,std::span<const float> lineColors);
void RecordBuildingSample(const BuildInfo&,int team);
}
