/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeFireSmoke.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaNativeUI.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeFireSmokeRecorder.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeFireSmokePrograms.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <map>
#include <string_view>

namespace LuaNativeFireSmoke {
namespace {
using namespace rendering;
NativeFireSmokeRecorder recorder;
void Check(bool valid,const char* message) { if (!valid) throw std::runtime_error(message); }
std::uint64_t Hash(std::string_view text)
{
	std::uint64_t hash=14695981039346656037ULL; for (unsigned char byte:text) hash=(hash^byte)*1099511628211ULL; return hash;
}
struct Program; struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::shared_ptr<const ScreenTexture>> images;
	std::array<std::shared_ptr<const ScreenTexture>,2> textures;
	GLenum depth=GL_LEQUAL,blendSource=GL_SRC_ALPHA,blendDestination=GL_ONE_MINUS_SRC_ALPHA;
	bool depthWrite=true,cull=false;
	void Reset() noexcept { auto b=std::move(buffers); auto i=std::move(images); *this=Context{}; buffers=std::move(b); images=std::move(i); }
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent)
	{
		Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER,"Original FireSmoke only uses vertex/index buffers");
		value=std::make_shared<LuaVBOImpl>(target,frequent); CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Define(int count,sol::optional<sol::object> layout) { value->Define(count,layout); }
	std::size_t Upload(const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last) { return value->Upload(data,attribute,offset,first,last); }
	void Delete() { value->Delete(); }
	auto GetBufferSize() { return value->GetBufferSize(); }
	std::string GetID() { auto id=std::to_string(value->CaptureNativeSnapshot()->object); if (auto owner=context.lock()) owner->buffers[id]=shared_from_this(); return id; }
	const std::string& GetLabel() const { return label; }
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	std::array<float,8> defines{};
	std::array<float,3> wind{};
	std::array<float,4> highlight{};
	bool windSet=false,highlightSet=false,deleted=false;
	void Activate() { Check(!deleted,"Deleted original FireSmoke program"); context->active=shared_from_this(); }
	void Deactivate() { if (context->active.lock().get()==this) context->active.reset(); }
	void Finalize() { Deactivate(); deleted=true; }
	void SetUniformFloat(const std::string& name,sol::variadic_args args)
	{
		Check(!deleted,"Deleted original FireSmoke uniform target");
		const unsigned count=name=="windVelocity"?3:name=="highlightPass"?4:0;
		Check(count && args.size()==count,"Original FireSmoke uniform name/extent differs");
		std::array<float,4> values{}; unsigned i=0;
		for (const auto& arg:args) { const float value=arg.as<float>(); Check(std::isfinite(value),"Nonfinite original FireSmoke uniform"); values[i++]=value; }
		if (count==3) { std::copy_n(values.begin(),3,wind.begin()); windSet=true; } else { highlight=values; highlightSet=true; }
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> buffer) { Check(!deleted && bool(buffer),"Original FireSmoke vertex buffer missing"); vertices=std::move(buffer); }
	void AttachIndexBuffer(std::shared_ptr<Buffer> buffer) { Check(!deleted && bool(buffer),"Original FireSmoke index buffer missing"); indices=std::move(buffer); }
	void AttachInstanceBuffer(std::shared_ptr<Buffer> buffer) { Check(!deleted && bool(buffer),"Original FireSmoke instance buffer missing"); instances=std::move(buffer); }
	void Delete() noexcept { deleted=true; vertices.reset(); indices.reset(); instances.reset(); }
	void Draw(bool indexed,GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance)
	{
		try {
			Check(IsNativeFireSmokeConsumerAvailable() && recorder.Active() && !deleted && vertices && indices && instances,"Original FireSmoke needs its connected world scope/buffers");
			Check(primitive==GL_TRIANGLES && count.value_or(6)==6 && first.value_or(0)==0 && baseInstance.value_or(0)==0 && instancesCount.value_or(0)>=0,"Original FireSmoke primitive/range differs");
			auto program=context->active.lock(); Check(program && !program->deleted && program->windSet && program->highlightSet,"Original FireSmoke actual uniforms/program are missing");
			Check(context->depth==GL_LEQUAL && !context->depthWrite && !context->cull && context->blendSource==GL_ONE && context->blendDestination==GL_ONE_MINUS_SRC_ALPHA,"Original FireSmoke premultiplied/depth state differs");
			NativeFireSmokeDraw draw; draw.indexed=indexed; draw.vertices=vertices->value->CaptureNativeSnapshot(); draw.indices=indices->value->CaptureNativeSnapshot(); draw.instances=instances->value->CaptureNativeSnapshot(); draw.instanceCount=instancesCount.value_or(0); draw.defines=program->defines; draw.windVelocity=program->wind; draw.highlightPass=program->highlight; draw.textures=context->textures;
			recorder.Record(std::move(draw));
		} catch (...) { context->Reset(); throw; }
	}
	void DrawElements(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance) { Draw(true,primitive,count,first,instancesCount,baseInstance); }
	void DrawArrays(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance) { Draw(false,primitive,count,first,instancesCount,baseInstance); }
};
std::shared_ptr<const ScreenTexture> Image(lua_State* L,Context& context,const std::string& name)
{
	Check(name=="bitmaps/projectiletextures/BARFlame02.tga" || name=="bitmaps/projectiletextures/smoke-beh-anim.tga","Unknown original FireSmoke texture");
	if (const auto found=context.images.find(name);found!=context.images.end()) return found->second;
	// Reuse the actual VFS/CBitmap loader and shared immutable GPU image path.
	// These exact unqualified TGAs use original generated mips/repeat/trilinear.
	auto binding=LuaNativeUI::ResolveTextureBinding(L,name);
	Check(binding && !binding->source && !binding->postprocess,"Original FireSmoke named atlas is missing or has a dynamic binding");
	auto original=binding->current;
	Check(original && !original->incomplete && original->format==ScreenTextureFormat::RGBA8Unorm && !original->bytes.empty(),"Original FireSmoke atlas decode failed");
	Check(original->generateMipmaps,"Original FireSmoke named atlas is missing its original mip producer");
	context.images[name]=original; return original;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeFireSmokeConsumerAvailable(),"Native FireSmoke requires unsynced connected compiled consumer");
	sol::state_view lua(L); auto table=lua.create_table(); auto context=std::make_shared<Context>();
	std::erase_if(contexts,[](const auto& entry){return entry.expired();}); contexts.emplace_back(context);
	table.set_function("GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent) { auto buffer=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false)); buffer->context=context; return buffer; });
	table.set_function("GetVAO",[context]() { auto array=std::make_shared<VertexArray>(); array->context=context; return array; });
	const auto createProgram=[context](const std::string& name,const std::string& vertex,const std::string& fragment,const sol::table& defines,const sol::table& floats,const sol::table& ints) {
		Check(name=="FireSmokeGL4" && Hash(vertex)==NativeFireSmokeVertexHash && Hash(fragment)==NativeFireSmokeFragmentHash,"Original FireSmoke shader source differs from actual native program");
		auto program=std::make_shared<Program>(); program->context=context; std::bitset<8> seen;
		for (const auto& item:defines) { const auto key=item.first.as<std::string>(); const auto found=std::find(NativeFireSmokeParameterNames.begin(),NativeFireSmokeParameterNames.end(),key); Check(found!=NativeFireSmokeParameterNames.end(),"Unknown original FireSmoke shader define"); const auto index=std::size_t(found-NativeFireSmokeParameterNames.begin()); const float value=item.second.as<float>(); Check(std::isfinite(value),"Nonfinite original FireSmoke shader define"); program->defines[index]=value; seen[index]=true; }
		Check(seen.all() && floats.empty(),"Original FireSmoke shader definitions/uniforms are missing");
		std::bitset<2> samplers; for (const auto& item:ints) { const auto key=item.first.as<std::string>(); const int slot=item.second.as<int>(); Check((key=="fireTex" && slot==0) || (key=="smokeTex" && slot==1),"Original FireSmoke sampler binding differs"); samplers[slot]=true; } Check(samplers.all(),"Original FireSmoke original sampler is missing"); return program;
	};
	table.set_function("CreateProgram",createProgram);
	// A real, private source-bound shader operation; not a public GLSL flag or
	// dummy success handle. The unchanged original checks this before init.
	table.set_function("CreateShader",[L,createProgram](sol::table descriptor) { sol::state_view lua(L); return createProgram("FireSmokeGL4",descriptor.get<std::string>("vertex"),descriptor.get<std::string>("fragment"),descriptor.get<sol::table>("shaderConfig"),lua.create_table(),descriptor.get<sol::table>("uniformInt")); });
	table.set_function("DepthTest",[context](sol::object value) { if (value.is<bool>()) context->depth=value.as<bool>()?GL_LEQUAL:0; else { Check(value.as<GLenum>()==GL_LEQUAL,"Original FireSmoke depth function unsupported"); context->depth=GL_LEQUAL; } });
	table.set_function("DepthMask",[context](bool value) { context->depthWrite=value; });
	table.set_function("Culling",[context](bool value) { Check(!value,"Original FireSmoke does not cull"); context->cull=value; });
	table.set_function("Blending",[context](GLenum source,GLenum destination) { Check((source==GL_ONE || source==GL_SRC_ALPHA) && destination==GL_ONE_MINUS_SRC_ALPHA,"Original FireSmoke blend state differs"); context->blendSource=source; context->blendDestination=destination; });
	table.set_function("Texture",[L,context](int unit,sol::object value) { Check(unit==0 || unit==1,"Original FireSmoke texture unit differs"); if (value.is<bool>()) { Check(!value.as<bool>(),"Original FireSmoke texture has no named image"); context->textures[unit].reset(); } else context->textures[unit]=Image(L,*context,value.as<std::string>()); return true; });
	table.set_function("ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label) { Check(type==0x82e0 && label.size()<=1024,"Invalid original FireSmoke buffer label"); const auto found=context->buffers.find(identity); Check(found!=context->buffers.end(),"Unknown original FireSmoke buffer identity"); auto buffer=found->second.lock(); Check(bool(buffer),"Deleted original FireSmoke buffer"); buffer->label=label; });
	return table;
}
}
void BeginWorldFrame() { recorder.Begin(); }
std::shared_ptr<const rendering::NativeFireSmokeFrame> EndWorldFrame()
{
	NativeFireSmokeFrame resources; const auto* view=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
	std::copy_n(view->GetViewProjectionMatrix().m,16,resources.cameraViewProjection.begin()); std::copy_n(view->GetViewMatrixInverse().m,16,resources.cameraViewInverse.begin());
	resources.timeInfo={float(gs->frameNum),spring_tomsecs(globalRendering->grTime)*.001f,(gs->GetLuaSimFrame()+globalRendering->timeOffset)/GAME_SPEED,globalRendering->timeOffset};
	recorder.SetFrameResources(resources); return recorder.End();
}
void AbortWorldFrame() noexcept { recorder.Abort(); for (auto& weak:contexts) if (auto context=weak.lock()) context->Reset(); }
void Reset() { AbortWorldFrame(); for (auto& weak:contexts) if (auto context=weak.lock()) context->images.clear(); }
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl=sol::stack::get<sol::table>(L,-1); auto family=lua.create_table();
	family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel);
	family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawElements",&VertexArray::DrawElements,"DrawArrays",&VertexArray::DrawArrays,"Delete",&VertexArray::Delete);
	family.new_usertype<Program>("Program",sol::no_constructor,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize,"SetUniformFloat",&Program::SetUniformFloat);
	family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);
	family.set_function("Available",&IsNativeFireSmokeConsumerAvailable); family.set_function("CreateContext",[L]() { return CreateContext(L); });
	family.set_function("LoadArchiveSource",[L](const std::string& path) { Check(!CLuaHandle::GetHandleSynced(L),"Original FireSmoke source is unsynced-only"); Check(path=="LuaRules/Gadgets/gfx_fire_smoke_gl4.lua" || path=="modules/graphics/instancevbotable.lua","Unknown original FireSmoke archive source"); CFileHandler file(path,SPRING_VFS_ZIP); std::string source; Check(file.FileExists() && file.LoadStringData(source),"Original FireSmoke archive source missing"); return source; });
	family.set_function("PrepareSource",[](std::string source) { const auto hash=Hash(source); Check(hash==NativeFireSmokeGadgetHash || hash==NativeFireSmokeHelperHash,"Original FireSmoke archive source identity differs"); LuaUtils::TracyRemoveAlsoExtras(source.data()); return source; });
	family.set_function("LoadArchiveSource31657",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Pinned31657 FireSmoke archive is unsynced only");const auto expected=path=="LuaRules/Gadgets/gfx_fire_smoke_gl4.lua"?NativeFireSmokeGadgetHash31657:path=="modules/graphics/instancevbotable.lua"?NativeFireSmokeHelperHash31657:0;Check(expected!=0,"Unknown pinned31657 FireSmoke archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&Hash(source)==expected,"Pinned31657 FireSmoke archive identity differs");return source;});
	family.set_function("PrepareSource31657",[](std::string source){const auto hash=Hash(source);Check(hash==NativeFireSmokeGadgetHash31657||hash==NativeFireSmokeHelperHash31657,"Pinned31657 FireSmoke Lua source identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeFireSmoke",family); return true;
}
}
#else
namespace LuaNativeFireSmoke {
void BeginWorldFrame() {} std::shared_ptr<const rendering::NativeFireSmokeFrame> EndWorldFrame() { return {}; }
void AbortWorldFrame() noexcept {} void Reset() {} bool PushEntries(lua_State*) { return true; }
}
#endif
