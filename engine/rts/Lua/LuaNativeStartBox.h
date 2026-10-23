/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeStartBoxData.h"
#include <functional>
struct lua_State;
namespace rendering::ui {class NativeUIRecorder;}
namespace LuaNativeStartBox {
using MiniMapSink=std::function<void(rendering::ui::NativeUIRecorder&,rendering::NativeStartBoxDraw)>;
void SetMiniMapSink(MiniMapSink,bool actualConsumerAvailable);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeStartBoxFrame> EndWorldFrame();
void AbortWorldFrame()noexcept;
void Reset();
bool PushEntries(lua_State*);
}
