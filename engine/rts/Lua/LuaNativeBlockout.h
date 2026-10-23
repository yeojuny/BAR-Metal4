/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBlockoutData.h"
#include <functional>
struct lua_State;struct S3DModel;
namespace LuaNativeBlockout {
using ModelProvider=std::function<std::shared_ptr<const rendering::WorldModel>(const S3DModel*)>;
using TargetAllocator=std::function<std::shared_ptr<const rendering::NativeBlockoutTarget>(uint32_t,uint32_t)>;
void Configure(ModelProvider,TargetAllocator);
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeBlockoutFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
