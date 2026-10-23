/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeGameDecalData.h"
#include "NativeGameDecalPrograms.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool value,const char* message) { if(!value) throw std::invalid_argument(message); }
void Snapshot(const NativeBufferSnapshotRef& value)
{
	Check(value && value->object && value->revision,"Original game decal immutable buffer is missing");
	Check(value->bytes.size()==CheckedNativeBufferByteSize(value->layout),"Original game decal buffer extent differs");
}
void Attribute(const NativeBufferAttribute& a,unsigned id,unsigned offset,unsigned count)
{
	Check(a.id==id && a.offset==offset && a.declaredCount==count && a.byteSize==count*4 &&
		a.scalarType==NativeScalarType::Float32 && a.valueShape==NativeValueShape::Scalar && !a.normalized,
		"Original game decal typed attribute differs");
}
void Instances(const NativeBufferSnapshotRef& value,unsigned first,unsigned count,unsigned location)
{
	Snapshot(value);const auto& layout=value->layout;
	Check(layout.kind==NativeBufferKind::Vertex && layout.stride==80 && layout.attributes.size()==5 &&
		first<=layout.elements && count<=layout.elements-first,"Original game decal 80-byte stream/range differs");
	for(unsigned n=0;n<5;++n) Attribute(layout.attributes[n],n+location,n*16,4);
}
}
std::uint64_t NativeGameDecalSourceHash(std::string_view source) noexcept
{
	std::uint64_t hash=14695981039346656037ULL;
	for(unsigned char byte:source)hash=(hash^byte)*1099511628211ULL;
	return hash;
}
NativeGameDecalWater ParseNativeGameDecalWater(std::string_view source)
{
	// Validate every byte of the exact original engine-helper output apart from
	// its nine %f fields. This is a typed source contract, not a GLSL translator.
	Check(source.starts_with(gameDecalProgram::EnginePrefix),"Original game decal engine helper differs");
	source.remove_prefix(gameDecalProgram::EnginePrefix.size());
	std::string_view pattern=gameDecalProgram::WaterTemplate;
	std::array<float,9> values{};
	for(float& value:values){
		const auto token=pattern.find("%f");Check(token!=std::string_view::npos,"Original game decal water contract malformed");
		Check(source.starts_with(pattern.substr(0,token)),"Original game decal water helper differs");
		source.remove_prefix(token);pattern.remove_prefix(token+2);
		const auto end=source.find(pattern.front());Check(end!=std::string_view::npos,"Original game decal water constant missing");
		const auto text=source.substr(0,end);
		// Spring's bare Lua %f uses spring_lua_ftoa with precision=-1, so
		// integers, trimmed decimals and exponents are all original output.
		// Consume the emitted GLSL literal without rounding it to six places.
		const auto result=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
		Check(result.ec==std::errc{} && result.ptr==text.data()+text.size() && std::isfinite(value),"Original game decal water constant invalid");
		source.remove_prefix(end);
	}
	Check(source==pattern,"Original game decal water helper tail differs");
	NativeGameDecalWater result;
	std::copy_n(values.begin(),3,result.absorb.begin());std::copy_n(values.begin()+3,3,result.minimum.begin());std::copy_n(values.begin()+6,3,result.base.begin());
	return result;
}
NativeGameDecalUniformValues ReadNativeGameDecalUniforms(const NativeGameDecalUniforms& source)
{
	Check(bool(source.original),"Original game decal UBO publication is missing");
	ValidateNativeCUSUniformPublication(*source.original);
	const auto& matrices=*source.original->matrices;
	const auto& params=*source.original->params;
	Check(matrices.sourceDrawFrame<=source.sourceDrawFrame && params.sourceDrawFrame<=source.sourceDrawFrame,
		"Original game decal UBO publication is from a future draw");
	NativeGameDecalUniformValues value;
	// Exact UniformConstants / selected30770 LuaShader ABI. The production-flags
	// ABI test binds these offsets to the original engine structures.
	std::memcpy(value.cameraViewProjection.data(),matrices.bytes.data()+5*64,64);
	std::memcpy(value.cameraViewInverse.data(),matrices.bytes.data()+7*64,64);
	std::memcpy(value.shadowView.data(),matrices.bytes.data()+10*64,64);
	std::memcpy(value.timeInfo.data(),params.bytes.data()+16,16);
	std::memcpy(value.mapSize.data(),params.bytes.data()+48,16);
	std::memcpy(value.sunDirection.data(),params.bytes.data()+112,16);
	std::memcpy(value.sunAmbient.data(),params.bytes.data()+144,16);
	std::memcpy(value.sunDiffuse.data(),params.bytes.data()+176,16);
	uint32_t renderCaps;std::memcpy(&renderCaps,params.bytes.data()+12,4);
	value.clipDepth=(renderCaps&1u)?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
	for(const auto* matrix:{&value.cameraViewProjection,&value.cameraViewInverse,&value.shadowView})
		for(float f:*matrix)Check(std::isfinite(f),"Original game decal matrix is nonfinite");
	for(const auto* vector:{&value.timeInfo,&value.mapSize,&value.sunDirection,&value.sunAmbient,&value.sunDiffuse})
		for(float f:*vector)Check(std::isfinite(f),"Original game decal frame uniform is nonfinite");
	for(float size:value.mapSize)Check(size>0,"Original game decal map extent is empty");
	return value;
}
void ValidateNativeGameDecalUniforms(const NativeGameDecalUniforms& value) { (void)ReadNativeGameDecalUniforms(value); }
void ValidateNativeGameDecalDraw(const NativeGameDecalDraw& draw)
{
	Check(draw.stage==NativeWorldDrawStage::PreDecals,"Original game decal packet is outside DrawPreDecals");
	if(draw.program==NativeGameDecalProgram::Geometry){
		Check(!draw.indices && !draw.instances && draw.instanceCount==1 && draw.firstInstance==0,"Original game decal GS stream must be the original points draw");
		Instances(draw.vertices,draw.first,draw.count,0);
	}else{
		Check(draw.program==NativeGameDecalProgram::LargeMesh,"Unknown original game decal program");
		Snapshot(draw.vertices);Snapshot(draw.indices);Instances(draw.instances,draw.firstInstance,draw.instanceCount,1);
		const auto& vertex=draw.vertices->layout;const auto& index=draw.indices->layout;
		Check(vertex.kind==NativeBufferKind::Vertex && vertex.stride==8 && vertex.attributes.size()==1,"Original game decal plane vertex layout differs");
		Attribute(vertex.attributes[0],0,0,2);
		Check(index.kind==NativeBufferKind::Index && (index.stride==2 || index.stride==4) && draw.count%3==0 &&
			draw.first<=index.elements && draw.count<=index.elements-draw.first,"Original game decal triangle index range differs");
		for(std::size_t n=draw.first;n<std::size_t(draw.first)+draw.count;++n){
			std::uint32_t value=0;std::memcpy(&value,draw.indices->bytes.data()+n*index.stride,index.stride);
			Check(value<vertex.elements,"Original game decal index exceeds its actual plane");
		}
	}
	for(unsigned n=0;n<2;++n){
		const auto& image=draw.atlases[n].image;
		Check(image && image->sourceName==gameDecalProgram::AtlasNames[n] && !image->incomplete && image->width && image->height &&
			!image->renderTarget && !image->postprocessVersion && !image->mips.empty() && !image->generateMipmaps,
			"Original game decal named DDS atlas or embedded mips are missing");
		Check(image->format==ScreenTextureFormat::BC3RGBA,"Original game decal DXT5 atlas format differs");
		ValidateScreenSampler(draw.atlases[n].sampler);
	}
	for(const auto* vector:{&draw.water.absorb,&draw.water.minimum,&draw.water.base})
		for(float value:*vector)Check(std::isfinite(value),"Original game decal water program constant is nonfinite");
	Check(std::isfinite(draw.fadeDistance),"Original game decal fade-distance uniform is nonfinite");
}
void NativeGameDecalRecorder::Begin(const NativeGameDecalUniforms& uniforms)
{
	if(frame)throw std::logic_error("Original game decal frame is already active");
	ValidateNativeGameDecalUniforms(uniforms);auto next=std::make_shared<NativeGameDecalFrame>();next->uniforms=uniforms;frame=std::move(next);
}
void NativeGameDecalRecorder::Record(NativeGameDecalDraw draw)
{
	if(!frame)throw std::logic_error("Original game decal draw has no active frame");
	ValidateNativeGameDecalDraw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeGameDecalFrame> NativeGameDecalRecorder::End()
{
	if(!frame)throw std::logic_error("Original game decal frame is inactive");
	return std::exchange(frame,{});
}
void NativeGameDecalRecorder::Abort() noexcept { frame.reset(); }
void SetNativeGameDecalConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeGameDecalConsumerAvailable() noexcept { return available.load(); }
} // namespace rendering
