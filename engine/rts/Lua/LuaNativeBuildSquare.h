/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBuildSquareData.h"
#include <functional>
struct lua_State;
namespace rendering::ui {class NativeUIRecorder;}
namespace LuaNativeBuildSquare {
using MiniMapSink=std::function<void(rendering::ui::NativeUIRecorder&,rendering::NativeBuildSquareDraw)>;
void SetMiniMapSink(MiniMapSink,bool actualConsumerAvailable);
bool Available();
void BeginWorldFrame();
std::vector<rendering::NativeBuildSquareDraw> EndWorldFrame();
void AbortWorldFrame()noexcept;
void Reset();
bool PushEntries(lua_State*);
}
