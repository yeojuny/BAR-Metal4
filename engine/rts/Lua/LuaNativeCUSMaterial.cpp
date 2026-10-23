/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeCUSMaterial.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "lib/sol2/sol.hpp"
#include "Game/GlobalUnsynced.h"
#include "Rendering/Backend/NativeCUSMaterialControl.h"
#include "System/Config/ConfigHandler.h"
#include "System/FileSystem/FileHandler.h"
#include <stdexcept>
#include <string>

namespace LuaNativeCUSMaterial {
bool PushActionEntries(lua_State* L)
{
	sol::state_view lua(L);
	auto gl = sol::stack::get<sol::table>(L, -1);
	gl.set_function("NativeCUSMaterialAction", [L](const std::string& action, int playerID) {
		if (CLuaHandle::GetHandleSynced(L) || gu == nullptr || configHandler == nullptr)
			throw std::runtime_error("Native CUS material actions require the original unsynced local-player context");
		return rendering::ApplyNativeCUSMaterialAction(action, playerID, gu->myPlayerNum,
			configHandler->GetIntSafe("cus2", 1)).has_value();
	});
	gl.set_function("NativeCUSLoadArchiveSource", [L]() {
		if (CLuaHandle::GetHandleSynced(L))
			throw std::runtime_error("Native CUS material source is unsynced only");
		CFileHandler file("LuaRules/Gadgets/cus_gl4.lua", SPRING_VFS_ZIP);
		std::string source;
		if (!file.FileExists() || !file.LoadStringData(source))
			throw std::runtime_error("The original native CUS control source is unavailable");
		return source;
	});
	return true;
}
} // namespace LuaNativeCUSMaterial
#else
namespace LuaNativeCUSMaterial { bool PushActionEntries(lua_State*) { return false; } }
#endif
