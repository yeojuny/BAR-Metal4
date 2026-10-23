/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeSensorLOSData.h"
#include "NativeSensor31657CirclePrograms.h"
#include <atomic>
#include <cstring>
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
 Check(di!=pattern.npos&&ei!=pattern.npos,"31657 LOS shader marker missing");
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
NativeSensorLOSDefines Defines(const Substitutions& source)
{
 NativeSensorLOSDefines out;out.originalText=source.defines;out.originalEngineDefinitions=source.engine;
 std::map<std::string,float> values;std::istringstream lines(source.defines);std::string line;
 while(std::getline(lines,line)){
  if(line.find_first_not_of(" \t\r")==line.npos)continue;
  std::istringstream fields(line);std::string directive,name,value,extra;fields>>directive>>name>>value;
  Check(directive=="#define"&&!name.empty()&&!value.empty()&&!(fields>>extra),"31657 LOS define syntax differs");
  float number=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
  Check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&std::isfinite(number),"31657 LOS define is not finite numeric Lua output");
  Check(values.emplace(name,number).second,"31657 LOS define repeated");
 }
 for(auto name:{"VSX","VSY","RESOLUTION","PADDING","USE_TEAMCOLOR","VISIBILITYCULLING","LINE_THIN_START","LINE_THIN_END","LINE_THIN_SCALE","LINE_MIN_WIDTH"})Check(values.contains(name),"31657 LOS required define missing");
 out.stencilPass=values.contains("STENCILPASS");
 Check(values.size()==(out.stencilPass?11:10)&&(!out.stencilPass||values.at("STENCILPASS")==1)&&values.at("RESOLUTION")==4&&values.at("PADDING")==1&&values.at("USE_TEAMCOLOR")==1&&values.at("VISIBILITYCULLING")==1&&values.at("LINE_THIN_START")==3000&&values.at("LINE_THIN_END")==8000&&values.at("LINE_THIN_SCALE")==.4f&&values.at("LINE_MIN_WIDTH")==.7f,"31657 LOS selected R8/thinning family differs");
 out.viewWidth=values.at("VSX");out.viewHeight=values.at("VSY");
 Check(out.viewWidth>=1&&out.viewHeight>=1&&out.viewWidth<=32768&&out.viewHeight<=32768,"31657 LOS view dimensions invalid");return out;
}
constexpr std::array<std::string_view,5> names{"heightmapTex","losStencilTexture","rangeColor","teamColorMix","lineWidthPx"};
std::shared_ptr<const void> consumerLifetime;
void Snapshot(const NativeBufferSnapshotRef& b)
{
 Check(b&&b->object&&b->revision&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"31657 LOS buffer owner/layout missing");
}
void Vertices(const NativeBufferSnapshotRef& b)
{
 Snapshot(b);const auto& l=b->layout;
 Check(l.kind==NativeBufferKind::Vertex&&l.stride==16&&l.attributes.size()==1,"31657 original circle vertex layout differs");const auto& a=l.attributes[0];
 Check(a.id==0&&a.offset==0&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"31657 LOS attr0 differs");
}
void Instances(const NativeBufferSnapshotRef& b)
{
 Snapshot(b);const auto& l=b->layout;
 Check(l.kind==NativeBufferKind::Vertex&&l.stride==32&&l.attributes.size()==2,"31657 original circle instance layout differs");
 for(unsigned i=0;i<2;++i){const auto& a=l.attributes[i];Check(a.id==i+1&&a.offset==16*i&&a.byteSize==16&&a.declaredCount==4&&!a.normalized&&a.scalarType==(i?NativeScalarType::UInt32:NativeScalarType::Float32),"31657 original radius/instData attribute differs");}
}
template<size_t N>void Copy(std::array<float,N>& out,const auto& bytes,size_t at){Check(at+N*4<=bytes.size(),"31657 original UBO field exceeds retained backing");std::memcpy(out.data(),bytes.data()+at,N*4);}
}
void SetNativeSensorLOSConsumerLifetime(std::shared_ptr<const void> v){std::atomic_store(&consumerLifetime,std::move(v));}
std::shared_ptr<const void> NativeSensorLOSConsumerLifetime(){return std::atomic_load(&consumerLifetime);}
bool IsNativeSensorLOSConsumerAvailable(){return bool(NativeSensorLOSConsumerLifetime());}
NativeSensorLOSProgramState::NativeSensorLOSProgramState(std::string_view vertex,std::string_view fragment)
{
 const auto vs=Extract(vertex,sensor31657CircleProgram::CircleVertex),fs=Extract(fragment,sensor31657CircleProgram::CircleFragment);
 Check(vs&&fs&&vs->defines==fs->defines&&vs->engine==fs->engine,"31657 complete original circle shader sources differ");
 state.defines=std::make_shared<const NativeSensorLOSDefines>(Defines(*vs));
}
std::vector<std::string_view> NativeSensorLOSProgramState::ActiveUniforms()const{
 if(state.defines->stencilPass)return {names[0]};return {names.begin(),names.end()};
}
int NativeSensorLOSProgramState::Location(std::string_view name)const{
 const auto active=ActiveUniforms();if(std::find(active.begin(),active.end(),name)==active.end())return -1;return int(std::find(names.begin(),names.end(),name)-names.begin());
}
bool NativeSensorLOSProgramState::Floats(int location,std::span<const float> values){
 if(location==-1)return true;for(float v:values)if(!std::isfinite(v))return false;
 if(location==2&&values.size()==4){std::copy(values.begin(),values.end(),state.rangeColor.begin());return true;}
 if(values.size()==1&&(location==3||location==4)){(location==3?state.teamColorMix:state.lineWidthPx)=values[0];return true;}return false;
}
bool NativeSensorLOSProgramState::Integers(int location,std::span<const int32_t> values){
 if(location==-1)return true;if(values.size()!=1)return false;
 if(location==0){state.heightUnit=values[0];return true;}if(location==1){state.stencilUnit=values[0];return true;}return false;
}

NativeSensorLOSOriginalUniforms ReadNativeSensorLOSUniforms(const NativeSensorLOSDraw& draw)
{
 Check(draw.original&&draw.program.defines,"31657 original circle UBO/program missing");ValidateNativeCUSUniformPublication(*draw.original);
 const auto& m=*draw.original->matrices;const auto& p=*draw.original->params;
 Check(m.sourceDrawFrame<=draw.sourceDrawFrame&&p.sourceDrawFrame<=draw.sourceDrawFrame,"31657 LOS uses future original UBOs");
 NativeSensorLOSOriginalUniforms out;Copy(out.cameraViewProjection,m.bytes,5*64);Copy(out.cameraViewInverse,m.bytes,7*64);Copy(out.cameraViewProjectionInverse,m.bytes,9*64);
 Copy(out.timeInfo,p.bytes,16);Copy(out.viewGeometry,p.bytes,32);Copy(out.mapSize,p.bytes,48);std::memcpy(&out.renderCaps,p.bytes.data()+12,4);return out;
}
NativeSensorLOSInstanceValues ReadNativeSensorLOSInstances(const NativeSensorLOSDraw& draw)
{
 Check(draw.models&&draw.models->kind==NativeCUSPublishedKind::ModelUniforms&&draw.models->stride==128&&draw.models->publishedDrawFrame<=draw.sourceDrawFrame,"31657 LOS original SSBO1 publication missing/future");
 Instances(draw.instances);Check(draw.firstInstance<=draw.instances->layout.elements&&draw.instanceCount<=draw.instances->layout.elements-draw.firstInstance,"31657 LOS instance range invalid");
 NativeSensorLOSInstanceValues out;out.positions.resize(draw.instanceCount);out.colors.resize(draw.instanceCount);std::array<uint8_t,128> row;
 for(uint32_t i=0;i<draw.instanceCount;++i){
  const auto* instance=draw.instances->bytes.data()+size_t(draw.firstInstance+i)*32;uint32_t index=0,team=0;std::memcpy(&index,instance+20,4);
  ReadNativeCUSPublishedElement(*draw.models,index,row);std::memcpy(out.positions[i].data(),row.data()+32,16);
  if(!draw.program.defines->stencilPass){std::memcpy(&team,instance+24,4);const auto color=ReadNativeCUSTeamColor(*draw.original,team&255u);std::memcpy(out.colors[i].data(),color.data(),16);}
 }
 return out;
}
void ValidateNativeSensorLOSDraw(const NativeSensorLOSDraw& draw)
{
 Check(draw.program.defines&&draw.consumerLifetime&&draw.consumerLifetime==NativeSensorLOSConsumerLifetime(),"31657 LOS original program/compiled family missing");Vertices(draw.vertices);Instances(draw.instances);
 Check(draw.first==0&&draw.count==63&&draw.count<=draw.vertices->layout.elements&&draw.firstInstance==0&&draw.instanceCount<=draw.instances->layout.elements,"31657 LOS original center/rim draw interval differs");
 Check(!draw.raster.depthClamp&&!draw.raster.inheritedClipPlane&&draw.raster.frontPolygonMode==NativePolygonMode::Fill&&draw.raster.backPolygonMode==NativePolygonMode::Fill&&!draw.raster.lineStipple,"31657 LOS inherited raster state differs");
 Check(std::isfinite(draw.raster.lineWidth)&&draw.raster.lineWidth>0&&draw.raster.lineWidth<=128&&std::isfinite(draw.program.lineWidthPx)&&draw.program.lineWidthPx>0,"31657 LOS original line width invalid");
 Check(draw.program.heightUnit==0&&draw.heightImage.knowledge==NativeCUSBindingKnowledge::BoundObject&&draw.heightImage.object&&draw.heightImage.object->target==NativeCUSTextureTarget::Image2D,"31657 LOS actual height sampler missing");
 if(!draw.program.defines->stencilPass)Check(draw.program.stencilUnit==1&&draw.stencilImage.knowledge==NativeCUSBindingKnowledge::BoundObject&&draw.stencilImage.object&&draw.stencilImage.object->target==NativeCUSTextureTarget::Image2D&&draw.stencilDependency&&draw.stencilDependency->format==ScreenTextureFormat::R8Unorm&&draw.stencilDependency->renderTarget,"31657 LOS final outline lost its actual R8 target version");
 (void)ReadNativeSensorLOSUniforms(draw);(void)ReadNativeSensorLOSInstances(draw);
}
}
