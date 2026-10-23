/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <array>
#include <memory>
#include <vector>
namespace rendering {
struct NativeCUSTextureObject;
using NativeSnowPoints = std::vector<std::array<float,4>>;
struct NativeSnowUniforms {
	float time=0, scale=0, sizeMult=1;
	std::array<float,3> speed{}, camPos{};
	std::array<float,4> snowColor{.8f,.8f,.9f,.66f};
};
struct NativeSnowPointState {
	bool sprite=false, coordReplace=false, upperLeft=true;
	std::array<float,3> attenuation{1,0,0};
	float sizeMin=0, sizeMax=1e9f, fadeThreshold=1;
};
struct NativeSnowDraw {
	std::uint64_t sequence=0;
	std::shared_ptr<const NativeSnowPoints> points;
	NativeSnowUniforms uniforms;
	NativeSnowPointState pointState;
	std::array<float,16> modelView{}, projection{};
	std::shared_ptr<const ScreenTexture> texture;
	std::shared_ptr<const NativeCUSTextureObject> originalTexture;
	ScreenSampler sampler;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeSnowFrame {
	std::uint64_t sourceDrawFrame=0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::vector<NativeSnowDraw> draws;
};
ClipDepthConvention ReadNativeSnowClipDepth(const NativeSnowFrame&);
void ValidateNativeSnowDraw(const NativeSnowDraw&);
class NativeSnowRecorder {
public:
	void Begin(std::uint64_t,std::shared_ptr<const NativeCUSUniformPublication>);
	void Record(NativeSnowDraw);
	std::shared_ptr<const NativeSnowFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeSnowFrame> frame;
};
void SetNativeSnowConsumerAvailable(bool) noexcept;
bool IsNativeSnowConsumerAvailable() noexcept;
}
