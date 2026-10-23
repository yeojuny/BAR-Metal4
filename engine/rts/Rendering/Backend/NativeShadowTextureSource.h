/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include <atomic>

namespace rendering {
enum class NativeShadowTextureKind { Depth, Color };
enum class NativeShadowTextureOperation { InitialClear, DepthPass, ColorPass };
struct NativeShadowTextureRequest {
	NativeShadowTextureKind kind=NativeShadowTextureKind::Depth;
	uint32_t size=0,originalFormat=0;
	bool colored=true;
};
struct NativeShadowTextureResource {virtual ~NativeShadowTextureResource()=default;};
struct NativeShadowTextureBacking final:NativeTextureBacking {
	NativeShadowTextureRequest request;
	NativeShadowTextureOperation operation=NativeShadowTextureOperation::InitialClear;
	std::shared_ptr<const NativeShadowTextureResource> resource;
	mutable std::atomic<uint64_t> encodedSubmission{0};
	mutable std::atomic<bool> completed{false};
	NativeTextureBackingKind Kind()const noexcept override {
		// A shadow-depth image is neither a map nor a model GBuffer image.
		return request.kind==NativeShadowTextureKind::Color?NativeTextureBackingKind::ColorImage:NativeTextureBackingKind::Other;
	}
};
struct NativeShadowTextureWrite {
	std::shared_ptr<const NativeCUSTextureObject> previous,current;
	std::shared_ptr<const NativeShadowTextureBacking> backing;
};
struct NativeShadowTextureFrame {
	uint64_t sourceDrawFrame=0;
	NativeShadowTextureWrite depth,color;
	// A zero-intensity sun clears depth, but does not write shadow color.
	std::shared_ptr<const NativeShadowTextureBacking> retainedColor;
	bool redrawColor=false;
};
struct NativeShadowTextureServices {
	// This is the installed actual native shadow graph, not GL/FBO support.
	std::function<bool()> ready;
	std::function<std::shared_ptr<const NativeShadowTextureResource>(const NativeShadowTextureRequest&)> allocate;
	std::function<bool(const NativeShadowTextureBacking&,const NativeShadowTextureBacking&,bool compatiblePreset)> checkFramebuffer;
	std::function<uint64_t()> sourceDrawFrame;
	// Actual paired native depth-rendering support. HEADLESS's unprobed GL
	// default is not this backend's depth capability.
	std::function<int()> depthBufferBits;
	// Actual engine/Lua shadow source capture, under the original shadow
	// camera and draw mode. It must retain the frame for the common submit.
	std::function<void(std::shared_ptr<const NativeShadowTextureFrame>)> capture;
};
class NativeShadowTextureSource {
public:
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&,NativeShadowTextureServices);
	void Detach(); // After the actual ShadowHandler Kill and GPU retirement.
	bool Attached()const noexcept{return registry!=nullptr;}
	bool Ready()const;
	int SupportedDepthBits()const;
	// The actual InitFBOAndTextures call. Preserves both preset attempts,
	// current active-unit binding, final Bind0, and original RGB1/RRR1 state.
	bool Initialize(uint32_t& depth,uint32_t& color,uint32_t size,uint32_t originalDepthFormat,bool colored);
	void Delete(uint32_t);
	void Bind(uint32_t);
	void SetupSampler(unsigned actualUnit,bool enable);
	void ResetSampler(unsigned actualUnit,bool disable);
	void SetupSamplerRaw();
	void ResetSamplerRaw();
	std::shared_ptr<const NativeShadowTextureBacking> Image(uint32_t)const;
	// Called once at the original CreateShadows source boundary, before IBL
	// reflection and PreUnit. Preparation/consumption never advances history.
	std::shared_ptr<const NativeShadowTextureFrame> BeginFrame(uint32_t depth,uint32_t color,bool sunPositive);
	void Capture(uint32_t depth,uint32_t color,bool sunPositive);
	void Cancel(const NativeShadowTextureFrame&)noexcept;
private:
	NativeTextureRegistry* registry=nullptr;
	NativeCUSTextureHistory* history=nullptr;
	NativeShadowTextureServices services;
	uint32_t depthName=0;
	uint32_t Create(const NativeShadowTextureRequest&,bool compatiblePreset);
	NativeShadowTextureWrite Advance(uint32_t,NativeShadowTextureOperation);
	void SamplerRaw(bool compare);
};
NativeShadowTextureSource& OriginalNativeShadowTextures();
} // namespace rendering
