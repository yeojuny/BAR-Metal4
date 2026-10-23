/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeNapalmPreviewData.h"
#include "NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool valid,const char* reason) { if (!valid) throw std::invalid_argument(reason); }
}
ClipDepthConvention ReadNativeNapalmPreviewClipDepth(const NativeNapalmPreviewFrame& source)
{
	Check(bool(source.original),"Original Napalm preview render-cap publication is missing");
	ValidateNativeCUSUniformPublication(*source.original);
	Check(source.original->params->sourceDrawFrame<=source.sourceDrawFrame,"Original Napalm preview render-cap publication is from a future draw");
	std::uint32_t caps; std::memcpy(&caps,source.original->params->bytes.data()+12,4);
	return caps&1u?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
}
void ValidateNativeNapalmPreviewDraw(const NativeNapalmPreviewDraw& draw)
{
	Check(draw.stage==NativeWorldDrawStage::PreUnit,"Original Napalm preview draw is outside DrawWorldPreUnit");
	Check(draw.primitive==NativeDrawPrimitive::TriangleFan || draw.primitive==NativeDrawPrimitive::TriangleStrip,
		"Original Napalm preview source primitive differs");
	Check(draw.vertices.size()<=std::numeric_limits<std::uint32_t>::max(),"Original Napalm preview vertex range exceeds index storage");
	for (const auto& vertex:draw.vertices) for (float f:vertex) Check(std::isfinite(f),"Original Napalm preview vertex is nonfinite");
	for (float f:draw.modelViewProjection) Check(std::isfinite(f),"Original Napalm preview matrix is nonfinite");
	for (float f:draw.color) Check(std::isfinite(f),"Original Napalm preview color is nonfinite");
	for (float f:draw.center) Check(std::isfinite(f),"Original Napalm preview center is nonfinite");
	Check(std::isfinite(draw.time),"Original Napalm preview time is nonfinite");
}
std::vector<std::uint32_t> NativeNapalmPreviewTriangleIndices(const NativeNapalmPreviewDraw& draw)
{
	ValidateNativeNapalmPreviewDraw(draw); std::vector<std::uint32_t> result;
	if (draw.vertices.size()<3) return result;
	Check(draw.vertices.size()-2<=std::numeric_limits<std::size_t>::max()/3,"Original Napalm preview triangle storage overflows");
	result.reserve((draw.vertices.size()-2)*3);
	for (std::uint32_t n=2;n<draw.vertices.size();++n) {
		if (draw.primitive==NativeDrawPrimitive::TriangleFan) result.insert(result.end(),{0,n-1,n});
		else if (n&1u) result.insert(result.end(),{n-1,n-2,n});
		else result.insert(result.end(),{n-2,n-1,n});
	}
	return result;
}
void NativeNapalmPreviewRecorder::Begin(std::uint64_t sourceDrawFrame,std::shared_ptr<const NativeCUSUniformPublication> original)
{
	if (frame) throw std::logic_error("Original Napalm preview frame is already active");
	auto next=std::make_shared<NativeNapalmPreviewFrame>(); next->sourceDrawFrame=sourceDrawFrame; next->original=std::move(original);
	(void)ReadNativeNapalmPreviewClipDepth(*next); frame=std::move(next);
}
void NativeNapalmPreviewRecorder::Record(NativeNapalmPreviewDraw draw)
{
	if (!frame) throw std::logic_error("Original Napalm preview draw has no active frame");
	ValidateNativeNapalmPreviewDraw(draw); draw.sequence=NextNativeWorldDrawSequence(); frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeNapalmPreviewFrame> NativeNapalmPreviewRecorder::End()
{
	if (!frame) throw std::logic_error("Original Napalm preview frame is inactive");
	return std::exchange(frame,{});
}
void NativeNapalmPreviewRecorder::Abort() noexcept { frame.reset(); }
void SetNativeNapalmPreviewConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeNapalmPreviewConsumerAvailable() noexcept { return available.load(); }
}
