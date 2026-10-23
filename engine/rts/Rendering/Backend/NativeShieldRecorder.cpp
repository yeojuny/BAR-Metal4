/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShieldRecorder.h"
#include "NativeShieldTextureState.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool value,const char* message) { if(!value)throw std::invalid_argument(message); }
void Finite(const auto& range) { for(float value:range)Check(std::isfinite(value),"Original Shield input is nonfinite"); }
}
void ValidateNativeShieldGeometry(const NativeShieldGeometry& source)
{
	Check(source.identity!=0,"Original Shield retained mesh has no identity");
	Check(source.vertices.size()==3840 || source.vertices.size()==15360,"Original Shield requires exact small4/large5 icosahedron triangles");
	for(const auto& v:source.vertices) { Finite(v.position);Finite(v.normal);Finite(v.uv); }
}
void ValidateNativeShieldDraw(const NativeShieldDraw& draw)
{
	Check(bool(draw.geometry),"Original Shield has no actual retained geometry");
	ValidateNativeShieldGeometry(*draw.geometry);
	const auto& u=draw.uniforms;
	Check(bool(draw.original),"Original current31657 Shield requires actual engine UBO publication");
	ValidateNativeCUSUniformPublication(*draw.original);
	Check(draw.vertexSource&&draw.instanceSource&&!draw.instances.empty(),"Original current31657 Shield has no actual VBO instances");
	Finite(u.view);Finite(u.projection);Finite(u.cameraPosition);Finite(u.timeInfo);
	Check(std::isfinite(u.gameFrame),"Original Shield game time is nonfinite");
	if(draw.impactSource.snapshot) {
		const auto& range=draw.impactSource;
		Check(range.binding==6&&range.offset<=range.snapshot->bytes.size()&&range.size<=range.snapshot->bytes.size()-range.offset,"Original Shield impact storage binding/range differs");
		Check(range.size%16==0&&draw.impactPoints.size()==range.size/16&&draw.impactUploadedBytes<=range.size&&draw.impactUploadedBytes%16==0,"Original Shield impact upload/range size differs");
	}else Check(draw.impactPoints.empty()&&draw.impactUploadedBytes==0,"Original Shield invented unbound impact data");
	unsigned effects=0;
	for(const auto& instance:draw.instances) {
		Finite(instance.posRadius);Finite(instance.rotMargin);Finite(instance.color1);Finite(instance.color2);Finite(instance.params);
		Check(instance.posRadius[3]>0,"Original Shield instance radius is invalid");
		for(float value:instance.params)Check(value>=0&&value<=16777216.0f&&std::floor(value)==value,"Original Shield integer instance parameter differs");
		const auto bits=unsigned(instance.params[0]),base=unsigned(instance.params[1]),count=unsigned(instance.params[2]),flags=unsigned(instance.params[3]);
		Check((bits&~0x3feu)==0&&count<=24&&flags<=1,"Original Shield effect/impact/palette layout differs");
		effects|=bits;
		if(count) {
			Check(draw.impactSource.snapshot&&base<=draw.impactUploadedBytes/16&&count<=draw.impactUploadedBytes/16-base,"Original Shield impact instance exceeds actual uploaded binding6 data");
			for(unsigned i=0;i<count;++i)Finite(draw.impactPoints[base+i]);
		}
	}
	Check(u.effects==effects,"Original Shield outline pipeline does not cover its actual instance effects");
	if(draw.textures)ValidateNativeShieldTextureSnapshot(*draw.textures,u.effects);
	else {
		// Controlled producer fixtures retain their explicit same-frame inputs.
		// Actual Lua draws always capture the shared context instead.
		Check(!(u.effects&2u)||draw.mapDepthBound,"Original Shield terrain outline has no original map depth binding");
		Check(!(u.effects&4u)||draw.modelsDepthBound,"Original Shield model outline has no original models depth binding");
	}
	Check(draw.raster.depthTest && !draw.raster.depthWrite && draw.raster.depthCompare==ScreenAlphaCompare::LessEqual,"Original Shield requires LEQUAL and no depth writes");
	Check(draw.raster.frontPolygonMode==NativePolygonMode::Fill && draw.raster.backPolygonMode==NativePolygonMode::Fill,"Original Shield non-fill inherited raster requires its original polygon consumer");
	Check(!draw.raster.inheritedClipPlane,"Original Shield inherited user plane requires an explicit original shader clip consumer");
	Check(draw.surface.blend==ScreenBlendState{},"Original Shield requires the original alpha blending state");
}
void NativeShieldRecorder::Begin(std::uint64_t sourceDrawFrame)
{
	if(frame)throw std::logic_error("Original Shield world scope is already active");
	frame=std::make_shared<NativeShieldFrame>();frame->sourceDrawFrame=sourceDrawFrame;
}
void NativeShieldRecorder::Record(NativeShieldDraw draw)
{
	if(!frame)throw std::logic_error("Original Shield draw is outside its world scope");
	ValidateNativeShieldDraw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeShieldFrame> NativeShieldRecorder::End()
{
	if(!frame)throw std::logic_error("Original Shield world scope is inactive");
	return std::exchange(frame,{});
}
void NativeShieldRecorder::Abort() noexcept { frame.reset(); }
void SetNativeShieldConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeShieldConsumerAvailable() noexcept { return available.load(); }
} // namespace rendering
