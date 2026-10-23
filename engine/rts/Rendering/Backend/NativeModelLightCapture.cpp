/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeModelLightCapture.h"
#include "Rendering/Common/ModelDrawer.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/GlobalSynced.h"

namespace rendering {
std::shared_ptr<const NativeModelLightFrame> CaptureNativeModelLights()
{
	auto* handler=CModelDrawerConcept::GetLightHandler();
	auto result=std::make_shared<NativeModelLightFrame>();
	result->sourceDrawFrame=globalRendering->drawFrame;
	result->simulationFrame=gs->frameNum;
	result->baseLight=handler->GetBaseLight();
	result->configuredLights=handler->GetMaxLights();
	handler->Update(nullptr);
	result->lights=handler->CaptureNativeUniforms();
	ValidateNativeModelLightFrame(*result);
	return result;
}
}
