/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeSensorPreviewData.h"
#include "NativeSensorPreviewPrograms.h"
#include "NativeJammerPreviewPrograms.h"
#include "NativeMiniMapData.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
namespace rendering {
namespace {
void Check(bool value,const char* why){if(!value)throw std::invalid_argument(why);}
std::optional<std::string> Extract(std::string_view source,std::string_view pattern)
{
 std::string expected(pattern);constexpr std::string_view engine="//__ENGINEUNIFORMBUFFERDEFS__",defines="//__DEFINES__";
 if(const auto at=expected.find(engine);at!=expected.npos)expected.replace(at,engine.size(),sensorPreviewProgram::EngineDefinitions);
 const auto at=expected.find(defines);Check(at!=expected.npos,"31657 Preview define marker missing");
 const std::string_view p(expected);const auto prefix=p.substr(0,at),suffix=p.substr(at+defines.size());
 if(!source.starts_with(prefix)||!source.ends_with(suffix)||source.size()<prefix.size()+suffix.size())return {};
 return std::string(source.substr(prefix.size(),source.size()-prefix.size()-suffix.size()));
}
float Number(std::string_view text)
{
 float result=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),result);
 Check(p.ec==std::errc{}&&p.ptr==text.data()+text.size()&&std::isfinite(result),"31657 Preview define is not finite numeric original output");return result;
}
void ValidateJammerDefines(NativeSensorPreviewProgramSource& source,const std::map<std::string,std::string>& values)
{
 const std::map<std::string,float> numbers{
  {"RADAR_CELL_SIZE",32},{"ALLIED_ALPHA",.85f},{"SHEET_ALPHA",.17f},{"SHEET_OUTLINE_ALPHA",.8f},
  {"OUTLINE_GAP_ALPHA",.3f},{"OUTLINE_WIDTH",1.2f},{"OUTLINE_WORLD_WIDTH",.89f},
  {"OUTLINE_THIN_START",3000},{"OUTLINE_THIN_END",8000},{"OUTLINE_THIN_SCALE",.6f},
  {"OUTLINE_MIN_WIDTH",1},{"FADE_START",4000},{"FADE_END",8000},{"OFFMAP_OUTLINE_FADE",50},{"OUTLINE_DASHES",4},
  {"MINIMAP_ALPHA",.33f},{"MINIMAP_OUTLINE_ALPHA",.4f},{"MINIMAP_ALLIED_OUTLINE_ALPHA",.25f},
  {"MINIMAP_OUTLINE_WIDTH",2},{"INACTIVE_ALPHA",.5f},{"INACTIVE_OUTLINE_ALPHA",.4f},
  {"MIN_COVERAGE",.04f},{"SPAWN_SPEED",18},{"MAX_ALLIED_CIRCLES",64},
  {"CIRCLE_SDF_RANGE",128},{"CIRCLE_SDF_SLACK",8}
 };
 Check(values.size()==numbers.size()+6,"31657 Jammer selected define set differs");
 for(const auto& [name,expected]:numbers){const auto at=values.find(name);Check(at!=values.end()&&Number(at->second)==expected,"31657 Jammer original numeric define differs");}
 for(const auto* name:{"MAP_CELLS_X","MAP_CELLS_Z"}){const auto at=values.find(name);Check(at!=values.end(),"31657 Jammer map-cell define missing");const auto n=Number(at->second);Check(n>=1&&n<=32768&&std::floor(n)==n,"31657 Jammer map-cell extent invalid");}
 const auto color=[&](const char* name){const auto at=values.find(name);Check(at!=values.end(),"31657 Jammer color missing");auto v=std::string_view(at->second);Check(v.starts_with("vec3(")&&v.ends_with(")"),"31657 Jammer color syntax differs");v.remove_prefix(5);v.remove_suffix(1);std::array<float,3> out{};
  for(unsigned i=0;i<3;++i){const auto comma=v.find(',');Check((i==2)==(comma==v.npos),"31657 Jammer color arity differs");auto item=v.substr(0,comma);while(!item.empty()&&(item.front()==' '||item.front()=='\t'))item.remove_prefix(1);while(!item.empty()&&(item.back()==' '||item.back()=='\t'))item.remove_suffix(1);out[i]=Number(item);if(i<2)v.remove_prefix(comma+1);}return out;
 };
 const auto allied=color("ALLIED_COLOR"),outline=color("OUTLINE_COLOR");
 Check(color("SHEET_COLOR")==std::array<float,3>{.90f,.22f,.20f}&&
  ((allied==std::array<float,3>{.62f,.35f,.33f}&&outline==std::array<float,3>{1,.32f,.22f})||
   (allied==std::array<float,3>{.55f,.25f,.45f}&&outline==std::array<float,3>{.90f,.4f,.45f})),"31657 Jammer allied/enemy color pair differs");
 const auto model=values.find("MODEL_DEPTH_TEST");Check(model!=values.end(),"31657 Jammer model-depth branch missing");const auto flag=Number(model->second);Check(flag==0||flag==1,"31657 Jammer model-depth branch invalid");
 source.modelDepthTest=flag==1;source.jammer=true;
}
void ValidateDefines(NativeSensorPreviewProgramSource& source)
{
 std::map<std::string,std::string> values;std::istringstream lines(source.defines);std::string line;
 while(std::getline(lines,line)){
  if(line.find_first_not_of(" \t\r")==line.npos)continue;std::istringstream fields(line);std::string directive,name,value;fields>>directive>>name;std::getline(fields,value);
  const auto first=value.find_first_not_of(" \t\r"),last=value.find_last_not_of(" \t\r");Check(first!=value.npos,"31657 Preview empty define");value=value.substr(first,last-first+1);
  Check(directive=="#define"&&!name.empty()&&values.emplace(name,value).second,"31657 Preview define syntax/duplicate differs");
 }
 if(values.contains("MAX_ALLIED_CIRCLES")){ValidateJammerDefines(source,values);return;}
 const std::map<std::string,float> numbers{
#include "NativeSensorPreviewDefineValues.inc"
 };
 const std::map<std::string,std::array<float,3>> colors{
#include "NativeSensorPreviewDefineColors.inc"
 };
 Check(values.size()==numbers.size()+colors.size()+1,"31657 Preview selected define set differs");
 for(const auto& [name,expected]:numbers){const auto at=values.find(name);Check(at!=values.end()&&Number(at->second)==expected,"31657 Preview original numeric define differs");}
 for(const auto& [name,expected]:colors){const auto at=values.find(name);Check(at!=values.end(),"31657 Preview original color define missing");auto v=std::string_view(at->second);
  Check(v.starts_with("vec3(")&&v.ends_with(")"),"31657 Preview original color syntax differs");v.remove_prefix(5);v.remove_suffix(1);
  for(unsigned i=0;i<3;++i){const auto comma=v.find(',');Check((i==2)==(comma==v.npos),"31657 Preview original color arity differs");auto item=v.substr(0,comma);while(!item.empty()&&(item.front()==' '||item.front()=='\t'))item.remove_prefix(1);while(!item.empty()&&(item.back()==' '||item.back()=='\t'))item.remove_suffix(1);Check(Number(item)==expected[i],"31657 Preview original color component differs");if(i<2)v.remove_prefix(comma+1);}
 }
 const auto model=values.find("MODEL_DEPTH_TEST");Check(model!=values.end(),"31657 Preview actual model-depth branch missing");const float flag=Number(model->second);Check(flag==0||flag==1,"31657 Preview model-depth branch invalid");source.modelDepthTest=flag==1;
}
constexpr std::array<std::string_view,35> names{"passRect","mipParams","losParams","coverageAbsolute","alliedCirclePass","alliedCircle","smoothParams","radarcenter_range","lookupParams","animParams","circleParams","discParams","inactiveParams","mapParams","heightmapTex","mipHeightTex","alliedWinnerTex","prevTex","targetTex","mapDepths","modelDepths","coverageTex","radarInfoTex","alliedRunnerUpTex","previewParams","previewInactive","stippleOffset","circleMode","emitterXZ","alliedParams","alliedCircles","alliedCircleCount","alliedTex","alliedSdfTex","fadeStrength"};
int Sampler(NativeSensorPreviewProgramKind kind,int location)
{
 using K=NativeSensorPreviewProgramKind;switch(kind){
 case K::Mip:return location==14?0:-1;
 case K::Coverage:return location==15?0:location==16?1:-1;
 case K::Smooth:return location==17?0:location==18?1:-1;
 case K::Sheet:return location==19?0:location==20?1:location==21?2:location==22?3:location==16?4:location==23?5:location==18?6:-1;
 case K::Minimap:return location==21?0:location==22?1:location==18?2:-1;
 case K::JammerDisc:return -1;
 case K::JammerSheet:return location==19?0:location==20?1:location==21?2:location==32?3:location==18?4:location==33?5:-1;}
 return -1;
}
std::array<float,4>* Field(NativeSensorPreviewUniformBlock& u,int location)
{
 switch(location){case 0:return &u.passRect;case 1:return &u.mipParams;case 2:return &u.losParams;case 5:return &u.alliedCircle;case 6:return &u.smoothParams;case 7:return &u.radarcenter_range;case 8:return &u.lookupParams;case 9:return &u.animParams;case 10:return &u.circleParams;case 11:return &u.discParams;case 12:return &u.inactiveParams;case 13:return &u.mapParams;case 24:return &u.previewParams;default:return nullptr;}
}
}
std::uint64_t NativeSensorPreviewSourceHash(std::string_view text)noexcept{uint64_t h=14695981039346656037ull;for(unsigned char c:text)h=(h^c)*1099511628211ull;return h;}
namespace {
NativeSensorPreviewProgramSource ParseProgramSource(std::string_view vertex,std::string_view fragment)
{
 using namespace sensorPreviewProgram;using K=NativeSensorPreviewProgramKind;
 struct Candidate{K kind;std::string_view vs,fs,vsHash,fsHash;};
 const std::array candidates{Candidate{K::Mip,PassVertex,MipFragment,PassVertexSHA512,MipFragmentSHA512},Candidate{K::Coverage,PassVertex,CoverageFragment,PassVertexSHA512,CoverageFragmentSHA512},Candidate{K::Smooth,PassVertex,SmoothFragment,PassVertexSHA512,SmoothFragmentSHA512},Candidate{K::Sheet,PassVertex,SheetFragment,PassVertexSHA512,SheetFragmentSHA512},Candidate{K::Minimap,MinimapVertex,MinimapFragment,MinimapVertexSHA512,MinimapFragmentSHA512},Candidate{K::JammerDisc,PassVertex,JammerDiscFragment,PassVertexSHA512,JammerDiscFragmentSHA512},Candidate{K::JammerSheet,PassVertex,JammerSheetFragment,PassVertexSHA512,JammerSheetFragmentSHA512}};
 std::shared_ptr<NativeSensorPreviewProgramSource> source;
 for(const auto& candidate:candidates){auto vs=Extract(vertex,candidate.vs),fs=Extract(fragment,candidate.fs);if(!vs||!fs||*vs!=*fs)continue;
  source=std::make_shared<NativeSensorPreviewProgramSource>();source->kind=candidate.kind;source->vertex=vertex;source->fragment=fragment;source->defines=*vs;
  if(candidate.kind==K::Sheet||candidate.kind==K::JammerSheet)source->engineDefinitions=EngineDefinitions;source->originalVertexSHA512=candidate.vsHash;source->originalFragmentSHA512=candidate.fsHash;break;
 }
 Check(bool(source),"31657 complete original Preview program strings differ");ValidateDefines(*source);
 Check(source->jammer?(source->kind!=K::Mip&&source->kind!=K::Coverage&&source->kind!=K::Sheet):(source->kind!=K::JammerDisc&&source->kind!=K::JammerSheet),"31657 Preview source family/define combination differs");
 source->expandedVertexHash=NativeSensorPreviewSourceHash(vertex);source->expandedFragmentHash=NativeSensorPreviewSourceHash(fragment);
 return std::move(*source);
}
}
void ValidateNativeSensorPreviewProgramSource(const NativeSensorPreviewProgramSource& source)
{
 const auto actual=ParseProgramSource(source.vertex,source.fragment);
 Check(source.kind==actual.kind&&source.defines==actual.defines&&source.engineDefinitions==actual.engineDefinitions&&source.originalVertexSHA512==actual.originalVertexSHA512&&source.originalFragmentSHA512==actual.originalFragmentSHA512&&source.expandedVertexHash==actual.expandedVertexHash&&source.expandedFragmentHash==actual.expandedFragmentHash&&source.modelDepthTest==actual.modelDepthTest&&source.jammer==actual.jammer,"31657 Preview typed source metadata differs from complete original program");
}
NativeSensorPreviewProgramState::NativeSensorPreviewProgramState(std::string_view vertex,std::string_view fragment,const NativeSensorPreviewCompileFunction& compiler)
{
 auto source=std::make_shared<NativeSensorPreviewProgramSource>(ParseProgramSource(vertex,fragment));
 Check(bool(compiler),"31657 Preview actual typed Metal compile service unavailable");auto consumer=compiler(*source);
 Check(consumer&&consumer->pipelineLifetime&&consumer->source.kind==source->kind&&consumer->source.vertex==source->vertex&&consumer->source.fragment==source->fragment&&consumer->source.defines==source->defines&&consumer->source.engineDefinitions==source->engineDefinitions&&consumer->source.originalVertexSHA512==source->originalVertexSHA512&&consumer->source.originalFragmentSHA512==source->originalFragmentSHA512&&consumer->source.expandedVertexHash==source->expandedVertexHash&&consumer->source.expandedFragmentHash==source->expandedFragmentHash&&consumer->source.modelDepthTest==source->modelDepthTest,"31657 Preview actual compiled consumer/source owner differs");
 state.source=std::move(source);state.consumer=std::move(consumer);
 // Original GLSL initializers. Other non-sampler uniforms have GL's initial
 // zero values; the unmodified original CreateShader applies its real table.
 state.uniforms.passRect={-1,-1,1,1};
}
std::vector<std::string_view> NativeSensorPreviewProgramState::ActiveUniforms()const
{
 using K=NativeSensorPreviewProgramKind;switch(state.source->kind){
 case K::Mip:return {names[0],names[1],names[14]};
 case K::Coverage:return {names[0],names[2],names[3],names[4],names[5],names[15],names[16]};
 case K::Smooth:return {names[0],names[6],names[17],names[18]};
 case K::Sheet:{std::vector<std::string_view> out{names[0],names[7],names[8],names[9],names[10],names[19],names[21],names[22],names[16],names[23],names[18]};if(state.source->modelDepthTest)out.push_back(names[20]);return out;}
 case K::Minimap:return {names[11],names[12],names[13],names[21],names[22],names[18]};
 case K::JammerDisc:return {names[0],names[11]};
 case K::JammerSheet:{std::vector<std::string_view> out{names[0],names[10],names[24],names[25],names[26],names[27],names[28],names[29],names[30],names[31],names[19],names[21],names[32],names[18],names[33],names[34]};if(state.source->modelDepthTest)out.push_back(names[20]);return out;}
 }return {};
}
int NativeSensorPreviewProgramState::Location(std::string_view name)const{if(name=="alliedCircles[0]")name="alliedCircles";const auto active=ActiveUniforms();if(std::find(active.begin(),active.end(),name)==active.end())return -1;return int(std::find(names.begin(),names.end(),name)-names.begin());}
bool NativeSensorPreviewProgramState::Floats(int location,std::span<const float> v)
{
 if(location==-1)return true;if(location<0||location>=int(names.size())||Location(names[location])==-1)return false;
 if(location==34){if(v.size()!=1||!std::isfinite(v[0]))return false;state.uniforms.distanceFade[0]=v[0];return true;}
 if(location==30){if(v.size()>192||v.size()%3)return false;for(float x:v)if(!std::isfinite(x))return false;std::copy(v.begin(),v.end(),state.uniforms.jammerCircles.begin());state.jammerCircleValues=std::uint32_t(v.size());return true;}
 if(location==25||location==26||location==27||location==31){if(v.size()!=1||!std::isfinite(v[0]))return false;const unsigned slot=location==31?3:unsigned(location-25);if(location==31&&(v[0]<0||v[0]>64||std::floor(v[0])!=v[0]||v[0]*3>state.jammerCircleValues))return false;state.uniforms.jammerFlags[slot]=v[0];return true;}
 if(location==28||location==29){if(v.size()!=2)return false;std::copy(v.begin(),v.end(),state.uniforms.emitterAllied.begin()+(location==28?0:2));return true;}
 if(location==3||location==4){if(v.size()!=1)return false;state.uniforms.coverageFlags[location-3]=v[0];return true;}
 auto out=Field(state.uniforms,location);if(!out||v.size()!=(location==12?3:4))return false;std::copy(v.begin(),v.end(),out->begin());return true;
}
bool NativeSensorPreviewProgramState::Integers(int location,std::span<const int32_t> v)
{
 if(location==-1)return true;if(v.size()!=1||location<0||location>=int(names.size())||Location(names[location])==-1)return false;
 const int sampler=Sampler(state.source->kind,location);if(sampler<0)return false;state.samplerUnits[sampler]=v[0];return true;
}
std::uint32_t NativeSensorPreviewRequiredTextureMask(const NativeSensorPreviewProgramSnapshot& p)
{
 Check(bool(p.source),"31657 Preview source owner missing");using K=NativeSensorPreviewProgramKind;const auto& u=p.uniforms;
 switch(p.source->kind){case K::Mip:return 1;case K::Coverage:return 1u|(u.coverageFlags[1]>1.5f?2u:0u);case K::Smooth:return 3;
 case K::Sheet:return 1u|4u|64u|(p.source->modelDepthTest?2u:0u)|(u.lookupParams[3]>.5f?(u.circleParams[0]>.5f?48u:8u):0u);
 case K::Minimap:return (p.source->jammer&&u.discParams[2]<0?0u:5u)|(u.discParams[3]>=.5f?2u:0u);
 case K::JammerDisc:return 0;
 case K::JammerSheet:return 1u|(p.source->modelDepthTest?2u:0u)|(u.jammerFlags[2]>.5f?(u.emitterAllied[2]>.5f?32u:0u):((u.previewParams[2]>=0?20u:0u)|(u.emitterAllied[2]>.5f?8u:0u)));}return 0;
}
std::uint32_t NativeSensorPreviewRequiredContentsMask(const NativeSensorPreviewProgramSnapshot& p){const auto mask=NativeSensorPreviewRequiredTextureMask(p);return p.source->kind==NativeSensorPreviewProgramKind::Smooth&&p.uniforms.smoothParams[3]>=.5f?mask&~1u:mask;}
bool NativeSensorPreviewRequiresStorage(const NativeSensorPreviewProgramSnapshot& p){Check(bool(p.source),"31657 Preview source owner missing");return p.source->kind==NativeSensorPreviewProgramKind::Coverage||(p.source->kind==NativeSensorPreviewProgramKind::Sheet&&p.uniforms.lookupParams[3]>.5f&&p.uniforms.circleParams[0]>.5f);}
void ValidateNativeSensorPreviewDraw(const NativeSensorPreviewDraw& draw)
{
 const auto& p=draw.program;Check(p.source&&p.consumer&&p.consumer->pipelineLifetime&&p.consumer->source.vertex==p.source->vertex&&p.consumer->source.fragment==p.source->fragment,"31657 Preview actual compiled program/source missing");
 Check(draw.vertices&&draw.vertices->object&&draw.vertices->revision&&draw.vertices->bytes.size()==CheckedNativeBufferByteSize(draw.vertices->layout),"31657 Preview actual vertex owner missing");const auto& l=draw.vertices->layout;
 Check(l.kind==NativeBufferKind::Vertex&&l.stride==16&&l.attributes.size()==1,"31657 Preview quad layout differs");const auto& a=l.attributes[0];Check(a.id==0&&a.offset==0&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"31657 Preview original quad attr0 differs");
 Check(draw.first<=l.elements&&draw.count==6&&draw.count<=l.elements-draw.first,"31657 Preview actual six-vertex TRIANGLES range invalid");
 Check(!draw.raster.depthClamp&&!draw.raster.depthTest&&!draw.raster.cullEnabled&&!draw.raster.lineStipple&&draw.raster.frontPolygonMode==NativePolygonMode::Fill&&draw.raster.backPolygonMode==NativePolygonMode::Fill,"31657 Preview actual original raster differs");
 const auto mask=NativeSensorPreviewRequiredTextureMask(p),contents=NativeSensorPreviewRequiredContentsMask(p);for(unsigned i=0;i<7;++i)if(mask&(1u<<i)){const auto& image=draw.images[i];
  if(image.radarSource){
   const auto& source=*image.radarSource;
   const bool selectedRadar=!p.source->jammer&&((p.source->kind==NativeSensorPreviewProgramKind::Sheet&&i==3)||(p.source->kind==NativeSensorPreviewProgramKind::Minimap&&i==1));
   Check(selectedRadar&&image.effectiveUnit==p.samplerUnits[i]&&image.effectiveUnit>=0&&image.effectiveUnit<int(NativeCUSTextureHistory::Units)&&
    !image.binding.object&&!image.dependency&&!image.version&&!image.allocation&&source.base&&source.heights&&source.info&&
    source.info->mode==NativeMiniMapInfoMode::Los&&source.info->groundLos&&source.info->radar&&source.info->jammer,
    "31657 Preview radar suffix lacks its actual selected viewer source or has competing image storage");
   const auto& radar=*source.info->radar;
   Check(radar.width&&radar.height&&radar.counts.size()==size_t(radar.width)*radar.height,"31657 Preview original radar source extent differs");
   continue;
  }
  Check(image.effectiveUnit==p.samplerUnits[i]&&image.effectiveUnit>=0&&image.effectiveUnit<int(NativeCUSTextureHistory::Units)&&(image.binding.object||image.allocation)&&(!image.binding.object||image.binding.object->target==NativeCUSTextureTarget::Image2D),"31657 Preview required actual sampler binding missing");if(image.allocation)Check(image.allocation->object&&image.allocation->width&&image.allocation->height,"31657 Preview dimension query lacks actual allocated storage");if(contents&(1u<<i))Check(image.version||(!image.allocation&&image.binding.object&&image.binding.object->storage==NativeTextureStorageKnowledge::Published&&image.binding.object->backing),"31657 Preview required contents have no actual producer publication");if(image.version)Check(image.version->image&&image.version->generation&&(!image.allocation||image.version->image==image.allocation),"31657 Preview invalid RTT content generation/allocation identity");}
 if(NativeSensorPreviewRequiresStorage(p)){const auto& r=draw.storage;Check(r.snapshot&&r.snapshot->object&&r.snapshot->revision&&r.binding==5&&r.snapshot->bytes.size()==CheckedNativeBufferByteSize(r.snapshot->layout),"31657 Preview actual SSBO5 range missing");const auto& s=r.snapshot->layout;Check(s.kind==NativeBufferKind::Storage&&s.stride==64&&s.attributes.size()==1&&r.offset%64==0&&r.offset<=r.snapshot->bytes.size()&&r.size<=r.snapshot->bytes.size()-r.offset&&draw.storageDefinedBytes<=r.size,"31657 Preview original SSBO5 byte range differs");const auto& f=s.attributes[0];Check(f.id==0&&f.declaredCount==4&&f.byteSize==64&&f.offset==0&&f.scalarType==NativeScalarType::Float32&&f.valueShape==NativeValueShape::Vector4&&!f.normalized&&f.name==(p.source->kind==NativeSensorPreviewProgramKind::Coverage?"rayData":"alliedCircles"),"31657 Preview ray/allied raw storage declaration differs");}
 if(p.source->kind==NativeSensorPreviewProgramKind::JammerSheet){const auto count=p.uniforms.jammerFlags[3];Check(count>=0&&count<=64&&std::floor(count)==count&&count*3<=p.jammerCircleValues,"31657 Jammer exact circle list lacks its original uniform upload");}
 if(p.source->kind==NativeSensorPreviewProgramKind::Sheet||p.source->kind==NativeSensorPreviewProgramKind::JammerSheet){Check(bool(draw.original),"31657 Preview Sheet requires actual complete engine UBOs");ValidateNativeCUSUniformPublication(*draw.original);Check(draw.original->matrices->sourceDrawFrame<=draw.sourceDrawFrame&&draw.original->params->sourceDrawFrame<=draw.sourceDrawFrame,"31657 Preview Sheet UBO publication is from future");}
 Check((p.source->kind==NativeSensorPreviewProgramKind::Minimap)==(draw.callin==NativeSensorPreviewCallin::Minimap),"31657 Preview original world/minimap callin differs");
}
}
