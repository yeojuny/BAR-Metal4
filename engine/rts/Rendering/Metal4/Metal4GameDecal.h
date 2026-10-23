/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Metal4WorldInfo.h"
#include "Rendering/Backend/NativeGameDecalData.h"
namespace rendering::metal4 {
struct GameDecalSceneImage {
	__strong id<MTLTexture> texture=nil;
	ScreenSampler sampler;
	bool flipV=false;
};
struct GameDecalSceneImages {
	std::uint64_t sourceDrawFrame=0;
	// Height, original map minimap, current $info, shadow depth, map RG normals.
	// No terrain-depth copy and no independent info/normal/height re-capture.
	std::array<GameDecalSceneImage,5> images;
	std::shared_ptr<PreparedWorldInfo> infoOwner;
	std::shared_ptr<const NativeWorldInfoFrame> requestedInfo;
};
class GameDecalGPU;
class PreparedGameDecal {
public:
	~PreparedGameDecal();
	std::size_t DrawCount()const;
	std::uint64_t SequenceAt(std::size_t)const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>)const;
private:
	PreparedGameDecal();struct Impl;std::unique_ptr<Impl> impl;friend class GameDecalGPU;
};
class GameDecalGPU {
public:
	GameDecalGPU();~GameDecalGPU();
	// Real PSOs compile before root may publish the private consumer readiness.
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		std::uint32_t samples,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedGameDecal> Prepare(RuntimeLease&,const NativeGameDecalFrame&,const GameDecalSceneImages&,
		const WorldCamera&,std::uint32_t renderWidth,std::uint32_t renderHeight,
		std::uint32_t logicalWidth,std::uint32_t logicalHeight,ScreenGPU&,const PreparedScreenFrame*,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,std::uint32_t samples,bool validation,double& shaderMilliseconds);
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
