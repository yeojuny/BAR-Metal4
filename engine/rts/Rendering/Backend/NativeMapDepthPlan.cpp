/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMapDepthPlan.h"
#include <stdexcept>
namespace rendering {
NativeMapDepthAction SelectNativeMapDepthAction(const NativeMapDepthSelection& s)
{
	if(unsigned(s.pass)>unsigned(NativeMapDepthPass::TerrainReflection))
		throw std::invalid_argument("Original map GBuffer pass is invalid");
	// CSMFGroundDrawer::Draw: neither branch reaches or clears the GBuffer.
	if(!s.drawGround||s.onlyVoidWater||!s.requestedDeferred)
		return NativeMapDepthAction::Retain;
	// DrawDeferredPass skips these before changing depth storage.
	if(!s.geometryBufferValid||s.pass!=NativeMapDepthPass::Main)
		return NativeMapDepthAction::Retain;
	// SMFRenderStateGLSL::CanDrawDeferred -> CanUseAdvShading. A standard
	// fallback is an actual clear-one pass, not the forward terrain depth.
	return s.selectedDeferredShaderValid&&s.advancedShading
		?NativeMapDepthAction::DrawTerrain:NativeMapDepthAction::ClearOne;
}
} // namespace rendering
