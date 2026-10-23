/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWaterSceneData.h"
#include "NativeWorldParticleData.h"
#include "NativeWaterGroundSourceData.h"
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}
void ValidateNativeWaterSceneView(const NativeWaterSceneView& view, uint64_t expectedDrawFrame)
{
	Check(view.pass == NativeWaterScenePass::Reflection || view.pass == NativeWaterScenePass::Refraction,
		"Water offscreen scene must identify its original reflection/refraction pass");
	Check(view.preParticlesLua && view.worldLua && view.preParticlesBoundary > 0, "Water scene requires both original Lua callbacks and their actual ordering boundary");
	Check(view.initialClearSequence&&view.initialClearSequence<view.preParticlesBoundary,
		"Water scene lacks its original ordered color/depth clear");
	Check(bool(view.groundSource)==view.drawGround&&bool(view.groundCUS)==view.drawGround&&bool(view.groundLua)==view.drawGround,
		"Water ground invocation and its actual same-call source owners disagree");
	if(const auto& ground=view.groundSource){
		Check(ground->sourceDrawFrame==expectedDrawFrame&&ground->contextGeneration&&view.initialClearSequence<ground->source.begin&&ground->source.begin<ground->source.end&&
			ground->source.end<view.preParticlesBoundary,"Water ground source is stale or follows its original pre-particle boundary");
		Check(ground->stage==(view.pass==NativeWaterScenePass::Reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction),
			"Water ground source stage does not match its actual target");
	}
	const auto& frame = view.frame;
	Check(frame.sourceDrawFrame == expectedDrawFrame, "Water scene belongs to a different source draw");
	Check(!frame.waterScene && !frame.bumpWater && !frame.basicWater && !frame.legacyWater && !frame.dynamicWater, "Recursive water capture is not an original reflection input");
	Check(frame.terrain.heights && frame.terrain.tiles && frame.terrain.material && frame.environmentFrame,
		"Water scene lacks actual terrain/environment owners");
	Check(view.terrainClipPlane == (view.legacyReflective ? std::array<float,4>{0,1,0,0} : NativeWaterTerrainClipPlane(view.pass)) && view.modelClipPlane == NativeWaterModelClipPlane(view.pass),
		"Water reflection/refraction clipping differs from original source");
	Check(!view.legacyReflective || (view.pass==NativeWaterScenePass::Reflection && view.drawGround && view.frame.viewCamera.viewport[2]==512 && view.frame.viewCamera.viewport[3]==512), "Legacy water reflection source contract differs");
	Check(!view.dynamicWater || (view.drawGround && !view.legacyReflective && (frame.viewCamera.viewport[2]==512 || (view.pass==NativeWaterScenePass::Refraction && frame.viewCamera.viewport[2]==1024)) && frame.viewCamera.viewport[2]==frame.viewCamera.viewport[3]), "Dynamic scene original extent differs");
	Check(view.dynamicWater || view.drawFog == (view.pass == NativeWaterScenePass::Reflection), "Water refraction must preserve original disabled fog");
	Check(frame.viewCamera.clipDepth != ClipDepthConvention::Unsupported && frame.viewCamera.viewport[2] > 0 && frame.viewCamera.viewport[3] > 0,
		"Water scene requires its actual camera and viewport");
	for (const auto& matrix : {frame.viewCamera.view, frame.viewCamera.projection})
		for (float value : matrix) Check(std::isfinite(value), "Non-finite water scene matrix");
	Check(frame.particleFrame && frame.particleFrame->drawFrame == expectedDrawFrame,
		"Water scene requires original same-draw particle capture");
	Check(frame.particleFrame->drawReflection == (view.pass == NativeWaterScenePass::Reflection) &&
		frame.particleFrame->drawRefraction == (view.pass == NativeWaterScenePass::Refraction),
		"Water scene particle camera/mask does not match its pass");
	Check(!frame.particleFrame->softenRequested, "Original reflection/refraction does not soften particles against main-view depth");
}
void ValidateNativeWaterSceneFrame(const NativeWaterSceneFrame& scenes, uint64_t expectedDrawFrame,
	bool requireReflection, bool requireRefraction)
{
	Check(scenes.sourceDrawFrame == expectedDrawFrame, "Water scene collection source draw differs");
	Check(scenes.belowWaterLua && scenes.waterPostLua && scenes.aboveWaterLua, "Water scene lacks its original below/surface-post/above callback scopes");
	Check(bool(scenes.reflection) == requireReflection && bool(scenes.refraction) == requireRefraction,
		"Water scene pass presence differs from original effective settings");
	if (scenes.reflection) {
		Check(scenes.reflection->pass == NativeWaterScenePass::Reflection, "Reflection scene is misclassified");
		ValidateNativeWaterSceneView(*scenes.reflection, expectedDrawFrame);
	}
	if (scenes.refraction) {
		Check(scenes.refraction->pass == NativeWaterScenePass::Refraction, "Refraction scene is misclassified");
		ValidateNativeWaterSceneView(*scenes.refraction, expectedDrawFrame);
	}
}
} // namespace rendering
