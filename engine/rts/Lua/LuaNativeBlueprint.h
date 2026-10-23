/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBlueprintData.h"
#include <functional>
struct lua_State;
struct S3DModel;
namespace LuaNativeBlueprint {
using ModelProvider = std::function<std::shared_ptr<const rendering::WorldModel>(const S3DModel*)>;
// The scene owns the existing immutable model/material cache; do not duplicate it.
void SetModelProvider(ModelProvider provider);
bool HasModelProvider();
std::shared_ptr<const rendering::WorldModel> CaptureUnitModel(int unitID);
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeBlueprintFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void RecordStencilClear(rendering::NativeWorldStencilClear clear);
void Reset();
bool PushEntries(lua_State* L);
}
