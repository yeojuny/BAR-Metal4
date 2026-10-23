/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <array>
#include <memory>
#include <vector>

namespace rendering {
// Original Attack AoE preview, not the damaging ground-fire effect. The
// unchanged widget supplies sector/fan vertices and os.clock-based uniforms.
struct NativeNapalmPreviewDraw {
	std::uint64_t sequence=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::PreUnit;
	NativeDrawPrimitive primitive=NativeDrawPrimitive::TriangleFan;
	std::vector<std::array<float,4>> vertices;
	std::array<float,16> modelViewProjection{};
	std::array<float,4> color{1,0,0,.5f};
	std::array<float,2> center{};
	float time=0;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeNapalmPreviewFrame {
	std::uint64_t sourceDrawFrame=0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::vector<NativeNapalmPreviewDraw> draws;
};
ClipDepthConvention ReadNativeNapalmPreviewClipDepth(const NativeNapalmPreviewFrame&);
void ValidateNativeNapalmPreviewDraw(const NativeNapalmPreviewDraw&);
std::vector<std::uint32_t> NativeNapalmPreviewTriangleIndices(const NativeNapalmPreviewDraw&);
class NativeNapalmPreviewRecorder {
public:
	void Begin(std::uint64_t sourceDrawFrame,std::shared_ptr<const NativeCUSUniformPublication>);
	void Record(NativeNapalmPreviewDraw);
	std::shared_ptr<const NativeNapalmPreviewFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeNapalmPreviewFrame> frame;
};
void SetNativeNapalmPreviewConsumerAvailable(bool) noexcept;
bool IsNativeNapalmPreviewConsumerAvailable() noexcept;
}
