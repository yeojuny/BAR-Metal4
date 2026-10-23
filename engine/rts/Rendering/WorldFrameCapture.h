/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Backend/WorldFrameData.h"
#include "Backend/NativeWaterSceneData.h"

#include <memory>

struct S3DModel;
class CUnit;
class CFeature;
class CSMFReadMap;

namespace rendering {
struct NativeEngineModelObject;
struct NativeMiniMapBackground;

// Call on the normal unsynced draw thread after the engine's CPU map, camera
// and unit-draw updates, at a stable simulation boundary. No engine state is
// written; only this adapter's resource caches change. Reset before map unload.
class WorldFrameCapture {
public:
	WorldFrameCapture();
	~WorldFrameCapture();
	WorldFrameCapture(const WorldFrameCapture&) = delete;
	WorldFrameCapture& operator=(const WorldFrameCapture&) = delete;

	void Reset();
	// Native counterpart of the original pre-unit IBL CPU stage. Call once
	// after DrawGenesis and before DrawWorldPreUnit. Consumes original dirty
	// flags once into retained revisions; performs no GL/GPU work or camera
	// mutation. Capture() later attaches this immutable stage snapshot.
	std::shared_ptr<const NativeWorldEnvironmentFrame> PrepareEnvironmentBeforePreUnit();
	std::shared_ptr<const WorldTerrainHeights> PrepareMapHeights();
	// Actual SMF writer shares the existing height and texture source cache.
	std::shared_ptr<const NativeMiniMapBackground> CaptureTerrainShadingSource(
		const CSMFReadMap& actualSourceMap,std::uint64_t actualWriteRevision);
	WorldFrame Capture(NativeWaterScenePass pass = NativeWaterScenePass::Main,
		std::shared_ptr<const NativeOriginalShadowFrame> originalShadow = {});
	std::shared_ptr<const WorldTexture> CaptureTexture(const std::string& path);
	// Original source loader shares this cache. Missing/failed source decode
	// returns null; strict bounds/layout/unsupported-format errors still throw.
	std::shared_ptr<const WorldTexture> CaptureTextureSource(const std::string& path, bool reload);
	// Engine-side provider for original projectile model capture. The returned
	// immutable model shares the same map-scoped geometry/texture cache as units
	// and features. Call only on the stable unsynced draw thread.
	std::shared_ptr<const WorldModel> CaptureModel(const S3DModel* model);
	// No visibility/material selection here: callers have already run the
	// original drawer predicate at the original source draw position.
	std::shared_ptr<const NativeEngineModelObject> CaptureEngineUnit(const CUnit&,bool modern,const WorldMatrix& actualModelView,const std::array<float,4>& actualProgramTeam);
	std::shared_ptr<const NativeEngineModelObject> CaptureEngineFeature(const CFeature&,bool modern,const WorldMatrix& actualModelView,const std::array<float,4>& actualProgramTeam);
	std::shared_ptr<const NativeEngineModelObject> CaptureEngineStatic(const S3DModel*,const WorldMatrix&,int definition,int team,bool modern,const std::array<float,4>& actualProgramTeam);

private:
	struct State;
	std::unique_ptr<State> state;
};

} // namespace rendering
