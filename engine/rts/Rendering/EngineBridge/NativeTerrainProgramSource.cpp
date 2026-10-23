/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTerrainProgramSource.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace rendering {
namespace {
NativeTerrainProgramServices services;
uint64_t nextGeneration=0;
class SourceProgram;
std::vector<SourceProgram*> livePrograms; // Actual creation/deletion events; no program cache.
void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
uint64_t Context(){
 Require(bool(services.contextGeneration),"Actual terrain program context service is not installed");
 const auto context=services.contextGeneration();Require(context!=0,"Actual terrain program context is not alive");return context;
}
uint64_t Generation(){
 Require(nextGeneration!=std::numeric_limits<uint64_t>::max(),"Original terrain program lifetime counter overflow");return ++nextGeneration;
}
uint64_t Hash(const std::string& source){uint64_t result=14695981039346656037ULL;for(unsigned char c:source)result=(result^c)*1099511628211ULL;return result;}
std::string Stage(const char* path){
 CFileHandler source(path);std::string bytes;
 Require(source.FileExists()&&source.LoadStringData(bytes)&&!bytes.empty(),"Actual original SMF shader source is missing");return bytes;
}
std::vector<std::pair<std::string,std::string>> Defines(const std::string& source){
 std::vector<std::pair<std::string,std::string>> result;std::istringstream input(source);std::string line;
 while(std::getline(input,line)){
  if(line.empty())continue;
  Require(line.starts_with("#define "),"Original SMF compile definition syntax differs");
  const auto split=line.find_first_of(" \t",8);const auto name=line.substr(8,split==std::string::npos?split:split-8);
  std::string value;if(split!=std::string::npos){const auto start=line.find_first_not_of(" \t",split);if(start!=std::string::npos)value=line.substr(start);}
  result.emplace_back(name,std::move(value));
 }
 std::sort(result.begin(),result.end());return result;
}
class SourceProgram final:public Shader::IProgramObject {
public:
 SourceProgram(NativeTerrainProgramSelection choice,std::string originalName,std::string definitions):
  Shader::IProgramObject(originalName),selection(choice),rawDefinitions(std::move(definitions)){
  identity.contextGeneration=Context();identity.originalProgram=this;identity.originalName=std::move(originalName);
  identity.originalProgramGeneration=Generation();identity.selection=choice;
  uniforms=std::make_shared<NativeTerrainUniformSnapshot>();uniforms->contextGeneration=identity.contextGeneration;
  uniforms->originalProgramGeneration=identity.originalProgramGeneration;
  livePrograms.push_back(this);
 }
 ~SourceProgram()override{
  const auto current=OriginalNativeSourceProgramBinding().Current();
  if(current.terrainSource&&current.terrainSource->program.identity.originalProgram==this)OriginalNativeSourceProgramBinding().Unbind();
  if(shaderHandler->GetCurrentlyBoundProgram()==this)shaderHandler->SetCurrentlyBoundProgram(nullptr);
  livePrograms.erase(std::find(livePrograms.begin(),livePrograms.end(),this));
 }
 bool OriginalValid()const{
  CheckContext();
  if(state==NativeTerrainProgramPublication::State::Unsupported)throw std::runtime_error(unsupportedReason);
  if(state!=NativeTerrainProgramPublication::State::Linked)return false;
  ValidateNativeTerrainProgram(identity,consumer,identity.contextGeneration);return true;
 }
 void LoadLua(uint32_t handle){
  CheckContext();Require(IsLua(),"Original Lua map shader was assigned to a built-in terrain owner");
  consumer.reset();unsupportedReason.clear();valid=false;objID=handle;
  identity.originalGLName=handle;identity.originalProgramGeneration=Generation();identity.sourceRevision=0;
  identity.vertexSourceHash=identity.fragmentSourceHash=0;identity.defines.clear();
  uniforms=std::make_shared<NativeTerrainUniformSnapshot>();uniforms->contextGeneration=identity.contextGeneration;
  uniforms->originalProgramGeneration=identity.originalProgramGeneration;uniformStates.clear();locations.clear();
  if(!handle){state=NativeTerrainProgramPublication::State::Invalid;return;}
  if(!services.resolveLua){Unsupported("Actual nonzero Lua terrain shader has no observed native source/lifetime owner");return;}
  const auto imported=services.resolveLua(handle);
  if(imported.status==NativeTerrainLuaProgramImport::Status::Unsupported){
   Require(!imported.unsupportedReason.empty(),"Unsupported original Lua terrain source has no failure reason");Unsupported(imported.unsupportedReason);return;
  }
  Require(imported.status==NativeTerrainLuaProgramImport::Status::Observed&&imported.unsupportedReason.empty(),"Original Lua terrain source observation status is invalid");
  request=imported.request;
  Require(request.identity.contextGeneration==identity.contextGeneration&&request.identity.originalProgramGeneration&&request.identity.sourceRevision&&
   request.identity.originalGLName==handle&&!request.vertexSource.empty()&&!request.fragmentSource.empty(),"Original Lua terrain source/lifetime publication is incomplete");
  request.identity.originalProgram=this;request.identity.originalName=name;request.identity.selection=selection;
  identity=request.identity;
  Require(identity.vertexSourceHash==Hash(request.vertexSource)&&identity.fragmentSourceHash==Hash(request.fragmentSource),"Original Lua terrain retained stage source identity differs");
  if(imported.uniforms){
   Require(imported.uniforms->contextGeneration==identity.contextGeneration&&imported.uniforms->originalProgramGeneration==identity.originalProgramGeneration,
    "Original Lua terrain uniforms belong to another source lifetime");
   uniforms=std::make_shared<NativeTerrainUniformSnapshot>(*imported.uniforms);
  }else{
   uniforms->originalProgramGeneration=identity.originalProgramGeneration;
  }
  CompileRequest();
 }
 void Link()override{
  CheckContext();
  if(IsLua())return; // Original Lua programs arrive already linked via LoadFromID.
  Reload(true,false);
 }
 void Reload(bool reloadFromDisk,bool)override{
  CheckContext();Require(!IsLua(),"Lua terrain shader cannot be recompiled as an engine SMF variant");
  if(reloadFromDisk||request.vertexSource.empty()){
   const bool border=selection==NativeTerrainProgramSelection::Border;
   const bool composite=selection==NativeTerrainProgramSelection::Composite;
   request.vertexSource=Stage(composite?"shaders/GLSL/SMFCompositeVertProg.glsl":border?"shaders/GLSL/SMFBorderVertProg.glsl":"shaders/GLSL/SMFVertProg.glsl");
   request.fragmentSource=Stage(composite?"shaders/GLSL/SMFCompositeFragProg.glsl":border?"shaders/GLSL/SMFBorderFragProg.glsl":"shaders/GLSL/SMFFragProg.glsl");
  }
  Require(identity.sourceRevision!=std::numeric_limits<uint64_t>::max(),"Original terrain source revision overflow");++identity.sourceRevision;
  identity.vertexSourceHash=Hash(request.vertexSource);identity.fragmentSourceHash=Hash(request.fragmentSource);
  request.rawDefinitions=rawDefinitions;request.flagDefinitions=shaderFlags.GetString();
  identity.defines=Defines(rawDefinitions+"\n"+request.flagDefinitions);request.identity=identity;
  request.attributeBindings.clear();request.outputBindings.clear();
  for(const auto& [attribute,index]:attribLocations)request.attributeBindings.emplace_back(attribute,uint32_t(index));
  for(const auto& [output,index]:outputLocations)request.outputBindings.emplace_back(output,uint32_t(index));
  std::sort(request.attributeBindings.begin(),request.attributeBindings.end());std::sort(request.outputBindings.begin(),request.outputBindings.end());
  CompileRequest();shaderFlags.UpdateHash();
  // Original successful GLSLCopyState ends with glUseProgram(0). It does not
  // restore an unrelated program which happened to be current before reload.
  if(state==NativeTerrainProgramPublication::State::Linked)OriginalNativeSourceProgramBinding().Unbind();
 }
 void ReloadOriginalOwner(){if(IsLua())LoadLua(objID);else Reload(true,true);}
 bool MatchesCurrentPublication(const NativeTerrainProgramPublication& source)const{
  CheckContext();return source.state==state&&source.program.identity==identity&&
   source.program.consumer==consumer&&source.program.uniforms==uniforms;
 }
 bool Validate()override{
  CheckContext();if(state!=NativeTerrainProgramPublication::State::Linked)return false;
  ValidateNativeTerrainProgram(identity,consumer,identity.contextGeneration);return valid=true;
 }
 void Enable()override{
  CheckContext();if(!IsLua()&&(!shaderFlags.HashSet()||shaderFlags.Updated()))Reload(!shaderFlags.HashSet(),true);
  EnableRaw();
 }
 void EnableRaw()override{
  CheckContext();Require(!bound,"Original terrain program is already enabled");
  Shader::IProgramObject::Enable();Publish(true);
 }
 void Disable()override{DisableRaw();}
 void DisableRaw()override{
  CheckContext();Require(bound,"Original terrain program disable has no matching enable");
  Shader::IProgramObject::Disable();OriginalNativeSourceProgramBinding().Unbind();
 }
 void Release()override{
  consumer.reset();request={};unsupportedReason.clear();valid=false;objID=0;state=NativeTerrainProgramPublication::State::Unlinked;
  uniformStates.clear();locations.clear();
  uniforms=std::make_shared<NativeTerrainUniformSnapshot>();uniforms->contextGeneration=identity.contextGeneration;
  uniforms->originalProgramGeneration=identity.originalProgramGeneration;
 }
 void BindAttribLocation(const std::string& attribute,uint32_t index)override{attribLocations[attribute]=int(index);}
 void BindOutputLocation(const std::string& output,uint32_t index)override{outputLocations[output]=int(index);}
#define TERRAIN_SCALAR(TYPE,SUFFIX) \
 void SetUniform1##SUFFIX(int i,TYPE a)override{const TYPE v[]{a};Write(i,1,1,1,false,v);} \
 void SetUniform2##SUFFIX(int i,TYPE a,TYPE b)override{const TYPE v[]{a,b};Write(i,2,1,1,false,v);} \
 void SetUniform3##SUFFIX(int i,TYPE a,TYPE b,TYPE c)override{const TYPE v[]{a,b,c};Write(i,3,1,1,false,v);} \
 void SetUniform4##SUFFIX(int i,TYPE a,TYPE b,TYPE c,TYPE d)override{const TYPE v[]{a,b,c,d};Write(i,4,1,1,false,v);}
 TERRAIN_SCALAR(int,i)
 TERRAIN_SCALAR(float,f)
#undef TERRAIN_SCALAR
#define TERRAIN_VECTOR(TYPE,SUFFIX) \
 void SetUniform2##SUFFIX(int i,const TYPE* v)override{Write(i,2,1,1,false,v);} \
 void SetUniform3##SUFFIX(int i,const TYPE* v)override{Write(i,3,1,1,false,v);} \
 void SetUniform4##SUFFIX(int i,const TYPE* v)override{Write(i,4,1,1,false,v);} \
 void SetUniform1##SUFFIX(int i,GLsizei n,const TYPE* v)override{Write(i,1,1,n,false,v);} \
 void SetUniform2##SUFFIX(int i,GLsizei n,const TYPE* v)override{Write(i,2,1,n,false,v);} \
 void SetUniform3##SUFFIX(int i,GLsizei n,const TYPE* v)override{Write(i,3,1,n,false,v);} \
 void SetUniform4##SUFFIX(int i,GLsizei n,const TYPE* v)override{Write(i,4,1,n,false,v);}
 TERRAIN_VECTOR(int,iv)
 TERRAIN_VECTOR(float,fv)
#undef TERRAIN_VECTOR
 void SetUniformMatrix2fv(int i,bool transpose,const float* v)override{Write(i,2,2,1,transpose,v);}
 void SetUniformMatrix3fv(int i,bool transpose,const float* v)override{Write(i,3,3,1,transpose,v);}
 void SetUniformMatrix4fv(int i,bool transpose,const float* v)override{Write(i,4,4,1,transpose,v);}
private:
 NativeTerrainProgramSelection selection;
 std::string rawDefinitions,unsupportedReason;
 NativeTerrainProgramIdentity identity;
 NativeTerrainProgramRequest request;
 NativeTerrainProgramPublication::State state=NativeTerrainProgramPublication::State::Unlinked;
 std::shared_ptr<const NativeTerrainProgramConsumer> consumer;
 std::shared_ptr<NativeTerrainUniformSnapshot> uniforms;
 std::vector<std::string> locations; // Source-name lookup slots, NEVER GL uniform locations or program names.
 bool IsLua()const{return selection==NativeTerrainProgramSelection::LuaForward||selection==NativeTerrainProgramSelection::LuaDeferred;}
 void CheckContext()const{Require(identity.contextGeneration==Context(),"Original terrain program crossed actual native context lifetime");}
 int GetUniformLoc(const char* uniform)override{
  Require(uniform&&*uniform,"Original terrain uniform name is missing");
  const auto it=std::find(locations.begin(),locations.end(),uniform);
  if(it!=locations.end())return int(it-locations.begin());
  Require(locations.size()<size_t(std::numeric_limits<int>::max()),"Original terrain uniform name slots overflow");
  locations.emplace_back(uniform);return int(locations.size()-1);
 }
 int GetUniformType(int)override{return -1;} // No GL type query is invented; publication retains actual writer shape/type.
 void Unsupported(std::string reason){
  Require(!reason.empty(),"Original unsupported terrain source has no reason");consumer.reset();unsupportedReason=std::move(reason);
  state=NativeTerrainProgramPublication::State::Unsupported;valid=false;
 }
 void CompileRequest(){
  Require(bool(services.compile),"Actual terrain library/PSO factory is not installed");
  const auto linked=services.compile(request); // Real compilation failures propagate; no STD fallback.
  Require(bool(linked.consumer)!=!linked.unsupportedReason.empty(),"Terrain factory must return exactly one compiled owner or unsupported reason");
  if(!linked.consumer){Unsupported(linked.unsupportedReason);return;}
  ValidateNativeTerrainProgram(identity,linked.consumer,identity.contextGeneration);
  Require(!linked.linkDefaults.empty(),"Successful terrain link did not publish its actual uniform ABI defaults");
  Require(uniforms->revision!=std::numeric_limits<uint64_t>::max(),"Original terrain link uniform revision overflow");
  auto next=std::make_shared<NativeTerrainUniformSnapshot>(*uniforms);++next->revision;
  // GLSLCopyState retains the original named writer state, including names
  // previously optimized out. Only genuinely new active names get link zero.
  for(const auto& [name,value]:linked.linkDefaults)next->values.try_emplace(name,value);
  uniforms=std::move(next);
  consumer=linked.consumer;unsupportedReason.clear();state=NativeTerrainProgramPublication::State::Linked;valid=true;
 }
 bool OwnsCurrent()const{
  const auto current=OriginalNativeSourceProgramBinding().Current();
  return current.terrainSource&&current.terrainSource->program.identity.originalProgram==this&&
   current.terrainSource->program.identity.originalProgramGeneration==identity.originalProgramGeneration&&
   current.terrainSource->program.identity.sourceRevision==identity.sourceRevision;
 }
 void Publish(bool originalEnable=false){
  if(!originalEnable)Require(OwnsCurrent(),"Original terrain uniform write targets another actual current program; cross-program source location mapping is unsupported");
  auto publication=std::make_shared<NativeTerrainProgramPublication>();publication->state=state;
  publication->program={identity,consumer,uniforms};publication->unsupportedReason=unsupportedReason;
  OriginalNativeSourceProgramBinding().BindTerrainSource(std::move(publication));
 }
 template<typename T>void Write(int location,uint32_t rows,uint32_t columns,GLsizei count,bool transpose,const T* input){
  CheckContext();Require(bound,"Original terrain uniform writer has no enabled original program");
  if(location==-1||count==0)return;
  Require(count>0&&location>=0&&size_t(location)<locations.size()&&input,"Original terrain uniform source location/count is invalid");
  Require(OwnsCurrent(),"Original terrain uniform source program is no longer the actual current program");
  Require(uniforms->revision!=std::numeric_limits<uint64_t>::max(),"Original terrain uniform publication revision overflow");
  Require(size_t(count)<=std::numeric_limits<size_t>::max()/rows/columns,"Original terrain uniform extent overflow");
  const size_t size=size_t(count)*rows*columns;
  NativeTerrainUniformValue value;value.rows=rows;value.columns=columns;value.count=uint32_t(count);value.transpose=transpose;
  if constexpr(std::is_same_v<T,float>){
   for(size_t i=0;i<size;++i)Require(std::isfinite(input[i]),"Original terrain uniform source is nonfinite");
   value.values=std::vector<float>(input,input+size);
  }else{
   static_assert(std::is_same_v<T,int> && sizeof(int)==sizeof(int32_t));
   value.values=std::vector<int32_t>(input,input+size);
  }
  auto next=std::make_shared<NativeTerrainUniformSnapshot>(*uniforms);++next->revision;
  next->values[locations[size_t(location)]]=std::move(value);uniforms=std::move(next);Publish();
 }
};
SourceProgram& Actual(Shader::IProgramObject* source){
 auto* program=dynamic_cast<SourceProgram*>(source);Require(program!=nullptr,"Actual selected terrain source is not owned by the native SMF program writer");return *program;
}
}
void NativeSourceProgramBinding::BindTerrainSource(std::shared_ptr<const NativeTerrainProgramPublication> source)
{
 Require(bool(source),"Original terrain source publication is missing");
 if(source->state==NativeTerrainProgramPublication::State::Linked){
  ValidateNativeTerrainProgram(source->program.identity,source->program.consumer,Context());
  BindTerrain(source->program.consumer);current.terrainSource=std::move(source);return;
 }
 current={};current.terrainSource=std::move(source);
 if(current.terrainSource->state==NativeTerrainProgramPublication::State::Invalid){current.kind=Kind::Unbound;return;}
 current.kind=Kind::UnsupportedTerrain; // Preserve real unlinked/unsupported source, never a fabricated GL name.
}
void ConfigureNativeTerrainProgramSource(NativeTerrainProgramServices actual)
{
 Require(livePrograms.empty(),"Actual terrain source services cannot be replaced while original programs live");
 Require(bool(actual.contextGeneration)==bool(actual.compile),"Terrain source factory/context services must be installed together");
 services=std::move(actual);
}
std::shared_ptr<Shader::IProgramObject> CreateNativeTerrainProgramSource(NativeTerrainProgramSelection selection,std::string name,std::string definitions)
{
 return std::make_shared<SourceProgram>(selection,std::move(name),std::move(definitions));
}
void LoadNativeTerrainLuaProgram(Shader::IProgramObject* source,uint32_t handle){Actual(source).LoadLua(handle);}
void ReloadNativeTerrainPrograms(){for(auto* source:livePrograms)source->ReloadOriginalOwner();}
bool RequireNativeTerrainProgramValid(const Shader::IProgramObject* source)
{
 if(!source)return false;return Actual(const_cast<Shader::IProgramObject*>(source)).OriginalValid();
}
NativeTerrainProgramSnapshot CaptureCurrentNativeTerrainProgram()
{
 const auto current=OriginalNativeSourceProgramBinding().Current();
 Require(bool(current.terrainSource),"Actual current water-ground program has no native terrain source publication");
 const auto& source=*current.terrainSource;
 Require(source.program.identity.originalProgram&&
  Actual(const_cast<Shader::IProgramObject*>(source.program.identity.originalProgram)).MatchesCurrentPublication(source),
  "Actual current terrain source publication no longer matches its original owner revision");
 if(source.state==NativeTerrainProgramPublication::State::Unsupported)throw std::runtime_error(source.unsupportedReason);
 Require(source.state==NativeTerrainProgramPublication::State::Linked&&current.kind==NativeSourceProgramBinding::Kind::Terrain&&
  current.terrain&&current.terrain==source.program.consumer,"Actual current terrain binding differs from its source publication");
 ValidateNativeTerrainProgram(source.program.identity,source.program.consumer,Context());
 Require(source.program.uniforms&&source.program.uniforms->contextGeneration==Context()&&
  source.program.uniforms->originalProgramGeneration==source.program.identity.originalProgramGeneration,
  "Actual current terrain uniform publication belongs to another original lifetime");
 return source.program;
}
} // namespace rendering
