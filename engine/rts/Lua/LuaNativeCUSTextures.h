/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeTextureRegistry.h"
#include <string>

struct lua_State;
class LuaMatTexture;

namespace LuaNativeCUSTextures {
// Actual resource callbacks for NativeCUSSourceProviders. Installation and
// full source readiness remain with the Session owner, before original Lua.
rendering::NativeTextureLookup ParseTexture(lua_State*,const std::string&);
bool DeleteTexture(lua_State*,const std::string&);
// Material parsing retains the ORIGINAL typed descriptor and its namespace.
// Unsupported native UI/named aliases fail before raw GL0 can erase them.
bool ParseMaterialTexture(lua_State*,LuaMatTexture&,const std::string&);
// Query the real parsed owner at its original Bind event. No second parser,
// load, Bind or history event. parsed=false means unsupported/unobserved.
rendering::NativeTextureLookup ResolveMaterialTexture(const LuaMatTexture&);
}
