/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4CUSSource.h"
#include "Rendering/Backend/NativeOriginalShadowParticlesData.h"

namespace rendering::metal4 {
// Uses the existing named shadow encoder. This family never clears an image,
// creates another render pass, starts a lease, submits, or waits.
class PreparedOriginalShadowParticles {
public:
	~PreparedOriginalShadowParticles();
	size_t DrawCount() const;
	uint64_t SequenceAt(size_t) const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void Finish();
	void DidComplete();
private:
	PreparedOriginalShadowParticles();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class OriginalShadowParticlesGPU;
};
class OriginalShadowParticlesGPU {
public:
	OriginalShadowParticlesGPU();
	~OriginalShadowParticlesGPU();
	// Called by the actual ProjFX shadow shader/variant construction owner.
	// Target-specific PSOs are compiled only for its real captured submission.
	std::shared_ptr<const void> CompileOriginalProgram(RuntimeLease&,
		bool textureArray,double& milliseconds);
	std::shared_ptr<PreparedOriginalShadowParticles> PrepareOriginalShadowParticles(RuntimeLease&,
		std::shared_ptr<const NativeOriginalShadowParticlesFrame>,const CUSSourceTarget&,
		const CUSSourceImageProvider&,bool validation,double& milliseconds);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
