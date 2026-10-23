/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeVolumetricClouds.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "NativeVolumetricCloudLuaContext.h"
#include "Lua/LuaHandle.h"
#include "lib/lua/include/LuaInclude.h"
#include "Lua/LuaNativePostprocess.h"
#include "Lua/LuaNativeUI.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/Bitmap.h"
#include "System/FileSystem/FileHandler.h"
#include "Game/Camera.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace LuaNativeVolumetricClouds {
namespace {
rendering::NativeVolumetricCloudRecorder recorder;
void Check(bool valid,const char* reason) { if (!valid) throw std::runtime_error(reason); }
struct Resources {
	std::shared_ptr<const rendering::NativeVolumetricCloudVolume> volume;
	bool noise2DLoaded = false;
};
int Factory(lua_State* L)
{
	std::array<char,2048> failure{};
	try {
		Check(!CLuaHandle::GetHandleSynced(L),"Native volumetric clouds are unsynced only");
		if (!rendering::IsNativeVolumetricCloudConsumerAvailable() || !rendering::IsNativeScreenCopyConsumerAvailable()) {
			lua_pushnil(L); return 1;
		}
		Check(globalRendering && globalRendering->maxTextureSize > 0,"Clouds require actual native device limits");
		auto context = std::make_shared<rendering::NativeVolumetricCloudContext>(
			LuaNativePostprocess::GetRecorder(),recorder,std::uint32_t(globalRendering->maxTextureSize));
		auto resources = std::make_shared<Resources>();
		CloudCallbacks callbacks;
		callbacks.stage = [] {
			const auto stage = rendering::ui::GetRecorder().CurrentWorldStage();
			Check(stage.has_value(),"Original clouds require the actual world callback scope");
			return *stage == rendering::NativeWorldDrawStage::PreUnit ? rendering::NativePostprocessStage::PreUnit : rendering::NativePostprocessStage::World;
		};
		callbacks.inverseDepth01ViewProjection = [] {
			const auto* current = CCamera::GetActive();
			Check(current != nullptr,"Original clouds require the actual camera inverse");
			std::array<float,16> inverse{},control{},identity{};
			std::copy_n(current->GetViewProjectionMatrixInverse().m,16,inverse.begin());
			std::copy_n(current->GetClipControlMatrix().m,16,control.begin());
			identity[0]=identity[5]=identity[10]=identity[15]=1;
			if (control == identity) return rendering::NativeVolumetricCloudDepth01Inverse(inverse,false);
			identity[10]=identity[14]=.5f;
			Check(control == identity,"Unsupported actual cloud camera clip convention");
			return rendering::NativeVolumetricCloudDepth01Inverse(inverse,true);
		};
		callbacks.registerImage = [L](std::shared_ptr<rendering::NativePostprocessBinding> image) {
			return LuaNativeUI::RegisterPostprocessTexture(L,std::move(image));
		};
		callbacks.loadOriginalVolume = [resources](std::string_view name) {
			if (!resources->volume) {
				const std::string path(name);
				CFileHandler file(path,SPRING_VFS_RAW_FIRST); std::string bytes;
				Check(file.FileSize() == 8388736 && file.LoadStringData(bytes),"Missing/invalid original128 cubed cloud Worley resource");
				resources->volume = std::make_shared<const rendering::NativeVolumetricCloudVolume>(
					rendering::DecodeNativeVolumetricCloudDDS({reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()},path));
			}
			return resources->volume;
		};
		callbacks.loadOriginalNoise2D = [resources](std::string_view name) {
			if (resources->noise2DLoaded) return;
			Check(name == ":l:LuaUI/Images/rgbnoise.png","Original cloud unit1 resource identity differs");
			CBitmap image;
			Check(image.Load("LuaUI/Images/rgbnoise.png",1.0f,4u,0x1401) && !image.compressed &&
				image.xsize == 256 && image.ysize == 256 && image.channels == 4 && image.dataType == 0x1401 &&
				image.GetRawMem() && image.GetMemSize() >= 256u*256u*4u,"Missing/invalid original cloud RGBA noise image");
			// The complete source has a dead noise_old function. Its real image
			// is decoded/validated as the original binding requests, but no fake
			// GPU allocation is needed for an optimized-out shader resource.
			resources->noise2DLoaded = true;
		};
		lua_getglobal(L,"gl");
		PushCloudContext(L,std::move(context),std::move(callbacks));
		lua_remove(L,-2); return 1;
	} catch (const std::exception& error) { std::snprintf(failure.data(),failure.size(),"%s",error.what()); }
	catch (...) { std::snprintf(failure.data(),failure.size(),"Unknown native-cloud context creation failure"); }
	return luaL_error(L,"%s",failure.data());
}
}
void BeginFrame(std::uint64_t frame) { recorder.BeginFrame(frame); }
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> EndFrame() { return recorder.EndFrame(); }
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> SnapshotPrefix(std::uint64_t cutoff) { return recorder.SnapshotPrefix(cutoff); }
void AbortFrame() noexcept { recorder.AbortFrame(); }
void RetireCompletedFrame(std::uint64_t frame) { recorder.RetireCompletedFrame(frame); }
void Reset() noexcept { recorder.Reset(); }
bool PushEntries(lua_State* L) { lua_pushcfunction(L,Factory); lua_setfield(L,-2,"NativeVolumetricCloudContext"); return true; }
}
#else
namespace LuaNativeVolumetricClouds {
void BeginFrame(std::uint64_t) {}
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> EndFrame() { return {}; }
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> SnapshotPrefix(std::uint64_t) { return {}; }
void AbortFrame() noexcept {}
void RetireCompletedFrame(std::uint64_t) {}
void Reset() noexcept {}
bool PushEntries(lua_State*) { return true; }
}
#endif
