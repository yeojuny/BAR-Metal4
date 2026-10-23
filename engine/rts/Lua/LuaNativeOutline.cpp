/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeOutline.h"

#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaVBOImpl.h"
#include "Lua/LuaNativeCUSTextures.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeOutlinePrograms.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace LuaNativeOutline {
namespace {
using namespace rendering;
Services services;
std::shared_ptr<NativeOutlineFrame> frame;
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
template<typename Signature> struct Boundary;
template<typename R, typename... Args> struct Boundary<R(Args...)> {
	template<typename F> static auto Wrap(F function)
	{
		return [function = std::move(function)](sol::this_state state, Args... args) -> R {
			try { return function(std::forward<Args>(args)...); }
			catch (const std::exception& error) { lua_pushstring(state, error.what()); }
			lua_error(state);
			std::terminate();
		};
	}
};
template<typename T, typename F> void Bind(T& table, const char* name, F function)
{
	table.set_function(name, Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
std::string Hash(std::string_view text)
{
	sha512::raw_digest digest;
	sha512::calc_digest(reinterpret_cast<const uint8_t*>(text.data()), text.size(), digest.data());
	return sha512::dump_digest(digest);
}
uint64_t TemplateHash(std::string_view text)
{
	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char character: text) hash = (hash ^ character) * 1099511628211ULL;
	return hash;
}
std::string Lower(std::string text)
{
	for (char& character: text) character = char(std::tolower(static_cast<unsigned char>(character)));
	return text;
}
std::map<std::string, std::vector<float>> UniformTable(const sol::table& table, bool integer)
{
	std::map<std::string, std::vector<float>> result;
	for (const auto& entry: table) {
		std::vector<float> values;
		if (entry.second.is<sol::table>()) {
			const auto data = entry.second.as<sol::table>();
			for (size_t i = 1; i <= data.size(); ++i) values.push_back(data.get<float>(i));
		} else values.push_back(entry.second.as<float>());
		for (float value: values)
			Check(std::isfinite(value) && (!integer || value == std::trunc(value)), "Invalid original outline initial uniform scalar");
		result.emplace(entry.first.as<std::string>(), std::move(values));
	}
	return result;
}
struct Context;
struct Program {
	std::weak_ptr<Context> context;
	std::shared_ptr<const NativeOutlineProgramConsumer> consumer;
	std::map<std::string, std::vector<float>> floats, ints;
	std::string label;
	bool deleted = false;
};
struct Buffer {
	std::weak_ptr<Context> context;
	std::shared_ptr<LuaVBOImpl> value;
	unsigned target = 0;
	bool deleted = false;
	void Live() const { Check(!deleted && bool(value), "Original outline VBO was deleted"); }
	void Delete() { if (!deleted) value->Delete(); deleted = true; }
};
struct Location { std::shared_ptr<Program> program; NativeCUSUniformDescription description; };
struct Context {
	uint64_t textureContext = 0;
	std::vector<std::shared_ptr<Program>> programs;
	std::vector<Location> locations;
	std::vector<std::weak_ptr<Buffer>> buffers;
	NativeTextureContext& Actual() const
	{
		auto& actual = OriginalNativeTextureContext();
		Check(actual.Initialized() && actual.UIAttached() && actual.Registry().ContextGeneration() == textureContext,
			"Outline context crossed its actual native texture lifetime");
		return actual;
	}
	std::shared_ptr<Program> Get(unsigned handle) const
	{
		Check(handle && handle <= programs.size() && !programs[handle - 1]->deleted,
			"Unknown or deleted original outline program handle");
		return programs[handle - 1];
	}
	std::shared_ptr<Program> Active() const
	{
		const auto selected = OriginalNativeSourceProgramBinding().Current();
		if (selected.kind != NativeSourceProgramBinding::Kind::Outline || !selected.owner) return {};
		auto program = std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(selected.owner));
		if (program->context.lock().get() != this || program->deleted) return {};
		return program;
	}
	unsigned GetLocation(unsigned handle, const std::string& name)
	{
		auto program = Get(handle);
		for (unsigned i = 0; i < locations.size(); ++i)
			if (locations[i].program == program && locations[i].description.name == name) return i + 1;
		for (const auto& uniform: program->consumer->uniforms) {
			if (uniform.name == name) {
				locations.push_back({program, uniform});
				return unsigned(locations.size());
			}
		}
		return 0; // Genuine source compiler reflection reported this uniform inactive.
	}
	void Uniform(unsigned handle, const std::string& name, const std::string& kind, sol::variadic_args input)
	{
		auto program = Get(handle);
		Check(Active() == program && OriginalNativeSourceProgramBinding().IsOutline(program),
			"Outline uniform does not own the actual current source program");
		Check(kind == "int" || kind == "float", "Selected original outline program has no matrix uniform setters");
		const bool integer = kind == "int";
		const auto location = GetLocation(handle, name);
		if (!location) return; // Original inactive location writes have no effect.
		Check(locations[location - 1].description.integer == integer, "Outline uniform scalar type differs from compiled reflection");
		std::vector<float> values;
		if (input.size() == 1 && input[0].is<sol::table>()) {
			const auto table = input[0].as<sol::table>();
			for (size_t i = 1; i <= table.size(); ++i) values.push_back(table.get<float>(i));
		} else for (auto item: input) values.push_back(item.as<float>());
		Check(!values.empty() && values.size() <= 4, "Outline scalar/vector uniform arity differs");
		for (float value: values)
			Check(std::isfinite(value) && (!integer || value == std::trunc(value)), "Invalid original outline uniform scalar");
		(integer ? program->ints : program->floats).insert_or_assign(name, std::move(values));
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices, indices, instances;
	bool deleted = false;
	void Attach(std::shared_ptr<Buffer>& destination, std::shared_ptr<Buffer> buffer, unsigned target)
	{
		Check(!deleted && buffer && !buffer->deleted && buffer->target == target && buffer->context.lock() == context,
			"Outline VAO attachment has a different original target or context");
		destination = std::move(buffer);
	}
	void Delete() { deleted = true; vertices.reset(); indices.reset(); instances.reset(); }
	void Draw(unsigned mode, sol::optional<int> count, sol::optional<int> first,
		sol::optional<int> instanceCount, sol::optional<int> firstInstance)
	{
		Check(Available() && frame && !deleted && vertices && instances,
			"Outline draw lacks its original frame or actual VAO attachments");
		// The original fillVAOSlots attaches an unused placeholder index VBO.
		// DrawArrays retains that VAO owner and never reads its index attachment.
		vertices->Live(); instances->Live();
		auto program = context->Active();
		Check(program && OriginalNativeSourceProgramBinding().IsOutline(program),
			"Outline draw has no actual active original source program");
		auto& recorder = ui::GetRecorder();
		const auto stage = recorder.CurrentWorldStage();
		Check(stage && *stage == NativeWorldDrawStage::World, "Outline capture must run at original widget DrawWorld");
		NativeOutlineDraw draw;
		draw.vertices = vertices->value->CaptureNativeSnapshot();
		draw.instances = instances->value->CaptureNativeSnapshot();
		draw.mode = mode; draw.count = uint32_t(std::max(count.value_or(4), 0));
		draw.firstVertex = uint32_t(std::max(first.value_or(0), 0));
		draw.instanceCount = uint32_t(std::max(instanceCount.value_or(0), 0));
		draw.firstInstance = uint32_t(std::max(firstInstance.value_or(0), 0));
		draw.program = program->consumer;
		draw.floatUniforms = program->floats; draw.intUniforms = program->ints;
		auto& actual = context->Actual();
		for (unsigned i = 0; i < draw.textures.size(); ++i)
			draw.textures[i] = actual.History().Binding(i, NativeCUSTextureTarget::Image2D);
		draw.engineUniforms = OriginalCUSUniformCapture().Current();
		draw.transforms = OriginalCUSModelPublication().Transforms();
		draw.modelUniforms = OriginalCUSModelPublication().ModelUniforms();
		draw.surface = recorder.CurrentState().draw; draw.world = recorder.CurrentWorldState();
		draw.sequence = NextNativeWorldDrawSequence();
		ValidateNativeOutlineDraw(draw);
		frame->operations.emplace_back(std::move(draw));
	}
};
sol::table CreateContext(lua_State* state)
{
	Check(!CLuaHandle::GetHandleSynced(state) && Available(), "Outlines require genuine unsynced original source services");
	sol::state_view lua(state);
	auto table = lua.create_table();
	auto context = std::make_shared<Context>();
	context->textureContext = OriginalNativeTextureContext().Registry().ContextGeneration();
	Bind(table, "GetVBO", [state, context](sol::optional<unsigned> target, sol::optional<bool> frequent) {
		context->Actual();
		auto buffer = std::make_shared<Buffer>();
		buffer->target = target.value_or(GL_ARRAY_BUFFER);
		Check(buffer->target == GL_ARRAY_BUFFER || buffer->target == GL_ELEMENT_ARRAY_BUFFER,
			"Selected outline source requested an unsupported original VBO target");
		buffer->context = context;
		buffer->value = std::make_shared<LuaVBOImpl>(buffer->target, frequent.value_or(false));
		CLuaHandle::GetActiveVBOs(state).luaVBOs.emplace_back(buffer->value);
		context->buffers.emplace_back(buffer);
		return buffer;
	});
	Bind(table, "GetVAO", [context]() {
		context->Actual();
		auto array = std::make_shared<VertexArray>(); array->context = context; return array;
	});
	Bind(table, "CreateProgram", [context](const std::string& name, const std::string& vertex, const std::string& fragment,
		const sol::table& defines, const sol::table& floats, const sol::table& integers, sol::optional<sol::table> metadata) {
		context->Actual();
		Check(Available() && metadata, "Outline program has no actual native compiler or original helper assembly");
		Check(TemplateHash(vertex) == outlineProgram::VertexHash && TemplateHash(fragment) == outlineProgram::FragmentHash,
			"Original selected outline GLSL source identity differs");
		NativeOutlineProgramSource source;
		source.name = name; source.vertex = vertex; source.fragment = fragment;
		for (const auto& entry: defines) {
			const auto key = entry.first.as<std::string>();
			if (entry.second.get_type() == sol::type::string) source.defines.emplace(key, entry.second.as<std::string>());
			else source.defines.emplace(key, entry.second.as<float>());
		}
		source.initialFloat = UniformTable(floats, false); source.initialInt = UniformTable(integers, true);
		source.assembledVertex = metadata->get<std::string>("assembledVS");
		source.assembledFragment = metadata->get<std::string>("assembledFS");
		source.engineDefinitions = metadata->get<std::string>("engineDefs");
		source.quaternionDefinitions = metadata->get<std::string>("quaternionDefs");
		source.definitions = metadata->get<std::string>("defines");
		const auto paths = metadata->get<sol::table>("paths");
		source.vertexPath = paths.get<std::string>("vertex"); source.fragmentPath = paths.get<std::string>("fragment");
		Check(Lower(source.vertexPath) == "luaui/widgets/gfx_outline_shader_gl4.lua#vssrc" &&
			Lower(source.fragmentPath) == "luaui/widgets/gfx_outline_shader_gl4.lua#fssrc",
			"Original outline source-cache paths differ");
		source.vertexSHA512 = Hash(vertex); source.fragmentSHA512 = Hash(fragment);
		ValidateNativeOutlineProgramSource(source);
		auto consumer = services.compile(source);
		Check(consumer && consumer->pipelineLifetime && consumer->vertexSHA512 == source.vertexSHA512 &&
			consumer->fragmentSHA512 == source.fragmentSHA512, "Outline original source has no matching compiled native program");
		auto program = std::make_shared<Program>(); program->context = context; program->consumer = std::move(consumer);
		program->floats = program->consumer->defaultFloat; program->ints = program->consumer->defaultInt;
		for (auto& [key, values]: source.initialFloat) program->floats.insert_or_assign(key, std::move(values));
		for (auto& [key, values]: source.initialInt) program->ints.insert_or_assign(key, std::move(values));
		context->programs.push_back(program);
		return unsigned(context->programs.size());
	});
	Bind(table, "Activate", [context](unsigned handle) { context->Actual(); OriginalNativeSourceProgramBinding().BindOutline(context->Get(handle)); });
	Bind(table, "Deactivate", [context](unsigned handle) {
		auto program = context->Get(handle);
		Check(OriginalNativeSourceProgramBinding().IsOutline(program), "Outline Deactivate does not own the actual current source program");
		OriginalNativeSourceProgramBinding().Unbind();
	});
	Bind(table, "Finalize", [context](unsigned handle) {
		auto program = context->Get(handle);
		if (OriginalNativeSourceProgramBinding().IsOutline(program)) OriginalNativeSourceProgramBinding().Unbind();
		program->deleted = true;
	});
	Bind(table, "GetProgramLog", [context](unsigned handle) { return context->Get(handle)->consumer->log; });
	Bind(table, "GetActiveUniforms", [state, context](unsigned handle) {
		sol::state_view lua(state); auto result = lua.create_table(); unsigned index = 1;
		for (const auto& uniform: context->Get(handle)->consumer->uniforms) {
			auto entry = lua.create_table(); entry["name"] = uniform.name; entry["type"] = uniform.glType; entry["size"] = uniform.size;
			result[index++] = entry;
		}
		return result;
	});
	Bind(table, "GetUniformLocation", [state, context](unsigned handle, const std::string& name) {
		const auto location = context->GetLocation(handle, name);
		return location ? sol::make_object(state, location) : sol::make_object(state, sol::lua_nil);
	});
	Bind(table, "Uniform", [context](unsigned handle, const std::string& name, const std::string& kind, sol::variadic_args input) {
		context->Uniform(handle, name, kind, input);
	});
	Bind(table, "UniformLocation", [context](unsigned handle, unsigned location, const std::string& kind, sol::variadic_args input) {
		Check(location && location <= context->locations.size() && context->locations[location - 1].program == context->Get(handle),
			"Outline private reflected location belongs to another compiled source program");
		context->Uniform(handle, context->locations[location - 1].description.name, kind, input);
	});
	Bind(table, "Texture", [state, context](sol::object first, sol::optional<sol::object> second) {
		unsigned unit = 0; auto value = first;
		if (second) {
			const auto number = first.as<double>();
			Check(std::isfinite(number) && number == std::trunc(number) && number >= 0 && number < NativeCUSTextureHistory::Units,
				"Original outline texture unit is out of range");
			unit = unsigned(number); value = *second;
		}
		auto& actual = context->Actual(); auto& recorder = ui::GetRecorder();
		if (value.is<bool>()) {
			const unsigned observed = actual.UI().LuaEnable(unit, value.as<bool>());
			recorder.SyncObservedTextureBinding(observed); return true;
		}
		const auto name = value.as<std::string>(); const auto key = Lower(name);
		NativeTextureLookup lookup;
		const std::map<std::string, std::pair<NativeGeometryTextureRole, unsigned>, std::less<>> geometry{
			{"$map_gbuffer_zvaltex", {NativeGeometryTextureRole::Map, 5}},
			{"$model_gbuffer_zvaltex", {NativeGeometryTextureRole::Models, 5}},
			{"$model_gbuffer_misctex", {NativeGeometryTextureRole::Models, 4}}
		};
		const unsigned observed = actual.UI().LuaLookup(unit, [&]() {
			if (const auto selected = geometry.find(key); selected != geometry.end()) {
				const auto logicalName = actual.Geometry(selected->second.first).Name(selected->second.second);
				lookup = {true, logicalName ? actual.Registry().Find(logicalName) : nullptr, false};
			} else lookup = LuaNativeCUSTextures::ParseTexture(state, name);
			return lookup;
		});
		recorder.SyncObservedTextureBinding(observed);
		return lookup.parsed;
	});
	Bind(table, "Clear", [context](unsigned mask, int value) {
		context->Actual();
		auto& recorder = ui::GetRecorder();
		const auto stage = recorder.CurrentWorldStage();
		Check(Available() && frame && stage && *stage == NativeWorldDrawStage::World && mask == GL_STENCIL_BUFFER_BIT,
			"Original Outline Clear requires its actual DrawWorld stencil-only invocation");
		// Mask zero updates the original persistent clear value without dispatching
		// a generic clear event. This family alone records the real clear once.
		recorder.ClearAttachments(0, {}, {}, value);
		NativeOutlineClear clear;
		clear.sequence = NextNativeWorldDrawSequence(); clear.stage = *stage;
		clear.mask = ScreenAttachmentStencil; clear.values = recorder.CurrentClearValues();
		const auto world = recorder.CurrentWorldState();
		clear.colorWrite = world.colorWrite; clear.depthWrite = world.depthWrite;
		clear.frontStencilWriteMask = world.frontStencil.writeMask;
		clear.scissor = recorder.CurrentState().draw.scissor;
		ValidateNativeOutlineClear(clear);
		frame->operations.emplace_back(std::move(clear));
	});
	sol::protected_function originalLabel = lua["gl"]["ObjectLabel"];
	Bind(table, "ObjectLabel", [context, originalLabel](unsigned kind, unsigned identity, const std::string& label) {
		if (kind == 0x82e2) { context->Get(identity)->label = label; return; }
		Check(kind == 0x82e0, "Selected outline annotation requested an unsupported object namespace");
		bool owned = false;
		for (const auto& weak: context->buffers)
			if (auto buffer = weak.lock(); buffer && !buffer->deleted && buffer->value->GetID() == identity) { owned = true; break; }
		Check(owned && originalLabel.valid(), "Outline buffer annotation lacks an actual original GL buffer owner");
		auto result = originalLabel(kind, identity, label);
		if (!result.valid()) { sol::error error = result; throw std::runtime_error(error.what()); }
	});
	return table;
}
const std::map<std::string, std::string, std::less<>>& SourceIdentities()
{
	static const std::map<std::string, std::string, std::less<>> identities{
		{"luaui/widgets/gfx_outline_shader_gl4.lua", "bacedc03d1ed762fb3f1ac7b0404ace65563ceb4ba3269927bb66573f86d75d83382d7bb4e2638876f7e4b4f2a9a359722609b9fd1137b78f40e7ce9c1539386"},
		{"modules/graphics/instancevbotable.lua", "bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256"},
		{"modules/graphics/luashader.lua", "81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436"},
		{"luaui/include/instancevbotable.lua", "560d78e197982d7aabf9f7f13723ed14737c1556db9c25aeb6eb6a37a10b6eb9d9887672e368b54a83b97c855fc134b25eef931a6ae859ada955fcc2ea9e9dfe"},
		{"luaui/include/luashader.lua", "182d1627951e914c5da9fa8495613e9148edbf378d4ade162b59fcbde678c6439c5805f754493e7d537921f40e4af093c93df1708748ed687da3828eb67d1e5c"},
	};
	return identities;
}
}
void Configure(Services value)
{
	Check(!frame, "Cannot replace outline compiler services during original source capture");
	services = std::move(value);
}
bool Available()
{
	auto& actual = OriginalNativeTextureContext();
	return services.ready && services.compile && services.ready() && actual.Initialized() && actual.UIAttached() &&
		actual.Registry().ContextGeneration() && ui::GetRecorder().TextureObserver() == &actual.UI();
}
void BeginWorldFrame()
{
	Check(!frame && globalRendering, "Nested or ownerless original outline DrawWorld capture");
	frame = std::make_shared<NativeOutlineFrame>();
	frame->sourceDrawFrame = uint64_t(globalRendering->drawFrame);
}
std::shared_ptr<const NativeOutlineFrame> EndWorldFrame()
{
	Check(bool(frame), "Original outline DrawWorld capture is not active");
	frame->exitWorldState = ui::GetRecorder().CurrentWorldState();
	return std::exchange(frame, {});
}
void AbortWorldFrame() noexcept { frame.reset(); }
void Reset() { AbortWorldFrame(); }
bool PushEntries(lua_State* state)
{
	sol::state_view lua(state); auto gl = sol::stack::get<sol::table>(state, -1); auto family = lua.create_table();
	auto buffer = family.new_usertype<Buffer>("Buffer", sol::no_constructor);
	Bind(buffer, "Define", [](Buffer& value, int count, sol::optional<sol::object> layout) { value.Live(); value.value->Define(count, layout); });
	Bind(buffer, "Upload", [](Buffer& value, const sol::stack_table& data, sol::optional<int> attribute, sol::optional<int> offset, sol::optional<int> first, sol::optional<int> last) {
		value.Live(); return value.value->Upload(data, attribute, offset, first, last);
	});
	Bind(buffer, "Download", [](Buffer& value, sol::optional<int> attribute, sol::optional<int> offset, sol::optional<int> count, sol::optional<bool> forceGPU) {
		value.Live(); return value.value->Download(attribute, offset, count, forceGPU);
	});
	Bind(buffer, "InstanceDataFromUnitIDs", [](Buffer& value, sol::stack_object ids, int attribute, sol::optional<int> offset) {
		value.Live(); Check(attribute == 2 || attribute == 5, "Original outline unit attachment attribute differs");
		if (ids.is<int>()) return value.value->InstanceDataFromUnitIDs(ids.as<int>(), attribute, offset);
		return value.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(), attribute, offset);
	});
	Bind(buffer, "Delete", [](Buffer& value) { value.Delete(); });
	Bind(buffer, "GetID", [](Buffer& value) { value.Live(); return value.value->GetID(); });
	Bind(buffer, "GetBufferSize", [](Buffer& value) { value.Live(); return value.value->GetBufferSize(); });
	Bind(buffer, "Clear", [](Buffer& value) { value.Live(); value.value->Clear(); });
	Bind(buffer, "DumpDefinition", [](Buffer& value) { value.Live(); value.value->DumpDefinition(); });
	auto array = family.new_usertype<VertexArray>("VertexArray", sol::no_constructor);
	Bind(array, "AttachVertexBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.vertices, std::move(buffer), GL_ARRAY_BUFFER); });
	Bind(array, "AttachIndexBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.indices, std::move(buffer), GL_ELEMENT_ARRAY_BUFFER); });
	Bind(array, "AttachInstanceBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.instances, std::move(buffer), GL_ARRAY_BUFFER); });
	Bind(array, "Delete", [](VertexArray& value) { value.Delete(); });
	Bind(array, "DrawArrays", [](VertexArray& value, unsigned mode, sol::optional<int> count, sol::optional<int> first, sol::optional<int> instances, sol::optional<int> firstInstance) {
		value.Draw(mode, count, first, instances, firstInstance);
	});
	family.set("Buffer", sol::lua_nil, "VertexArray", sol::lua_nil);
	Bind(family, "Available", &Available);
	Bind(family, "CreateContext", [state]() { return CreateContext(state); });
	Bind(family, "LoadArchiveSource", [state](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(state), "Original outline archive source is unsynced only");
		const auto identity = SourceIdentities().find(Lower(path));
		Check(identity != SourceIdentities().end(), "Unknown selected original outline archive source");
		CFileHandler file(path, SPRING_VFS_ZIP); std::string source;
		Check(file.FileExists() && file.LoadStringData(source), "Selected original outline archive source is missing");
		Check(Hash(source) == identity->second, "Selected original outline archive source identity changed");
		return source;
	});
	Bind(family, "PrepareSource", [](std::string source) {
		const auto hash = Hash(source);
		Check(std::any_of(SourceIdentities().begin(), SourceIdentities().end(), [&hash](const auto& item) { return item.second == hash; }),
			"Original outline Lua source identity differs before private adaptation");
		return source;
	});
	gl.set("NativeOutline", family); return true;
}
} // namespace LuaNativeOutline
#else
namespace LuaNativeOutline {
void Configure(Services) {}
bool Available() { return false; }
void BeginWorldFrame() {}
std::shared_ptr<const rendering::NativeOutlineFrame> EndWorldFrame() { return {}; }
void AbortWorldFrame() noexcept {}
void Reset() {}
bool PushEntries(lua_State*) { return false; }
}
#endif
