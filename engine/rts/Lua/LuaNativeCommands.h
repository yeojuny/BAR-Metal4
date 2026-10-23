/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCommandsData.h"
#include <functional>
struct lua_State;struct S3DModel;
namespace rendering{struct WorldModel;}
namespace LuaNativeCommands {
using ModelProvider=std::function<std::shared_ptr<const rendering::WorldModel>(const S3DModel*)>;
void SetModelProvider(ModelProvider);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeCommandsFrame> EndWorldFrame();
void AbortWorldFrame()noexcept;
void Reset();
bool PushEntries(lua_State*);
}
