/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <optional>
#include <string_view>
struct lua_State;
namespace rendering::ui {class NativeUIRecorder;}
namespace LuaNativeUIDrawState {
// The normal public LuaNativeUI dispatcher and source-host fixtures call the
// SAME implementation. Unsupported operations continue in the public router.
std::optional<int> Dispatch(lua_State*,rendering::ui::NativeUIRecorder&,std::string_view);
}
