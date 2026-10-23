/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeUnitStencilData.h"
#include "Rendering/Backend/NativeTextureRegistry.h"
struct lua_State;
namespace LuaNativeUnitStencil {
struct Services {
	std::function<bool()> ready;
	std::function<std::shared_ptr<const rendering::NativeUnitStencilProgramConsumer>(const rendering::NativeUnitStencilProgramSource&)> compile;
};
void Configure(Services);
bool Available();
void BeginGenesisFrame();
std::shared_ptr<const rendering::NativeUnitStencilFrame> EndGenesisFrame();
void AbortGenesisFrame()noexcept;
void Reset();
bool PushEntries(lua_State*);
// Shared gl.Texture/SSAO selected-name seam; true only for this family's live
// logical Lua name. Parsing performs no Bind/history event of its own.
bool Lookup(lua_State*,const std::string&,rendering::NativeTextureLookup&);
}
