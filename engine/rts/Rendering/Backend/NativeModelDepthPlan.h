/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering {
enum class NativeModelDepthAlpha { CUSOpaque, CUSAlphaMask, EngineGL4, EngineLegacy };
struct NativeModelDepthObject {
	bool feature=false;
	std::uint32_t sourceIndex=0;
	std::int32_t objectId=-1;
	NativeModelDepthAlpha alpha=NativeModelDepthAlpha::CUSOpaque;
	std::vector<std::uint32_t> pieces;
};
struct NativeModelDepthSelection {
	bool drawDeferredEnabled=false,geometryBufferValid=false,canDrawDeferred=false;
	bool unitsDeferred=true,featuresDeferred=true,allowSecondClear=false;
};
struct NativeModelDepthSourcePhase {bool feature=false,clearBefore=true;};
struct NativeModelDepthPlan {
	std::vector<NativeModelDepthObject> objects;
	std::vector<NativeModelDepthSourcePhase> phases; // Empty phases still clear if source does.
	// Producer matches CURRENT actual native model geometry. These requested
	// original deformation families still require their separate parity gates.
	std::vector<std::string> pendingOriginalGeometry;
};
NativeModelDepthPlan BuildNativeModelDepthPlan(const WorldFrame&,const NativeModelDepthSelection&);
}
