/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeSnowData.h"
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
void Check(bool valid,const char* why) { if(!valid) throw std::invalid_argument(why); }
}
ClipDepthConvention ReadNativeSnowClipDepth(const NativeSnowFrame& source)
{
	Check(bool(source.original),"Original Snow lacks its render-cap publication");
	ValidateNativeCUSUniformPublication(*source.original);
	Check(source.original->params->sourceDrawFrame<=source.sourceDrawFrame,"Original Snow render-cap publication is from a future draw");
	std::uint32_t caps; std::memcpy(&caps,source.original->params->bytes.data()+12,4);
	return caps&1u?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
}
void ValidateNativeSnowDraw(const NativeSnowDraw& draw)
{
	Check(draw.points && draw.points->size()<=std::numeric_limits<std::uint32_t>::max()/6,"Original Snow point list is missing/too large");
	for(const auto& point:*draw.points) for(float f:point) Check(std::isfinite(f),"Original Snow point is nonfinite");
	for(float f:draw.modelView) Check(std::isfinite(f),"Original Snow model-view is nonfinite");
	for(float f:draw.projection) Check(std::isfinite(f),"Original Snow projection is nonfinite");
	const auto& u=draw.uniforms;
	Check(std::isfinite(u.time)&&std::isfinite(u.scale)&&u.scale>0&&std::isfinite(u.sizeMult),"Original Snow draw uniform is invalid");
	for(float f:u.speed) Check(std::isfinite(f),"Original Snow speed is nonfinite");
	for(float f:u.camPos) Check(std::isfinite(f),"Original Snow camera is nonfinite");
	for(float f:u.snowColor) Check(std::isfinite(f),"Original Snow color is nonfinite");
	Check(draw.pointState.sprite&&draw.pointState.coordReplace,"Original Snow point sprite state is missing");
	for(float f:draw.pointState.attenuation) Check(std::isfinite(f),"Original Snow point attenuation is nonfinite");
	Check(draw.originalTexture&&draw.texture && !draw.texture->incomplete&&draw.texture->generateMipmaps,"Original Snow sampler0 has no actual complete source image");
	ValidateScreenSampler(draw.sampler);
}
void NativeSnowRecorder::Begin(std::uint64_t sourceDrawFrame,std::shared_ptr<const NativeCUSUniformPublication> original)
{
	if(frame) throw std::logic_error("Original Snow frame is already active");
	auto next=std::make_shared<NativeSnowFrame>(); next->sourceDrawFrame=sourceDrawFrame; next->original=std::move(original);
	(void)ReadNativeSnowClipDepth(*next); frame=std::move(next);
}
void NativeSnowRecorder::Record(NativeSnowDraw draw)
{
	if(!frame) throw std::logic_error("Original Snow draw has no actual frame");
	ValidateNativeSnowDraw(draw); draw.sequence=NextNativeWorldDrawSequence(); frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeSnowFrame> NativeSnowRecorder::End()
{
	if(!frame) throw std::logic_error("Original Snow frame is inactive");
	return std::exchange(frame,{});
}
void NativeSnowRecorder::Abort() noexcept { frame.reset(); }
void SetNativeSnowConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeSnowConsumerAvailable() noexcept { return available.load(); }
}
