/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShadowLuaCapture.h"
#include "NativeWaterLuaCapture.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Game.h"
#include "Rendering/Backend/NativeShadowColorData.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "System/EventHandler.h"
#include <stdexcept>

namespace rendering {
namespace {
class ShadowDrawScope {
	CCamera* previous;
	CGame::GameDrawMode previousMode;
	bool previousShadow;
public:
	ShadowDrawScope()
		: previous(CCameraHandler::GetActiveCamera())
		, previousMode(game->GetDrawMode())
		, previousShadow(shadowHandler.InShadowPass())
	{
		CCameraHandler::SetActiveCamera(CCamera::CAMTYPE_SHADOW);
		game->SetDrawMode(CGame::gameShadowDraw);
		shadowHandler.SetNativeDrawPass(true);
	}
	~ShadowDrawScope()
	{
		shadowHandler.SetNativeDrawPass(previousShadow);
		game->SetDrawMode(previousMode);
		CCameraHandler::SetActiveCamera(previous->GetCamType());
		previous->Update();
	}
};
}

std::shared_ptr<const NativeWaterLuaFrame> CaptureNativeShadowLuaFrame(const NativeShadowColorFrame& frame)
{
	if (!game || !globalRendering || !frame.size || frame.sourceDrawFrame != globalRendering->drawFrame)
		throw std::logic_error("Native shadow Lua requires its actual source frame");
	if (!frame.redraw || !(shadowHandler.shadowGenBits & CShadowHandler::SHADOWGEN_BIT_PROJ))
		throw std::logic_error("Native shadow Lua called outside original transparent-shadow eligibility");
	ShadowDrawScope active;
	auto& recorder = ui::GetRecorder();
	// SetShadowCamera deliberately leaves CPU culling matrices in camera.
	// Its drawing matrices loaded into original GL are retained in this frame.
	recorder.BeginWorldStage(NativeWorldDrawStage::ShadowTransparent, frame.view, frame.projection,
		std::to_array(camera->GetBillBoardMatrix().m), true, frame.zeroToOneDepth);
	try {
		recorder.SetViewport({0, 0, frame.size, frame.size});
		recorder.Color({1, 1, 1, 1});
		recorder.SetBlend(ScreenBlendState{.enabled = false});
		recorder.SetAlphaTest(ScreenAlphaTest{.enabled = false});
		recorder.SetTextureEnabled(false);
		recorder.UpdateWorldState([&](NativeWorldDrawState& state) {
			// CreateShadows establishes depth writes, no blending and back
			// culling. DrawShadowTransparent's GL::SubState restores those
			// attributes; only a submitted particle draw enables RGB output.
			state.depthTest = true;
			state.depthWrite = true;
			state.depthCompare = ScreenAlphaCompare::LessEqual;
			state.cullEnabled = true;
			state.cull = NativeWorldCull::Back;
			const bool colorOutput = !frame.indices.empty();
			state.colorWrite = {colorOutput, colorOutput, colorOutput, false};state.combinedColorWrite=state.colorWrite;
		});
		auto result = CaptureNativeWaterLuaFrame([] { eventHandler.DrawShadowPassTransparent(); });
		recorder.EndWorldStage();
		return result;
	} catch (...) {
		recorder.EndWorldStage();
		throw;
	}
}
}
