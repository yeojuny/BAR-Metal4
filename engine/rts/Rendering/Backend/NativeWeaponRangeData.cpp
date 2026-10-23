/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWeaponRangeData.h"
#include "NativeWeaponRangePrograms.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
#include <cstring>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr std::array<std::string_view,21> Names{
	"lineAlphaUniform","cannonmode","fadeDistOffset","inMiniMap","selUnitCount",
	"drawAlpha","drawMode","staticUnits","rotationMiniMap","heightmapTex","pipVisibleArea","selBuilderCount","maskTex","maskClip","maskChannelBit","maskWriteValue","maskDepthBase","fillColor0","fillColor1","fillColor2","fillColor3"};
struct Substitutions {std::string defines,engine;};
Substitutions Extract(std::string_view source,std::string_view pattern)
{
	constexpr std::string_view d="//__DEFINES__",e="//__ENGINEUNIFORMBUFFERDEFS__";
	const auto a=pattern.find(d),b=pattern.find(e);Check(a!=pattern.npos && b>a,"Original range template markers differ");
	const auto prefix=pattern.substr(0,a),middle=pattern.substr(a+d.size(),b-a-d.size()),suffix=pattern.substr(b+e.size());
	Check(source.starts_with(prefix)&&source.ends_with(suffix),"Original range shader prefix/suffix differs from exact31657");
	const auto at=source.find(middle,prefix.size());Check(at!=source.npos,"Original range shader body differs from exact31657");
	const auto start=at+middle.size(),end=source.size()-suffix.size();Check(start<=end,"Original range substituted engine block overlaps source");
	Substitutions out{std::string(source.substr(prefix.size(),at-prefix.size())),std::string(source.substr(start,end-start))};
	Check(std::string(prefix)+out.defines+std::string(middle)+out.engine+std::string(suffix)==source,"Original range shader substitution differs");return out;
}
void Buffer(const NativeBufferSnapshotRef& value,bool instance)
{
	Check(value && value->object && value->revision,"Original range buffer owner missing");const auto& l=value->layout;
	Check(l.kind==NativeBufferKind::Vertex && l.stride==(instance?96:16) && l.attributes.size()==(instance?6:1) && value->bytes.size()==CheckedNativeBufferByteSize(l),"Original range buffer layout differs");
	for(unsigned i=0;i<l.attributes.size();++i){const auto& a=l.attributes[i];Check(a.id==i+unsigned(instance) && a.offset==i*16 && a.byteSize==16 && a.declaredCount==4 && a.valueShape==NativeValueShape::Scalar && !a.normalized && a.scalarType==((instance&&i==5)?NativeScalarType::UInt32:NativeScalarType::Float32),"Original range typed attributes differ");}
}
template<size_t N>void Copy(std::array<float,N>& destination,const auto& source,size_t offset){Check(offset+N*4<=source.size(),"Original range uniform offset exceeds publication");std::memcpy(destination.data(),source.data()+offset,N*4);}
}
std::uint64_t NativeWeaponRangeSourceHash(std::string_view text)noexcept{std::uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}return hash;}
NativeWeaponRangeDefines ParseNativeWeaponRangeDefines(std::string_view text)
{
	NativeWeaponRangeDefines out;out.originalText=text;std::map<std::string,float> values;std::istringstream lines{std::string(text)};std::string line;
	while(std::getline(lines,line)){if(line.find_first_not_of(" \t\r")==line.npos)continue;std::istringstream tokens(line);std::string directive,name,value,extra;tokens>>directive>>name>>value;Check(directive=="#define"&&!name.empty()&&!value.empty()&&!(tokens>>extra),"Original range define syntax differs");
		float number=0;auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);Check(parsed.ec==std::errc{} && parsed.ptr==value.data()+value.size() && std::isfinite(number),"Original range define is not a finite Lua-formatted value");Check(values.emplace(name,number).second,"Original range define duplicated");}
	Check(values.contains("MYGRAVITY")&&values.contains("DEBUG")&&values.contains("MOUSEOVERALPHAMULTIPLIER")&&values.at("DEBUG")==0,"Original range non-debug shader definitions differ");
	out.gravity=values.at("MYGRAVITY");out.mouseMultiplier=values.at("MOUSEOVERALPHAMULTIPLIER");
	Check(values.contains("MASKPASS") && values.contains("HEIGHTMAP_SAMPLE_STEPS") && (values.at("MASKPASS")==0 || values.at("MASKPASS")==1) && values.at("HEIGHTMAP_SAMPLE_STEPS")==8,"Original31657 mask/sample definitions differ");
	out.maskPass=values.at("MASKPASS")==1;out.heightmapSteps=8;
	if(values.contains("STATICUNITS")){Check(values.size()==6 && values.at("STATICUNITS")==0 && out.mouseMultiplier==1,"Original Attack range definitions differ");out.family=NativeWeaponRangeFamily::Attack;}
	else{Check(values.size()==5 && out.mouseMultiplier==5,"Original Defense range definitions differ");out.family=NativeWeaponRangeFamily::Defense;}return out;
}
NativeWeaponRangeProgramState::NativeWeaponRangeProgramState(std::string_view vertex,std::string_view fragment)
{
	std::string compositeVertex(weaponRangeProgram::CompositeVertex);
 constexpr std::string_view marker="//__DEFINES__";
 compositeVertex.replace(compositeVertex.find(marker),marker.size(),"");
 if(vertex==compositeVertex){
  const auto fs=Extract(fragment,weaponRangeProgram::CompositeFragment);
  Check(fs.defines.empty() && fs.engine==weaponRangeProgram::EngineBlock,"Original31657 composite substitutions differ");
  NativeWeaponRangeDefines definitions;definitions.composite=true;
  state.defines=std::make_shared<const NativeWeaponRangeDefines>(std::move(definitions));return;
 }
 const auto vs=Extract(vertex,weaponRangeProgram::Vertex),fs=Extract(fragment,weaponRangeProgram::Fragment);
	Check(vs.defines==fs.defines && vs.engine==fs.engine,"Original range vertex/fragment substitution owners differ");Check(vs.engine==weaponRangeProgram::EngineBlock,"Original31657 range engine block differs");
	state.defines=std::make_shared<const NativeWeaponRangeDefines>(ParseNativeWeaponRangeDefines(vs.defines));
}
std::span<const std::string_view> NativeWeaponRangeProgramState::ActiveUniforms(){return Names;}
int NativeWeaponRangeProgramState::Location(std::string_view name)const{const auto i=std::find(Names.begin(),Names.end(),name);return i==Names.end()?-1:int(i-Names.begin());}
bool NativeWeaponRangeProgramState::Floats(int location,std::span<const float> values)
{
	if(location==-1)return true;
 if(location>=17 && location<=20){if(values.size()!=4)return false;std::copy(values.begin(),values.end(),state.fillColors[location-17].begin());return true;}
 if(location==11 || (location>=13 && location<=16)){if(values.size()!=1)return false;float* targets[]={&state.builderCount,&state.maskClip,&state.maskChannelBit,&state.maskWriteValue,&state.maskDepthBase};*targets[location==11?0:location-12]=values[0];return true;}
 if(location==10){if(values.size()!=4)return false;std::copy(values.begin(),values.end(),state.pipVisibleArea.begin());return true;}
	if(location<0||location>7||values.size()!=1)return false;std::array<float*,8> target{&state.lineAlpha,&state.cannon,&state.fadeOffset,&state.inMiniMap,&state.selectionCount,&state.drawAlpha,&state.drawMode,&state.staticUnits};*target[location]=values[0];return true;
}
bool NativeWeaponRangeProgramState::Integers(int location,std::span<const std::int32_t> values)
{
	if(location==-1)return true;if((location!=8&&location!=9&&location!=12)||values.size()!=1)return false;(location==8?state.rotation:location==9?state.heightUnit:state.maskUnit)=values[0];return true;
}
NativeWeaponRangeUniformBlock ReadNativeWeaponRangeUniforms(const NativeWeaponRangeDraw& draw)
{
	Check(draw.original && draw.program.defines,"Original range uniform/program owner missing");ValidateNativeCUSUniformPublication(*draw.original);
	Check(draw.original->matrices->sourceDrawFrame<=draw.sourceDrawFrame && draw.original->params->sourceDrawFrame<=draw.sourceDrawFrame,"Original range uses future uniform publication");
	NativeWeaponRangeUniformBlock u;const auto& m=draw.original->matrices->bytes;const auto& p=draw.original->params->bytes;
	Copy(u.cameraView,m,3*64);Copy(u.cameraViewProjection,m,5*64);Copy(u.cameraViewInverse,m,7*64);Copy(u.miniMapViewProjection,m,19*64);
	Copy(u.mapSize,p,48);Copy(u.fogColor,p,80);Copy(u.fogParams,p,96);Copy(u.mouseWorldPos,p,272);
	const auto& s=draw.program;u.values0={s.lineAlpha,s.cannon,s.fadeOffset,s.inMiniMap};u.values1={s.selectionCount,s.drawAlpha,s.drawMode,s.staticUnits};u.pipVisibleArea=s.pipVisibleArea;u.raster={s.defines->mouseMultiplier,draw.raster.lineWidth,draw.surface.alphaTest.reference,s.maskDepthBase};
	u.maskParameters={s.builderCount,s.maskClip,s.maskChannelBit,s.maskWriteValue};u.fillColors=s.fillColors;Copy(u.viewGeometry,p,32);u.programOptions={unsigned(s.defines->maskPass),s.defines->heightmapSteps,unsigned(s.defines->composite),0};
	if(draw.surface.viewport){const auto& v=*draw.surface.viewport;u.sourceViewport={float(v.x),float(v.y),float(v.width),float(v.height)};}
	else u.sourceViewport={u.viewGeometry[2],u.viewGeometry[3],u.viewGeometry[0],u.viewGeometry[1]};
	std::uint32_t caps=0;std::memcpy(&caps,p.data()+12,4);u.options={std::uint32_t(s.rotation),std::uint32_t(!(caps&1)),0,draw.vertexCount};return u;
}
std::vector<std::array<float,4>> ReadNativeWeaponRangeModelPositions(const NativeWeaponRangeDraw& draw)
{
	if(draw.program.defines->composite || draw.program.staticUnits>0.5f || !draw.instanceCount)return {};
	Check(draw.models && draw.models->kind==NativeCUSPublishedKind::ModelUniforms && draw.models->stride==128 && draw.models->publishedDrawFrame<=draw.sourceDrawFrame,"Original range dynamic model publication missing or from future");
	std::vector<std::array<float,4>> positions(draw.instanceCount);std::array<std::uint8_t,128> row;
	for(std::uint32_t n=0;n<draw.instanceCount;++n){std::uint32_t index=0;std::memcpy(&index,draw.instances->bytes.data()+size_t(draw.firstInstance+n)*96+84,4);ReadNativeCUSPublishedElement(*draw.models,index,row);std::memcpy(positions[n].data(),row.data()+32,16);}return positions;
}
void ValidateNativeWeaponRangeDraw(const NativeWeaponRangeDraw& draw)
{
 Buffer(draw.vertices,false);
 Check(bool(draw.program.defines),"Original31657 range program owner missing");
 const bool composite=draw.program.defines->composite;
 if(composite){
  Check(draw.primitive==NativeDrawPrimitive::Triangles && draw.vertexCount==6 && draw.instanceCount==1 && draw.firstInstance==0,"Original31657 full-screen composite draw differs");
  Check(draw.instances && draw.instances->object && draw.instances->revision && draw.instances->layout.kind==NativeBufferKind::Vertex && draw.instances->layout.stride==4 && draw.instances->layout.elements==1 && draw.instances->layout.attributes.size()==1,"Original31657 composite placeholder slot differs");
  const auto& a=draw.instances->layout.attributes[0];
  Check(a.id==15 && a.offset==0 && a.declaredCount==1 && a.scalarType==NativeScalarType::Float32 && !a.normalized && draw.instances->bytes.size()==CheckedNativeBufferByteSize(draw.instances->layout),"Original31657 placeholder instance ABI differs");
 }else{
  Buffer(draw.instances,true);
  Check(draw.primitive==NativeDrawPrimitive::TriangleFan||draw.primitive==NativeDrawPrimitive::LineLoop,"Original range primitive differs");
  Check(draw.vertexCount>=3 && draw.vertexCount<=1024,"Original range selected circle tessellation exceeds source");
  Check(draw.program.heightUnit>=0 && draw.program.heightUnit<int(NativeCUSTextureHistory::Units),"Original range height sampler unit differs");
  Check(draw.height.knowledge==NativeCUSBindingKnowledge::BoundObject && draw.height.object && draw.height.object->identity && draw.height.object->contextGeneration && draw.height.object->version && draw.height.object->target==NativeCUSTextureTarget::Image2D && draw.height.object->storage!=NativeTextureStorageKnowledge::Unobserved,"Original range has no real observed height image binding");
 }
 Check(draw.firstVertex<=draw.vertices->layout.elements && draw.vertexCount<=draw.vertices->layout.elements-draw.firstVertex && draw.firstInstance<=draw.instances->layout.elements && draw.instanceCount<=draw.instances->layout.elements-draw.firstInstance,"Original range draw exceeds uploaded buffers");
 Check(draw.stage==NativeWorldDrawStage::World && (draw.program.inMiniMap<=0.5f || draw.program.defines->family==NativeWeaponRangeFamily::Attack),"Original range family or callback differs");
 if(draw.program.defines->maskPass)Check(draw.primitive==NativeDrawPrimitive::TriangleFan && draw.program.inMiniMap<0.5f,"Original31657 coverage is world filled discs only");
 if(composite || draw.program.maskClip>0.5f){
  Check(draw.program.maskUnit>=0 && draw.program.maskUnit<int(NativeCUSTextureHistory::Units) && draw.coverageMask.knowledge==NativeCUSBindingKnowledge::BoundObject && draw.coverageMask.object && draw.coverageMask.object->version && draw.coverageDependencyImage,"Original31657 mask read lacks an observed version/producer");
 }
 Check(std::isfinite(draw.raster.lineWidth) && draw.raster.lineWidth>0 && draw.raster.lineWidth<=64 && !draw.raster.lineStipple,"Original range aliased line state differs");
 Check(!draw.raster.inheritedClipPlane && draw.raster.frontPolygonMode==NativePolygonMode::Fill && draw.raster.backPolygonMode==NativePolygonMode::Fill,"Original range raster is outside source scope");
 (void)ReadNativeWeaponRangeUniforms(draw);(void)ReadNativeWeaponRangeModelPositions(draw);
}
} // namespace rendering
