/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeMiniMapData.h"
#include <array>
#include <cstdint>
#include <memory>
namespace rendering::metal4 {
// Appended to the shared screen shader; uses that vertex/layout/clip contract.
const char* MiniMapScreenShaderSource();
class MiniMapGPU;
struct PreparedMiniMapHeight {
	__strong id<MTLTexture> texture=nil;
	std::shared_ptr<const WorldTerrainHeights> source;
};
class PreparedMiniMap {
public:
	~PreparedMiniMap();
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	void DidComplete() const;
	void RegisterInfoResources(RuntimeLease&) const; // retained deferred global-info plans only
	id<MTLTexture> Shading() const;
	// Individual original engine image producers used by exact PiP programs.
	// Their generation is valid only after this plan's dependencies encode.
	id<MTLTexture> Height() const;
	id<MTLTexture> GroundLOS() const;
	id<MTLTexture> Radar() const;
	id<MTLTexture> Info() const;
	id<MTLBuffer> Uniforms() const;
private:
	PreparedMiniMap();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class MiniMapGPU;
};
// Uses the same serialized lease/device/queue and retirement as its screen
// consumer. Cached native resources retain their immutable CPU owners.
class MiniMapGPU {
public:
	MiniMapGPU();~MiniMapGPU();
	// Compile real source pipelines before exposing paired native controls.
	void Initialize(RuntimeLease&,bool validation,double& shaderMs);
	void ClearResources(); // Between retired submissions; preserves compiled programs.
	std::shared_ptr<const PreparedMiniMapHeight> PrepareHeight(RuntimeLease&,std::shared_ptr<const WorldTerrainHeights>);
	// Actual original SMF FBO write, into the caller's retained image version.
	// The caller owns prior-image copying; only this exact half-open rectangle
	// is shaded by the existing MiniMapShade program, with no base/info upload.
	std::shared_ptr<PreparedMiniMap> PrepareShadingImage(RuntimeLease&,
		std::shared_ptr<const NativeMiniMapBackground>,id<MTLTexture> destination,
		std::array<int32_t,4> writtenRect,bool validation,double& shaderMs);
	// Shared global combiner producer. No shading/base upload or independent
	// temporal history: caller supplies the real ordered RGB10A2 target.
	std::shared_ptr<PreparedMiniMap> PrepareInfoImage(RuntimeLease&,
		std::shared_ptr<const NativeMiniMapInfo>,std::shared_ptr<const NativeMiniMapCoverage> radarGroundLos,bool radarGroundGlobalLos,
		id<MTLTexture> destination,std::uint32_t squaresX,std::uint32_t squaresZ,
		bool clear,bool validation,double& shaderMs);
	std::shared_ptr<PreparedMiniMap> Prepare(RuntimeLease&,std::shared_ptr<const NativeMiniMapBackground>,bool validation,double& shaderMs,bool individualImagesOnly=false);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
