/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeOriginalShadowParticlesProgramSource.h"
#include "Rendering/Backend/NativeEngineModelData.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <functional>
#include <span>
struct VA_TYPE_PROJ;
namespace rendering {
class NativeOriginalShadowParticlesCapture {
public:
	NativeOriginalShadowParticlesCapture(NativeTextureContext&,ui::NativeUIRecorder&,NativeEngineModelClipState&);
	std::shared_ptr<const NativeOriginalShadowParticlesFrame> Capture(uint64_t sourceDrawFrame,unsigned size,bool colored,const std::function<void()>& actualDrawShadowTransparent);
	void EnableColorOutput();
	void WithOriginalSubState(const std::function<void()>&);
	void EnableProgram();void SetTextureArray(bool);void SetShadowColorMode(float);void DisableProgram();
	void Submit(uint32_t mode,std::span<const VA_TYPE_PROJ>,std::span<const uint32_t>);
	void UnbindCurrent2D();
private:
	NativeTextureContext& textures;ui::NativeUIRecorder& recorder;NativeEngineModelClipState& clips;
	NativeOriginalShadowParticlesFrame* frame=nullptr;
	bool subState=false;
	void RequireScope()const;
};
NativeOriginalShadowParticlesCapture* CurrentNativeOriginalShadowParticlesCapture()noexcept;
}
