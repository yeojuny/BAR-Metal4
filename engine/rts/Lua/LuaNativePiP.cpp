/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativePiP.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#endif
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaNativeUI.h"
#include "Lua/LuaNativeCUSTextures.h"
#include "Lua/LuaNativeGameDecal.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaOpenGL.h"
#include "Lua/LuaOpenGLUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/UI/MiniMap.h"
#include "Rendering/Backend/NativeMiniMapCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/WorldFrameData.h"
#include "Rendering/IconHandler.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Features/FeatureDef.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace LuaNativePiP {
namespace {
using namespace rendering;
using namespace rendering::ui;
DrawSink drawSink;
ModelProvider modelProvider;
bool consumerReady = false;
void Check(bool valid, const char* message) { if (!valid) throw std::runtime_error(message); }
struct CallbackError { sol::object value; };
void RethrowCallback(const sol::protected_function_result& result)
{
	throw CallbackError{sol::object(result.lua_state(),result.stack_index())};
}
template<typename Signature> struct LuaBoundary;
template<typename R,typename... Args> struct LuaBoundary<R(Args...)> {
	template<typename F> static auto Wrap(F function)
	{
		return [function=std::move(function)](sol::this_state lua,Args... args)->R {
			// This pinned sol2 set_function functor route has no C++ exception
			// trampoline. Recoil's Lua catches foreign exceptions as status -1,
			// exposing the current stack's callback instead of the real error.
			// Serialize only C++ failures here; retain original Lua error objects.
			try { return function(std::forward<Args>(args)...); }
			catch(const CallbackError& error) { error.value.push(); }
			catch(const std::exception& error) { lua_pushstring(lua,error.what()); }
			lua_error(lua);
			std::terminate();
		};
	}
};
template<typename F> void BindFunction(sol::table& table,const char* name,F function)
{
	table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
struct Program;
struct Context;
std::vector<std::weak_ptr<Context>> contexts;
struct Binding {
	std::shared_ptr<NativeUITextureBinding> resource;
 std::function<NativePiPSampledImage(unsigned)> sourceDrawImage; // actual CPU draw binding
 std::function<unsigned(unsigned)> sourceBind; // ONE original Texture execution
	NativePiPEngineImage engine = NativePiPEngineImage::None;
	ScreenSampler sampler;
	bool unitsAtlas = false;
};
struct Program {
	explicit Program(NativePiPProgramState value): state(std::move(value)) {}
	NativePiPProgramState state;
	bool deleted = false;
};
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;
	Buffer(lua_State* L, GLenum target, bool frequent)
	{
		Check(target == GL_ARRAY_BUFFER, "Original PiP uses vertex and instance array buffers");
		value = std::make_shared<LuaVBOImpl>(target, frequent);
		CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Define(int count, sol::optional<sol::object> layout) { value->Define(count,layout); }
	size_t Upload(const sol::stack_table& data, sol::optional<int> attr, sol::optional<int> offset, sol::optional<int> first, sol::optional<int> last)
	{ return value->Upload(data,attr,offset,first,last); }
	void Delete() { value->Delete(); }
	auto GetBufferSize() { return value->GetBufferSize(); }
};
NativeDrawPrimitive Primitive(GLenum mode)
{
	switch (mode) {
		case GL_TRIANGLES: return NativeDrawPrimitive::Triangles;
		case GL_TRIANGLE_STRIP: return NativeDrawPrimitive::TriangleStrip;
		case GL_TRIANGLE_FAN: return NativeDrawPrimitive::TriangleFan;
		case GL_QUADS: return NativeDrawPrimitive::Quads;
		case GL_LINES: return NativeDrawPrimitive::Lines;
		case GL_LINE_STRIP: return NativeDrawPrimitive::LineStrip;
		case GL_LINE_LOOP: return NativeDrawPrimitive::LineLoop;
		case GL_POINTS: return NativeDrawPrimitive::Points;
	}
	throw std::runtime_error("Unknown original PiP primitive");
}
struct Context: std::enable_shared_from_this<Context> {
	std::weak_ptr<Program> active;
	std::array<Binding,16> bindings;
	NativeMiniMapCapture mapCapture;
	std::unordered_map<const WorldTexture*,std::pair<std::weak_ptr<const WorldTexture>,std::shared_ptr<const ScreenTexture>>> featureImages;
	bool inPrimitive = false, typedPrimitive = false;
	NativeDrawPrimitive primitive = NativeDrawPrimitive::Triangles;
	NativePiPDraw pending;
	unsigned error = 0;
	std::uint64_t invalidUniformOperations = 0;

	void InvalidUniformOperation()
	{
		// LuaShaders::Uniform dispatches by argument count. A linked scalar
		// receiving glUniform2f records GL_INVALID_OPERATION without changing
		// the value or raising a Lua exception (original PiP sunDirY does this).
		if (error == 0) error = 0x0502;
		++invalidUniformOperations;
	}

	NativePiPSampledImage Sample(const Binding& bound, std::shared_ptr<const NativeMiniMapBackground>& engineFrame,unsigned actualUnit)
	{
		NativePiPSampledImage result;
		if (bound.engine != NativePiPEngineImage::None) {
			Check(minimap != nullptr, "PiP engine image requires the real minimap/viewer state");
			result.engineImage = bound.engine;
			if (!engineFrame) engineFrame = mapCapture.CapturePiP(minimap->GetNativeState());
			result.engineSource = engineFrame;
			result.sampler = bound.sampler;
		} else if(bound.sourceDrawImage) {
   result=bound.sourceDrawImage(actualUnit);
  } else if (bound.resource) {
			const auto value = bound.resource->Resolve();
			if (value.image && value.image->incomplete) result.incomplete = true;
			else { Check(bool(value.image), "Original PiP named/RTT image has no generation"); result.image = value.image; result.sampler = value.sampler; }
		} else {
			Check(!bound.unitsAtlas, "Original 3DO atlas needs its real image producer before PiP can sample it");
			result.incomplete = true; // Actual original texture-zero binding only.
		}
		return result;
	}
	NativePiPDraw Capture()
	{
		auto program = active.lock();
		Check(program && !program->deleted, "Original PiP draw has no live private program");
		NativePiPDraw draw; draw.program = program->state.Capture();
		std::shared_ptr<const NativeMiniMapBackground> engineFrame;
		for (const auto& uniform: GetNativePiPProgramSpec(draw.program.program).uniforms) {
			if (!uniform.sampler || !uniform.active) continue;
			const int unit = draw.program.samplerUnits[uniform.slot];
			Check(unit >= 0 && std::size_t(unit) < bindings.size(), "Original PiP sampler unit exceeds bindings");
			auto sampled = Sample(bindings[unit],engineFrame,unsigned(unit));
			draw.program.uniforms.flipV[uniform.slot] = sampled.image && sampled.image->flipV;
			if (sampled.incomplete) draw.program.uniforms.flipV[3] |= 1u << uniform.slot;
			draw.sampledImages[uniform.slot] = std::move(sampled);
		}
		return draw;
	}
	void Begin(NativeUIRecorder& replay, NativeDrawPrimitive mode)
	{
		Check(!inPrimitive, "Nested PiP immediate begin requires original invalid-operation handling");
		if (active.expired()) {
			replay.BeginLegacyPrimitive(mode,"exact original PiP");
			inPrimitive = true; primitive = mode; typedPrimitive = false; return;
		}
		auto captured = Capture();
		Check(GetNativePiPProgramSpec(captured.program.program).geometry == NativePiPGeometry::Immediate,
			"Original PiP immediate call does not match its program inputs");
		Check(mode == NativeDrawPrimitive::Triangles || mode == NativeDrawPrimitive::TriangleStrip ||
			mode == NativeDrawPrimitive::TriangleFan || mode == NativeDrawPrimitive::Quads, "Original PiP shader primitive has no paired topology");
		pending = std::move(captured); inPrimitive = true; primitive = mode; typedPrimitive = true;
	}
	void Vertex(NativeUIRecorder& replay, std::array<float,4> position)
	{
		if (!inPrimitive || !typedPrimitive) { replay.LegacyVertex(position[0],position[1],position[2],position[3]); return; }
		const auto state = replay.CurrentState();
		pending.immediate.push_back({position,state.color,{state.texCoord[0],state.texCoord[1],0,1}});
	}
	void End(NativeUIRecorder& replay)
	{
		if (!inPrimitive || !typedPrimitive) { replay.EndLegacyPrimitive(); inPrimitive = false; return; }
		inPrimitive = false;
		const auto count = std::uint32_t(pending.immediate.size());
		const auto triangle = [&](unsigned a,unsigned b,unsigned c){pending.indices.insert(pending.indices.end(),{a,b,c});};
		if (primitive == NativeDrawPrimitive::Triangles) for (unsigned i=0;i+2<count;i+=3) triangle(i,i+1,i+2);
		if (primitive == NativeDrawPrimitive::Quads) for (unsigned i=0;i+3<count;i+=4) { triangle(i,i+1,i+2); triangle(i,i+2,i+3); }
		if (primitive == NativeDrawPrimitive::TriangleFan) for (unsigned i=2;i<count;++i) triangle(0,i-1,i);
		if (primitive == NativeDrawPrimitive::TriangleStrip) for (unsigned i=2;i<count;++i) {
			if (i & 1) triangle(i-1,i-2,i); else triangle(i-2,i-1,i);
		}
		if (pending.indices.empty()) { pending = {}; return; }
		pending.vertexCount = count;
		Check(Available(), "PiP original program consumer is not paired");
		drawSink(replay,std::move(pending)); pending = {};
	}
	std::shared_ptr<const WorldModel> FeatureModel(int id,std::shared_ptr<const NativeS3OTexturePair>* textures=nullptr)
	{
		const auto* def = featureDefHandler ? featureDefHandler->GetFeatureDefByID(id) : nullptr;
		if (!def) return {};
		const auto* model = def->LoadModel(); if (!model) return {};
		Check(bool(modelProvider), "PiP FeatureShape requires the original immutable model provider");
		if(textures)*textures=CaptureNativeS3OModelTextures(model);
		return modelProvider(model);
	}
	std::shared_ptr<const ScreenTexture> FeatureTexture(std::shared_ptr<const WorldTexture> source)
	{
		Check(source && !source->mips.empty(), "Original PiP feature texture storage is missing");
		auto& entry = featureImages[source.get()];
		if (!entry.first.expired()) return entry.second;
		auto image = std::make_shared<ScreenTexture>(); image->sourceName = source->sourcePath;
		image->width = source->mips.front().width; image->height = source->mips.front().height;
		image->flipV = source->flipV; image->generateMipmaps = source->generateMipmaps;
		switch (source->format) {
			case WorldTextureFormat::BC1RGBA: image->format = ScreenTextureFormat::BC1RGBA; break;
			case WorldTextureFormat::BC2RGBA: image->format = ScreenTextureFormat::BC2RGBA; break;
			case WorldTextureFormat::BC3RGBA: image->format = ScreenTextureFormat::BC3RGBA; break;
			case WorldTextureFormat::BC4Unorm: image->format = ScreenTextureFormat::BC4Unorm; break;
			case WorldTextureFormat::BC5Unorm: image->format = ScreenTextureFormat::BC5Unorm; break;
			case WorldTextureFormat::BC7RGBA: image->format = ScreenTextureFormat::BC7RGBA; break;
			case WorldTextureFormat::BC7RGBASRGB: image->format = ScreenTextureFormat::BC7RGBASRGB; break;
			case WorldTextureFormat::RGBA8Unorm: image->format = ScreenTextureFormat::RGBA8Unorm; break;
			case WorldTextureFormat::BGRA8Unorm: image->format = ScreenTextureFormat::BGRA8Unorm; break;
			default: throw std::runtime_error("Original PiP feature compressed texture format needs its paired screen uploader");
		}
		for (const auto& mip: source->mips) image->mips.push_back({mip.width,mip.height,mip.bytesPerRow,mip.bytes});
		entry = {source,image}; return image;
	}
	Binding Resolve(lua_State* L, const std::string& name)
	{
		Binding result;
		if (name == "$heightmap" || name == "$shading" || name == "$info:los" || name == "$info:radar") {
			result.engine = name == "$heightmap" ? NativePiPEngineImage::Height : name == "$shading" ? NativePiPEngineImage::Shading :
				name == "$info:los" ? NativePiPEngineImage::GroundLOS : NativePiPEngineImage::Radar;
			result.sampler.filter = name == "$info:los" ? ScreenFilter::Linear : ScreenFilter::Nearest;
			result.sampler.magnificationFilter = ScreenFilter::Linear;
			if (name == "$info:los") result.sampler.mipFilter = ScreenMipFilter::Nearest;
			return result;
		}
		if (name == "$info") {
			const auto lookup = LuaNativeCUSTextures::ParseTexture(L,name);
			Check(lookup.parsed && lookup.object && std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(lookup.object->backing), "PiP $info requires its actual event-published source owner");
			const auto object = lookup.object;
			const auto current = [object] {
				auto& textures = OriginalNativeTextureContext();
				Check(textures.Registry().ContextGeneration() == object->contextGeneration, "PiP $info source context changed");
				auto actual = textures.Registry().Find(object->name);
				Check(actual && actual->identity == object->identity, "PiP $info source allocation was deleted or replaced");
				return actual;
			};
			result.sourceBind = [current](unsigned requested) {
				auto& textures = OriginalNativeTextureContext();
				const auto actual = textures.UI().LuaLookup(requested,[&] { return NativeTextureLookup{true,current(),false}; });
				if(actual < 2) GetRecorder().SyncObservedTextureBinding(actual);
				return actual;
			};
			result.sourceDrawImage = [generation=object->contextGeneration](unsigned unit) {
				auto& textures = OriginalNativeTextureContext();
				Check(textures.Registry().ContextGeneration() == generation, "PiP source sampler crossed its original context lifetime");
				const auto bound = textures.History().Binding(unit,NativeCUSTextureTarget::Image2D);
				// The original sampler reads the actual unit at this draw. In
				// normal/LOS mode PiP leaves unit 2 inherited and changes only
				// infoTexMul; another real writer may have rebound that unit.
				// Retain its current immutable object/version, not the old $info
				// selector. The original shader still executes its texture read.
				NativePiPSampledImage image;
				if (bound.object && std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(bound.object->backing)) {
					image.original = bound;
					image.sampler = NativeUITextureHistory::DescribeSampler(bound);
				} else {
					const auto observed = textures.UI().DescribeBinding(bound);
					Check(bool(observed.image), "PiP actual inherited sampler has no image descriptor");
					if (observed.image->incomplete) image.incomplete = true;
					else { image.image = observed.image; image.sampler = observed.sampler; }
				}
				return image;
			};
			return result;
		}
		if (name == "$units") { result.unitsAtlas = true; return result; }
		if (name == "$minimap" || name == "$icons" || name == "$icons0") {
			result.resource = std::make_shared<NativeUITextureBinding>();
			if (name == "$minimap") {
				Check(minimap != nullptr, "PiP minimap image requires the actual map state");
				result.resource->current = mapCapture.CapturePiP(minimap->GetNativeState())->base;
				result.resource->sampler.wrapS = result.resource->sampler.wrapT = ScreenWrap::Repeat;
			} else result.resource->current = icon::iconHandler.GetNativeAtlasTexture(0);
			Check(bool(result.resource->current), "PiP engine icon/map image is unavailable");
			result.resource->sampler.mipFilter = ScreenMipFilter::Linear;
			return result;
		}
		if (name.starts_with("%-") && name.ends_with(":0")) {
			const std::string number = name.substr(2,name.size()-4);
			Check(!number.empty() && std::all_of(number.begin(),number.end(),[](char c){return c >= '0' && c <= '9';}), "Invalid original feature texture reference");
			std::shared_ptr<const NativeS3OTexturePair> textures;
			const auto model = FeatureModel(std::stoi(number),&textures); if (!model) return {};
			
			if(textures){
    const auto object=textures->objects[0];const auto context=object->contextGeneration,name=object->name,identity=object->identity;
    // Persist the actual bound logical name, never reselect by feature ID.
    // Only an emitted draw retains the current immutable image/sampler.
    const auto describe=[](const std::shared_ptr<const NativeCUSTextureObject>& actual){
     const auto& object=*actual;const auto& p=object.sampling;
    Check(p.baseLevel==0&&p.maxLevel>=int(object.allocation.levels)-1&&p.minLod<=0&&p.maxLod>=float(object.allocation.levels-1)&&p.lodBias==0&&p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,
     "Original PiP S3O sampler requires its actual base/max/LOD consumer");
    Check((p.minFilter==0x2600||p.minFilter==0x2601||(p.minFilter>=0x2700&&p.minFilter<=0x2703))&&(p.magFilter==0x2600||p.magFilter==0x2601),"Original PiP S3O filter has no actual consumer");
    Check(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{1,1,1,1},"Original PiP S3O sampler requires its actual custom border consumer");
    Check(p.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original PiP S3O source swizzle requires its actual consumer");
    Check(p.wrapS!=0x2900&&p.wrapT!=0x2900,"Original PiP S3O GL_CLAMP requires its actual coordinate/gradient consumer");
    NativeCUSTextureBinding binding;binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=actual;
    auto image=OriginalNativeTextureContext().UI().DescribeBinding(binding);
    NativePiPSampledImage sampled;sampled.image=std::move(image.image);sampled.sampler=image.sampler;return sampled;
    
    };
    result.sourceDrawImage=[context,describe](unsigned actualUnit){
     auto& textures=OriginalNativeTextureContext();const auto current=textures.History().Binding(actualUnit,NativeCUSTextureTarget::Image2D);
     Check(current.knowledge==NativeCUSBindingKnowledge::BoundObject&&current.object,"Original PiP source sampler has no actual bound native object");
     return describe(OriginalNativeS3OTextures().CurrentObject(context,current.object->name,current.object->identity));
    };
    result.sourceBind=[context,name,identity,limit=bindings.size()](unsigned requested){
     auto& textures=OriginalNativeTextureContext();const auto active=textures.History().ActiveUnit();Check(bool(active),"Original PiP source bind has unobserved active texture unit");
     const auto actual=requested?requested:*active;Check(actual<limit,"Original PiP actual source bind unit exceeds private bindings");
     auto object=OriginalNativeS3OTextures().CurrentObject(context,name,identity);
     textures.Registry().LuaTexture(requested,[object]{return NativeTextureLookup{true,object,false};});
     GetRecorder().SyncObservedTextureBinding(actual);return actual;
    };
    return result;
   }
   result.resource=std::make_shared<NativeUITextureBinding>();
   result.resource->current = FeatureTexture(model->textures[0]);
			result.resource->sampler.wrapS = result.resource->sampler.wrapT = ScreenWrap::Repeat;
			result.resource->sampler.mipFilter = model->textures[0]->mipmappedSampling ? ScreenMipFilter::Linear : ScreenMipFilter::None;
			return result;
		}
		result.resource = LuaNativeUI::ResolveTextureBinding(L,name);
		return result;
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	LuaNativeGameDecal::PiPBuffer vertices, instances;
	bool deleted = false;
	static LuaNativeGameDecal::PiPBuffer BorrowBuffer(sol::stack_object value)
	{
		if (value.is<std::shared_ptr<Buffer>>()) {
			auto buffer = value.as<std::shared_ptr<Buffer>>();
			if (buffer) return {buffer,buffer->value};
		}
		// Original WG.decalsgl4.GetVBOData shares the producer's live VBO.
		// Only its exact native wrapper is accepted across this boundary.
		return LuaNativeGameDecal::BorrowPiPBuffer(value.lua_state(),value.stack_index());
	}
	void AttachVertexBuffer(sol::stack_object value)
	{
		Check(!deleted, "PiP VAO vertex attachment is deleted");
		auto buffer = BorrowBuffer(value);
		Check(buffer.keeper && buffer.value, "PiP VAO vertex attachment is not an original PiP or shared decal buffer");
		vertices = std::move(buffer);
	}
	void AttachInstanceBuffer(sol::stack_object value)
	{
		Check(!deleted, "PiP VAO instance attachment is deleted");
		auto buffer = BorrowBuffer(value);
		Check(buffer.keeper && buffer.value, "PiP VAO instance attachment is not an original PiP or shared decal buffer");
		instances = std::move(buffer);
	}
	void Delete() { deleted = true; vertices = {}; instances = {}; }
	void DrawArrays(GLenum mode, sol::optional<int> count, sol::optional<int> first, sol::optional<int> instanceCount, sol::optional<int> firstInstance)
	{
		Check(!deleted && vertices.keeper && vertices.value, "Original PiP VAO has no live vertex buffer");
		// Capture at each draw so original uploads/growth/deletion remain
		// visible; already submitted immutable snapshots keep their own bytes.
		const auto vertexData = vertices.value->CaptureNativeSnapshot();
		const auto instanceData = instances.value ? instances.value->CaptureNativeSnapshot() : nullptr;
		const int start = first.value_or(0), number = count.value_or(int(vertexData->layout.elements));
		const int repeats = instanceCount.value_or(0), instanceStart = firstInstance.value_or(0);
		Check(start >= 0 && number >= 0 && repeats >= 0 && instanceStart >= 0, "Original PiP VAO draw range is negative");
		Check(!GetRecorder().IsCompiling(),"Exact PiP VAO invocations occur outside display-list compilation");
		{
			auto& replay=GetRecorder();
			auto draw = context->Capture();
			const auto geometry = GetNativePiPProgramSpec(draw.program.program).geometry;
			Check((geometry == NativePiPGeometry::Line && mode == GL_LINES) ||
				(geometry == NativePiPGeometry::Instanced && mode == GL_TRIANGLE_STRIP), "PiP original VAO primitive does not match its exact NoGS program");
			draw.vertices = vertexData; draw.instances = instanceData; draw.firstVertex = start; draw.vertexCount = number;
			draw.instanceCount = repeats; draw.firstInstance = instanceStart;
			Check(Available(), "PiP original VAO consumer is not paired"); drawSink(replay,std::move(draw));
		}
	}
};

template<typename T> std::vector<T> Values(const sol::object& value)
{
	if (value.is<sol::table>()) {
		const auto table = value.as<sol::table>(); std::vector<T> result;
		for (std::size_t i=1; i<=table.size(); ++i) result.push_back(table.get<T>(i));
		return result;
	}
	return {value.as<T>()};
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && Available(), "PiP private API requires unsynced code and the complete paired consumer");
	sol::state_view lua(L); auto result = lua.create_table(); auto context = std::make_shared<Context>();
	const sol::table base = lua["gl"];
	const sol::protected_function baseBeginEnd = base["BeginEnd"], baseTexRect = base["TexRect"], baseCreateList = base["CreateList"];
	std::erase_if(contexts,[](const auto& value){return value.expired();}); contexts.push_back(context);
	BindFunction(result,"CreateShader",[](const sol::table& table) {
		auto program = std::make_shared<Program>(NativePiPProgramState(table.get<std::string>("vertex"),table.get<std::string>("fragment"),table.get_or("geometry",std::string{})));
		for (bool integers: {false,true}) {
			const auto uniforms = table.get<sol::optional<sol::table>>(integers ? "uniformInt" : "uniformFloat"); if (!uniforms) continue;
			for (const auto& value: *uniforms) {
				const int location = program->state.GetUniformLocation(value.first.as<std::string>());
				const bool valid = integers ? program->state.SetIntegers(location,Values<int>(value.second)) : program->state.SetFloats(location,Values<float>(value.second));
				Check(valid,"Original PiP initial uniform type/arity differs");
			}
		}
		return program;
	});
	BindFunction(result,"DeleteShader",[context](std::shared_ptr<Program> value){if(!value)return;value->deleted=true;if(context->active.lock()==value)context->active.reset();});
	BindFunction(result,"GetUniformLocation",[](std::shared_ptr<Program> program,const std::string& name){Check(program && !program->deleted,"PiP uniform query needs a live program");return program->state.GetUniformLocation(name);});
	BindFunction(result,"GetShaderLog",[](){return std::string{};});
	BindFunction(result,"UseShader",[context](sol::object value) {
		std::shared_ptr<Program> program;
		if (value.is<int>()) Check(value.as<int>()==0,"Unknown numeric PiP program"); else program=value.as<std::shared_ptr<Program>>();
		Check(!program || !program->deleted,"Deleted PiP program");
		Check(!GetRecorder().IsCompiling(),"Exact PiP source never changes its private program while compiling a display list");
		context->active=program;
	});
	BindFunction(result,"UniformFloat",[context](int location,sol::variadic_args args) {
		std::vector<float> values;for(auto value:args)values.push_back(value.as<float>());
		Check(values.size()>=1 && values.size()<=4,"Incorrect arguments to gl.Uniform()");
		if(location==-1)return;Check(!GetRecorder().IsCompiling(),"Exact PiP float uniforms are immediate program state");auto program=context->active.lock();
		if(!program || program->deleted || !program->state.SetFloats(location,values))context->InvalidUniformOperation();
	});
	BindFunction(result,"UniformInt",[context](int location,sol::variadic_args args) {
		std::vector<int> values;for(auto value:args)values.push_back(value.as<int>());
		Check(values.size()>=1 && values.size()<=4,"Incorrect arguments to gl.UniformInt()");
		if(location==-1)return;Check(!GetRecorder().IsCompiling(),"Exact PiP integer uniforms are immediate program state");auto program=context->active.lock();
		if(!program || program->deleted || !program->state.SetIntegers(location,values))context->InvalidUniformOperation();
	});
	BindFunction(result,"GetVBO",[L](sol::optional<GLenum> target,sol::optional<bool> frequent){return std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));});
	BindFunction(result,"GetVAO",[context](){auto vao=std::make_shared<VertexArray>();vao->context=context;return vao;});
	BindFunction(result,"BeginEnd",[context,baseBeginEnd](GLenum mode,sol::protected_function callback,sol::variadic_args args) {
		if(context->active.expired()){auto call=baseBeginEnd(mode,callback,sol::as_args(args));if(!call.valid())RethrowCallback(call);return;}
		Check(!GetRecorder().IsCompiling(),"Exact PiP program draws occur outside display-list compilation");
		context->Begin(GetRecorder(),Primitive(mode));
		auto call=callback(sol::as_args(args));
		if(!call.valid()){context->inPrimitive=false;context->pending={};GetRecorder().AbortPrimitive();RethrowCallback(call);}
		context->End(GetRecorder());
	});
	BindFunction(result,"Vertex",[context](sol::variadic_args args) {
		std::vector<float> values;
		if(args.size()==1)values=Values<float>(args[0]);else for(auto value:args)values.push_back(value.as<float>());
		Check(values.size()>=2 && values.size()<=4,"Original PiP vertex arity differs");std::array<float,4> v{values[0],values[1],0,1};for(std::size_t i=2;i<values.size();++i)v[i]=values[i];
		if(context->inPrimitive && context->typedPrimitive)context->Vertex(GetRecorder(),v);else GetRecorder().LegacyVertex(v[0],v[1],v[2],v[3]);
	});
	BindFunction(result,"TexRect",[context,baseTexRect](sol::variadic_args args) {
		if(context->active.expired()){auto call=baseTexRect(sol::as_args(args));if(!call.valid())RethrowCallback(call);return;}
		Check(!GetRecorder().IsCompiling(),"Exact PiP shader rectangles are immediate calls");
		Check(args.size()>=4,"Original PiP textured rectangle needs four coordinates");
		const float x1=args[0].as<float>(),y1=args[1].as<float>(),x2=args[2].as<float>(),y2=args[3].as<float>();
		// Match LuaOpenGL::TexRect, including Spring's default vertical flip.
		float s1=0,t1=1,s2=1,t2=0;
		if(args.size()>6){Check(args.size()>=8,"Original PiP UV rectangle needs four coordinates");s1=args[4].as<float>();t1=args[5].as<float>();s2=args[6].as<float>();t2=args[7].as<float>();}
		else{if(args.size()>4 && args[4].as<bool>())std::swap(s1,s2);if(args.size()>5 && args[5].as<bool>())std::swap(t1,t2);}
		auto& replay=GetRecorder();context->Begin(replay,NativeDrawPrimitive::Quads);replay.TexCoord(s1,t1);context->Vertex(replay,{x1,y1,0,1});replay.TexCoord(s2,t1);context->Vertex(replay,{x2,y1,0,1});replay.TexCoord(s2,t2);context->Vertex(replay,{x2,y2,0,1});replay.TexCoord(s1,t2);context->Vertex(replay,{x1,y2,0,1});context->End(replay);
	});
	BindFunction(result,"Texture",[L,context](sol::variadic_args args) {
		Check(args.size()>=1 && args.size()<=2,"Original PiP texture invocation differs");
		const unsigned unit=args.size()==2?args[0].as<unsigned>():0;Check(unit<context->bindings.size(),"Original PiP texture unit out of range");
		const sol::object value=args[args.size()-1];
		if(value.is<bool>()){
   const bool enabled=value.as<bool>();
   GetRecorder().RecordOperation([context,unit,enabled](NativeUIRecorder& replay){
    // Emit previews operations during GL_COMPILE. Only the outer command
    // belongs in the list; record no nested enable/mirror or shared event.
    if(replay.IsCompiling())return;
    auto& textures=OriginalNativeTextureContext();
    if(!textures.Initialized()){if(unit<2)replay.SetTextureEnabled(enabled,unit);return;}
    const auto active=textures.History().ActiveUnit();Check(bool(active),"Original PiP texture enable has unobserved active source unit");
    const unsigned actual=unit?unit:*active;
    if(actual<context->bindings.size()&&context->bindings[actual].sourceBind)replay.LuaTextureEnabled(enabled,unit);
    else if(unit<2)replay.SetTextureEnabled(enabled,unit);
   });return true;
  }
		const auto bound=context->Resolve(L,value.as<std::string>());
		if(!bound.resource && !bound.sourceBind && bound.engine==NativePiPEngineImage::None && !bound.unitsAtlas)return false;
		GetRecorder().RecordOperation([context,unit,bound](NativeUIRecorder& replay){
   // Compile only the private binding preview. At replay, sourceBind uses
   // the then-current actual active unit and emits the one shared event.
   if(replay.IsCompiling()){context->bindings[unit]=bound;return;}
   const unsigned actual=bound.sourceBind?bound.sourceBind(unit):unit;
   Check(actual<context->bindings.size(),"Original PiP actual texture unit exceeds private source bindings");context->bindings[actual]=bound;
   if(actual<2&&bound.resource)replay.SetTextureResource(bound.resource,actual);
  });return true;
	});
	BindFunction(result,"TextureInfo",[L,context](const std::string& name)->sol::object {
		if(OriginalNativeS3OTextures().Attached()&&name.starts_with("%-")&&name.ends_with(":0")){
			// Original TextureInfo queries S3OTexMat metadata through GetSize.
			// Reload changes storage, not those original material dimensions.
			LuaMatTexture original;
			if(!LuaOpenGLUtils::ParseTextureImage(L,original,name))return sol::make_object(sol::state_view(L),sol::lua_nil);
			const auto dimensions=original.GetSize();
			auto info=sol::state_view(L).create_table();info.set("xsize",std::get<0>(dimensions),"ysize",std::get<1>(dimensions),"target",original.GetTextureTarget());return sol::make_object(sol::state_view(L),info);
		}
		if(name=="$info") {
			const auto lookup=LuaNativeCUSTextures::ParseTexture(L,name);
			if(!lookup.parsed || !lookup.object)return sol::make_object(sol::state_view(L),sol::lua_nil);
			const auto& a=lookup.object->allocation;auto info=sol::state_view(L).create_table();
			info.set("xsize",a.width,"ysize",a.height,"target",GL_TEXTURE_2D);return sol::make_object(sol::state_view(L),info);
		}
		const auto binding=context->Resolve(L,name);
  std::shared_ptr<const ScreenTexture> image;
  if(binding.resource)image=binding.resource->Resolve().image;
  if(!image)return sol::make_object(sol::state_view(L),sol::lua_nil);
		auto info=sol::state_view(L).create_table();info.set("xsize",image->width,"ysize",image->height,"target",GL_TEXTURE_2D);return sol::make_object(sol::state_view(L),info);
	});
	BindFunction(result,"FeatureShape",[context](int defID,int,sol::optional<bool> raw,sol::optional<bool> screen,sol::optional<bool> opaque) {
		Check(raw.value_or(true) && !screen.value_or(false) && opaque.value_or(true),"PiP FeatureShape invocation differs from exact original raw draw");
		const auto model=context->FeatureModel(defID);if(!model)return;
		GetRecorder().RecordOperation([model](NativeUIRecorder& replay){if(replay.IsCompiling())return;for(const auto& piece:model->pieces){if(piece.indices.empty())continue;replay.PushMatrix();replay.MultMatrix(piece.bindPose);replay.Begin(ScreenPrimitive::Triangles,"original PiP raw FeatureShape bind pose");for(auto index:piece.indices){Check(index<piece.vertices.size(),"Original feature piece index exceeds geometry");const auto& v=piece.vertices[index];replay.Normal(v.normal[0],v.normal[1],v.normal[2]);replay.TexCoord(v.uv[0][0],v.uv[0][1]);replay.MultiTexCoord(1,v.uv[1][0],v.uv[1][1]);replay.Vertex(v.position[0],v.position[1],v.position[2]);}replay.End();replay.PopMatrix();}});
	});
	BindFunction(result,"RunMiniMapCallback",[](sol::protected_function callback,sol::variadic_args args) {
		GetRecorder().RunPiPMiniMapCallback([&] {
			auto call=callback(sol::as_args(args));
			if(!call.valid())RethrowCallback(call);
		});
	});
	BindFunction(result,"CreateList",[context,baseCreateList](sol::protected_function callback,sol::variadic_args args)->sol::object {
		const auto saved=context->bindings;
		auto call=baseCreateList(callback,sol::as_args(args));context->bindings=saved;
		if(!call.valid())RethrowCallback(call);return call.get<sol::object>();
	});
	return result;
}
} // namespace
void SetDrawSink(DrawSink value,bool available){drawSink=std::move(value);consumerReady=available;}
void SetModelProvider(ModelProvider value){modelProvider=std::move(value);}
bool Available(){return consumerReady && bool(drawSink) && bool(modelProvider);}
void Reset(){for(const auto& weak:contexts)if(auto context=weak.lock()){context->active.reset();context->bindings={};context->mapCapture.Reset();context->featureImages.clear();context->inPrimitive=false;context->pending={};context->error=0;context->invalidUniformOperations=0;}}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	family.new_usertype<Program>("Program",sol::no_constructor);
	family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize);
	family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawArrays",&VertexArray::DrawArrays,"Delete",&VertexArray::Delete);
	family.set("Program",sol::lua_nil,"Buffer",sol::lua_nil,"VertexArray",sol::lua_nil);
	family.set_function("Available",&Available);BindFunction(family,"CreateContext",[L](){return CreateContext(L);});gl.set("NativePiP",family);return true;
}
} // namespace LuaNativePiP
#else
namespace LuaNativePiP {
void SetDrawSink(DrawSink,bool){} void SetModelProvider(ModelProvider){} bool Available(){return false;} bool PushEntries(lua_State*){return true;} void Reset(){}
}
#endif
