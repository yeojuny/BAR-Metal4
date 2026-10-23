/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4CUSSource.h"
#include "Rendering/EngineBridge/NativeTerrainProgramSource.h"

namespace rendering::metal4 {
// Geometry is retained from each original Basic/ROAM source submission.
// The provider reuses existing image/producer owners. Its image is unswizzled
// and begins at the original GL object's level0 (including SMF streaming).
// This consumer applies base/max levels, depth mode and swizzle exactly once.
// The passed sampler is the actual source sampler, not a screen approximation.
using TerrainSourceImageProvider = std::function<CUSSourceImage(RuntimeLease&,
	const NativeCUSTextureBinding&, unsigned originalUnit, id<MTLSamplerState> actualSampler)>;

class PreparedTerrainSource {
public:
	~PreparedTerrainSource();
	size_t DrawCount() const;
	uint32_t EncodedDrawCount() const; // Actual indexed/arrays calls, not source-square events.
	uint64_t SequenceAt(size_t) const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t, id<MTL4RenderCommandEncoder>);
	void FinishOrderedDraws();
	void DidComplete();
private:
	PreparedTerrainSource();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class TerrainSourceGPU;
};

class TerrainSourceGPU {
public:
	TerrainSourceGPU();
	~TerrainSourceGPU();
	// One immutable translated original library per successful original link.
	// The genuine link target primes a real PSO; later raster/blend/attachment
	// variants retain that same library. DeferredAdvanced requires five RGBA8
	// Map colors plus D32 depth/no stencil; forward/Border require one color.
	// Compile failures always propagate; an output family never falls back.
	NativeTerrainProgramLink CompileOriginalProgram(RuntimeLease&,
		const NativeTerrainProgramRequest&, const CUSSourceTarget& linkTarget,
		bool validation, double& compileMilliseconds);
	// The actual target selects draw.deferred or forward/Border submissions
	// from the same immutable source frame, retaining their original ordinals.
	// FinishOrderedDraws/DidComplete apply separately to each prepared list.
	std::shared_ptr<PreparedTerrainSource> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeWaterGroundSourceFrame>,
		const CUSSourceTarget&, const TerrainSourceImageProvider&,
		bool validation, double& compileMilliseconds);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
