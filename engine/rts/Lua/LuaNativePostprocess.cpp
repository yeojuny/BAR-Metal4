/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativePostprocess.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "NativeDOFLuaContext.h"
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/GlobalRendering.h"
#include "Game/Camera.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <stdexcept>
#include "NativeCASLuaContext.h"
#include "NativeSepiaLuaContext.h"
#include "NativeTargetPriorityLuaContext.h"
#include "NativeFrameGrapherLuaContext.h"
#include "NativeSpectatorHUDLuaContext.h"
#include "NativeWaterOverlayLuaContext.h"
#include "NativeProfilerHistogramLuaContext.h"
#include "NativeSquadHullLuaContext.h"
#include "NativeTerraformOverlayLuaContext.h"
#include "NativeLOSViewLuaContext.h"
#include "NativeUnitUniformCopyLuaContext.h"
#include "NativeDeferredVisualizerLuaContext.h"
#include "NativeDecalCaptureLuaContext.h"
#include "NativeSplatPainterLuaContext.h"
#include "NativeDiffusePainterLuaContext.h"
#include "NativeTerraformCaptureLuaContext.h"
namespace LuaNativePostprocess {
namespace {
rendering::NativePostprocessRecorder recorder;
void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
}
void BeginFrame(std::uint32_t width, std::uint32_t height) {
	Check(globalRendering != nullptr,"Native postprocess host has no actual draw generation");
recorder.SetPublicationObserver([](const auto& binding,const auto& version,std::uint64_t frame)->std::shared_ptr<rendering::NativePostprocessPublication>{
  auto& owner=rendering::OriginalNativeTextureContext();return owner.Initialized()?owner.UI().PublishPostprocess(binding,version,frame):nullptr;
 });
	recorder.BeginFrame(width,height,globalRendering->drawFrame);
	rendering::ui::GetRecorder().SetSourceDependencyQuery([](const auto& version){return recorder.OwnsPendingVersion(version);});
}
rendering::NativePostprocessRecorder& GetRecorder() { return recorder; }
std::shared_ptr<const rendering::NativePostprocessFrame> EndFrame() { return recorder.EndFrame(); }
void AbortFrame() noexcept { recorder.AbortFrame(); }
void RetireCompletedFrame(std::uint64_t serial) { recorder.RetireCompletedFrame(serial); }
void Reset() noexcept { recorder.Reset(); }
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl = sol::stack::get<sol::table>(L,-1);
 LuaNativeCAS::Bind(gl,"NativeCASLoadSource",[L](){
  Check(!CLuaHandle::GetHandleSynced(L),"CAS archive is unsynced-only");CFileHandler file("LuaUI/Widgets/gfx_cas.lua",SPRING_VFS_ZIP);std::string source;
  Check(file.FileExists() && file.LoadStringData(source),"Original CAS archive source missing");return source;
 });
 LuaNativeCAS::Bind(gl,"NativeCASContext",[L](){return LuaNativeCAS::Create(L,recorder);});
 LuaNativeSepia::Bind(gl,"NativeSepiaLoadSource",[L](){
  Check(!CLuaHandle::GetHandleSynced(L),"Sepia archive is unsynced-only");CFileHandler file("LuaUI/Widgets/gfx_sepia_tone.lua",SPRING_VFS_ZIP);std::string source;
  Check(file.FileExists()&&file.LoadStringData(source),"Original Sepia archive missing");return source;
 });
 LuaNativeDeferredVisualizer::Bind(gl,"NativeDeferredVisualizerContext",[L](){return LuaNativeDeferredVisualizer::Create(L);});
 LuaNativeDecalCapture::Bind(gl,"NativeDecalCaptureContext",[L](){return LuaNativeDecalCapture::Create(L);});
 LuaNativeSplatPainter::Bind(gl,"NativeSplatPainterContext",[L](){return LuaNativeSplatPainter::Create(L);});
 LuaNativeDiffusePainter::Bind(gl,"NativeDiffusePainterContext",[L](){return LuaNativeDiffusePainter::Create(L);});
 LuaNativeTerraformCapture::Bind(gl,"NativeTerraformCaptureContext",[L](){return LuaNativeTerraformCapture::Create(L);});
 LuaNativeUnitUniformCopy::Bind(gl,"NativeUnitUniformCopyContext",[L](){return LuaNativeUnitUniformCopy::Create(L);});
 LuaNativeLOSView::Bind(gl,"NativeLOSViewContext",[L](){return LuaNativeLOSView::Create(L);});
 LuaNativeTerraformOverlay::Bind(gl,"NativeTerraformOverlayContext",[L](){return LuaNativeTerraformOverlay::Create(L);});
 LuaNativeSquadHull::Bind(gl,"NativeSquadHullContext",[L](){return LuaNativeSquadHull::Create(L);});
 LuaNativeProfilerHistogram::Bind(gl,"NativeProfilerHistogramContext",[L](){return LuaNativeProfilerHistogram::Create(L);});
 LuaNativeWaterOverlay::Bind(gl,"NativeWaterOverlayContext",[L](){return LuaNativeWaterOverlay::Create(L);});
 LuaNativeSpectatorHUD::Bind(gl,"NativeSpectatorHUDContext",[L](){return LuaNativeSpectatorHUD::Create(L);});
 LuaNativeFrameGrapher::Bind(gl,"NativeFrameGrapherContext",[L](){return LuaNativeFrameGrapher::Create(L);});
 LuaNativeTargetPriority::Bind(gl,"NativeTargetPriorityContext",[L](){return LuaNativeTargetPriority::Create(L);});
 LuaNativeSepia::Bind(gl,"NativeSepiaContext",[L](){return LuaNativeSepia::Create(L,recorder);});
	gl.set_function("NativeDOFLoadSource",[L](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(L),"Original native DOF source is unsynced only");
		const bool widget=path=="luaui/widgets/gfx_dof.lua";
		Check(widget || path=="LuaUI/Shaders/dof.vs" || path=="LuaUI/Shaders/dof.fs","Unknown original31657 DOF source path");
		// Widget must come from the selected archive; shader lookup preserves
		// original RAW_FIRST and the paired program checks both exact identities.
		CFileHandler file(path,widget?SPRING_VFS_ZIP:SPRING_VFS_RAW_FIRST);std::string source;
		Check(file.FileExists() && file.LoadStringData(source),"Original31657 DOF source missing");return source;
	});
	gl.set_function("NativeDOFContext", [L]() -> sol::object {
		Check(!CLuaHandle::GetHandleSynced(L), "Original native DOF is unsynced only");
		if (!rendering::IsNativeDOFConsumerAvailable()) return sol::make_object(L, sol::lua_nil);
		Check(globalRendering && globalRendering->maxTextureSize>0,"Native DOF requires actual device texture limits");
		auto context = std::make_shared<rendering::NativeDOFContext>(recorder,std::uint32_t(globalRendering->maxTextureSize)); DOFCallbacks callbacks;
  auto& textureOwner=rendering::OriginalNativeTextureContext();
  Check(textureOwner.Initialized(),"DOF requires the actual common texture context");
  const auto generation=textureOwner.Registry().ContextGeneration();
  const auto actualOwner=[generation]()->rendering::NativeTextureContext& {auto& owner=rendering::OriginalNativeTextureContext();Check(owner.Initialized()&&owner.Registry().ContextGeneration()==generation,"DOF texture crossed actual context lifetime");return owner;};
  rendering::NativeDOFTextureEvents events;
  events.created=[actualOwner](const auto& binding,const auto& name){actualOwner().UI().CreatePostprocess(binding,name);};
  events.deleted=[generation](const auto& binding){auto& owner=rendering::OriginalNativeTextureContext();if(owner.Initialized()&&owner.Registry().ContextGeneration()==generation)owner.UI().DeletePostprocess(binding);};
  events.bound=[actualOwner](unsigned unit,const auto& binding){const auto actual=actualOwner().UI().BindPostprocess(unit,binding);rendering::ui::GetRecorder().SyncObservedTextureBinding(actual);return actual;};
  events.disabled=[actualOwner](unsigned unit){const auto actual=actualOwner().UI().LuaEnable(unit,false);rendering::ui::GetRecorder().SyncObservedTextureBinding(actual);};
  context->SetTextureEvents(std::move(events));
		callbacks.screenCommandOffset = []() {
			auto& ui = rendering::ui::GetRecorder(); Check(ui.IsActive() && !ui.IsWorldStage() && !ui.HasActiveTarget() && !ui.HasActiveTypedTarget(), "Original DOF requires the actual root screen stage"); return ui.RootCommandCount();
		};
		callbacks.viewExtent = []() { Check(globalRendering != nullptr,"DOF requires original view geometry"); return std::array<std::uint32_t,2>{std::uint32_t(globalRendering->viewSizeX),std::uint32_t(globalRendering->viewSizeY)}; };
		callbacks.projection = []() {
			// Original UniformMatrix("projection") uses GetNamedMatrix, hence the
			// actual camera matrix, independently of the mutable GL matrix stack.
			std::array<float,16> matrix;std::copy_n(CCamera::GetActive()->GetProjectionMatrix().m,16,matrix.begin());return matrix;
		};
		callbacks.viewport = [](rendering::NativePostprocessRect rectangle) {
			auto& ui=rendering::ui::GetRecorder();Check(ui.IsActive(),"Original DOF viewport requires the actual draw stage");
			ui.SetViewport({rectangle.x,rectangle.y,rectangle.width,rectangle.height});
		};
		callbacks.blending = [](bool enabled) { auto& ui = rendering::ui::GetRecorder(); auto state = ui.CurrentState().draw.blend; state.enabled = enabled; ui.SetBlend(state); };
  // The private source operation already emitted the one shared writer event
  // and synchronized its actual unit. These legacy callbacks are no-ops.
  callbacks.textureUnit0Bound=[](){};callbacks.textureUnit0Disabled=[](){};
		callbacks.currentTexCoord = [](float u,float v) { rendering::ui::GetRecorder().TexCoord(u,v); };
		PushDOFContext(L,std::move(context),std::move(callbacks)); auto table = sol::stack::get<sol::object>(L,-1); lua_pop(L,1); return table;
	});
	return true;
}
}
#else
namespace LuaNativePostprocess {
void BeginFrame(std::uint32_t,std::uint32_t) {}
std::shared_ptr<const rendering::NativePostprocessFrame> EndFrame() { return {}; }
void AbortFrame() noexcept {}
void RetireCompletedFrame(std::uint64_t) {}
void Reset() noexcept {}
bool PushEntries(lua_State*) { return true; }
}
#endif
