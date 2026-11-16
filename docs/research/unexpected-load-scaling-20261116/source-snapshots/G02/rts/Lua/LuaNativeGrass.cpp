/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeGrass.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaNativeCUSTextures.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeGrassData.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <set>
namespace LuaNativeGrass {
namespace {
using namespace rendering;
void Check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<typename Signature>struct Boundary;
template<typename R,typename...Args>struct Boundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
uint64_t Expected(const std::string& name){
	if(name=="LuaUI/Widgets/map_grass_gl4.lua")return grass::WidgetHash;
	if(name=="LuaUI/Shaders/map_grass_gl4.vert.glsl")return grass::VertexHash;
	if(name=="LuaUI/Shaders/map_grass_gl4.frag.glsl")return grass::FragmentHash;
	if(name=="LuaUI/Include/grassPatches.lua")return grass::MeshHash;
	if(name=="modules/graphics/luashader.lua")return grass::HelperHash;
	throw std::runtime_error("Unknown original31657 Grass source");
}
std::string Archive(const std::string& name){CFileHandler f(name,SPRING_VFS_ZIP);std::string s;Check(f.FileExists()&&f.LoadStringData(s)&&NativeGrassSourceHash(s)==Expected(name),"Original31657 Grass source identity differs");return s;}
struct Buffer;struct Renderer;
struct Context {std::shared_ptr<Buffer> bend;std::shared_ptr<Renderer> active;};
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;GLenum target;bool deleted=false;
	Buffer(lua_State* L,GLenum t,bool frequent,std::shared_ptr<Context> c):context(c),target(t){
		Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER||t==GL_SHADER_STORAGE_BUFFER,"Original Grass buffer target differs");
		value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Delete(){if(auto c=context.lock();c&&c->bend.get()==this)c->bend.reset();value->Delete();deleted=true;}
};
struct Renderer {
	std::weak_ptr<Context> context;NativeGrassUniforms uniforms;std::array<float,18> defines{};bool deleted=false;
	void Floats(const std::string& name,std::span<const float> v){
		Check(!deleted,"Original Grass shader has been finalized");
		if(name=="grassuniforms"||name=="nightFactor"){Check(v.size()==4,"Original Grass vec4 uniform arity differs");auto& out=name=="grassuniforms"?uniforms.wind:uniforms.night;std::copy(v.begin(),v.end(),out.begin());}
		else{Check(name=="distanceMult"&&v.size()==1,"Unknown original Grass float uniform");uniforms.distanceMult=v[0];}
	}
	void Integer(const std::string& name,int n){
		Check(!deleted,"Original Grass shader has been finalized");
		if(name=="unitBendCount"){Check(defines[13]==1&&n>=0&&n<=128,"Original Grass unit bend count differs");uniforms.unitBendCount=n;return;}
		const std::array<std::string_view,6> names={"grassBladeColorTex","mapGrassColorModTex","grassWindPerturbTex","shadowTex","losTex","heightmapTex"};
		auto it=std::find(names.begin(),names.end(),name);Check(it!=names.end()&&n==int(it-names.begin()),"Original Grass sampler unit differs");
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,indices,instances;bool deleted=false;
	void Draw(GLenum mode,int count,int offset,int n,int baseVertex,int baseInstance){
		Check(IsNativeGrassConsumerAvailable()&&!deleted&&context->active&&!context->active->deleted&&vertices&&indices&&instances&&!vertices->deleted&&!indices->deleted&&!instances->deleted,"Original Grass draw lacks its real shader/VAO/buffers/consumer");
		Check(mode==GL_TRIANGLES&&count>=0&&offset>=0&&n>=0&&baseVertex==0&&baseInstance>=0,"Original Grass indexed cull run arguments differ");if(!count||!n)return;
		auto& recorder=ui::GetRecorder();Check(recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::PreUnit&&!recorder.IsCompiling()&&!recorder.HasActiveTarget(),"Original Grass draw is outside its real main DrawWorldPreUnit");
		auto d=std::make_shared<NativeGrassInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->vertexHash=grass::VertexHash;d->fragmentHash=grass::FragmentHash;
		d->vertices=vertices->value->CaptureNativeSnapshot();d->indices=indices->value->CaptureNativeSnapshot();d->instances=instances->value->CaptureNativeSnapshot();
		d->defines=context->active->defines;d->uniforms=context->active->uniforms;d->original=OriginalCUSUniformCapture().Current();
		d->count=count;d->indexOffset=offset;d->instanceCount=n;d->baseVertex=baseVertex;d->baseInstance=baseInstance;
		for(unsigned slot=0;slot<6;++slot)d->images[slot]=OriginalNativeTextureContext().History().Binding(slot,NativeCUSTextureTarget::Image2D);
		if(d->defines[13]==1){Check(context->bend&&!context->bend->deleted,"Original Grass shader selected its real unit bend SSBO6 but none is bound");auto b=context->bend->value;d->bend=b->CaptureNativeRange(6,0,b->CaptureNativeSnapshot()->layout.elements,16,16);}
		GetWorldDrawRecorder().RecordGrass(std::move(d),recorder.CurrentState().draw,recorder.CurrentWorldState());
	}
};
sol::table CreateContext(lua_State* L){
	Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeGrassConsumerAvailable(),"Original Grass requires its unsynced paired native consumers");
	sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();
	Bind(t,"NewBuffer",[L,c](GLenum target,bool frequent){return std::make_shared<Buffer>(L,target,frequent,c);});
	Bind(t,"NewVertexArray",[c](){auto a=std::make_shared<VertexArray>();a->context=c;return a;});
	Bind(t,"Texture",[L](unsigned unit,sol::object value){
		Check(unit<6&&!CLuaHandle::GetHandleSynced(L)&&IsNativeGrassConsumerAvailable(),"Original Grass texture unit or native consumer differs");
		auto& recorder=ui::GetRecorder();auto& textures=OriginalNativeTextureContext();
		Check(textures.Initialized()&&textures.UIAttached()&&recorder.TextureObserver()==&textures.UI()&&
			recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::PreUnit&&!recorder.IsCompiling()&&!recorder.HasActiveTarget(),
			"Original Grass Texture requires its actual shared DrawWorldPreUnit texture owner");
		unsigned observed;bool parsed=true;
		if(value.is<bool>())observed=textures.UI().LuaEnable(unit,value.as<bool>());
		else{
			const auto name=value.as<std::string>();
			// Parse after the original active-unit change, then bind exactly once.
			// The registry retains ID0/failed-lookup and shadow compare semantics.
			observed=textures.UI().LuaLookup(unit,[&]{auto lookup=LuaNativeCUSTextures::ParseTexture(L,name);parsed=lookup.parsed;return lookup;});
		}
		recorder.SyncObservedTextureBinding(observed);return parsed;
	});
	Bind(t,"CreateRenderer",[c](sol::table cfg){
		Check(IsNativeGrassConsumerAvailable(),"Original Grass native shader consumers were retired");
		Check(cfg["vssrcpath"].get<std::string>()=="LuaUI/Shaders/map_grass_gl4.vert.glsl"&&cfg["fssrcpath"].get<std::string>()=="LuaUI/Shaders/map_grass_gl4.frag.glsl"&&cfg["shaderName"].get<std::string>()=="GrassShaderGL4","Original Grass actual shader configuration source differs");
		Archive("LuaUI/Shaders/map_grass_gl4.vert.glsl");Archive("LuaUI/Shaders/map_grass_gl4.frag.glsl");Archive("modules/graphics/luashader.lua");
		auto p=std::make_shared<Renderer>();p->context=c;std::bitset<18> seen;sol::table defs=cfg["shaderConfig"];
		for(const auto& kv:defs){const auto name=kv.first.as<std::string>();auto it=std::find(grass::DefineNames.begin(),grass::DefineNames.end(),name);Check(it!=grass::DefineNames.end(),"Original Grass compile definition requires its actual native translation");const size_t i=it-grass::DefineNames.begin();const float v=kv.second.as<float>();Check(!seen[i]&&std::isfinite(v),"Original Grass define is duplicated or nonfinite");seen.set(i);p->defines[i]=v;}
		for(size_t i=0;i<17;++i)Check(seen[i],"Original Grass required compile definition is missing");
		for(size_t i:{9u,10u,12u,13u,17u})Check(std::floor(p->defines[i])==p->defines[i],"Original Grass preprocessor condition requires its original integer literal");
		const sol::table ints=cfg["uniformInt"];std::set<std::string> names;
		for(const auto& kv:ints){const auto name=kv.first.as<std::string>();p->Integer(name,kv.second.as<int>());names.insert(name);}Check(names.size()==size_t(p->defines[13]==1?7:6),"Original Grass sampler/bend defaults are incomplete");
		const sol::table floats=cfg["uniformFloat"];names.clear();for(const auto& kv:floats){std::vector<float> v;if(kv.second.is<sol::table>()){sol::table a=kv.second.as<sol::table>();for(size_t i=1;i<=a.size();++i)v.push_back(a[i].get<float>());}else v.push_back(kv.second.as<float>());const auto name=kv.first.as<std::string>();p->Floats(name,v);names.insert(name);}Check(names.size()==3,"Original Grass float defaults are incomplete");return p;
	});return t;
}
}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int n,sol::optional<sol::object> layout){Check(!b.deleted,"Original Grass buffer deleted");b.value->Define(n,layout);});
	Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!b.deleted,"Original Grass buffer deleted");return b.value->Upload(data,attr,offset,first,last);});
	Bind(b,"Delete",[](Buffer& b){b.Delete();});
	Bind(b,"BindBufferRange",[](std::shared_ptr<Buffer> b,int binding){auto c=b->context.lock();Check(c&&!b->deleted&&b->target==GL_SHADER_STORAGE_BUFFER&&binding==6&&!c->bend,"Original Grass SSBO6 bind owner differs");c->bend=std::move(b);});
	Bind(b,"UnbindBufferRange",[](std::shared_ptr<Buffer> b,int binding){auto c=b->context.lock();Check(c&&binding==6&&c->bend==b,"Original Grass SSBO6 unbind owner differs");c->bend.reset();});
	auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Original Grass vertex attachment differs");a.vertices=std::move(b);});
	Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Original Grass index attachment differs");a.indices=std::move(b);});
	Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Original Grass instance attachment differs");a.instances=std::move(b);});
	Bind(a,"DrawElements",[](VertexArray& a,GLenum mode,int count,int offset,int instances,int baseVertex,int baseInstance){a.Draw(mode,count,offset,instances,baseVertex,baseInstance);});
	Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.indices.reset();a.instances.reset();});
	auto p=family.new_usertype<Renderer>("Renderer",sol::no_constructor);
	Bind(p,"Activate",[](std::shared_ptr<Renderer> p){auto c=p->context.lock();Check(c&&!p->deleted&&!c->active,"Original Grass shader activation differs");c->active=std::move(p);});
	Bind(p,"Deactivate",[](Renderer& p){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Original Grass shader deactivation differs");c->active.reset();});
	Bind(p,"Finalize",[](Renderer& p){if(auto c=p.context.lock();c&&c->active.get()==&p)c->active.reset();p.deleted=true;});
	Bind(p,"SetUniform",[](Renderer& p,const std::string& name,sol::variadic_args args){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Original Grass uniform write has no active shader");std::vector<float> v;for(auto x:args)v.push_back(x.as<float>());p.Floats(name,v);});
	Bind(p,"SetUniformInt",[](Renderer& p,const std::string& name,int n){auto c=p.context.lock();Check(c&&c->active.get()==&p,"Original Grass integer write has no active shader");p.Integer(name,n);});
	family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Renderer",sol::lua_nil);
	Bind(family,"Available",&IsNativeGrassConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& name){Check(!CLuaHandle::GetHandleSynced(L),"Original Grass source belongs to the unsynced renderer");return Archive(name);});
	Bind(family,"PrepareSource",[](std::string s){Check(NativeGrassSourceHash(s)==grass::WidgetHash,"Original31657 Grass widget identity differs");LuaUtils::TracyRemoveAlsoExtras(s.data());return s;});
	gl.set("NativeGrass",family);return true;
}
}
#else
namespace LuaNativeGrass {bool PushEntries(lua_State*){return true;}}
#endif
