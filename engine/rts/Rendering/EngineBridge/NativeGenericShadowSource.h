/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeGenericShadowData.h"
#include "Rendering/Backend/NativeEngineModelData.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <span>
namespace rendering {
// The sole common recorder retains the command and assigns its ordinal.
// This leaf snapshots original inputs only; it has no queue or replay loop.
class NativeGenericShadowSource {
public:
	NativeGenericShadowSource(NativeTextureContext&,ui::NativeUIRecorder&,NativeEngineModelClipState&);
	std::shared_ptr<const NativeGenericShadowInput> Snapshot(std::span<const NativeGenericShadowVertex>,
		const std::array<float,16>& modelView,const std::array<float,16>& projection,bool zeroToOneDepth)const;
private:
	NativeTextureContext& textures;ui::NativeUIRecorder& recorder;NativeEngineModelClipState& clips;
};
}
