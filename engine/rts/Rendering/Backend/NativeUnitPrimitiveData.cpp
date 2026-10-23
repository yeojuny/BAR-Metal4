/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeUnitPrimitiveData.h"
#include "NativeUnitPrimitivePrograms.h"
#include "NativeUnitIconsPrograms.h"
#include "NativeParticleAtlasSource.h"
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
void Check(bool value,const char* reason){if(!value)throw std::invalid_argument(reason);}
}
std::uint64_t NativeUnitPrimitiveSourceHash(std::string_view source) noexcept
{
	std::uint64_t value=14695981039346656037ULL;for(unsigned char c:source)value=(value^c)*1099511628211ULL;return value;
}
void ValidateNativeUnitPrimitiveEngineDefinitions(std::string_view source)
{
	Check(source.starts_with(unitPrimitiveProgram::EnginePrefix),"Original unit primitive engine definitions differ");source.remove_prefix(unitPrimitiveProgram::EnginePrefix.size());
	auto pattern=unitPrimitiveProgram::WaterTemplate;
	// These original helper functions are not called by DPAT, but changed helper
	// source must not silently enter a supposedly source-bound program family.
	for(unsigned n=0;n<9;++n){
		const auto token=pattern.find("%f");Check(token!=std::string_view::npos && source.starts_with(pattern.substr(0,token)),"Original unit primitive unused water helper differs");
		source.remove_prefix(token);pattern.remove_prefix(token+2);const auto end=source.find(pattern.front());Check(end!=std::string_view::npos,"Original unit primitive water constant missing");
		// Original Lua %f delegates to spring_lua_ftoa (precision=-1), which
		// emits integers, trimmed decimals or exponents instead of fixed six places.
		const auto text=source.substr(0,end);float value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
		Check(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && std::isfinite(value),"Original unit primitive helper constant invalid");source.remove_prefix(end);
	}
	Check(source==pattern,"Original unit primitive helper suffix differs");
}
NativeUnitPrimitiveUniformValues ReadNativeUnitPrimitiveUniforms(const NativeUnitPrimitiveUniforms& source)
{
	Check(bool(source.original),"Original unit primitive UBO publication is missing");ValidateNativeCUSUniformPublication(*source.original);
	Check(source.original->matrices->sourceDrawFrame<=source.sourceDrawFrame && source.original->params->sourceDrawFrame<=source.sourceDrawFrame,"Original unit primitive UBO publication is from a future draw");
	NativeUnitPrimitiveUniformValues result;const auto& matrices=source.original->matrices->bytes;const auto& params=source.original->params->bytes;
	std::memcpy(result.viewProjection.data(),matrices.data()+5*64,64);std::memcpy(result.viewInverse.data(),matrices.data()+7*64,64);std::memcpy(result.time.data(),params.data()+16,16);
	std::uint32_t flags;std::memcpy(&flags,params.data()+12,4);result.clipDepth=(flags&1u)?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
	for(const auto* m:{&result.viewProjection,&result.viewInverse})for(float f:*m)Check(std::isfinite(f),"Original unit primitive matrix is nonfinite");
	for(float f:result.time)Check(std::isfinite(f),"Original unit primitive time is nonfinite");return result;
}
void ValidateNativeUnitPrimitiveDraw(const NativeUnitPrimitiveDraw& draw)
{
	const bool selected=IsNativeSelectedUnits(draw.kind),icon=IsNativeUnitIcon(draw.kind);
	Check(icon==draw.screenEffects,"Original unit icon screen/world destination mismatch");
	Check(draw.kind==NativeUnitPrimitiveKind::BuildingAO || draw.kind==NativeUnitPrimitiveKind::FeatureAO || draw.kind==NativeUnitPrimitiveKind::TeamPlatter || selected || draw.kind==NativeUnitPrimitiveKind::AllySelected || icon || draw.kind==NativeUnitPrimitiveKind::ResurrectionHalo || draw.kind==NativeUnitPrimitiveKind::UnitTrackerDebug || draw.kind==NativeUnitPrimitiveKind::EnemySpotter,"Unknown original unit primitive family");
	const auto stage=draw.kind==NativeUnitPrimitiveKind::SelectedUnfinished || draw.kind==NativeUnitPrimitiveKind::SelectedWater || draw.kind==NativeUnitPrimitiveKind::ResurrectionHalo?NativeWorldDrawStage::World:NativeWorldDrawStage::PreUnit;
	Check(icon || (draw.kind==NativeUnitPrimitiveKind::AllySelected ? (draw.stage==NativeWorldDrawStage::PreUnit || draw.stage==NativeWorldDrawStage::World) : draw.stage==stage),"Original unit primitive draw stage differs from its actual program");
	if(draw.kind==NativeUnitPrimitiveKind::AllySelected)Check(draw.disableQuads && (draw.maxVertices==8 || draw.maxVertices==64) && std::isfinite(draw.lineOpacity),"Original ally selected compile definitions differ");
	if(selected)Check(std::isfinite(draw.whiteMix) && draw.initialSize==(draw.kind==NativeUnitPrimitiveKind::SelectedUnfinished?.9999f:.75f),"Original selected unit compile definitions differ");
	Check(draw.vertices && draw.vertices->object && draw.vertices->revision,"Original unit primitive immutable vertex buffer missing");const auto& layout=draw.vertices->layout;
	Check(layout.kind==NativeBufferKind::Vertex && layout.stride==72 && layout.attributes.size()==6 && draw.vertices->bytes.size()==CheckedNativeBufferByteSize(layout) && draw.first<=layout.elements && draw.count<=layout.elements-draw.first,"Original unit primitive 72-byte stream/range differs");
	constexpr std::array<unsigned,6> offsets{0,16,20,24,40,56},counts{4,1,1,4,4,4};
	for(unsigned n=0;n<6;++n){const auto& a=layout.attributes[n];const bool integer=n==1 || n==2 || n==5;
		Check(a.id==n && a.offset==offsets[n] && a.declaredCount==counts[n] && a.byteSize==4*counts[n] && a.scalarType==(integer?NativeScalarType::UInt32:NativeScalarType::Float32) && a.valueShape==NativeValueShape::Scalar && !a.normalized,"Original unit primitive mixed attribute layout differs");}
	Check(draw.transforms && draw.transforms->kind==NativeCUSPublishedKind::Transforms && draw.transforms->stride==32 && draw.modelUniforms && draw.modelUniforms->kind==NativeCUSPublishedKind::ModelUniforms && draw.modelUniforms->stride==128,"Original unit primitive SSBO publications are missing");
	Check(draw.transforms->contextGeneration && draw.transforms->contextGeneration==draw.modelUniforms->contextGeneration && draw.transforms->allocationSerial && draw.modelUniforms->allocationSerial && draw.transforms->publicationSerial && draw.modelUniforms->publicationSerial,"Original unit primitive SSBO lifetime generations differ");
	for(float f:{draw.addRadius,draw.iconDistance,draw.opacity})Check(std::isfinite(f),"Original unit primitive scalar is nonfinite");
	if(draw.kind==NativeUnitPrimitiveKind::EnemySpotter){
		const auto& image=draw.atlas;
		Check(image&&image->sourceName=="LuaUI/Images/enemyspotter.dds"&&!image->incomplete&&!image->renderTarget&&!image->postprocessVersion,"Original EnemySpotter image differs");
		Check(draw.maxVertices==4&&!draw.disableQuads,"Original EnemySpotter quad profile differs");ValidateScreenSampler(draw.sampler);
	}else if(draw.kind==NativeUnitPrimitiveKind::UnitTrackerDebug){
		const auto& image=draw.atlas;
		Check(image&&image->sourceName=="luaui/images/solid.png"&&!image->incomplete&&!image->renderTarget&&!image->postprocessVersion,"Original unit tracker debug texture owner differs");
		Check(draw.opacity==.5f&&!draw.disableQuads&&draw.maxVertices==64,"Original unit tracker debug definitions differ");
		ValidateScreenSampler(draw.sampler);
	}else if(draw.kind==NativeUnitPrimitiveKind::RankIcon){
  const auto& image=draw.atlas;Check(draw.maxVertices==4 && !draw.disableQuads,"Original rank quad variant differs");
  Check(image && image->format==ScreenTextureFormat::ExternalColor && image->sourceTexture && !image->incomplete && !image->renderTarget && !image->postprocessVersion,"Original rank atlas owner missing");
  const auto& object=*image->sourceTexture;auto backing=std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(object.backing);
  Check(backing && backing->atlas && backing->atlas->pages.size()==1 && object.target==NativeCUSTextureTarget::Image2D && object.source.starts_with("CTextureAtlas: *") && image->width==object.allocation.width && image->height==object.allocation.height,"Original rank atlas is not its actual packed Lua atlas");ValidateScreenSampler(draw.sampler);
 }else if(draw.kind==NativeUnitPrimitiveKind::ResurrectionHalo){
  const auto& image=draw.atlas;Check(draw.maxVertices==4 && !draw.disableQuads,"Original resurrection halo quad variant differs");
  Check(image && image->sourceName=="LuaUI/Images/halo.dds" && image->format==ScreenTextureFormat::BC3RGBA && !image->mips.empty() && !image->incomplete && !image->renderTarget && !image->postprocessVersion,"Original resurrection halo DDS missing");ValidateScreenSampler(draw.sampler);
 }else if(icon){
		const auto& image=draw.atlas;
		Check(draw.maxVertices==4 && !draw.disableQuads,"Original unit icon quad variant differs");
		Check(image && unitIconsProgram::ImageMatches(draw.kind,image->sourceName) && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm && image->width && image->height && !image->renderTarget && !image->postprocessVersion,"Original unit icon actual image missing");ValidateScreenSampler(draw.sampler);
	}else if(selected){
		const auto& image=draw.atlas;
		Check(image && image->sourceName==unitPrimitiveProgram::SelectedTexture && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm && image->width && image->height && !image->renderTarget && !image->postprocessVersion,"Original selected unit source image missing");ValidateScreenSampler(draw.sampler);
	}else if(draw.kind!=NativeUnitPrimitiveKind::TeamPlatter && draw.kind!=NativeUnitPrimitiveKind::AllySelected){const auto& image=draw.atlas;const auto index=draw.kind==NativeUnitPrimitiveKind::FeatureAO?1:0;
		Check(image && image->sourceName==unitPrimitiveProgram::AtlasNames[index] && !image->incomplete && image->format==ScreenTextureFormat::BC3RGBA && !image->mips.empty() && !image->generateMipmaps && !image->renderTarget && !image->postprocessVersion,"Original unit primitive DDS atlas/mips missing");ValidateScreenSampler(draw.sampler);
	}else Check(!draw.atlas,"Original team platter must not acquire an invented texture");
}
std::vector<NativeUnitPrimitiveObject> ReadNativeUnitPrimitiveObjects(const NativeUnitPrimitiveDraw& draw,const NativeUnitPrimitiveUniforms& uniforms)
{
	ValidateNativeUnitPrimitiveDraw(draw);(void)ReadNativeUnitPrimitiveUniforms(uniforms);
	Check(draw.transforms->publishedDrawFrame<=uniforms.sourceDrawFrame && draw.modelUniforms->publishedDrawFrame<=uniforms.sourceDrawFrame,"Original unit primitive model publication is from a future draw");
	std::vector<NativeUnitPrimitiveObject> result;result.reserve(draw.count);
	for(std::size_t n=draw.first;n<std::size_t(draw.first)+draw.count;++n){
		NativeUnitPrimitiveInstance row;std::memcpy(&row,draw.vertices->bytes.data()+n*72,72);NativeUnitPrimitiveObject object{};
		auto bytes=[](auto& x){return std::span(reinterpret_cast<std::uint8_t*>(&x),sizeof(x));};
		std::array<float,8> previous,current;std::array<std::uint8_t,128> model;
		ReadNativeCUSPublishedElement(*draw.transforms,row.instanceData[0],bytes(previous));
		ReadNativeCUSPublishedElement(*draw.transforms,std::size_t(row.instanceData[0])+1,bytes(current));
		ReadNativeCUSPublishedElement(*draw.modelUniforms,row.instanceData[1],model);
		std::memcpy(object.previousQuaternion.data(),previous.data(),16);std::memcpy(object.previousTranslationScale.data(),previous.data()+4,16);std::memcpy(object.currentQuaternion.data(),current.data(),16);std::memcpy(object.currentTranslationScale.data(),current.data()+4,16);
		std::uint32_t composite;std::memcpy(&composite,model.data(),4);object.metadata={composite,row.instanceData[0],row.instanceData[1],0};
		// AO's original fragment replacement never consumes the team varying.
		// An unobserved team row must not be presented as a real color there.
		if(draw.kind==NativeUnitPrimitiveKind::TeamPlatter || IsNativeSelectedUnits(draw.kind) || draw.kind==NativeUnitPrimitiveKind::AllySelected || draw.kind==NativeUnitPrimitiveKind::UnitTrackerDebug || draw.kind==NativeUnitPrimitiveKind::EnemySpotter){const auto color=ReadNativeCUSTeamColor(*uniforms.original,row.team);std::memcpy(object.teamColor.data(),color.data(),16);}
		result.push_back(object);
	}
	return result;
}
void ValidateNativeUnitIconFrame(const NativeUnitPrimitiveFrame& frame)
{
 Check(frame.draws.size()==1,"Original unit icon marker must own exactly one original draw");
 const auto& draw=frame.draws.front();Check(IsNativeUnitIcon(draw.kind) && draw.screenEffects,"Original unit icon marker contains a world-family draw");
 ValidateNativeUnitPrimitiveDraw(draw);(void)ReadNativeUnitPrimitiveUniforms(frame.uniforms);
}
void NativeUnitPrimitiveRecorder::Begin(const NativeUnitPrimitiveUniforms& uniforms)
{
	if(frame)throw std::logic_error("Original unit primitive frame is already active");(void)ReadNativeUnitPrimitiveUniforms(uniforms);auto value=std::make_shared<NativeUnitPrimitiveFrame>();value->uniforms=uniforms;frame=std::move(value);
}
void NativeUnitPrimitiveRecorder::Record(NativeUnitPrimitiveDraw draw)
{
	if(!frame)throw std::logic_error("Original unit primitive draw has no frame");ValidateNativeUnitPrimitiveDraw(draw);Check(!draw.screenEffects,"Screen icons cannot enter the world recorder");draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeUnitPrimitiveFrame> NativeUnitPrimitiveRecorder::End(){if(!frame)throw std::logic_error("Original unit primitive frame is inactive");return std::exchange(frame,{});}
void NativeUnitPrimitiveRecorder::Abort()noexcept{frame.reset();}
void SetNativeUnitPrimitiveConsumerAvailable(bool value)noexcept{available.store(value);}
bool IsNativeUnitPrimitiveConsumerAvailable()noexcept{return available.load();}
}
