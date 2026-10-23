/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePBRSourceData.h"
#include <functional>
struct lua_State;

namespace LuaNativePBRSource {
struct Services {
	std::function<bool()> ready;
	std::function<std::shared_ptr<const rendering::NativePBRProgram>(const rendering::NativePBRProgramSource&)> compile;
	// This allocates real format-correct storage and validates its actual target.
	std::function<std::shared_ptr<rendering::NativePBRImage>(const rendering::NativePBRAllocation&)> allocate;
	std::function<rendering::NativeTextureLookup(lua_State*,const std::string&)> parseTexture;
	std::function<uint64_t()> sourceDrawFrame;
	// Owns pending original commands across no-drawable frames. The sink must
	// encode them offscreen or retain them; it cannot mark them completed.
	std::function<void(std::shared_ptr<const rendering::NativePBRImage>)> enqueue;
	std::function<void(unsigned)> syncObservedBinding;
	// The selected source writes a complete color target with blending/depth
	// disabled. Verify inherited masks/scissor/stencil rather than ignoring it.
	std::function<void()> validateFullTargetRaster;
	// Same original FBO viewport and RenderToTexture identity-matrix scope,
	// while the private native producer owns the actual offscreen encoder.
	std::function<void(uint32_t,uint32_t,bool,const std::function<void()>&)> targetRaster;
};
void Configure(Services,rendering::NativeTextureRegistry*,rendering::NativeCUSTextureHistory*);
bool Available();
bool PushEntries(lua_State*);
// Shared source parsers may resolve these opaque Lua names without decoding
// them as filenames. False means this is not a name owned by this family.
bool Lookup(lua_State*,const std::string&,rendering::NativeTextureLookup&);
// Uses the same private source owner as its original DeleteTexture closure.
bool DeleteTexture(lua_State*,const std::string&);
const std::vector<rendering::NativePBRLoadedSource>& LoadedSources();
void ReleaseLuaState(lua_State*);
} // namespace LuaNativePBRSource
