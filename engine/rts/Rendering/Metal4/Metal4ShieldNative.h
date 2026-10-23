/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeShieldData.h"
#include "Rendering/Backend/NativeShieldDepthBacking.h"
#include "Rendering/Backend/NativeShieldTextureState.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <functional>
namespace rendering::metal4 {
// Actual cropped GBuffer producers, never the combined scene or shadow camera.
enum class ShieldDepthRole { TerrainOnly, ModelsOnly, CombinedScene, ShadowCamera };
enum class ShieldDepthOperation { Raster, ClearOne, AllocationOnly };
struct ShieldDepthProducerState {
	ShieldDepthRole role=ShieldDepthRole::CombinedScene;
	std::uint64_t sourceDrawFrame=0, identity=0;
	bool encoded=false, completed=false;
	ShieldDepthOperation operation=ShieldDepthOperation::Raster;
	bool allocationVerified=false; // Only after actual native storage allocation. No invented pixels.
	std::uint64_t submissionIdentity=0; // Actual common submission, not draw-frame or object identity.
};
// Allocates only a typed epoch, never texture pixels or encoded work.
std::shared_ptr<ShieldDepthProducerState> CreateShieldDepthProducer(ShieldDepthRole,std::uint64_t sourceDrawFrame);
std::uint64_t CreateShieldSubmissionIdentity(); // Once per actual shared world preparation; no GPU work.
struct ShieldDepthImage {
	id<MTLTexture> texture=nil;
	ShieldDepthRole role=ShieldDepthRole::CombinedScene;
	std::uint64_t sourceDrawFrame=0, producerIdentity=0;
	std::array<float,16> view{}, projection{};
	std::array<std::int32_t,4> viewport{};
	std::shared_ptr<const void> storageOwner; // Retains immutable producer epoch/cache ownership.
	std::shared_ptr<const ShieldDepthProducerState> producer;
};
struct ShieldDepthSources { ShieldDepthImage map, models;std::uint64_t submissionIdentity=0; };
// Validates the real image against the already published immutable version,
// then fulfills it exactly once. Retaining the prior backing is the no-draw path.
void FulfillShieldDepthBacking(const NativeShieldDepthBacking&,const ShieldDepthImage&);
const ShieldDepthImage& GetShieldDepthBackingImage(const NativeShieldDepthBacking&);
struct ShieldSampledImage {
	id<MTLTexture> texture=nil;
	id<MTLSamplerState> sampler=nil;
	std::shared_ptr<const void> owner;
	bool flipV=false;
	std::function<void()> requireReadable; // Optional real same-submit color producer check.
};
// Only needed when the actual shared binding history names a color image.
// It must resolve that retained object/version and exact sampler, never a
// currently bound substitute or a fabricated GBuffer/default texture.
using ShieldColorImageResolver=std::function<ShieldSampledImage(RuntimeLease&,const NativeCUSTextureBinding&)>;
class ShieldGPU;
class PreparedShield {
public:
	~PreparedShield();
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
	PreparedShield();struct Impl;std::unique_ptr<Impl> impl;friend class ShieldGPU;
};
class ShieldGPU {
public:
	ShieldGPU();~ShieldGPU();
	// Compilation is necessary but does not publish the paired family gate.
	void Initialize(RuntimeLease&,MTLPixelFormat,bool,double&,unsigned samples=1);
	std::unique_ptr<PreparedShield> Prepare(RuntimeLease&,const NativeShieldFrame&,const WorldCamera&,
		std::uint32_t renderWidth,std::uint32_t renderHeight,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		const ShieldDepthSources&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double&,
		const ShieldColorImageResolver& = {},unsigned outputSamples=1);
	void ClearResources(); // After common serialized retirement; retains real PSOs.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
