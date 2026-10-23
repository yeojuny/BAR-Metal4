/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeWorldParticleData.h"
#include "Rendering/Backend/NativeLuaNanoData.h"
#include "Rendering/Backend/NativeShadowColorData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <memory>
#include <functional>

namespace rendering::metal4 {
class PreparedShadowColor {
public:
	~PreparedShadowColor();
	void EncodeDependencies(id<MTL4CommandBuffer>) const; // actual atlas mips, before first scene encoder
	void Encode(id<MTL4CommandBuffer>,const std::function<void(id<MTL4RenderCommandEncoder>)>& afterParticles={}) const; // same original pass
	void DidComplete() const; // only after feedback AND event retirement
	id<MTLTexture> Color() const; // RGB1 RGBA8 view or RRR1 R8 view; one level
	id<MTLTexture> RawColor() const; // Original storage before sampler swizzle.
	std::uint32_t SourceDrawFrame() const;
	std::uint32_t DrawCount() const;
private:
	PreparedShadowColor();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class WorldParticlesGPU;
};
class WorldParticlesGPU;
class PreparedWorldParticles {
public:
	~PreparedWorldParticles();
	// Before the first scene render encoder; registers and generates the
	// original requested atlas mip chain on the same Metal4 command buffer.
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	void Encode(NativeWorldParticleStage stage,id<MTL4RenderCommandEncoder> encoder,id<MTLDepthStencilState> inheritedAlphaDepth=nil) const;
	void DidComplete() const; // Only after feedback AND shared-event retirement.
	uint32_t DrawCount(NativeWorldParticleStage stage) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder,id<MTLDepthStencilState> inheritedAlphaDepth=nil) const;
private:
	PreparedWorldParticles();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class WorldParticlesGPU;
};
class WorldParticlesGPU {
public:
	WorldParticlesGPU();~WorldParticlesGPU();
	std::unique_ptr<PreparedWorldParticles> Prepare(RuntimeLease& lease,uint64_t actualSubmission,
		const NativeWorldParticleFrame& frame,const WorldCamera& viewCamera,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,id<MTLTexture> terrainDepth,
		bool shaderValidation,double& shaderMilliseconds,uint32_t disabledAlphaClipStages=0,unsigned outputSamples=1);
	// Called by the connected WorldRenderer before original gadget init.
	// Real PSO creation establishes only the narrowly named original family.
	void InitializeLuaNano(RuntimeLease& lease,MTLPixelFormat colorFormat,bool shaderValidation,double& shaderMilliseconds,unsigned outputSamples=1);
	std::unique_ptr<PreparedWorldParticles> PrepareLuaNano(RuntimeLease& lease,
		const NativeLuaNanoFrame& frame,const WorldCamera& viewCamera,float animationFrame,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,unsigned outputSamples=1);
	std::unique_ptr<PreparedShadowColor> PrepareShadowColor(RuntimeLease&,uint64_t actualSubmission,
		std::shared_ptr<const NativeShadowColorFrame>,id<MTLTexture> actualShadowDepth,
		bool shaderValidation,double& shaderMilliseconds,id<MTLTexture> actualNamedColor=nil);
	// The same persistent atlas upload owner used by ordinary world particles.
	// A named 2D atlas exposes its actual single slice as2D; array atlases retain
	// their actual page count. The original captured binding supplies sampling.
	CUSSourceImage AtlasImage(RuntimeLease&,const NativeCUSTextureBinding&,id<MTLSamplerState>,uint64_t actualSubmission);
	// Actual common preparation entry, also when this frame uses no atlas.
	// Discards only unsubmitted/retired image plans; keeps the persistent cache.
	void PrepareAtlasImages(uint64_t actualSubmission);
	void EncodeAtlasImages(id<MTL4CommandBuffer>,uint64_t actualSubmission);
	void CompleteAtlasImages(uint64_t actualSubmission); // same successful common wait
	void ClearResources(); // Only after the same runtime's previous retirement.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
