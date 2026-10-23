/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeNano.h"

#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeLuaNanoRecorder.h"
#include "Rendering/Backend/NativeNano31657Programs.h"
#include "Rendering/Backend/NativeMiniMapCapture.h"
#include "System/FileSystem/FileHandler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string_view>

namespace LuaNativeNano {
namespace {
using namespace rendering;
NativeLuaNanoRecorder recorder;
NativeMiniMapCapture infoCapture;
bool haveDraws = false;

void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
uint64_t SourceHash(std::string_view source)
{
	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char byte: source) { hash ^= byte; hash *= 1099511628211ULL; }
	return hash;
}

struct Program;
struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	bool infoBound = false;
	GLenum depth = GL_LEQUAL;
	bool depthWrite = true, cull = false, alpha = false, scissor = false, offset = false;
	std::array<float,4> color{1,1,1,1};
	std::array<bool,4> colorMask{true,true,true,true};
	GLenum blendSrc = GL_SRC_ALPHA, blendDst = GL_ONE_MINUS_SRC_ALPHA;
	bool stencil = false;
	unsigned stencilWriteMask = 0xff, stencilReference = 0, stencilCompareMask = 0xff;
	GLenum stencilCompare = GL_ALWAYS;
	std::array<GLenum,3> stencilOperations{GL_KEEP,GL_KEEP,GL_KEEP};
	void Reset() noexcept { auto retained=std::move(buffers); *this=Context{}; buffers=std::move(retained); }
};
std::vector<std::weak_ptr<Context>> contexts;

// These wrappers expose the production typed parser and owned CPU shadow,
// without claiming a GL object or enabling the general gl.GetVBO API.
struct Buffer: std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	std::string label;
	Buffer(lua_State* L, GLenum target, bool frequent)
	{
		Check(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER, "Native nano supports only original vertex/index buffers");
		value = std::make_shared<LuaVBOImpl>(target, frequent);
		CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Delete() { value->Delete(); }
	void Define(int count, sol::optional<sol::object> layout) { value->Define(count, layout); }
	size_t Upload(const sol::stack_table& table, sol::optional<int> attribute, sol::optional<int> offset, sol::optional<int> first, sol::optional<int> last)
	{ return value->Upload(table, attribute, offset, first, last); }
	auto GetBufferSize() { return value->GetBufferSize(); }
	std::string GetID() { auto identity=std::to_string(value->CaptureNativeSnapshot()->object); if(auto owner=context.lock())owner->buffers[identity]=shared_from_this(); return identity; }
	const std::string& GetLabel() const { return label; }
};

constexpr std::array<std::string_view,16> uniformNames{
	"losAlwaysVisible", "drawRadius", "cubeShowInside", "cubeNoise", "cubeNoiseSpeed", "cubeNoiseScale",
	"glowScale", "glowIntensity", "glowFalloff", "coreBoost", "hueJitter", "sizePulseAmp", "sizePulseFreq",
	"whiteHotspot", "whiteHotspotThreshold", "u_shape"
};
bool KnownUniform(std::string_view name) { return std::find(uniformNames.begin(), uniformNames.end(), name) != uniformNames.end(); }
struct Program: std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	std::map<std::string,float> uniforms;
	bool deleted = false, rejectFutureSpawns = false;
	void Activate() { Check(!deleted, "Deleted native nano program"); context->active = shared_from_this(); }
	void Deactivate() { if (context->active.lock().get() == this) context->active.reset(); }
	void Finalize() { Deactivate(); deleted = true; }
	void SetUniform(const std::string& name, float value)
	{
		Check(!deleted && KnownUniform(name) && std::isfinite(value), "Unknown/nonfinite native nano uniform");
		if (name == "u_shape") Check(value == 0 || value == 1, "Unsupported native nano shape");
		uniforms[name] = value;
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices, indices, instances;
	bool deleted = false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> value) { Check(!deleted && bool(value), "Invalid native nano vertex attachment"); vertices = std::move(value); }
	void AttachIndexBuffer(std::shared_ptr<Buffer> value) { Check(!deleted && bool(value), "Invalid native nano index attachment"); indices = std::move(value); }
	void AttachInstanceBuffer(std::shared_ptr<Buffer> value) { Check(!deleted && bool(value), "Invalid native nano instance attachment"); instances = std::move(value); }
	void Delete() noexcept { deleted = true; vertices.reset(); indices.reset(); instances.reset(); }
	void DrawElements(GLenum primitive, sol::optional<int> count, sol::optional<int> first, sol::optional<int> instanceCount, sol::optional<int> baseInstance)
	{
		try {
			Check(IsNativeLuaNanoConsumerAvailable(), "Native nano translated consumer is unavailable");
			Check(recorder.Active() && !deleted && vertices && indices && instances, "Native nano draw outside world scope or with missing attachments");
			Check(primitive == GL_TRIANGLES && first.value_or(0) == 0 && baseInstance.value_or(0) == 0, "Unsupported native nano draw range/topology");
			auto program = context->active.lock();
			Check(program && !program->deleted && context->infoBound, "Native nano needs its active original program and real $info binding");
			Check(!context->depthWrite && !context->cull && !context->alpha && !context->scissor && !context->offset &&
				context->color == std::array<float,4>{1,1,1,1} && context->colorMask == std::array<bool,4>{true,true,true,true} &&
				context->blendSrc == GL_ONE && context->blendDst == GL_ONE_MINUS_SRC_ALPHA, "Native nano draw differs from original premultiplied world state");
			Check(context->stencil && context->stencilWriteMask == 0 && context->stencilReference == 0x40 && context->stencilCompareMask == 0x40 &&
				context->stencilOperations == std::array<GLenum,3>{GL_KEEP,GL_KEEP,GL_KEEP}, "Native nano draw differs from original ghost stencil state");
			const bool ghost = context->stencilCompare == GL_EQUAL;
			Check((ghost && context->depth == 0) || (!ghost && context->stencilCompare == GL_NOTEQUAL && context->depth == GL_LEQUAL), "Unsupported native nano depth/stencil pass");
			NativeLuaNanoDraw draw;
			draw.vertexSlots = vertices->value->CaptureNativeSnapshot();
			draw.indices = indices->value->CaptureNativeSnapshot();
			draw.instances = instances->value->CaptureNativeSnapshot();
			Check(count.value_or(draw.indices->layout.elements) == int(draw.indices->layout.elements), "Partial native nano index draw is unsupported");
			Check(instanceCount.value_or(0) >= 0, "Negative native nano instance count");
			draw.instanceCount = instanceCount.value_or(0);
			draw.uniforms = program->uniforms;
			draw.shape = int(draw.uniforms.at("u_shape"));
			draw.ghostStencilPass = ghost;
			draw.rejectFutureSpawns = program->rejectFutureSpawns;
			recorder.Record(std::move(draw)); haveDraws = true;
		} catch (...) {
			// Lua RunCallIn may remove this gadget and continue. Restore its
			// private state without invalidating earlier valid world commands.
			context->Reset(); throw;
		}
	}
};

sol::table CreateContext(lua_State* L,bool game31657=false)
{
	Check(IsNativeLuaNanoConsumerAvailable(), "Native nano family requested before its translated consumer compiled");
	sol::state_view lua(L);
	auto table = lua.create_table();
	auto context = std::make_shared<Context>();
	contexts.erase(std::remove_if(contexts.begin(), contexts.end(), [](const auto& value) { return value.expired(); }), contexts.end());
	contexts.emplace_back(context);
	table.set_function("GetVBO", [L,context](sol::optional<GLenum> target, sol::optional<bool> frequent) { auto buffer=std::make_shared<Buffer>(L, target.value_or(GL_ARRAY_BUFFER), frequent.value_or(false)); buffer->context=context; return buffer; });
	table.set_function("GetVAO", [context]() { auto vao = std::make_shared<VertexArray>(); vao->context = context; return vao; });
	table.set_function("CreateProgram", [context,L,game31657](const std::string& vertex, const std::string& fragment, sol::optional<std::string> geometry, const sol::table& floats, const sol::table& integers) -> sol::object {
		// Metal has no geometry stage. The original gadget's genuine compile
		// failure selects its existing NoGS source and exact indexed template.
		if (geometry && !geometry->empty()) return sol::make_object(sol::state_view(L), sol::lua_nil);
		Check(SourceHash(vertex) == (game31657?nano31657Program::VertexHash:0x6680a1835b107bafULL) && SourceHash(fragment) == (game31657?nano31657Program::FragmentHash:0x7527fb7d38d2a2bbULL), "Native nano shader source differs from the translated original NoGS family");
		Check(IsNativeLuaNanoConsumerAvailable(), "Native nano translated consumer is unavailable");
		auto program = std::make_shared<Program>(); program->context = context;program->rejectFutureSpawns=game31657;
		bool haveInfoSampler=false;
		for (const auto& item: floats) program->SetUniform(item.first.as<std::string>(), item.second.as<float>());
		for (const auto& item: integers) {
			const auto name = item.first.as<std::string>(); const float value = item.second.as<float>();
			if (name == "infoTex") { Check(value == 1, "Native nano original info sampler must use unit one"); haveInfoSampler=true; }
			else program->SetUniform(name, value);
		}
		Check(haveInfoSampler,"Native nano original program has a missing info sampler");
		for (auto name: uniformNames) Check(program->uniforms.contains(std::string(name)), "Native nano original program has a missing uniform");
		return sol::make_object(sol::state_view(L), program);
	});
	table.set_function("DepthTest", [context](sol::object value) { if (value.is<bool>()) context->depth = value.as<bool>() ? GL_LEQUAL : 0; else { auto mode=value.as<GLenum>(); Check(mode==GL_LEQUAL,"Unsupported native nano depth function"); context->depth=mode; } });
	table.set_function("DepthMask", [context](bool value) { context->depthWrite=value; });
	table.set_function("Culling", [context](bool value) { Check(!value,"Unsupported native nano culling"); context->cull=value; });
	table.set_function("AlphaTest", [context](bool value) { Check(!value,"Unsupported native nano alpha test"); context->alpha=value; });
	table.set_function("Scissor", [context](bool value) { Check(!value,"Unsupported native nano scissor"); context->scissor=value; });
	table.set_function("PolygonOffset", [context](bool value) { Check(!value,"Unsupported native nano polygon offset"); context->offset=value; });
	table.set_function("PolygonMode", [](GLenum face, GLenum mode) { Check(face==GL_FRONT_AND_BACK && mode==GL_FILL,"Unsupported native nano polygon mode"); });
	table.set_function("Color", [context](float r,float g,float b,float a) { Check(std::isfinite(r)&&std::isfinite(g)&&std::isfinite(b)&&std::isfinite(a),"Nonfinite native nano color");context->color={r,g,b,a}; });
	table.set_function("ColorMask", [context](bool r,bool g,bool b,bool a) { context->colorMask={r,g,b,a}; });
	table.set_function("Blending", [context](GLenum source,GLenum destination) { Check((source==GL_ONE||source==GL_SRC_ALPHA)&&destination==GL_ONE_MINUS_SRC_ALPHA,"Unsupported native nano blend factors");context->blendSrc=source;context->blendDst=destination; });
	table.set_function("StencilTest", [context](bool value) { context->stencil=value; });
	table.set_function("StencilMask", [context](unsigned mask) { Check(mask==0||mask==0xff,"Unsupported native nano stencil write mask");context->stencilWriteMask=mask; });
	table.set_function("StencilFunc", [context](GLenum compare,unsigned reference,unsigned mask) { Check((compare==GL_EQUAL||compare==GL_NOTEQUAL||compare==GL_ALWAYS)&&reference<=0xff&&mask<=0xff,"Unsupported native nano stencil function");context->stencilCompare=compare;context->stencilReference=reference;context->stencilCompareMask=mask; });
	table.set_function("StencilOp", [context](GLenum fail,GLenum depthFail,GLenum pass) { Check(fail==GL_KEEP&&depthFail==GL_KEEP&&pass==GL_KEEP,"Native nano never writes stencil");context->stencilOperations={fail,depthFail,pass}; });
	table.set_function("Texture", [context](int unit, sol::object value) { Check(unit==1,"Native nano only binds its original info sampler");if(value.is<bool>()){Check(!value.as<bool>(),"Native nano cannot bind an unspecified image");context->infoBound=false;}else{Check(value.as<std::string>()=="$info","Unsupported native nano texture");context->infoBound=true;}return true; });
	table.set_function("ObjectLabel", [context](GLenum type,const std::string& identity,const std::string& label) { Check(type==0x82e0&&!identity.empty()&&label.size()<=1024,"Invalid native nano debug buffer label");auto found=context->buffers.find(identity);Check(found!=context->buffers.end(),"Unknown native nano buffer label identity");auto buffer=found->second.lock();Check(bool(buffer),"Deleted native nano buffer label identity");buffer->label=label; });
	return table;
}
}

void BeginWorldFrame() { recorder.Begin(); haveDraws=false; }
std::shared_ptr<const rendering::NativeLuaNanoFrame> EndWorldFrame()
{
	NativeLuaNanoFrame resources;
	const auto* playerCamera = CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
	std::copy_n(playerCamera->GetViewMatrixInverse().m,16,resources.cameraViewInverse.begin());
	std::copy_n(playerCamera->GetViewProjectionMatrix().m,16,resources.cameraViewProjection.begin());
	resources.mapSquares={uint32_t(mapDims.mapx),uint32_t(mapDims.mapy),uint32_t(mapDims.pwr2mapx),uint32_t(mapDims.pwr2mapy)};
	if(haveDraws){
		Check(globalRendering!=nullptr,"Native nano actual info capture requires world draw generation");
		// This private capture is refreshed by the actual world source generation,
		// independently of minimap visibility, dimensions or its cache frequency.
		// Only the cache key differs; original map/InfoTextureHandler/LOS data and
		// immutable resource ownership still come from the shared real producer.
		NativeMiniMapState worldSource;
		worldSource.updateSerial=globalRendering->drawFrame;
		resources.infoBackground=infoCapture.Capture(worldSource);
		resources.info=resources.infoBackground->info;
		resources.infoIsNormal=!resources.info||resources.info->mode==NativeMiniMapInfoMode::Normal;
	}
	recorder.SetFrameResources(resources);
	return recorder.End();
}
void AbortWorldFrame() noexcept { recorder.Abort();haveDraws=false;for(auto& weak:contexts)if(auto context=weak.lock())context->Reset(); }
void Reset() { AbortWorldFrame();infoCapture.Reset(); }
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel,"Delete",&Buffer::Delete);
	family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawElements",&VertexArray::DrawElements,"Delete",&VertexArray::Delete);
	family.new_usertype<Program>("Program",sol::no_constructor,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"SetUniform",&Program::SetUniform,"Finalize",&Program::Finalize);
	family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);
	family.set_function("Available",&IsNativeLuaNanoConsumerAvailable);
	family.set_function("CreateContext",[L]() {return CreateContext(L);});
	family.set_function("LoadArchiveSource",[L](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(L),"Native nano archive source is unsynced only");
		Check(path=="LuaRules/Gadgets/gfx_nano_particles_gl4.lua" || path=="modules/graphics/instancevbotable.lua" ||
			path=="LuaUI/Shaders/nano_particles_gl4_nogs.vert.glsl","Unknown native nano archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;
		Check(file.FileExists()&&file.LoadStringData(source),"Original native nano archive source is missing");
		return source; // Raw bytes are hashed before the usual Tracy preprocessing.
	});
	family.set_function("PrepareSource",[](std::string source) {
		const auto hash=SourceHash(source);
		Check(hash==0xf170aac8dc7bedf1ULL || hash==0xa3fc701948f45d37ULL,"Native nano preprocessing requires a verified original Lua source");
		LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
	});
	family.set_function("CreateContext31657",[L](){return CreateContext(L,true);});
	family.set_function("LoadArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Pinned31657 nano archive is unsynced only");
		for(const auto& entry:nano31657Program::Sources)if(entry.path==path){CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&SourceHash(source)==entry.hash,"Pinned31657 nano archive identity differs");return source;}
		throw std::runtime_error("Unknown pinned31657 nano archive source");
	});
	family.set_function("PrepareSource31657",[](std::string source){const auto hash=SourceHash(source);bool known=false;for(const auto& entry:nano31657Program::Sources)known|=entry.lua&&entry.hash==hash;Check(known,"Pinned31657 nano preprocessing requires a verified Lua source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeNano",family);return true;
}
}
#else
namespace LuaNativeNano {
void BeginWorldFrame() {}
std::shared_ptr<const rendering::NativeLuaNanoFrame> EndWorldFrame() { return {}; }
void AbortWorldFrame() noexcept {}
void Reset() {}
bool PushEntries(lua_State*) { return true; }
}
#endif
