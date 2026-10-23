/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4CUSSource.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <memory>

namespace rendering::metal4 {
using MapEdgeImageProvider = std::function<CUSSourceImage(RuntimeLease&, const std::shared_ptr<const WorldTexture>&)>;
class MapEdgeGPU;
class PreparedMapEdge {
public:
	~PreparedMapEdge();
	void Encode(id<MTL4RenderCommandEncoder> encoder) const;
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	void FinishDeferredDraw() const;
	void DidComplete() const;
	uint32_t MirrorInstances() const;
private:
	PreparedMapEdge();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class MapEdgeGPU;
};
// Private paired consumers of the exact original map-edge family. Reuse the
// caller's device/queue/lease/pass/depth/viewport; never presents or reads files.
class MapEdgeGPU {
public:
	MapEdgeGPU();
	~MapEdgeGPU();
	std::unique_ptr<PreparedMapEdge> Prepare(RuntimeLease& lease,const WorldFrame& frame,
		MTLPixelFormat colorFormat,bool shaderValidation,double& shaderMilliseconds,
		const MapEdgeImageProvider& images,unsigned outputSamples=1);
	// The actual callback owns this packet, phase and five-color/D32 target.
	// This consumer neither opens a pass nor substitutes for PreUnit forward.
	std::shared_ptr<PreparedMapEdge> PrepareDeferred(RuntimeLease&,std::shared_ptr<const mapedge::DrawPacket>,
		const CUSSourceTarget&,const CUSSourceImageProvider&,bool validation,double& shaderMilliseconds);
	void ClearResources(); // Only after the shared runtime's previous retirement.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
