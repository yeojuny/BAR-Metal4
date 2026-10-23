/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCUSDeferredData.h"
#include "Rendering/Backend/NativeEngineModelState.h"
#include "Rendering/EngineBridge/NativeCUSSourceCapture.h"
#include "NativeEngineModelCapture.h"
#include "Rendering/Backend/NativeWorldDrawData.h"

namespace rendering {
struct NativeWaterLuaFrame;
struct NativeOpaqueModelPass {
	uint64_t sequence=0,postDeferredSequence=0;
	bool feature=false,deferred=false;
	bool postDeferredFixedFogEnabled=false;
	// GeometryBuffer::Clear at this exact deferred entry, before all model
	// callbacks. Keep this with its MRT owner, outside the scene clear queue.
	std::optional<NativeWorldAttachmentClear> initialClear;
	bool FullInitialClear() const noexcept {
		return initialClear&&initialClear->mask==static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth)&&
			initialClear->values.color==ScreenColor{0,0,0,0}&&initialClear->values.depth==1&&
			initialClear->colorWrite==std::array<bool,4>{true,true,true,true}&&
			initialClear->depthWrite&&!initialClear->scissor;
	}
	std::shared_ptr<const NativeEngineModelPhase> engine;
	// Same original source scope includes per-object callbacks, one whole
	// DrawOpaque*Lua callback and real legacy callbacks, in source sequence.
	std::shared_ptr<const NativeCUSSourceFrame> lua;
	// Typed callbacks share this exact engine/CUS/material source invocation.
	std::shared_ptr<const NativeWaterLuaFrame> luaEffects;
	std::shared_ptr<const NativeCUSDeferredPhase> mrt;
};
struct NativeOpaqueModelFrame {
	uint64_t sourceDrawFrame=0;
 bool drawReflection=false,drawRefraction=false;
	std::optional<NativeWorldDrawStage> alphaStage;
	// The real source scope ends before later DrawWorld callbacks. This ordinal
	// prevents final-pass generic commands from drifting into that later queue.
	uint64_t endSequence=0;
	// Exact engine order: unit deferred/forward, feature deferred/forward.
	// Optional original stages are absent, rather than reordered by type.
	std::vector<NativeOpaqueModelPass> passes;
	bool HasWriters()const noexcept;
};
void CancelNativeOpaqueModels(NativeTextureRegistry&,const NativeOpaqueModelFrame&)noexcept;

struct NativeOpaqueModelCallbacks {
	// Graph readiness must include the native standard/legacy remainder and
	// all selected callbacks, before original LuaUI initialization queries it.
	std::function<bool()> ready;
	NativeEngineModelSourceHooks engine;
	std::function<NativeEngineModelTextureNames()> commonTextures;
	// Runs after ResetOpaqueDrawing, at the real legacy material-bin site.
	// Required even for empty observed bins; never an implicit omitted list.
	std::function<void(NativeEngineModelPhase&,bool deferred)> legacyMaterials;
	// Executes the supplied actual PostDeferred event at that exact point.
	// The surrounding renderer owns typed/generic callback capture scopes.
	std::function<void(bool feature,const std::function<void()>&)> postDeferred;
};

class NativeOpaqueModelCapture {
public:
	NativeOpaqueModelCapture(WorldFrameCapture&,NativeCUSSourceCapture&,
		NativeTextureContext&,ui::NativeUIRecorder&);
	std::shared_ptr<const NativeOpaqueModelFrame> Capture(uint64_t sourceDrawFrame,
		const WorldCamera& actualView,const NativeOpaqueModelCallbacks&,bool drawReflection=false,bool drawRefraction=false);
	std::shared_ptr<const NativeOpaqueModelFrame> CaptureAlpha(uint64_t sourceDrawFrame,
		const WorldCamera& actualView,NativeWorldDrawStage,const NativeOpaqueModelCallbacks&,bool drawReflection=false,bool drawRefraction=false);
private:
	NativeEngineModelCapture models;
	NativeCUSSourceCapture& cus;
	NativeTextureContext& textures;
	ui::NativeUIRecorder& recorder;
};
} // namespace rendering
