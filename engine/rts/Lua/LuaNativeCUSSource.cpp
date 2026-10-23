/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeCUSSource.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaShaders.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/VFSHandler.h"
#include "System/Sync/SHA512.hpp"
#include "Rendering/Backend/NativeCUSSourceIdentity.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace LuaNativeCUSSource {
namespace {
using namespace rendering;
Services services;
NativeCUSTextureHistory* sharedHistory=nullptr;
NativeTextureRegistry* sharedRegistry=nullptr;
std::shared_ptr<NativeCUSSourceFrame> frame;
std::vector<NativeCUSLoadedSource> loadedSources;
void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename... Args>struct LuaBoundary<R(Args...)>{
 template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args... args)->R{
  try{return f(std::forward<Args>(args)...);}catch(const std::exception&e){lua_pushstring(L,e.what());}
  lua_error(L);std::terminate();
 };}
};
template<typename F>void BindFunction(sol::table&t,const char*name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
std::string Hash(std::string_view bytes){sha512::raw_digest d;sha512::calc_digest(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),d.data());return sha512::dump_digest(d);}
struct Context;
struct Program {
	std::weak_ptr<Context> sourceContext;
	std::shared_ptr<const NativeCUSProgramConsumer> consumer;
	std::map<std::string,std::vector<float>> floats,ints;
	bool deleted=false;
};
struct Uniform {std::shared_ptr<Program> program;NativeCUSUniformDescription description;};
struct Context {
	lua_State* lua=nullptr;
	std::vector<std::shared_ptr<Program>> programs;
	std::vector<Uniform> locations;
	std::shared_ptr<Program> Active()const{
		const auto selected=OriginalNativeSourceProgramBinding().Current();
		if(selected.kind!=NativeSourceProgramBinding::Kind::CUS||!selected.owner)return {};
		auto active=std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(selected.owner));
		if(active->sourceContext.lock().get()!=this||active->deleted)return {};
		return active;
	}
	sol::protected_function definitionAndBin;
	std::shared_ptr<Program> Get(unsigned handle){Check(handle&&handle<=programs.size()&&!programs[handle-1]->deleted,"Unknown or deleted original CUS native program handle");return programs[handle-1];}
	unsigned Location(unsigned handle,const std::string&name){
		auto p=Get(handle);
		for(unsigned i=0;i<locations.size();++i)if(locations[i].program==p&&locations[i].description.name==name)return i+1;
		for(const auto& d:p->consumer->uniforms)if(d.name==name){locations.push_back({p,d});return locations.size();}
		return 0; // An actually inactive original uniform, as reported by the source-paired pipeline.
	}
	void UniformValue(int location,sol::variadic_args values,bool integer){
		if(location==-1){ // OpenGL ignores inactive writes, but still requires a current program.
			Check(bool(Active()),"CUS inactive uniform write has no actual current source program");return;
		}
		Check(location>0&&size_t(location)<=locations.size(),"Unknown original CUS native uniform location");auto& l=locations[location-1];
		const auto active=Active();
		Check(active&&active==l.program&&!active->deleted&&l.description.integer==integer,"CUS uniform belongs to another active program or scalar type");
		Check(OriginalNativeSourceProgramBinding().IsCUS(active),"CUS uniform does not belong to the actual current source program");
		std::vector<float> v;for(auto value:values){float n=value.as<float>();Check(std::isfinite(n)&&(!integer||n==std::trunc(n)),"CUS uniform contains an invalid original scalar");v.push_back(n);}
		Check(!v.empty()&&v.size()<=4,"CUS original uniform arity differs");(integer?active->ints:active->floats)[l.description.name]=std::move(v);
	}
};
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;GLenum target=0;bool modelView=false,deleted=false;
	Buffer(lua_State* L,GLenum t,bool frequent):target(t){
		Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER,"CUS original buffer target differs");
		value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Owned()const{Check(!deleted&&!modelView,"CUS operation needs a live owned instance buffer");}
	void Define(int count,sol::optional<sol::object> layout){Owned();value->Define(count,layout);}
	void ModelsVBO(){Check(!deleted&&Available(),"CUS actual model provider unavailable");modelView=true;}
	void Delete(){if(!deleted)value->Delete();deleted=true;modelView=false;}
	size_t Units(int id,int attr,sol::optional<int> offset){Owned();return value->InstanceDataFromUnitIDs(id,attr,offset);}
	size_t UnitList(const sol::stack_table& ids,int attr,sol::optional<int> offset){Owned();return value->InstanceDataFromUnitIDs(ids,attr,offset);}
	size_t Features(int id,int attr,sol::optional<int> offset){Owned();return value->InstanceDataFromFeatureIDs(id,attr,offset);}
	size_t FeatureList(const sol::stack_table& ids,int attr,sol::optional<int> offset){Owned();return value->InstanceDataFromFeatureIDs(ids,attr,offset);}
	size_t UnitDefs(int id,int attr,sol::optional<int> team,sol::optional<int> offset){Owned();return value->InstanceDataFromUnitDefIDs(id,attr,team,offset);}
	size_t UnitDefList(const sol::stack_table& ids,int attr,sol::optional<int> team,sol::optional<int> offset){Owned();return value->InstanceDataFromUnitDefIDs(ids,attr,team,offset);}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	std::vector<NativeCUSSubmittedObject> objects;
	std::vector<std::shared_ptr<const NativeCUSModelSubmission>> submissions;
	std::vector<std::string> uniformBins;
	bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->modelView&&b->target==GL_ARRAY_BUFFER,"CUS original model vertex attachment differs");vertices=std::move(b);}
	void AttachIndexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->modelView&&b->target==GL_ELEMENT_ARRAY_BUFFER,"CUS original model index attachment differs");indices=std::move(b);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&!b->modelView&&b->target==GL_ARRAY_BUFFER,"CUS original instance attachment differs");instances=std::move(b);}
	void AddOne(int id,NativeCUSObjectKind kind){
		Check(!deleted&&Available(),"CUS original submission has no live provider");NativeCUSSubmittedObject o{kind,id};
		auto model=services.captureSubmission(o);Check(model&&model->object.kind==o.kind&&model->object.id==o.id,"CUS original AddToSubmission model owner differs");
		// Original VAO commands outlive live-object lookup until the producer
		// removes them. Retain bin identity at the same AddToSubmission boundary:
		// shadow drawing can precede DrawWorldPreUnit's destroyed-object cleanup.
		Check(context->definitionAndBin.valid(),"CUS original bin/definition resolver unavailable");
		auto metadata=context->definitionAndBin(unsigned(o.kind),o.id);
		if(!metadata.valid()){sol::error error=metadata;throw std::runtime_error(error.what());}
		auto bin=metadata.get<std::string>();
		submissions.push_back(std::move(model));
		try{uniformBins.push_back(std::move(bin));}catch(...){submissions.pop_back();throw;}
		try{objects.push_back(o);}catch(...){uniformBins.pop_back();submissions.pop_back();throw;}
	}
	void Add(sol::object ids,NativeCUSObjectKind kind){if(ids.is<int>()){AddOne(ids.as<int>(),kind);return;}auto values=ids.as<sol::table>();for(size_t i=1;i<=values.size();++i)AddOne(values.get<int>(i),kind);}
	void AddUnits(sol::object ids){Add(ids,NativeCUSObjectKind::Unit);}
	void AddFeatures(sol::object ids){Add(ids,NativeCUSObjectKind::Feature);}
	void AddUnitDefs(sol::object ids){Add(ids,NativeCUSObjectKind::UnitDefinition);}
	void ClearSubmission(){Check(!deleted,"CUS source VAO deleted");objects.clear();submissions.clear();uniformBins.clear();}
	void RemoveFromSubmission(int index){Check(!deleted&&index>=0&&size_t(index)<objects.size(),"CUS source removal index differs");uniformBins[index].swap(uniformBins.back());uniformBins.pop_back();objects[index]=objects.back();objects.pop_back();submissions[index]=submissions.back();submissions.pop_back();}
	void Delete(){deleted=true;objects.clear();submissions.clear();uniformBins.clear();vertices.reset();indices.reset();instances.reset();}
	void Submit(){
		const auto actualProgram=context->Active();
		Check(Available()&&frame&&!deleted&&vertices&&indices&&instances&&!objects.empty()&&actualProgram&&!actualProgram->deleted,"CUS source draw lacks an actual active frame/program/model submission");
		Check(OriginalNativeSourceProgramBinding().IsCUS(actualProgram),"CUS draw does not own the actual current source program");
		Check(uniformBins.size()==objects.size()&&submissions.size()==objects.size(),"CUS retained submission metadata differs");
		const auto& bin=uniformBins.front();
		auto draw=services.inheritedRaster();draw.program=actualProgram->consumer;
		draw.floatUniforms=actualProgram->floats;draw.intUniforms=actualProgram->ints;
		draw.instances=instances->value->CaptureNativeSnapshot();draw.objects=objects;draw.submissions=submissions;
		draw.textures=std::make_shared<NativeCUSDrawTextureSnapshot>(TextureHistory().Submit(frame->sourceDrawFrame,draw.program->originalPass,draw.program->family,bin,objects));
		// The texture history ordinal and cross-family draw ordinal have
		// different meanings. Preserve both instead of sorting by bind events.
		draw.sequence=NextNativeWorldDrawSequence();
		draw.modelInputs=services.captureObjectInputs(draw);
		draw.engineUniforms=services.captureEngineUniforms();ValidateNativeCUSSourceDraw(draw);
		frame->draws.push_back(std::move(draw));
	}
};
std::map<std::string,std::vector<float>> UniformTable(sol::object input){
	std::map<std::string,std::vector<float>> out;if(!input.valid()||input.get_type()==sol::type::lua_nil)return out;
	for(auto item:input.as<sol::table>()){auto name=item.first.as<std::string>();std::vector<float> v;
		if(item.second.is<float>())v.push_back(item.second.as<float>());else{auto a=item.second.as<sol::table>();for(size_t i=1;i<=a.size();++i)v.push_back(a.get<float>(i));}
		for(float f:v)Check(std::isfinite(f),"CUS shader initial uniform is not finite");out.emplace(std::move(name),std::move(v));
	}return out;
}
sol::table CreateContext(lua_State* L){
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"CUS source requires its complete native consumer and unsynced original owners");
	sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();c->lua=L;
	// These original CPU setters also run in non-draw unit lifecycle call-ins.
	sol::stack::push(L,t);LuaShaders::PushModelBufferUniformEntries(L);lua_pop(L,1);
	BindFunction(t,"Ready",[](){return Available();});
	BindFunction(t,"SetBinResolver",[c](sol::protected_function f){Check(f.valid(),"CUS source bin resolver missing");c->definitionAndBin=std::move(f);});
	BindFunction(t,"GetVBO",[L](sol::optional<GLenum> target,sol::optional<bool> frequent){return std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));});
	BindFunction(t,"GetVAO",[c](){auto v=std::make_shared<VertexArray>();v->context=c;return v;});
	BindFunction(t,"CreateShader",[c](const sol::table& params){
		NativeCUSProgramSource source;source.definitions=params.get_or("definitions",std::string{});source.vertex=params.get<std::string>("vertex");source.fragment=params.get<std::string>("fragment");
		for(const char* unsupported:{"geometry","tcs","tes","compute"})Check(!params[unsupported].valid(),"Unexpected CUS shader stage requires its original consumer");
		source.initialFloat=UniformTable(params["uniformFloat"]);source.initialInt=UniformTable(params["uniformInt"]);
		source.vertexSHA512=Hash(source.definitions+source.vertex);source.fragmentSHA512=Hash(source.definitions+source.fragment);
		const auto identity=ValidateNativeCUSProgramSource(source);
		auto actual=services.compile(source);Check(actual&&actual->pipelineLifetime&&actual->family==identity.family&&actual->originalPass==identity.pass&&actual->vertexSHA512==source.vertexSHA512&&actual->fragmentSHA512==source.fragmentSHA512,"CUS exact original program has no matching compiled native pipeline");
		auto p=std::make_shared<Program>();p->sourceContext=c;p->consumer=std::move(actual);
		p->floats=p->consumer->defaultFloat;p->ints=p->consumer->defaultInt;
		for(auto& [name,value]:source.initialFloat)p->floats.insert_or_assign(name,std::move(value));
		for(auto& [name,value]:source.initialInt)p->ints.insert_or_assign(name,std::move(value));
		c->programs.push_back(p);return unsigned(c->programs.size());
	});
	BindFunction(t,"GetShaderLog",[](){return std::string{};});
	BindFunction(t,"GetActiveUniforms",[L,c](unsigned handle){sol::state_view lua(L);auto out=lua.create_table();unsigned index=1;for(const auto& u:c->Get(handle)->consumer->uniforms){auto v=lua.create_table();v["name"]=u.name;v["type"]=u.glType;v["size"]=u.size;out[index++]=v;}return out;});
	BindFunction(t,"GetUniformLocation",[L,c](unsigned handle,const std::string& name){unsigned location=c->Location(handle,name);return sol::make_object(L,location?int(location):-1);});
	BindFunction(t,"UseShader",[c](unsigned handle){
		auto next=handle?c->Get(handle):nullptr;
		if(next)OriginalNativeSourceProgramBinding().BindCUS(next);else OriginalNativeSourceProgramBinding().Unbind();
	});
	BindFunction(t,"DeleteShader",[c](unsigned handle){auto p=c->Get(handle);
		if(OriginalNativeSourceProgramBinding().IsCUS(p))OriginalNativeSourceProgramBinding().Unbind();
		p->deleted=true;
	});
	BindFunction(t,"Uniform",[c](int location,sol::variadic_args values){c->UniformValue(location,values,false);});
	BindFunction(t,"UniformInt",[c](int location,sol::variadic_args values){c->UniformValue(location,values,true);});
	BindFunction(t,"Texture",[L](sol::object first,sol::optional<sol::object> second){
		unsigned unit=0;auto value=first;if(second){auto n=first.as<float>();Check(std::isfinite(n)&&n==std::trunc(n)&&n>=0&&n<NativeCUSTextureHistory::Units,"CUS texture unit differs");unit=unsigned(n);value=*second;}
		const auto inherited=TextureHistory().ActiveUnit();Check(bool(inherited),"CUS original texture event has no observed active unit");
		const unsigned actual=unit?unit:*inherited;
		const bool result=value.is<bool>()?TextureHistory().LuaTexture(unit,value.as<bool>()):sharedRegistry->LuaTexture(unit,[&]{return services.parseTexture(L,value.as<std::string>());});
		if(services.syncObservedBinding)services.syncObservedBinding(actual);
		return result;
	});
	BindFunction(t,"DeleteTexture",[L](const std::string& name){return services.deleteTexture(L,name);});
	BindFunction(t,"ColorMask",[](bool r,bool g,bool b,bool a){auto d=services.inheritedRaster();d.colorMask={r,g,b,a};services.applyRaster(d);});
	BindFunction(t,"DepthMask",[](bool value){auto d=services.inheritedRaster();d.depthWrite=value;services.applyRaster(d);});
	BindFunction(t,"DepthTest",[](sol::object value){auto d=services.inheritedRaster();if(value.is<bool>())d.depthTest=value.as<bool>();else{d.depthFunction=value.as<unsigned>();d.depthTest=true;}services.applyRaster(d);});
	BindFunction(t,"Culling",[](sol::object value){auto d=services.inheritedRaster();if(value.is<bool>())d.cullEnabled=value.as<bool>();else{d.cullFace=value.as<unsigned>();d.cullEnabled=true;}services.applyRaster(d);});
	BindFunction(t,"Blending",[](unsigned source,unsigned destination){auto d=services.inheritedRaster();SetNativeCUSBlend(d,source,destination);services.applyRaster(d);});
	BindFunction(t,"ClipDistance",[](unsigned plane,bool enabled){Check(plane==0,"CUS uses source clip distance zero only");auto d=services.inheritedRaster();d.clipDistance0=enabled;services.applyRaster(d);});
	return t;
}
class SourceFile final:public CFileHandler {
public:
 NativeCUSSourceLocation location=NativeCUSSourceLocation::Unobserved;
 int section=-1;
 std::string archive,resolvedPath;
 SourceFile(const std::string& path,const std::string& modes){Open(path,modes);}
protected:
 bool TryReadFromRawFS(const std::string& path)override{
  if(!CFileHandler::TryReadFromRawFS(path))return false;
  location=NativeCUSSourceLocation::Raw;resolvedPath=GetFileAbsolutePath(path,SPRING_VFS_RAW);return true;
 }
 bool TryReadFromPWD(const std::string& path)override{
  if(!CFileHandler::TryReadFromPWD(path))return false;
  location=NativeCUSSourceLocation::WorkingDirectory;resolvedPath=GetFileAbsolutePath(path,SPRING_VFS_PWD);return true;
 }
 bool TryReadFromVFS(const std::string& path,int selectedSection)override{
  if(!CFileHandler::TryReadFromVFS(path,selectedSection))return false;
  location=NativeCUSSourceLocation::Archive;section=selectedSection;
  archive=vfsHandler->GetFileArchiveName(path,static_cast<CVFSHandler::Section>(section));
  resolvedPath=vfsHandler->GetFileAbsolutePath(path,static_cast<CVFSHandler::Section>(section));return true;
 }
};
std::string LoadSource(lua_State* L,const std::string& path,sol::optional<std::string> requestedModes){
	Check(!CLuaHandle::GetHandleSynced(L),"CUS source is unsynced only");
	const auto modes=requestedModes.value_or(SPRING_VFS_RAW_FIRST);
	SourceFile file(path,modes);std::string bytes;Check(file.FileExists()&&file.LoadStringData(bytes),"CUS original selected VFS source missing");
	const auto hash=Hash(bytes);Check(IsNativeCUSSelectedSource(path,hash),"CUS original selected VFS/include source identity differs");
	Check(file.location!=NativeCUSSourceLocation::Unobserved,"CUS actual source owner was not observed");
 NativeCUSLoadedSource loaded;loaded.path=path;loaded.modes=modes;loaded.sha512=hash;
 loaded.location=file.location;loaded.archiveSection=file.section;loaded.archive=file.archive;loaded.resolvedPath=file.resolvedPath;
 loadedSources.push_back(std::move(loaded));return bytes;
}
}
void Configure(Services value){Check(!frame,"Cannot change CUS source providers inside a frame");services=std::move(value);}
void SetTextureContext(NativeCUSTextureHistory* history,NativeTextureRegistry* registry){
 Check(!frame,"Cannot change shared texture context inside CUS frame");
 Check(bool(history)==bool(registry)&&(!registry||registry->MatchesHistory(*history)),"CUS shared texture context owners differ");
 sharedHistory=history;sharedRegistry=registry;
}
bool Available(){return sharedHistory&&sharedRegistry&&sharedRegistry->ContextGeneration()&&services.ready&&services.ready()&&services.compile&&services.parseTexture&&services.deleteTexture&&services.inheritedRaster&&services.applyRaster&&services.captureSubmission&&services.captureObjectInputs&&services.captureEngineUniforms;}
NativeCUSTextureHistory& TextureHistory(){Check(sharedHistory&&sharedRegistry,"CUS has no actual shared texture context");return *sharedHistory;}
void NewContext(){Check(!frame,"CUS context reset during a source frame");Check(sharedRegistry!=nullptr,"CUS cannot invent a texture context");sharedRegistry->NewContext();}
void BeginFrame(uint64_t sourceDrawFrame){Check(!frame,"Nested CUS source frame");frame=std::make_shared<NativeCUSSourceFrame>();frame->sourceDrawFrame=sourceDrawFrame;}
std::shared_ptr<const NativeCUSSourceFrame> EndFrame(){Check(bool(frame),"CUS source frame is not active");frame->sourceLoads=loadedSources;return std::exchange(frame,{});}
void AbortFrame()noexcept{frame.reset();}
void Reset(){AbortFrame();loadedSources.clear();}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto f=lua.create_table();
	f.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Delete",&Buffer::Delete,"ModelsVBO",&Buffer::ModelsVBO,"InstanceDataFromUnitIDs",sol::overload(&Buffer::Units,&Buffer::UnitList),"InstanceDataFromFeatureIDs",sol::overload(&Buffer::Features,&Buffer::FeatureList),"InstanceDataFromUnitDefIDs",sol::overload(&Buffer::UnitDefs,&Buffer::UnitDefList));
	f.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"AddUnitsToSubmission",&VertexArray::AddUnits,"AddFeaturesToSubmission",&VertexArray::AddFeatures,"AddUnitDefsToSubmission",&VertexArray::AddUnitDefs,"ClearSubmission",&VertexArray::ClearSubmission,"RemoveFromSubmission",&VertexArray::RemoveFromSubmission,"Submit",&VertexArray::Submit,"Delete",&VertexArray::Delete);
	f.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil);BindFunction(f,"Available",Available);BindFunction(f,"CreateContext",[L](){return CreateContext(L);});
	BindFunction(f,"LoadSource",[L](const std::string& p,sol::optional<std::string> modes){return LoadSource(L,p,modes);});
	BindFunction(f,"PrepareSource",[](std::string source){
		Check(IsNativeCUSSelectedSource("LuaRules/Gadgets/cus_gl4.lua",Hash(source)),"CUS original source identity differs before adaptation");
		const std::string from="if Platform.glHaveGL4 ~= true then",to="if not gl.NativeCUSSourceReady() then";
		const auto pos=source.find(from);Check(pos!=std::string::npos&&source.find(from,pos+from.size())==std::string::npos,"CUS original capability site differs");source.replace(pos,from.size(),to);LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
	});
	gl.set("NativeCUSSource",f);return true;
}
} // namespace LuaNativeCUSSource
#else
namespace LuaNativeCUSSource {
void Configure(Services){}void SetTextureContext(rendering::NativeCUSTextureHistory*,rendering::NativeTextureRegistry*){}bool Available(){return false;}
rendering::NativeCUSTextureHistory& TextureHistory(){static rendering::NativeCUSTextureHistory history;return history;}
void NewContext(){}void BeginFrame(uint64_t){}std::shared_ptr<const rendering::NativeCUSSourceFrame> EndFrame(){return {};}
void AbortFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return false;}
}
#endif
