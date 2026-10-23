/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <memory>
#include <functional>
#include <string>

struct lua_State;
namespace rendering {
	struct ScreenTexture;
	struct NativePostprocessBinding;
	struct NativeTextureLookup;
	namespace ui { struct NativeUITextureBinding; }
}
namespace LuaNativeUI {
	void Install(lua_State* L);
	// Actual same-ScreenGPU R8+D16 attachments, allocated/validated without
	// submitting work. A null owner is a real allocation/FBO failure.
	using CoverageTargetAllocator=std::function<std::shared_ptr<const void>(
		const std::shared_ptr<const rendering::ScreenTexture>&,const std::shared_ptr<const rendering::ScreenTexture>&)>;
	void SetCoverageTargetAllocator(CoverageTargetAllocator allocator);
	void UpdateMiniMap();
	void ResetMiniMap();
	void DrawMiniMap(bool defaultTransform);
	// Direct real VFS decode; intentionally bypasses the qualified named-image
	// cache so SetUnitDefImage reloads preserve original replacement semantics.
	std::shared_ptr<const rendering::ScreenTexture> LoadBuildPicture(const std::string& fileName);
	// Private typed consumers share original image loading and immutable RTT cells.
	// Null means the original bind failed; callers retain their previous binding.
	std::shared_ptr<rendering::ui::NativeUITextureBinding> ResolveTextureBinding(lua_State* L,const std::string& name);
	// Runs the same actual UI name parser/loader without issuing a bind. The
	// typed caller owns the one original LuaTexture event in shared history.
	rendering::NativeTextureLookup CaptureTextureLookup(lua_State* L,const std::string& name);
	bool DeleteTexture(lua_State* L,const std::string& name);
	std::shared_ptr<rendering::ui::NativeUITextureBinding> GetTextureBinding(lua_State* L,const std::string& name);
	std::shared_ptr<rendering::NativePostprocessBinding> GetPostprocessTextureBinding(lua_State* L,const std::string& name);
	std::string RegisterPostprocessTexture(lua_State* L,std::shared_ptr<rendering::NativePostprocessBinding> binding);
}
