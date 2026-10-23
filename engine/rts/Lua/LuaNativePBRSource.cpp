/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativePBRSource.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativePBRSourceFiles.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/VFSHandler.h"
#include "System/Sync/SHA512.hpp"
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <utility>

namespace LuaNativePBRSource {
namespace {
using namespace rendering;
Services services;
NativeTextureRegistry* registry=nullptr;
NativeCUSTextureHistory* history=nullptr;
uint64_t nextTexture=0,nextVersion=0;
std::vector<NativePBRLoadedSource> loadedSources;
void Check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception&e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename F>void Bind(sol::table&t,const char*n,F f){t.set_function(n,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
void Call(sol::protected_function f){auto result=f();if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}}
struct Texture {uint64_t name=0,context=0;NativePBRAllocation allocation;bool fbo=false;};
struct Context {
	lua_State* lua=nullptr;
	std::map<std::string,Texture> textures;
	std::vector<std::shared_ptr<const NativePBRProgram>> programs;
	std::vector<std::string> fbos;
	std::shared_ptr<const NativePBRProgram> active;
	std::string target;
	std::shared_ptr<const NativePBRProgram> Program(unsigned n){Check(n&&n<=programs.size()&&programs[n-1],"Original PBR program is not live");return programs[n-1];}
	Texture& Image(const std::string& n){auto i=textures.find(n);Check(i!=textures.end()&&registry&&i->second.context==registry->ContextGeneration()&&registry->Find(i->second.name),"Original PBR texture is not live");return i->second;}
	void Delete(const std::string& n){auto i=textures.find(n);if(i==textures.end())return;if(registry&&i->second.context==registry->ContextGeneration())registry->Delete(i->second.name);textures.erase(i);}
};
std::unordered_map<lua_State*,std::vector<std::shared_ptr<Context>>> contexts;
class SourceFile final:public CFileHandler {
public:
	NativePBRLoadedSource observed;
	SourceFile(const std::string& path,const std::string& modes){observed.path=path;observed.modes=modes;Open(path,modes);}
protected:
	bool TryReadFromRawFS(const std::string& path)override{if(!CFileHandler::TryReadFromRawFS(path))return false;observed.owner="raw";observed.resolvedPath=GetFileAbsolutePath(path,SPRING_VFS_RAW);return true;}
	bool TryReadFromPWD(const std::string& path)override{if(!CFileHandler::TryReadFromPWD(path))return false;observed.owner="pwd";observed.resolvedPath=GetFileAbsolutePath(path,SPRING_VFS_PWD);return true;}
	bool TryReadFromVFS(const std::string& path,int section)override{if(!CFileHandler::TryReadFromVFS(path,section))return false;observed.owner="archive";observed.archiveSection=section;observed.archive=vfsHandler->GetFileArchiveName(path,static_cast<CVFSHandler::Section>(section));observed.resolvedPath=vfsHandler->GetFileAbsolutePath(path,static_cast<CVFSHandler::Section>(section));return true;}
};
std::string LoadSource(lua_State* L,const std::string& path,sol::optional<std::string> modes){
	Check(!CLuaHandle::GetHandleSynced(L),"PBR original source is unsynced only");SourceFile file(path,modes.value_or(SPRING_VFS_RAW_FIRST));std::string bytes;Check(file.FileExists()&&file.LoadStringData(bytes),"Original PBR source missing");
	sha512::raw_digest digest;sha512::calc_digest(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),digest.data());file.observed.sha512=sha512::dump_digest(digest);bool selected=false;
	// VFS names are case-insensitive; retain the actual observed spelling above.
	std::string selectedPath=path;for(auto& c:selectedPath)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	for(const auto& original:pbrsource::Files)if(original.path==selectedPath&&original.sha512==file.observed.sha512)selected=true;
	Check(selected&&!file.observed.owner.empty(),"Actual PBR source/VFS selection differs from selected31657");loadedSources.push_back(file.observed);return bytes;
}
std::map<std::string,std::vector<float>> Uniforms(sol::object value){std::map<std::string,std::vector<float>> out;if(!value.valid())return out;for(auto item:value.as<sol::table>()){std::vector<float> numbers;if(item.second.is<float>())numbers.push_back(item.second.as<float>());else{auto a=item.second.as<sol::table>();for(size_t i=1;i<=a.size();++i)numbers.push_back(a.get<float>(i));}out.emplace(item.first.as<std::string>(),std::move(numbers));}return out;}
sol::table Create(lua_State* L)
{
	Check(Available()&&!CLuaHandle::GetHandleSynced(L),"Original PBR native owners are not ready or are synced");
	sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();c->lua=L;contexts[L].push_back(c);
	// HEADLESS removes legacy GL from this executable; it does not describe
	// this private producer's actual connected native graphics environment.
	// Only this exact PBR source asks this query. Global config/caps stay real.
	Bind(t,"SourceHeadless",[]{Check(Available(),"Original PBR graphics owner is no longer connected");return 0;});
	Bind(t,"CreateTexture",[c](unsigned width,unsigned height,const sol::table& p){
		NativePBRAllocation a;a.width=width;a.height=height;a.originalFormat=p.get<unsigned>("format");
		Check(!p.get_or("border",false),"Original PBR border differs");
		a.sampling.minFilter=p.get<unsigned>("min_filter");a.sampling.magFilter=p.get<unsigned>("mag_filter");a.sampling.wrapS=p.get<unsigned>("wrap_s");a.sampling.wrapT=p.get<unsigned>("wrap_t");ValidateNativePBRAllocation(a);
		const auto unit=history->ActiveUnit();Check(bool(unit),"Original PBR allocation active unit unobserved");const auto previous=history->Binding(*unit,NativeCUSTextureTarget::Image2D);
		Check(previous.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original PBR allocation prior binding unobserved");
		auto image=services.allocate(a);Check(image&&image->content==NativeTextureContent::AllocationOnly&&image->allocation.width==a.width&&image->allocation.height==a.height&&image->allocation.originalFormat==a.originalFormat,"PBR allocation lacks the real original image");
		auto object=registry->Create(NativeCUSTextureTarget::Image2D,"original api_pbr_enabler Lua texture");
		const auto restore=[&]{registry->Bind(*unit,previous.object?previous.object->name:0,NativeCUSTextureTarget::Image2D);if(services.syncObservedBinding)services.syncObservedBinding(*unit);};
		try{registry->Bind(*unit,object->name,object->target);registry->Allocate(object->name,{width,height,1,1,1,a.originalFormat});registry->SetSampling(object->name,a.sampling);registry->PublishContent(object->name,{},image);restore();}
		catch(...){registry->Delete(object->name);restore();throw;}
		const auto name="!NativePBR"+std::to_string(++nextTexture);c->textures.emplace(name,Texture{object->name,registry->ContextGeneration(),a,p.get_or("fbo",false)});return name;
	});
	Bind(t,"CreateFBO",[c](const sol::table& p){auto name=p.get<std::string>("color0");c->Image(name);auto d=p.get<sol::table>("drawbuffers");Check(d.size()==1&&d.get<unsigned>(1)==0x8CE0,"Original BRDF draw buffer differs");c->fbos.push_back(name);return unsigned(c->fbos.size());});
	Bind(t,"IsValidFBO",[c](unsigned n){if(!n||n>c->fbos.size()||c->fbos[n-1].empty())return false;c->Image(c->fbos[n-1]);return true;});
	Bind(t,"DeleteFBO",[c](unsigned n){Check(n&&n<=c->fbos.size(),"Unknown original PBR FBO");c->fbos[n-1].clear();});
	Bind(t,"DeleteTexture",[c](const std::string& n){const bool exists=c->textures.contains(n);c->Delete(n);return exists;});
	// Original LuaTextures::FreeFBO deletes attachments, NOT the texture. Its
	// image remains until Lua-state resource cleanup or a real DeleteTexture.
	Bind(t,"DeleteTextureFBO",[c](const std::string& n){auto& image=c->Image(n);image.fbo=false;return true;});
	Bind(t,"CreateShader",[c](const sol::table& p){NativePBRProgramSource source{p.get<std::string>("vertex"),p.get<std::string>("fragment"),Uniforms(p["uniformInt"])};const auto identity=ValidateNativePBRProgram(source);auto compiled=services.compile(source);Check(compiled&&compiled->pipelineOwner&&compiled->kind==identity.kind&&compiled->samples==identity.samples,"Original PBR source has no actual compiled consumer");c->programs.push_back(compiled);return unsigned(c->programs.size());});
	Bind(t,"GetShaderLog",[](){return std::string{};});
	Bind(t,"DeleteShader",[c](unsigned n){auto p=c->Program(n);if(c->active==p)c->active.reset();c->programs[n-1].reset();});
	Bind(t,"UseShader",[c](unsigned n){c->active=n?c->Program(n):nullptr;});
	Bind(t,"ActiveShader",[c](unsigned n,sol::protected_function f){auto previous=std::exchange(c->active,c->Program(n));try{Call(f);}catch(...){c->active=std::move(previous);throw;}c->active=std::move(previous);});
	Bind(t,"ActiveFBO",[c](unsigned n,sol::protected_function f){Check(n&&n<=c->fbos.size()&&!c->fbos[n-1].empty(),"Original PBR active FBO unavailable");c->Image(c->fbos[n-1]);auto previous=std::exchange(c->target,c->fbos[n-1]);try{const auto& a=c->Image(c->target).allocation;services.targetRaster(a.width,a.height,false,[&]{Call(f);});}catch(...){c->target=std::move(previous);throw;}c->target=std::move(previous);});
	Bind(t,"RenderToTexture",[c](const std::string& n,sol::protected_function f){Check(c->Image(n).fbo,"Original EnvLUT FBO unavailable");auto previous=std::exchange(c->target,n);try{const auto& a=c->Image(c->target).allocation;services.targetRaster(a.width,a.height,true,[&]{Call(f);});}catch(...){c->target=std::move(previous);throw;}c->target=std::move(previous);});
	Bind(t,"Texture",[L](unsigned unit,sol::object value){Check(unit<NativeCUSTextureHistory::Units,"PBR texture unit out of range");const auto active=history->ActiveUnit();Check(bool(active),"PBR texture unit unobserved");const unsigned affected=unit?unit:*active;const bool result=value.is<bool>()?history->LuaTexture(unit,value.as<bool>()):registry->LuaTexture(unit,[&]{NativeTextureLookup owned;if(Lookup(L,value.as<std::string>(),owned))return owned;return services.parseTexture(L,value.as<std::string>());});if(services.syncObservedBinding)services.syncObservedBinding(affected);return result;});
	Bind(t,"TexRect",[c](float x1,float y1,float x2,float y2){
		Check(c->active&&x1==-1&&y1==-1&&x2==1&&y2==1&&!c->target.empty(),"Original PBR full-target draw differs");services.validateFullTargetRaster();auto& tex=c->Image(c->target);
		Check((c->active->kind==NativePBRKind::BRDF)==(tex.allocation.originalFormat==0x822F),"PBR original program/target differ");
		auto image=services.allocate(tex.allocation);Check(bool(image),"PBR draw storage unavailable");image->program=c->active;image->sourceDrawFrame=services.sourceDrawFrame();image->sourceSequence=NextNativeWorldDrawSequence();image->contentVersion=++nextVersion;image->content=NativeTextureContent::ProducerVersion;
		if(c->active->kind==NativePBRKind::Environment)image->reflection=history->Binding(7,NativeCUSTextureTarget::Cube);
		// Publish and enqueue transactionally. No-drawable requests retain this
		// real command; a failed queue insertion restores only its content
		// version, preserving intervening source name/binding/sampler events.
		const auto previous=registry->Find(tex.name);
		registry->PublishContent(tex.name,{},image);
		const auto current=registry->Find(tex.name);
		try { services.enqueue(image); }
		catch (...) { registry->CancelContentPublication(current,previous); throw; }
	});
	return t;
}
}
void Configure(Services s,NativeTextureRegistry* r,NativeCUSTextureHistory* h){Check(bool(r)==bool(h)&&(!r||r->MatchesHistory(*h)),"Original PBR texture context differs");Check(contexts.empty(),"Detach original PBR Lua owners before replacing services");services=std::move(s);registry=r;history=h;}
bool Available(){return registry&&history&&registry->ContextGeneration()&&services.ready&&services.ready()&&services.compile&&services.allocate&&services.parseTexture&&services.sourceDrawFrame&&services.enqueue&&services.validateFullTargetRaster&&services.targetRaster;}
bool Lookup(lua_State* L,const std::string& name,NativeTextureLookup& out){auto i=contexts.find(L);if(i==contexts.end())return false;for(const auto& c:i->second){auto t=c->textures.find(name);if(t!=c->textures.end()){out={true,registry&&t->second.context==registry->ContextGeneration()?registry->Find(t->second.name):nullptr,false};return true;}}return false;}
bool DeleteTexture(lua_State* L,const std::string& name){auto i=contexts.find(L);if(i==contexts.end())return false;for(const auto& c:i->second)if(c->textures.contains(name)){c->Delete(name);return true;}return false;}
const std::vector<NativePBRLoadedSource>& LoadedSources(){return loadedSources;}
void ReleaseLuaState(lua_State* L){auto i=contexts.find(L);if(i==contexts.end())return;for(auto& c:i->second){while(!c->textures.empty())c->Delete(c->textures.begin()->first);c->programs.clear();c->active.reset();}contexts.erase(i);}
bool PushEntries(lua_State* L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();Bind(family,"Available",Available);Bind(family,"CreateContext",[L]{return Create(L);});Bind(family,"LoadSource",[L](const std::string& p,sol::optional<std::string> modes){return LoadSource(L,p,modes);});gl["NativePBRSource"]=family;return true;}
} // namespace LuaNativePBRSource
#endif
