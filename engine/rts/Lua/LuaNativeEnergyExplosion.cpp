/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeEnergyExplosion.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeEnergyExplosionRecorder.h"
#include "Rendering/Backend/NativeEnergyExplosionProgram.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/FileHandler.h"
#include <cmath>
#include <algorithm>
#include <string_view>
namespace LuaNativeEnergyExplosion {
namespace {
using namespace rendering;
NativeEnergyExplosionRecorder recorder;
void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
uint64_t SourceHash(std::string_view s){uint64_t h=14695981039346656037ULL;for(uint8_t c:s)h=(h^c)*1099511628211ULL;return h;}
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> program;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	GLenum depth=GL_LEQUAL,blendSource=GL_SRC_ALPHA,blendDestination=GL_ONE_MINUS_SRC_ALPHA;
	bool depthWrite=true,cull=false,alpha=false,scissor=false,offset=false,stencil=false;
	std::array<float,4> color{1,1,1,1};std::array<bool,4> colorMask{true,true,true,true};
	void Reset()noexcept{auto saved=std::move(buffers);*this=Context{};buffers=std::move(saved);}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){Check(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER,"Energy burst only uses original vertex/index buffers");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	void Define(int n,sol::optional<sol::object> layout){value->Define(n,layout);}
	size_t Upload(const sol::stack_table& t,sol::optional<int> a,sol::optional<int> o,sol::optional<int> first,sol::optional<int> last){return value->Upload(t,a,o,first,last);}
	void Delete(){value->Delete();}
	auto GetBufferSize(){return value->GetBufferSize();}
	std::string GetID(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto c=context.lock())c->buffers[id]=shared_from_this();return id;}
	const std::string& GetLabel()const{return label;}
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;std::map<std::string,std::vector<float>> uniforms;bool deleted=false;
	void Activate(){Check(!deleted,"Deleted energy burst program");context->program=shared_from_this();}
	void Deactivate(){if(context->program.lock().get()==this)context->program.reset();}
	void Finalize(){Deactivate();deleted=true;}
	void Set(const std::string& name,std::vector<float> values){auto f=std::find_if(NativeEnergyUniforms.begin(),NativeEnergyUniforms.end(),[&](const auto& s){return s.name==name;});Check(!deleted&&f!=NativeEnergyUniforms.end()&&values.size()==f->count,"Unknown energy burst uniform/cardinality");for(float v:values)Check(std::isfinite(v),"Energy burst uniform is nonfinite");if(name=="u_shape")Check(values[0]==0||values[0]==1,"Energy burst shape differs");uniforms[name]=std::move(values);}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,indices,instances;bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&bool(b),"Missing energy template");vertices=std::move(b);}
	void AttachIndexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&bool(b),"Missing energy indices");indices=std::move(b);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&bool(b),"Missing energy instance buffer");instances=std::move(b);}
	void Delete()noexcept{deleted=true;vertices.reset();indices.reset();instances.reset();}
	void DrawElements(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> baseVertex,sol::optional<int> firstInstance){try{
		Check(IsNativeEnergyExplosionConsumerAvailable()&&recorder.Active()&&!deleted&&vertices&&indices&&instances,"Energy burst needs its compiled connected consumer and attachments");
		Check(primitive==GL_TRIANGLES&&first.value_or(0)==0&&baseVertex.value_or(0)==0&&instanceCount.value_or(0)>=0&&firstInstance.value_or(0)>=0,"Energy burst original indexed draw range differs");
		auto p=context->program.lock();Check(p&&!p->deleted,"Energy burst draw has no active original program");
		Check(context->depth==GL_LEQUAL&&!context->depthWrite&&!context->cull&&!context->alpha&&!context->scissor&&!context->offset&&!context->stencil&&context->color==std::array<float,4>{1,1,1,1}&&context->colorMask==std::array<bool,4>{true,true,true,true}&&context->blendSource==GL_ONE&&context->blendDestination==GL_ONE_MINUS_SRC_ALPHA,"Energy burst original pass state differs");
		NativeEnergyExplosionDraw d;d.vertexSlots=vertices->value->CaptureNativeSnapshot();d.indices=indices->value->CaptureNativeSnapshot();d.instances=instances->value->CaptureNativeSnapshot();Check(count.value_or(d.indices->layout.elements)==int(d.indices->layout.elements),"Partial original energy burst indices differ");d.instanceCount=instanceCount.value_or(0);d.firstInstance=firstInstance.value_or(0);d.uniforms=p->uniforms;recorder.Record(std::move(d));
	}catch(...){context->Reset();throw;}}
};
sol::table CreateContext(lua_State* L){Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeEnergyExplosionConsumerAvailable(),"Energy burst family needs its real unsynced consumer");sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();contexts.erase(std::remove_if(contexts.begin(),contexts.end(),[](const auto& p){return p.expired();}),contexts.end());contexts.push_back(c);
	t.set_function("GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));b->context=c;return b;});
	t.set_function("GetVAO",[c](){auto a=std::make_shared<VertexArray>();a->context=c;return a;});
	t.set_function("CreateProgram",[L,c](const std::string& vertex,const std::string& fragment,const sol::table& floats,const sol::table& ints){Check(SourceHash(vertex)==NativeEnergyVertexHash&&SourceHash(fragment)==NativeEnergyFragmentHash,"Energy burst exact original NoGS program identity differs");auto p=std::make_shared<Program>();p->context=c;for(const auto& x:floats){auto v=x.second;std::vector<float> values;if(v.is<sol::table>()){auto table=v.as<sol::table>();for(unsigned k=1;k<=table.size();++k)values.push_back(table[k].get<float>());}else values.push_back(v.as<float>());p->Set(x.first.as<std::string>(),std::move(values));}for(const auto& x:ints){Check(x.first.as<std::string>()=="u_shape","Unknown energy burst integer uniform");p->Set("u_shape",{x.second.as<float>()});}Check(p->uniforms.size()==NativeEnergyUniforms.size(),"Energy burst original uniforms missing");return sol::make_object(sol::state_view(L),p);});
	t.set_function("DepthTest",[c](sol::object v){c->depth=v.is<bool>()?(v.as<bool>()?GL_LEQUAL:0):v.as<GLenum>();Check(c->depth==0||c->depth==GL_LEQUAL,"Energy burst depth function differs");});
	t.set_function("DepthMask",[c](bool v){c->depthWrite=v;});t.set_function("Culling",[c](bool v){c->cull=v;});t.set_function("AlphaTest",[c](bool v){c->alpha=v;});t.set_function("Scissor",[c](bool v){c->scissor=v;});t.set_function("PolygonOffset",[c](bool v){c->offset=v;});t.set_function("StencilTest",[c](bool v){c->stencil=v;});
	t.set_function("PolygonMode",[](GLenum face,GLenum mode){Check(face==GL_FRONT_AND_BACK&&mode==GL_FILL,"Energy burst original polygon fill differs");});t.set_function("Color",[c](float r,float g,float b,float a){for(float v:{r,g,b,a})Check(std::isfinite(v),"Nonfinite energy burst color");c->color={r,g,b,a};});t.set_function("ColorMask",[c](bool r,bool g,bool b,bool a){c->colorMask={r,g,b,a};});t.set_function("Blending",[c](GLenum a,GLenum b){Check((a==GL_ONE||a==GL_SRC_ALPHA)&&b==GL_ONE_MINUS_SRC_ALPHA,"Energy burst original blend differs");c->blendSource=a;c->blendDestination=b;});
	t.set_function("ObjectLabel",[c](GLenum type,const std::string& id,const std::string& label){Check(type==0x82e0&&label.size()<=1024,"Energy buffer label differs");auto f=c->buffers.find(id);Check(f!=c->buffers.end(),"Unknown energy buffer label identity");auto b=f->second.lock();Check(bool(b),"Deleted energy buffer label");b->label=label;});return t;
}
}
void BeginWorldFrame(){recorder.Begin();}
std::shared_ptr<const rendering::NativeEnergyExplosionFrame> EndWorldFrame(){NativeEnergyExplosionFrame r;const auto* view=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);std::copy_n(view->GetViewProjectionMatrix().m,16,r.cameraViewProjection.begin());std::copy_n(view->GetViewMatrixInverse().m,16,r.cameraViewInverse.begin());r.timeInfo={float(gs->frameNum),spring_tomsecs(globalRendering->grTime)*.001f,(gs->GetLuaSimFrame()+globalRendering->timeOffset)/GAME_SPEED,globalRendering->timeOffset};recorder.SetFrameResources(r);return recorder.End();}
void AbortWorldFrame()noexcept{recorder.Abort();for(auto& w:contexts)if(auto c=w.lock())c->Reset();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State* L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto t=lua.create_table();t.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel);t.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawElements",&VertexArray::DrawElements,"Delete",&VertexArray::Delete);t.new_usertype<Program>("Program",sol::no_constructor,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize);t.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);t.set_function("Available",&IsNativeEnergyExplosionConsumerAvailable);t.set_function("CreateContext",[L](){return CreateContext(L);});t.set_function("LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Energy burst archive access is unsynced only");Check(path=="LuaRules/Gadgets/gfx_energy_explosion_particles_gl4.lua"||path=="LuaUI/Shaders/energy_explosion_particles_gl4_nogs.vert.glsl"||path=="modules/graphics/instancevbotable.lua","Unknown energy burst archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string s;Check(file.FileExists()&&file.LoadStringData(s),"Original energy burst archive source missing");return s;});t.set_function("PrepareSource",[](std::string source){const auto hash=SourceHash(source);Check(hash==0x9b1e211c584fb67bULL||hash==0x8cef105d28038367ULL,"Energy burst preprocessing requires exact archive source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeEnergyExplosion",t);return true;}
}
#else
namespace LuaNativeEnergyExplosion {void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeEnergyExplosionFrame> EndWorldFrame(){return {};}void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}}
#endif
