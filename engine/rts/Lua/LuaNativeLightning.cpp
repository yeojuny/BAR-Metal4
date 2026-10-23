/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeLightning.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeLightningEffectRecorder.h"
#include "Rendering/Backend/NativeLightningEffectPrograms.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string_view>

namespace LuaNativeLightning {
namespace {
using namespace rendering;
NativeLightningEffectRecorder recorder;
void Check(bool valid,const char* message){if(!valid)throw std::runtime_error(message);}
uint64_t SourceHash(std::string_view text){uint64_t value=14695981039346656037ULL;for(unsigned char byte:text)value=(value^byte)*1099511628211ULL;return value;}
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::shared_ptr<const NativeProjectileTexture>> images;
	std::shared_ptr<const NativeProjectileTexture> texture;
	GLenum depth=GL_LEQUAL;
	bool depthWrite=true,cull=false;
	GLenum blendSrc=GL_SRC_ALPHA,blendDst=GL_ONE_MINUS_SRC_ALPHA;
	void Reset() noexcept {auto b=std::move(buffers);auto i=std::move(images);*this=Context{};buffers=std::move(b);images=std::move(i);}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){Check(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER,"Original projectile effects only use vertex/index buffers");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	void Define(int count,sol::optional<sol::object> layout){value->Define(count,layout);}
	size_t Upload(const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return value->Upload(data,attr,offset,first,last);}
	void Delete(){value->Delete();}
	auto GetBufferSize(){return value->GetBufferSize();}
	std::string GetID(){auto identity=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[identity]=shared_from_this();return identity;}
	const std::string& GetLabel() const{return label;}
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	NativeLightningProgram kind;
	std::map<std::string,float> parameters;
	bool deleted=false;
	void Activate(){Check(!deleted,"Deleted native projectile program");context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Finalize(){Deactivate();deleted=true;}
	void SetUniform(const std::string& name,float value){Check(!deleted&&std::isfinite(value)&&std::find(NativeLightningParameterNames.begin(),NativeLightningParameterNames.end(),name)!=NativeLightningParameterNames.end(),"Unknown/nonfinite original projectile parameter");parameters[name]=value;}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> value){Check(!deleted&&bool(value),"Missing original projectile vertex buffer");vertices=std::move(value);}
	void AttachIndexBuffer(std::shared_ptr<Buffer> value){Check(!deleted&&bool(value),"Missing original projectile index buffer");indices=std::move(value);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> value){Check(!deleted&&bool(value),"Missing original projectile instance buffer");instances=std::move(value);}
	void Delete() noexcept{deleted=true;vertices.reset();indices.reset();instances.reset();}
	void Draw(bool indexed,GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance){try{
		Check(IsNativeLightningConsumerAvailable()&&recorder.Active()&&!deleted&&vertices&&indices&&instances,"Original projectile effect draw needs its connected world consumer and attachments");
		Check(primitive==GL_TRIANGLES&&first.value_or(0)==0&&baseInstance.value_or(0)==0&&count.value_or(0)>=0&&instancesCount.value_or(0)>=0,"Unsupported original projectile effect draw range");
		auto program=context->active.lock();Check(program&&!program->deleted,"Original projectile draw has no active translated program");
		Check(context->depth==GL_LEQUAL&&!context->depthWrite&&!context->cull&&context->blendSrc==GL_ONE&&context->blendDst==GL_ONE,"Original projectile additive/depth state differs");
		NativeLightningEffectDraw draw;draw.indexed=indexed;draw.program=program->kind;draw.vertices=vertices->value->CaptureNativeSnapshot();draw.indices=indices->value->CaptureNativeSnapshot();draw.indexCount=count.value_or(draw.indices->layout.elements);draw.instances=instances->value->CaptureNativeSnapshot();draw.instanceCount=instancesCount.value_or(0);draw.parameters=program->parameters;draw.texture=context->texture;draw.surface=rendering::ui::GetRecorder().CurrentState().draw;draw.world=rendering::ui::GetRecorder().CurrentWorldState();recorder.Record(std::move(draw));
	}catch(...){context->Reset();throw;}}
	void DrawElements(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance){Draw(true,primitive,count,first,instancesCount,baseInstance);}
	void DrawArrays(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance){Draw(false,primitive,count,first,instancesCount,baseInstance);}
};
std::shared_ptr<const NativeProjectileTexture> Image(Context& context,const std::string& name)
{
	Check(name=="bitmaps/projectiletextures/flare2.tga","Unknown original projectile named texture");
	if(auto found=context.images.find(name);found!=context.images.end())return found->second;
	CBitmap bitmap;Check(bitmap.Load(name,1.0f,4u,0u)&&!bitmap.compressed&&bitmap.channels==4&&bitmap.GetDataTypeSize()==1&&bitmap.xsize>0&&bitmap.ysize>0,"Original projectile named image decode failed");
	const size_t width=bitmap.xsize,height=bitmap.ysize;Check(width<=std::numeric_limits<size_t>::max()/height/4,"Original projectile image extent overflow");const size_t bytes=width*height*4;
	Check(bitmap.GetRawMem()!=nullptr&&bitmap.GetMemSize()>=bytes,"Original projectile image storage unavailable");auto image=std::make_shared<NativeProjectileTexture>();image->sourceName=name;image->width=width;image->height=height;image->rgba.assign(bitmap.GetRawMem(),bitmap.GetRawMem()+bytes);context.images[name]=image;return image;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeLightningConsumerAvailable(),"Native projectile family requires unsynced compiled consumer");sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();contexts.erase(std::remove_if(contexts.begin(),contexts.end(),[](const auto& p){return p.expired();}),contexts.end());contexts.emplace_back(context);
	table.set_function("GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto buffer=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));buffer->context=context;return buffer;});
	table.set_function("GetVAO",[context](){auto vao=std::make_shared<VertexArray>();vao->context=context;return vao;});
	table.set_function("CreateProgram",[L,context](const std::string& name,const std::string& vertex,const std::string& fragment,const sol::table& defines,const sol::table& floats,const sol::table& ints){auto spec=std::find_if(NativeLightningPrograms.begin(),NativeLightningPrograms.end(),[&](const auto& p){return p.name==name;});Check(spec!=NativeLightningPrograms.end()&&spec->vertexHash==SourceHash(vertex)&&spec->fragmentHash==SourceHash(fragment),"Original projectile program source differs from translated family");auto program=std::make_shared<Program>();program->context=context;program->kind=static_cast<NativeLightningProgram>(spec-NativeLightningPrograms.begin());for(const auto& item:defines)program->SetUniform(item.first.as<std::string>(),item.second.as<float>());Check(floats.empty(),"Unexpected original projectile float uniform");const auto samplerName=NativeLightningSamplerNames[static_cast<unsigned>(program->kind)];size_t samplerCount=0;for(const auto& item:ints){Check(item.first.as<std::string>()==samplerName&&item.second.as<int>()==0,"Original projectile sampler assignment differs");++samplerCount;}Check(samplerCount==(samplerName.empty()?0u:1u),"Original projectile sampler binding missing");return sol::make_object(sol::state_view(L),program);});
	table.set_function("DepthTest",[context](sol::object v){if(v.is<bool>())context->depth=v.as<bool>()?GL_LEQUAL:0;else{Check(v.as<GLenum>()==GL_LEQUAL,"Unsupported original projectile depth function");context->depth=GL_LEQUAL;}});
	table.set_function("DepthMask",[context](bool value){context->depthWrite=value;});table.set_function("Culling",[context](bool value){Check(!value,"Original projectile family does not cull");context->cull=value;});
	table.set_function("Blending",[context](GLenum source,GLenum dest){Check((source==GL_ONE&&dest==GL_ONE)||(source==GL_SRC_ALPHA&&dest==GL_ONE_MINUS_SRC_ALPHA),"Unsupported original projectile blend state");context->blendSrc=source;context->blendDst=dest;});
	table.set_function("Texture",[context](int unit,sol::object value){Check(unit==0,"Original projectile family texture unit differs");if(value.is<bool>()){Check(!value.as<bool>(),"Original projectile texture has no named image");context->texture.reset();}else context->texture=Image(*context,value.as<std::string>());return true;});
	table.set_function("ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label){Check(type==0x82e0&&label.size()<=1024,"Invalid original projectile buffer label");auto found=context->buffers.find(identity);Check(found!=context->buffers.end(),"Unknown original projectile buffer identity");auto buffer=found->second.lock();Check(bool(buffer),"Deleted original projectile buffer identity");buffer->label=label;});return table;
}
}
void BeginWorldFrame(){recorder.Begin();}
std::shared_ptr<const rendering::NativeLightningEffectFrame> EndWorldFrame(){NativeLightningEffectFrame resources;const auto* view=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);std::copy_n(view->GetViewProjectionMatrix().m,16,resources.cameraViewProjection.begin());std::copy_n(view->GetViewMatrixInverse().m,16,resources.cameraViewInverse.begin());resources.timeInfo={float(gs->frameNum),spring_tomsecs(globalRendering->grTime)*.001f,(gs->GetLuaSimFrame()+globalRendering->timeOffset)/GAME_SPEED,globalRendering->timeOffset};recorder.SetFrameResources(resources);return recorder.End();}
void AbortWorldFrame() noexcept{recorder.Abort();for(auto& weak:contexts)if(auto context=weak.lock())context->Reset();}
void Reset(){AbortWorldFrame();for(auto& weak:contexts)if(auto context=weak.lock())context->images.clear();}
bool PushEntries(lua_State* L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel);family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawElements",&VertexArray::DrawElements,"DrawArrays",&VertexArray::DrawArrays,"Delete",&VertexArray::Delete);family.new_usertype<Program>("Program",sol::no_constructor,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize,"SetUniform",&Program::SetUniform);family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);family.set_function("Available",&IsNativeLightningConsumerAvailable);family.set_function("CreateContext",[L](){return CreateContext(L);});family.set_function("LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Native projectile archive source is unsynced only");Check(path=="LuaRules/Gadgets/gfx_lightning_cannon_gl4.lua"||path=="LuaRules/Gadgets/gfx_environmental_lightning_gl4.lua"||path=="modules/graphics/instancevbotable.lua","Unknown native projectile source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original projectile archive source missing");return source;});family.set_function("PrepareSource",[](std::string source){const auto hash=SourceHash(source);Check(hash==0xea957e540a054527ULL || hash==0xb4d7ebd510317ecaULL || hash==0x8cef105d28038367ULL || hash==12100181168429277143ULL,"Original projectile preprocessing requires exact supported source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeLightning",family);return true;}
}
#else
namespace LuaNativeLightning {void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeLightningEffectFrame> EndWorldFrame(){return {};}void AbortWorldFrame() noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}}
#endif
