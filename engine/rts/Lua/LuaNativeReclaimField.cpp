/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeReclaimField.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeReclaimFieldData.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include <set>
namespace LuaNativeReclaimField {
namespace {
using namespace rendering;
void Check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<typename Signature>struct Boundary;
template<typename R,typename...Args>struct Boundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
uint64_t Expected(const std::string& path){
	if(path=="LuaUI/Widgets/gui_reclaim_field_highlight.lua")return reclaimfield::WidgetHash;
	if(path=="LuaUI/Include/reclaim_field_renderer_gl4.lua")return reclaimfield::HelperHash;
	if(path=="LuaUI/Shaders/reclaim_field_gl4.vert.glsl")return reclaimfield::VertexHash;
	if(path=="LuaUI/Shaders/reclaim_field_gl4.frag.glsl")return reclaimfield::FragmentHash;
	throw std::runtime_error("Unknown original Reclaim field source");
}
std::string Archive(const std::string& path){CFileHandler f(path,SPRING_VFS_ZIP);std::string s;Check(f.FileExists()&&f.LoadStringData(s)&&NativeReclaimFieldSourceHash(s)==Expected(path),"Original31657 Reclaim field source identity differs");return s;}
struct Buffer;struct Renderer;
struct Context {std::shared_ptr<Buffer> storage;std::shared_ptr<Renderer> active;};
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;bool deleted=false;
	Buffer(lua_State* L,GLenum target,bool frequent,std::shared_ptr<Context> c):context(c){
		Check(target==GL_ARRAY_BUFFER||target==GL_SHADER_STORAGE_BUFFER,"Reclaim field original buffer target differs");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Delete(){if(auto c=context.lock();c&&c->storage.get()==this)c->storage.reset();value->Delete();deleted=true;}
};
struct Renderer {
	std::weak_ptr<Context> context;NativeReclaimFieldUniforms uniforms;bool deleted=false;
	uint64_t vertexHash=0,fragmentHash=0;
	void Floats(const std::string& name,std::span<const float> v){
		Check(!deleted,"Reclaim field renderer was finalized");
		const auto copy=[&](auto& a){Check(v.size()==a.size(),"Reclaim field uniform arity differs");std::copy(v.begin(),v.end(),a.begin());};
		if(name=="toggles")copy(uniforms.toggles);else if(name=="fadeDist")copy(uniforms.fadeDist);
		else if(name=="fillColorMetal")copy(uniforms.fillMetal);else if(name=="fillColorEnergy")copy(uniforms.fillEnergy);
		else if(name=="edgeColorMetal")copy(uniforms.edgeMetal);else if(name=="edgeColorEnergy")copy(uniforms.edgeEnergy);
		else {Check(v.size()==1,"Reclaim field scalar uniform arity differs");if(name=="yOffset")uniforms.yOffset=v[0];else if(name=="gradientAlpha")uniforms.gradientAlpha=v[0];else if(name=="energyOpacity")uniforms.energyOpacity=v[0];else throw std::runtime_error("Unknown original Reclaim field uniform");}
	}
	void Integer(const std::string& name,int value){Check(!deleted&&name=="vertsPerChunk"&&(value==288||value==64),"Reclaim field integer uniform differs");uniforms.vertsPerChunk=value;}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices;bool deleted=false;
	void Draw(GLenum mode,int count,int first){
		Check(IsNativeReclaimFieldConsumerAvailable()&&!deleted&&vertices&&!vertices->deleted&&context->storage&&!context->storage->deleted&&context->active&&!context->active->deleted,"Reclaim field actual source owner/consumer/buffers missing");
		Check((mode==GL_TRIANGLES||mode==GL_LINES)&&count>=0&&first==0,"Reclaim field original DrawArrays arguments differ");if(!count)return;
		auto& r=ui::GetRecorder();Check(r.CurrentWorldStage()==NativeWorldDrawStage::PreUnit&&!r.IsCompiling()&&!r.HasActiveTarget(),"Reclaim field draw is outside its original main DrawWorldPreUnit");
		auto d=std::make_shared<NativeReclaimFieldInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->vertices=vertices->value->CaptureNativeSnapshot();
		auto storage=context->storage->value;const auto n=storage->CaptureNativeSnapshot()->layout.elements;d->params=storage->CaptureNativeRange(9,0,n,16,16);
		d->original=OriginalCUSUniformCapture().Current();d->uniforms=context->active->uniforms;d->vertexHash=context->active->vertexHash;d->fragmentHash=context->active->fragmentHash;
		d->edges=mode==GL_LINES;d->first=first;d->count=count;
		GetWorldDrawRecorder().RecordReclaimField(std::move(d),r.CurrentState().draw,r.CurrentWorldState());
	}
};
sol::table CreateContext(lua_State* L){
	Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeReclaimFieldConsumerAvailable(),"Reclaim field requires the unsynced real paired consumers");
	sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();
	Bind(t,"NewBuffer",[L,c](GLenum target,bool frequent){return std::make_shared<Buffer>(L,target,frequent,c);});
	Bind(t,"NewVertexArray",[c](){auto a=std::make_shared<VertexArray>();a->context=c;return a;});
	Bind(t,"CreateRenderer",[c](sol::table cfg){
		Check(IsNativeReclaimFieldConsumerAvailable(),"Reclaim field compiled consumers were retired");
		Check(cfg["vssrcpath"].get<std::string>()=="LuaUI/Shaders/reclaim_field_gl4.vert.glsl"&&cfg["fssrcpath"].get<std::string>()=="LuaUI/Shaders/reclaim_field_gl4.frag.glsl","Reclaim field renderer source path differs");
		const sol::table defines=cfg["shaderConfig"];Check(defines.size()==0,"Reclaim field indexed compile options differ");unsigned keys=0;for(const auto& kv:defines){Check(kv.first.as<std::string>()=="SSBO_BINDING"&&kv.second.as<int>()==9,"Reclaim field compile definition differs");++keys;}Check(keys==1,"Reclaim field requires its original SSBO9 definition");
		auto p=std::make_shared<Renderer>();p->context=c;p->vertexHash=NativeReclaimFieldSourceHash(Archive("LuaUI/Shaders/reclaim_field_gl4.vert.glsl"));p->fragmentHash=NativeReclaimFieldSourceHash(Archive("LuaUI/Shaders/reclaim_field_gl4.frag.glsl"));
		const sol::table ints=cfg["uniformInt"];unsigned integers=0;for(const auto& kv:ints){p->Integer(kv.first.as<std::string>(),kv.second.as<int>());++integers;}Check(integers==1,"Reclaim field original integer defaults missing");
		const sol::table floats=cfg["uniformFloat"];std::set<std::string> names;
		for(const auto& kv:floats){std::vector<float> v;if(kv.second.is<sol::table>()){sol::table a=kv.second.as<sol::table>();for(size_t i=1;i<=a.size();++i)v.push_back(a[i].get<float>());}else v.push_back(kv.second.as<float>());const auto name=kv.first.as<std::string>();Check(names.insert(name).second,"Reclaim field uniform default repeated");p->Floats(name,v);}
		Check(names.size()==9,"Reclaim field original float defaults missing");return p;
	});return t;
}
}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int n,sol::optional<sol::object> layout){Check(!b.deleted,"Reclaim field buffer deleted");b.value->Define(n,layout);});
	Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!b.deleted,"Reclaim field buffer deleted");return b.value->Upload(data,attr,offset,first,last);});
	Bind(b,"CopyTo",[](Buffer& b,std::shared_ptr<Buffer> dest,int bytes){Check(!b.deleted&&dest&&!dest->deleted,"Reclaim field copy owner deleted");return b.value->CopyNativeSnapshotTo(dest->value,bytes);});
	Bind(b,"Delete",[](Buffer& b){b.Delete();});
	Bind(b,"BindBufferRange",[](std::shared_ptr<Buffer> b,int binding){auto c=b->context.lock();Check(c&&!b->deleted&&binding==9&&!c->storage,"Reclaim field actual SSBO9 binding differs");Check(b->value->CaptureNativeSnapshot()->layout.kind==NativeBufferKind::Storage,"Reclaim field bound a non-storage buffer");c->storage=b;});
	Bind(b,"UnbindBufferRange",[](Buffer& b,int binding){auto c=b.context.lock();Check(c&&binding==9&&c->storage.get()==&b,"Reclaim field SSBO9 unbind differs");c->storage.reset();});
	auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted,"Reclaim field actual posKind buffer missing");a.vertices=std::move(b);});
	Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();});
	Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,int n,int first){a.Draw(mode,n,first);});
	auto p=family.new_usertype<Renderer>("Renderer",sol::no_constructor);
	Bind(p,"Activate",[](std::shared_ptr<Renderer> p){auto c=p->context.lock();Check(c&&!p->deleted&&!c->active,"Reclaim field renderer active scope differs");c->active=p;});
	Bind(p,"Deactivate",[](Renderer& p){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Reclaim field renderer deactivate differs");c->active.reset();});
	Bind(p,"Finalize",[](Renderer& p){if(auto c=p.context.lock();c&&c->active.get()==&p)c->active.reset();p.deleted=true;});
	Bind(p,"SetUniform",[](Renderer& p,const std::string& name,sol::variadic_args args){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Reclaim field actual uniform write lacks an active renderer");std::vector<float> v;for(auto x:args)v.push_back(x.as<float>());p.Floats(name,v);});
	Bind(p,"SetUniformInt",[](Renderer& p,const std::string& name,int value){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Reclaim field integer write lacks an active renderer");p.Integer(name,value);});
	family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Renderer",sol::lua_nil);
	Bind(family,"Available",&IsNativeReclaimFieldConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Reclaim field source belongs to the unsynced renderer");return Archive(path);});
	Bind(family,"PrepareSource",[](std::string s){const auto h=NativeReclaimFieldSourceHash(s);Check(h==reclaimfield::WidgetHash||h==reclaimfield::HelperHash,"Original31657 Reclaim Lua source identity differs");LuaUtils::TracyRemoveAlsoExtras(s.data());return s;});
	gl.set("NativeReclaimField",family);return true;
}
}
#else
namespace LuaNativeReclaimField {bool PushEntries(lua_State*){return true;}}
#endif
