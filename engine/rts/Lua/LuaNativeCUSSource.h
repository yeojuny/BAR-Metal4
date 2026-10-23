/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCUSSourceData.h"
#include "Rendering/Backend/NativeTextureRegistry.h"
#include <functional>
struct lua_State;
namespace LuaNativeCUSSource {
struct Services {
	// All callbacks refer to actual owners. No source-program success or image
	// default is inferred when a provider is absent.
	std::function<bool()> ready;
	std::function<std::shared_ptr<const rendering::NativeCUSProgramConsumer>(const rendering::NativeCUSProgramSource&)> compile;
	std::function<rendering::NativeTextureLookup(lua_State*,const std::string&)> parseTexture;
	std::function<bool(lua_State*,const std::string&)> deleteTexture;
	// Mirror the one observed texture event into generic UI state without
	// generating a second writer event. Required by the live family owner.
	std::function<void(unsigned)> syncObservedBinding;
	std::function<rendering::NativeCUSSourceDraw()> inheritedRaster;
	std::function<void(const rendering::NativeCUSSourceDraw&)> applyRaster;
	// Add resolves and retains the original model/indirect command. Submit
	// retains upload-stage published SSBO versions, never later live CPU bytes.
	std::function<std::shared_ptr<const rendering::NativeCUSModelSubmission>(const rendering::NativeCUSSubmittedObject&)> captureSubmission;
	std::function<std::shared_ptr<const rendering::NativeCUSModelInputs>(const rendering::NativeCUSSourceDraw&)> captureObjectInputs;
	std::function<std::shared_ptr<const rendering::NativeCUSUniformPublication>()> captureEngineUniforms;
};
void Configure(Services);
// Shared actual context owner injects once, clears before its destruction.
// This family neither owns nor silently resets another family's history.
void SetTextureContext(rendering::NativeCUSTextureHistory*,rendering::NativeTextureRegistry*);
bool Available();
rendering::NativeCUSTextureHistory& TextureHistory();
// Only the real native runtime context lifecycle may establish defaults.
void NewContext();
void BeginFrame(uint64_t sourceDrawFrame);
std::shared_ptr<const rendering::NativeCUSSourceFrame> EndFrame();
void AbortFrame() noexcept;
void Reset(); // Drops source Lua frames; does not fabricate a new GL context.
bool PushEntries(lua_State*);
} // namespace LuaNativeCUSSource
