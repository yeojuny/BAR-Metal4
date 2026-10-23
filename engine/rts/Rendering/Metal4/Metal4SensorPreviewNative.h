/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeSensorPreviewData.h"
#include <functional>
namespace rendering::metal4 {
class ScreenGPU;
struct PreparedSensorPreviewImage {
	id<MTLTexture> texture=nil;
	id<MTLSamplerState> sampler=nil;
	std::shared_ptr<const NativeCUSTextureObject> original;
	std::shared_ptr<const NativePostprocessImage> allocation;
	std::shared_ptr<const NativePostprocessVersion> version;
	std::shared_ptr<const void> storageOwner;
	bool flipV=false;
	std::function<void()> requireReadable;
	std::function<void(id<MTL4CommandBuffer>)> encodeDependencies;
	std::function<void()> didComplete;
};
using SensorPreviewImageResolver=std::function<PreparedSensorPreviewImage(RuntimeLease&,
	const NativeSensorPreviewImageInput&,bool requireContents,NativePostprocessConsumer)>;
struct SensorPreviewTarget {
	std::uint32_t width=0,height=0,logicalWidth=0,logicalHeight=0;
	std::array<std::int32_t,4> cameraViewport{};
	MTLPixelFormat color=MTLPixelFormatInvalid,depth=MTLPixelFormatInvalid,stencil=MTLPixelFormatInvalid;
	unsigned samples=1;
	// RTT-only storage, allocated by the SAME shared postprocess owner.
	id<MTLTexture> output=nil,previous=nil;
	std::shared_ptr<const NativePostprocessVersion> outputVersion,previousVersion;
	std::function<void()> requirePreviousReadable;
};
class PreparedSensorPreviewCommand {
public:
	~PreparedSensorPreviewCommand();
	bool Empty()const;
	bool IsTarget()const;
	bool Encoded()const;
	void EncodeDependencies(id<MTL4CommandBuffer>)const;
	void DidComplete()const;
	std::shared_ptr<const NativePostprocessVersion> OutputVersion()const;
	// Final World/Minimap draw inside the caller's actual attachment encoder.
	void Encode(id<MTL4RenderCommandEncoder>)const;
	// Caller stores/ends its main encoder before this actual original RTT.
	// It resumes that same scene after this command; no private submit/wait.
	void EncodeTarget(id<MTL4CommandBuffer>)const;
private:
	PreparedSensorPreviewCommand();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class SensorPreviewGPU;
};
class SensorPreviewGPU {
public:
	SensorPreviewGPU();~SensorPreviewGPU();
	std::shared_ptr<const NativeSensorPreviewProgramConsumer> CompileOriginalStages(RuntimeLease&,
		const NativeSensorPreviewProgramSource&,double& shaderMilliseconds);
	std::unique_ptr<PreparedSensorPreviewCommand> Prepare(RuntimeLease&,const NativeSensorPreviewCommand&,
		const SensorPreviewTarget&,const SensorPreviewImageResolver&,ScreenGPU& actualSharedScreen,
		bool validation,double& shaderMilliseconds,NativePostprocessConsumer);
	void ClearResources(); // Normal common frame retirement precedes release.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
