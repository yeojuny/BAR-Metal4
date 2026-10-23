/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// CPU validates source publications and copies original addressed bytes only.
// Original VS/GS math and geometry generation are performed by the GPU.
#include "NativeHealthBarsData.h"
#include "NativeHealthBarsPrograms.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<unsigned> consumers{0};
void Check(bool value,const char* reason){if(!value)throw std::invalid_argument(reason);}
}
void ValidateNativeHealthBarsDraw(const NativeHealthBarsDraw& draw)
{
	Check(draw.vertices && draw.vertices->object && draw.vertices->revision,"Original Healthbars point stream missing");
	const auto& layout=draw.vertices->layout;
	Check(layout.kind==NativeBufferKind::Vertex && layout.stride==80 && layout.attributes.size()==5 &&
		draw.vertices->bytes.size()==CheckedNativeBufferByteSize(layout) && draw.first<=layout.elements && draw.count<=layout.elements-draw.first,
		"Original Healthbars mixed 80-byte point stream/range differs");
	for(unsigned n=0;n<5;++n){const auto& a=layout.attributes[n];const bool integer=n==1 || n==4;
		Check(a.id==n && a.offset==n*16 && a.declaredCount==4 && a.byteSize==16 && !a.normalized && a.valueShape==NativeValueShape::Scalar &&
			a.scalarType==(integer?NativeScalarType::UInt32:NativeScalarType::Float32),"Original Healthbars attribute layout differs");}
	Check(draw.modelUniforms && draw.modelUniforms->kind==NativeCUSPublishedKind::ModelUniforms && draw.modelUniforms->stride==128 &&
		draw.modelUniforms->contextGeneration && draw.modelUniforms->allocationSerial && draw.modelUniforms->publicationSerial &&
		draw.modelUniforms->publishedDrawFrame<=draw.uniforms.sourceDrawFrame,"Original Healthbars model publication missing/stale-lifetime");
	(void)ReadNativeUnitPrimitiveUniforms(draw.uniforms);
	for(float v:{draw.config.height,draw.config.corner,draw.config.smallCorner,draw.iconDistance,draw.cameraDistanceMult,draw.cameraDistanceMultGlyph,draw.skipGlyphsNumbers})
		Check(std::isfinite(v),"Original Healthbars scalar is nonfinite");
	Check(draw.atlas && draw.atlas->sourceName==healthBarsProgram::Atlas && !draw.atlas->incomplete && draw.atlas->width && draw.atlas->height &&
		draw.atlas->format==ScreenTextureFormat::RGBA8Unorm && !draw.atlas->renderTarget && !draw.atlas->postprocessVersion,"Original Healthbars atlas missing");
	ValidateScreenSampler(draw.sampler);
	Check(!draw.surface.attachmentClear,"Original Healthbars draw cannot clear its attachments");
}
std::vector<NativeHealthBarsModelRow> ReadNativeHealthBarsModelRows(const NativeHealthBarsDraw& draw)
{
	ValidateNativeHealthBarsDraw(draw);
	std::vector<NativeHealthBarsModelRow> result(draw.count);
	for(std::size_t n=0;n<draw.count;++n){
		NativeHealthBarsInstance row;std::memcpy(&row,draw.vertices->bytes.data()+(draw.first+n)*sizeof(row),sizeof(row));
		ReadNativeCUSPublishedElement(*draw.modelUniforms,row.instanceData[1],
			{reinterpret_cast<std::uint8_t*>(&result[n]),sizeof(result[n])});
	}
	return result;
}
void AcquireNativeHealthBarsConsumer()noexcept{consumers.fetch_add(1,std::memory_order_relaxed);}
void ReleaseNativeHealthBarsConsumer()noexcept{consumers.fetch_sub(1,std::memory_order_relaxed);}
bool IsNativeHealthBarsConsumerAvailable()noexcept{return consumers.load(std::memory_order_relaxed)!=0;}
}
