/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWeaponRangeData.h"
#include <functional>
struct lua_State;
namespace rendering::ui {class NativeUIRecorder;}
namespace LuaNativeWeaponRange {
using MiniMapSink=std::function<void(rendering::ui::NativeUIRecorder&,rendering::NativeWeaponRangeDraw)>;
void SetMiniMapSink(MiniMapSink,bool actualConsumerAvailable);
// HQ enables only after ordered world-FBO, R8/D16 and mask-image consumers exist.
void SetCoverageMaskConsumerReady(bool);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeWeaponRangeFrame> EndWorldFrame();
void AbortWorldFrame()noexcept;
void Reset();
bool PushEntries(lua_State*);
}
