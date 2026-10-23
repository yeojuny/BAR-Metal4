/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// CPU fixture links the existing Recoil Lua 5.1 library. No new Lua dependency.
#include "Rendering/Backend/ScreenDrawList.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// The pinned LuaUser.h has ENABLE_USERSTATE_LOCKS=0; its corresponding engine
// lock functions have empty bodies. Supply that same single-threaded behavior
// without pulling the LuaUser.cpp object and engine allocator/handle graph into
// this CPU test. Every unsupported graphics operation below still errors.
void LuaCreateMutex(lua_State*) {}
void LuaDestroyMutex(lua_State*) {}
void LuaLinkMutex(lua_State*, lua_State*) {}
void LuaMutexLock(lua_State*) {}
void LuaMutexUnlock(lua_State*) {}

// These engine services are not used by this fixture. Fail if code starts to
// depend on them; do not substitute different RNG or number formatting behavior.
int spring_lua_unsynced_rand(lua_State* state) { return luaL_error(state, "Fixture excludes engine RNG"); }
int spring_lua_unsynced_srand(lua_State* state) { return luaL_error(state, "Fixture excludes engine RNG"); }
void spring_lua_ftoa(float, char*, int) { throw std::runtime_error("Fixture excludes engine float formatting"); }
void spring_lua_format(float, const char*, char*) { throw std::runtime_error("Fixture excludes engine float formatting"); }

namespace {
using namespace rendering;

struct Snapshot {
	std::string label;
	std::vector<ScreenVertex> vertices;
	std::vector<std::uint32_t> indices;
	std::vector<ScreenDrawCommand> commands;
	ScreenColor finalColor;
	ScreenDrawState finalState;
	bool recording;
};

struct Environment {
	ScreenDrawList list;
	std::vector<Snapshot> snapshots;
	std::vector<std::string> errors;
	std::uint32_t completedBatches = 0;
};

Environment& Env(lua_State* state)
{
	return *static_cast<Environment*>(lua_touserdata(state, lua_upvalueindex(1)));
}

template<typename F>
int Guard(lua_State* state, F&& function)
{
	char message[512];
	try {
		return function();
	} catch (const std::exception& error) {
		std::snprintf(message, sizeof(message), "%s", error.what());
		Env(state).errors.emplace_back(message);
	}
	// Exit the C++ catch scope before Lua's error recovery transfer.
	return luaL_error(state, "%s", message);
}

int Unsupported(lua_State* state)
{
	const char* name = luaL_checkstring(state, 2);
	char message[256];
	std::snprintf(message, sizeof(message), "Unsupported fixture operation: %s", name);
	Env(state).errors.emplace_back(message);
	return luaL_error(state, "%s", message);
}

int Color(lua_State* state)
{
	return Guard(state, [&] {
		const int count = lua_gettop(state);
		ScreenColor value{0, 0, 0, 1};
		if (count == 1 && lua_istable(state, 1)) {
			for (int index = 1; index <= 4; ++index) {
				lua_rawgeti(state, 1, index);
				value[index - 1] = index == 4 ? luaL_optnumber(state, -1, 1) : luaL_checknumber(state, -1);
				lua_pop(state, 1);
			}
		} else if (count == 3 || count == 4) {
			for (int index = 1; index <= 3; ++index)
				value[index - 1] = luaL_checknumber(state, index);
			value[3] = luaL_optnumber(state, 4, 1);
		} else {
			throw std::invalid_argument("Fixture gl.Color accepts RGB/RGBA arguments or table");
		}
		Env(state).list.Color(value);
		return 0;
	});
}

int Vertex(lua_State* state)
{
	return Guard(state, [&] {
		if (lua_gettop(state) < 2 || lua_gettop(state) > 3)
			throw std::invalid_argument("Fixture gl.Vertex accepts x,y[,z]");
		Env(state).list.Vertex(luaL_checknumber(state, 1), luaL_checknumber(state, 2), luaL_optnumber(state, 3, 0));
		return 0;
	});
}

int BeginEnd(lua_State* state)
{
	return Guard(state, [&] {
		const int count = lua_gettop(state);
		if (count < 2 || !lua_isfunction(state, 2))
			throw std::invalid_argument("Fixture gl.BeginEnd requires type, callback, arguments");
		if (luaL_checkinteger(state, 1) != 7)
			throw std::invalid_argument("Fixture gl.BeginEnd supports QUADS only");
		auto& environment = Env(state);
		environment.list.BeginQuads();
		lua_pushvalue(state, 2);
		for (int index = 3; index <= count; ++index)
			lua_pushvalue(state, index);
		if (lua_pcall(state, count - 2, 0, 0) != 0) {
			environment.list.Abort();
			return lua_error(state);
		}
		try {
			environment.list.End();
		} catch (...) {
			environment.list.Abort();
			throw;
		}
		++environment.completedBatches;
		return 0;
	});
}

ScreenBlendFactor Factor(lua_State* state, int index)
{
	switch (luaL_checkinteger(state, index)) {
		case 0: return ScreenBlendFactor::Zero;
		case 1: return ScreenBlendFactor::One;
		case 0x0302: return ScreenBlendFactor::SourceAlpha;
		case 0x0303: return ScreenBlendFactor::OneMinusSourceAlpha;
		default: throw std::invalid_argument("Fixture unsupported blend factor");
	}
}

int Blending(lua_State* state)
{
	return Guard(state, [&] {
		auto value = Env(state).list.CurrentState().blend;
		if (lua_gettop(state) == 1 && lua_isboolean(state, 1)) {
			value.enabled = lua_toboolean(state, 1);
		} else if (lua_gettop(state) == 2) {
			value.enabled = true;
			value.sourceColor = value.sourceAlpha = Factor(state, 1);
			value.destinationColor = value.destinationAlpha = Factor(state, 2);
		} else {
			throw std::invalid_argument("Fixture gl.Blending accepts bool or two supported factors");
		}
		Env(state).list.SetBlend(value);
		return 0;
	});
}

int Scissor(lua_State* state)
{
	return Guard(state, [&] {
		if (lua_gettop(state) == 1 && lua_isboolean(state, 1) && !lua_toboolean(state, 1)) {
			Env(state).list.SetScissor(std::nullopt);
		} else if (lua_gettop(state) == 4) {
			std::array<lua_Integer, 4> values;
			for (int index = 1; index <= 4; ++index) {
				values[index - 1] = luaL_checkinteger(state, index);
				if (values[index - 1] < std::numeric_limits<std::int32_t>::min() || values[index - 1] > std::numeric_limits<std::int32_t>::max())
					throw std::invalid_argument("Fixture scissor exceeds int32 range");
			}
			if (values[2] < 0 || values[3] < 0)
				throw std::invalid_argument("Fixture scissor dimensions must be nonnegative");
			Env(state).list.SetScissor(ScreenScissor{static_cast<std::int32_t>(values[0]), static_cast<std::int32_t>(values[1]), static_cast<std::uint32_t>(values[2]), static_cast<std::uint32_t>(values[3])});
		} else {
			throw std::invalid_argument("Fixture gl.Scissor accepts false or x,y,width,height; viewport offset is zero");
		}
		return 0;
	});
}

int GetConfigFloat(lua_State* state)
{
	luaL_checkstring(state, 1);
	lua_pushnumber(state, luaL_checknumber(state, 2));
	return 1;
}

int GetViewGeometry(lua_State* state)
{
	lua_pushnumber(state, 1920);
	lua_pushnumber(state, 1080);
	return 2;
}

int CaptureSnapshot(lua_State* state)
{
	return Guard(state, [&] {
		auto& environment = Env(state);
		auto& list = environment.list;
		environment.snapshots.push_back({luaL_checkstring(state, 1), {list.Vertices().begin(), list.Vertices().end()}, {list.Indices().begin(), list.Indices().end()}, {list.Commands().begin(), list.Commands().end()}, list.CurrentColor(), list.CurrentState(), list.IsRecording()});
		list.ClearGeometry();
		return 0;
	});
}

void SetFunction(lua_State* state, const char* name, lua_CFunction function, Environment& environment)
{
	lua_pushlightuserdata(state, &environment);
	lua_pushcclosure(state, function, 1);
	lua_setfield(state, -2, name);
}

void RejectMissing(lua_State* state, Environment& environment)
{
	lua_newtable(state);
	SetFunction(state, "__index", Unsupported, environment);
	lua_setmetatable(state, -2);
}

void Initialize(lua_State* state, Environment& environment)
{
	luaopen_base(state);
	lua_settop(state, 0);
	luaopen_math(state);
	lua_settop(state, 0);
	lua_newtable(state);
	lua_setglobal(state, "WG");
	lua_newtable(state);
	lua_setglobal(state, "widget");
	lua_newtable(state);
	SetFunction(state, "GetConfigFloat", GetConfigFloat, environment);
	SetFunction(state, "GetViewGeometry", GetViewGeometry, environment);
	RejectMissing(state, environment);
	lua_setglobal(state, "Spring");
	lua_newtable(state);
	SetFunction(state, "Color", Color, environment);
	SetFunction(state, "Vertex", Vertex, environment);
	SetFunction(state, "BeginEnd", BeginEnd, environment);
	SetFunction(state, "Blending", Blending, environment);
	SetFunction(state, "Scissor", Scissor, environment);
	RejectMissing(state, environment);
	lua_setglobal(state, "gl");
	lua_newtable(state);
	for (const auto& [name, value]: std::array<std::pair<const char*, int>, 6>{{{"QUADS", 7}, {"TRIANGLE_FAN", 6}, {"ZERO", 0}, {"ONE", 1}, {"SRC_ALPHA", 0x0302}, {"ONE_MINUS_SRC_ALPHA", 0x0303}}}) {
		lua_pushnumber(state, value);
		lua_setfield(state, -2, name);
	}
	RejectMissing(state, environment);
	lua_setglobal(state, "GL");
	lua_newtable(state);
	SetFunction(state, "Snapshot", CaptureSnapshot, environment);
	RejectMissing(state, environment);
	lua_setglobal(state, "Fixture");
}

void RunFile(lua_State* state, const char* path)
{
	if (luaL_loadfile(state, path) != 0 || lua_pcall(state, 0, 0, 0) != 0)
		throw std::runtime_error(lua_tostring(state, -1));
}

int checks = 0;
void Check(bool condition, const char* label)
{
	++checks;
	if (!condition)
		throw std::runtime_error(std::string("Fixture assertion: ") + label);
}

void Near(float actual, float expected, const char* label)
{
	Check(std::abs(actual - expected) < 2e-5f, label);
}

float GeometryArea(const Snapshot& value)
{
	float result = 0;
	for (std::size_t index = 0; index < value.indices.size(); index += 3) {
		const auto& a = value.vertices[value.indices[index]];
		const auto& b = value.vertices[value.indices[index + 1]];
		const auto& c = value.vertices[value.indices[index + 2]];
		result += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
	}
	return result;
}

void Validate(const Environment& environment)
{
	Check(environment.snapshots.size() == 7, "all real source fixture cases executed");
	Check(environment.completedBatches == 5, "valid source emits five BeginEnd batches");
	const auto& cut = environment.snapshots[0];
	Check(cut.label == "cut_corners" && cut.vertices.size() == 28 && cut.indices.size() == 42 && cut.commands.size() == 1, "RectRound emits seven original quad groups");
	Near(GeometryArea(cut), 5800, "cut corner area matches four half-square exclusions");
	Check(cut.vertices[12].x == 20 && cut.vertices[12].y == 20 && cut.vertices[13].x == 20, "cut corner retains duplicated vertex");
	for (const auto& vertex: cut.vertices) {
		Near(vertex.color[0], 0.2f, "solid red");
		Near(vertex.color[3], 0.37f, "continuous alpha retained");
	}
	for (std::size_t quad = 0; quad < 7; ++quad) {
		const std::array<std::uint32_t, 6> local{0, 1, 2, 0, 2, 3};
		for (std::size_t index = 0; index < local.size(); ++index)
			Check(cut.indices[quad * 6 + index] == quad * 4 + local[index], "all quad groups keep source order and diagonal");
	}
	const auto& square = environment.snapshots[1];
	Near(GeometryArea(square), 6000, "zero corner flags restore square corners");
	Check(square.vertices[12].x == 10 && square.vertices[16].x == 110 && square.vertices[20].x == 10 && square.vertices[24].x == 110, "corner flags preserve actual original vertices");
	const auto& gradient = environment.snapshots[2];
	Near(gradient.vertices[0].color[0], 1, "actual c1 at bottom");
	Near(gradient.vertices[0].color[3], 0.2f, "actual c1 bottom alpha");
	Near(gradient.vertices[2].color[2], 1, "actual c2 at top");
	Near(gradient.vertices[2].color[3], 0.8f, "actual c2 top alpha");
	Near(gradient.vertices[4].color[0], 5.0f / 6.0f, "edge red color");
	Near(gradient.vertices[4].color[2], 1.0f / 6.0f, "edge blue color");
	Near(gradient.vertices[4].color[3], 0.3f, "edge alpha interpolates");
	Near(gradient.vertices[6].color[0], 1.0f / 6.0f, "mid red color");
	Near(gradient.vertices[6].color[2], 5.0f / 6.0f, "mid blue color");
	Near(gradient.vertices[6].color[3], 0.7f, "mid alpha interpolates");
	Check(gradient.commands[0].state.blend.destinationColor == ScreenBlendFactor::One, "additive blend snapshot");
	Check(gradient.commands[0].state.scissor == ScreenScissor{20, 25, 50, 40}, "bottom-left scissor snapshot");
	const auto& inherited = environment.snapshots[3];
	Check(inherited.vertices.size() == 28 && inherited.commands[0].state == gradient.finalState, "next source call inherits blend and scissor");
	for (const auto& vertex: inherited.vertices)
		Check(vertex.color == gradient.finalColor, "next source call inherits final per-vertex color");
	Near(gradient.finalColor[0], 1.0f / 6.0f, "final gradient color leaks red");
	Near(gradient.finalColor[3], 0.7f, "final gradient color leaks alpha");
	const auto& invalid = environment.snapshots[4];
	Check(invalid.vertices.empty() && invalid.commands.empty() && invalid.finalColor == inherited.finalColor, "original invalid bounds return without hidden state reset");
	const auto& minimum = environment.snapshots[5];
	Check(minimum.vertices[0].x == 1, "original cs clamped to one pixel");
	Near(GeometryArea(minimum), 98, "one-pixel corner area");
	Check(!minimum.commands[0].state.scissor && minimum.commands[0].state.blend.destinationColor == ScreenBlendFactor::OneMinusSourceAlpha, "explicit state restore affects later command only");
	Check(!environment.snapshots[6].recording && environment.snapshots[6].commands.empty(), "unsupported callback and incomplete quad recover without active batch");
	Check(environment.errors.size() == 4, "all unsupported/incomplete graphics produce explicit errors");
}

std::string Quote(const std::string& text)
{
	std::string result = "\"";
	for (char value: text) {
		if (value == '\\' || value == '"') result += '\\';
		if (value == '\n') result += "\\n";
		else result += value;
	}
	return result + '"';
}

void WriteColor(std::ostream& output, const ScreenColor& color)
{
	output << '[' << color[0] << ',' << color[1] << ',' << color[2] << ',' << color[3] << ']';
}

void WriteState(std::ostream& output, const ScreenDrawState& state)
{
	output << "{\"blend\":{\"enabled\":" << (state.blend.enabled ? "true" : "false")
		<< ",\"source_color\":" << static_cast<int>(state.blend.sourceColor)
		<< ",\"destination_color\":" << static_cast<int>(state.blend.destinationColor)
		<< ",\"source_alpha\":" << static_cast<int>(state.blend.sourceAlpha)
		<< ",\"destination_alpha\":" << static_cast<int>(state.blend.destinationAlpha) << "},\"scissor\":";
	if (state.scissor) {
		const auto& value = *state.scissor;
		output << '[' << value.x << ',' << value.y << ',' << value.width << ',' << value.height << ']';
	} else output << "null";
	output << '}';
}

void WriteReport(const Environment& environment, const char* source, const char* path)
{
	std::ofstream output(path);
	if (!output) throw std::runtime_error("Cannot write fixture report");
	output << std::setprecision(std::numeric_limits<float>::max_digits10);
	output << "{\"schema_version\":1,\"lua_runtime\":" << Quote(LUA_VERSION) << ",\"lua_number_bytes\":" << sizeof(lua_Number)
		<< ",\"official_source\":" << Quote(source) << ",\"checks_passed\":" << checks << ",\"coordinate_origin\":\"bottom-left screen pixels\",\"blend_factor_enum\":{\"0\":\"Zero\",\"1\":\"One\",\"2\":\"SourceAlpha\",\"3\":\"OneMinusSourceAlpha\"},\"snapshots\":[";
	bool separator = false;
	for (const auto& value: environment.snapshots) {
		if (separator) output << ',';
		separator = true;
		output << "{\"label\":" << Quote(value.label) << ",\"geometry_area\":" << GeometryArea(value) << ",\"final_color\":";
		WriteColor(output, value.finalColor);
		output << ",\"final_state\":";
		WriteState(output, value.finalState);
		output << ",\"vertices\":[";
		for (std::size_t index = 0; index < value.vertices.size(); ++index) {
			if (index) output << ',';
			const auto& vertex = value.vertices[index];
			output << '[' << vertex.x << ',' << vertex.y << ',' << vertex.z << ',' << vertex.color[0] << ',' << vertex.color[1] << ',' << vertex.color[2] << ',' << vertex.color[3] << ']';
		}
		output << "],\"indices\":[";
		for (std::size_t index = 0; index < value.indices.size(); ++index) {
			if (index) output << ',';
			output << value.indices[index];
		}
		output << "],\"commands\":[";
		for (std::size_t index = 0; index < value.commands.size(); ++index) {
			if (index) output << ',';
			const auto& command = value.commands[index];
			output << "{\"first_index\":" << command.firstIndex << ",\"index_count\":" << command.indexCount << ",\"state\":";
			WriteState(output, command.state);
			output << '}';
		}
		output << "]}";
	}
	output << "],\"expected_errors\":[";
	for (std::size_t index = 0; index < environment.errors.size(); ++index) {
		if (index) output << ',';
		output << Quote(environment.errors[index]);
	}
	output << "]}\n";
}

} // namespace

int main(int argc, char** argv)
{
	if (argc != 4) {
		std::cerr << "Usage: flowui-fixture <official-gui_flowui.lua> <fixture.lua> <output.json>\n";
		return EXIT_FAILURE;
	}
	lua_State* state = luaL_newstate();
	if (!state) return EXIT_FAILURE;
	try {
		Environment environment;
		Initialize(state, environment);
		RunFile(state, argv[1]);
		RunFile(state, argv[2]);
		Validate(environment);
		WriteReport(environment, argv[1], argv[3]);
		lua_close(state);
		std::cout << "PASS: " << checks << " real FlowUI checks; " << LUA_VERSION << "; number bytes=" << sizeof(lua_Number) << '\n';
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		lua_close(state);
		return EXIT_FAILURE;
	}
}
