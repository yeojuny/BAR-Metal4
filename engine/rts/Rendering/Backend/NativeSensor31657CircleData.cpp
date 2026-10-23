/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeSensor31657CircleData.h"
#include "NativeSensor31657CirclePrograms.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
namespace rendering {
namespace {
void Check(bool value,const char* why) { if(!value) throw std::invalid_argument(why); }
struct Substitutions { std::string defines,engine; };
std::optional<Substitutions> Extract(std::string_view source,std::string_view pattern)
{
 constexpr std::string_view d="//__DEFINES__",e="//__ENGINEUNIFORMBUFFERDEFS__";
 const auto di=pattern.find(d),ei=pattern.find(e);
 Check(di!=pattern.npos&&ei!=pattern.npos,"31657 circle shader marker missing");
 const bool df=di<ei;const auto a=std::min(di,ei),b=std::max(di,ei),as=df?d.size():e.size(),bs=df?e.size():d.size();
 const auto prefix=pattern.substr(0,a),middle=pattern.substr(a+as,b-a-as),suffix=pattern.substr(b+bs);
 if(!source.starts_with(prefix)||!source.ends_with(suffix))return {};
 for(auto at=source.find(middle,prefix.size());at!=source.npos;at=source.find(middle,at+1)){
  const auto start=at+middle.size(),end=source.size()-suffix.size();if(start>end)break;
  const std::string first(source.substr(prefix.size(),at-prefix.size())),second(source.substr(start,end-start));
  Substitutions result=df?Substitutions{first,second}:Substitutions{second,first};
  if(result.engine!=sensor31657CircleProgram::EngineDefinitions)continue;
  if(std::string(prefix)+first+std::string(middle)+second+std::string(suffix)==source)return result;
 }
 return {};
}
NativeSensor31657CircleDefines Defines(const Substitutions& source)
{
 NativeSensor31657CircleDefines out;out.originalText=source.defines;out.originalEngineDefinitions=source.engine;
 std::map<std::string,float> values;std::istringstream lines(source.defines);std::string line;
 while(std::getline(lines,line)){
  if(line.find_first_not_of(" \t\r")==line.npos)continue;
  std::istringstream fields(line);std::string directive,name,value,extra;fields>>directive>>name>>value;
  Check(directive=="#define"&&!name.empty()&&!value.empty()&&!(fields>>extra),"31657 circle define syntax differs");
  float number=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
  Check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&std::isfinite(number),"31657 circle define is not finite numeric Lua output");
  Check(values.emplace(name,number).second,"31657 circle define repeated");
 }
 for(auto name:{"VSX","VSY","PADDING","EXACT_CIRCLES","CIRCLE_SEGMENTS","CIRCLE_SSBO_BINDING","STIPPLE_RATE"})
  Check(values.contains(name),"31657 circle required define missing");
 const bool sonar=values.at("CIRCLE_SEGMENTS")==61;
 Check(values.size()==(sonar?9:7)&&values.at("PADDING")==1&&values.at("EXACT_CIRCLES")==1&&values.at("CIRCLE_SSBO_BINDING")==5&&values.at("STIPPLE_RATE")==1,"31657 selected circle family differs");
 if(sonar){Check(values.contains("VISIBILITYCULLING")&&values.at("VISIBILITYCULLING")==1&&values.contains("MAX_HEIGHT")&&values.at("MAX_HEIGHT")==0,"31657 sonar defines differ");out.kind=NativeSensor31657CircleKind::Sonar;out.visibilityCulling=true;out.maxHeight=0;}
 else Check(values.at("CIRCLE_SEGMENTS")==125,"31657 radar polygon segment count differs");
 out.circleSegments=static_cast<uint32_t>(values.at("CIRCLE_SEGMENTS"));out.viewWidth=values.at("VSX");out.viewHeight=values.at("VSY");
 Check(out.viewWidth>=1&&out.viewHeight>=1&&out.viewWidth<=32768&&out.viewHeight<=32768,"31657 circle view dimensions invalid");return out;
}
constexpr std::array<std::string_view,4> names{"heightmapTex","rangeColor","circleCount","teamColorMix"};
void Snapshot(const NativeBufferSnapshotRef& b)
{
 Check(b&&b->object&&b->revision&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"31657 circle buffer owner/layout missing");
}
void Vertices(const NativeBufferSnapshotRef& b)
{
 Snapshot(b);const auto& l=b->layout;
 Check(l.kind==NativeBufferKind::Vertex&&l.stride==16&&l.attributes.size()==1,"31657 original circle vertex layout differs");const auto& a=l.attributes[0];
 Check(a.id==0&&a.offset==0&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"31657 circle attr0 differs");
}
void Instances(const NativeBufferSnapshotRef& b)
{
 Snapshot(b);const auto& l=b->layout;
 Check(l.kind==NativeBufferKind::Vertex&&l.stride==32&&l.attributes.size()==2,"31657 original circle instance layout differs");
 for(unsigned i=0;i<2;++i){const auto& a=l.attributes[i];Check(a.id==i+1&&a.offset==16*i&&a.byteSize==16&&a.declaredCount==4&&!a.normalized&&a.scalarType==(i?NativeScalarType::UInt32:NativeScalarType::Float32),"31657 original radius/instData attribute differs");}
}
void Circles(const NativeSensor31657CircleDraw& draw)
{
 const auto& r=draw.circles;Snapshot(r.snapshot);const auto& l=r.snapshot->layout;
 Check(r.binding==5&&l.kind==NativeBufferKind::Storage&&l.stride==32&&l.attributes.size()==1,"31657 original binding5 storage layout differs");const auto& a=l.attributes[0];
 // Lua storage size=2 means two vec4s, not two scalar floats. instData's bits
 // arrived by the original GPU CopyTo; never convert that second vec4 numerically.
 Check(a.id==0&&a.name=="circle"&&a.offset==0&&a.byteSize==32&&a.declaredCount==2&&a.valueShape==NativeValueShape::Vector4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"31657 original circle SSBO definition differs");
 Check(r.offset%32==0&&r.offset<=r.snapshot->bytes.size()&&r.size<=r.snapshot->bytes.size()-r.offset&&r.size%32==0,"31657 actual bound storage range differs");
 Check(draw.program.circleCount>=0&&size_t(draw.program.circleCount)<=r.size/32,"31657 circleCount exceeds actual binding5 range");
}
template<size_t N>void Copy(std::array<float,N>& out,const auto& bytes,size_t at){Check(at+N*4<=bytes.size(),"31657 original UBO field exceeds retained backing");std::memcpy(out.data(),bytes.data()+at,N*4);}
}
std::uint64_t NativeSensor31657SourceHash(std::string_view text)noexcept
{
 uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}return hash;
}
void ValidateNativeSensor31657EngineDefinitions(std::string_view text)
{
 Check(text==sensor31657CircleProgram::EngineDefinitions,"31657 complete original LuaShader engine expansion differs");
}
NativeSensor31657CircleProgramState::NativeSensor31657CircleProgramState(std::string_view vertex,std::string_view fragment)
{
 const auto vs=Extract(vertex,sensor31657CircleProgram::CircleVertex),fs=Extract(fragment,sensor31657CircleProgram::CircleFragment);
 Check(vs&&fs&&vs->defines==fs->defines&&vs->engine==fs->engine,"31657 complete original circle shader sources differ");
 state.defines=std::make_shared<const NativeSensor31657CircleDefines>(Defines(*vs));
}
std::vector<std::string_view> NativeSensor31657CircleProgramState::ActiveUniforms()const
{
 // USE_TEAMCOLOR is not selected by either source widget, so GLSL optimizes
 // teamColorMix out. Preserve its -1 location and normal no-op updates.
 return {names[0],names[1],names[2]};
}
int NativeSensor31657CircleProgramState::Location(std::string_view name)const
{
 const auto active=ActiveUniforms();if(std::find(active.begin(),active.end(),name)==active.end())return -1;
 return int(std::find(names.begin(),names.end(),name)-names.begin());
}
bool NativeSensor31657CircleProgramState::Floats(int location,std::span<const float> values)
{
 if(location==-1)return true;if(location!=1||values.size()!=4)return false;
 std::copy(values.begin(),values.end(),state.rangeColor.begin());return true;
}
bool NativeSensor31657CircleProgramState::Integers(int location,std::span<const int32_t> values)
{
 if(location==-1)return true;if(values.size()!=1)return false;
 if(location==0){state.heightUnit=values[0];return true;}if(location==2){state.circleCount=values[0];return true;}return false;
}
NativeSensor31657CircleOriginalUniforms ReadNativeSensor31657CircleUniforms(const NativeSensor31657CircleDraw& draw)
{
 Check(draw.original&&draw.program.defines,"31657 original circle UBO/program missing");ValidateNativeCUSUniformPublication(*draw.original);
 const auto& m=*draw.original->matrices;const auto& p=*draw.original->params;
 Check(m.sourceDrawFrame<=draw.sourceDrawFrame&&p.sourceDrawFrame<=draw.sourceDrawFrame,"31657 circle uses future original UBOs");
 NativeSensor31657CircleOriginalUniforms out;Copy(out.cameraViewProjection,m.bytes,5*64);Copy(out.cameraViewInverse,m.bytes,7*64);Copy(out.cameraViewProjectionInverse,m.bytes,9*64);
 Copy(out.timeInfo,p.bytes,16);Copy(out.viewGeometry,p.bytes,32);Copy(out.mapSize,p.bytes,48);std::memcpy(&out.renderCaps,p.bytes.data()+12,4);return out;
}
NativeSensor31657CircleModelPositions ReadNativeSensor31657CircleModelPositions(const NativeSensor31657CircleDraw& draw)
{
 Check(draw.models&&draw.models->kind==NativeCUSPublishedKind::ModelUniforms&&draw.models->stride==128&&draw.models->publishedDrawFrame<=draw.sourceDrawFrame,"31657 circle original SSBO1 publication missing/future");
 Instances(draw.instances);Circles(draw);
 Check(draw.firstInstance<=draw.instances->layout.elements&&draw.instanceCount<=draw.instances->layout.elements-draw.firstInstance,"31657 circle instance range invalid");
 NativeSensor31657CircleModelPositions out;out.instances.resize(draw.instanceCount);out.circles.resize(draw.program.circleCount);std::array<uint8_t,128> row;
 const auto read=[&](const uint8_t* instance,std::array<float,4>& position){uint32_t index=0;std::memcpy(&index,instance+20,4);ReadNativeCUSPublishedElement(*draw.models,index,row);std::memcpy(position.data(),row.data()+32,16);};
 for(uint32_t i=0;i<draw.instanceCount;++i)read(draw.instances->bytes.data()+size_t(draw.firstInstance+i)*32,out.instances[i]);
 for(int32_t i=0;i<draw.program.circleCount;++i)read(draw.circles.snapshot->bytes.data()+draw.circles.offset+size_t(i)*32,out.circles[i]);
 return out;
}
void ValidateNativeSensor31657CircleDraw(const NativeSensor31657CircleDraw& draw)
{
 Check(bool(draw.program.defines)&&bool(draw.consumerLifetime),"31657 circle source program/compiled family missing");Vertices(draw.vertices);Instances(draw.instances);Circles(draw);
 const auto& d=*draw.program.defines;const uint32_t originalCount=d.kind==NativeSensor31657CircleKind::Radar?127:63;
 Check(draw.first==0&&draw.count==originalCount&&draw.count<=draw.vertices->layout.elements&&draw.firstInstance==0&&draw.instanceCount<=draw.instances->layout.elements,"31657 circle original center/rim/LINE_LOOP draw differs");
 // The actual source calls CopyTo(usedElements*32), binds the destination,
 // then uses the identical usedElements for both draw instanceCount and uniform.
 Check(draw.program.circleCount>=0&&uint32_t(draw.program.circleCount)==draw.instanceCount,"31657 selected circleCount/instanceCount differ");
 Check(!draw.raster.depthClamp&&!draw.raster.inheritedClipPlane&&draw.raster.frontPolygonMode==NativePolygonMode::Fill&&draw.raster.backPolygonMode==NativePolygonMode::Fill&&!draw.raster.lineStipple,"31657 circle unsupported inherited raster state");
 Check(std::isfinite(draw.raster.lineWidth)&&draw.raster.lineWidth>0&&draw.raster.lineWidth<=128,"31657 circle actual line width invalid");
 Check(draw.program.heightUnit>=0&&draw.program.heightUnit<int(NativeCUSTextureHistory::Units)&&draw.heightImage.object&&draw.heightImage.object->target==NativeCUSTextureTarget::Image2D,"31657 circle original height image missing");
 (void)ReadNativeSensor31657CircleUniforms(draw);(void)ReadNativeSensor31657CircleModelPositions(draw);
}
}
