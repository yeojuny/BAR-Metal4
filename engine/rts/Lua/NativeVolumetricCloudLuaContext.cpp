/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeVolumetricCloudLuaContext.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lib/sol2/sol.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <type_traits>

namespace LuaNativeVolumetricClouds {
namespace {
using namespace rendering;
void Check(bool valid, const char* reason) { if (!valid) throw std::runtime_error(reason); }
template<class F,class C,class R,class... A>
std::function<R(A...)> Protected(lua_State* L,F function,R(C::*)(A...) const)
{
	return [L,function=std::move(function)](A... arguments)->R {
		std::array<char,2048> message{};
		try {
			if constexpr (std::is_void_v<R>) { function(std::forward<A>(arguments)...); return; }
			else return function(std::forward<A>(arguments)...);
		} catch (const std::exception& error) { std::snprintf(message.data(),message.size(),"%s",error.what()); }
		catch (...) { std::snprintf(message.data(),message.size(),"Unknown original native-cloud dispatch failure"); }
		// C++ Lua may otherwise catch a propagated exception with the old
		// function still on the error stack. Publish the real error string.
		luaL_error(L,"%s",message.data());
		if constexpr (!std::is_void_v<R>) return R{};
	};
}
NativePostprocessFilter Filter(unsigned value)
{
	Check(value == 0x2600 || value == 0x2601,"Unknown original cloud min/mag filter");
	return value == 0x2600 ? NativePostprocessFilter::Nearest : NativePostprocessFilter::Linear;
}
NativePostprocessWrap Wrap(unsigned value)
{
	Check(value == 0x2901 || value == 0x812f,"Unknown original cloud wrapping mode");
	return value == 0x2901 ? NativePostprocessWrap::Repeat : NativePostprocessWrap::ClampEdge;
}
NativeVolumetricCloudTextureOptions Options(const sol::table& source)
{
	NativeVolumetricCloudTextureOptions out;
	for (const auto& [key,value]: source) {
		Check(key.is<std::string>(),"Cloud texture option requires a name");
		const auto name = key.as<std::string>();
		if (name == "format") {
			const auto format = value.as<unsigned>();
			Check(format == 0x8058 || format == 0x81a6,"Unknown original cloud image format");
			out.format = format == 0x81a6 ? NativePostprocessFormat::Depth24Unorm : NativePostprocessFormat::RGBA8;
		} else if (name == "fbo") { Check(value.is<bool>(),"Cloud FBO flag must be boolean"); out.renderToTexture = value.as<bool>(); }
		else if (name == "min_filter") out.sampler.minFilter = Filter(value.as<unsigned>());
		else if (name == "mag_filter") out.sampler.magFilter = Filter(value.as<unsigned>());
		else if (name == "wrap_s") out.sampler.wrapS = Wrap(value.as<unsigned>());
		else if (name == "wrap_t") out.sampler.wrapT = Wrap(value.as<unsigned>());
		else throw std::runtime_error("Unknown original cloud texture option: " + name);
	}
	return out;
}
std::shared_ptr<NativeVolumetricCloudProgramState> CreateProgram(NativeVolumetricCloudContext& context, const sol::table& options)
{
	for (const auto& [key,value]: options) {
		(void)value; Check(key.is<std::string>(),"Cloud program option requires a name");
		const auto name = key.as<std::string>();
		Check(name == "vertex" || name == "fragment" || name == "uniformInt" || name == "uniformFloat","Unknown original cloud program stage/option");
	}
	const sol::table samplers = options["uniformInt"];
	unsigned count = 0;
	for (const auto& [key,value]: samplers) {
		const auto name = key.as<std::string>();
		Check(name.size() == 4 && name.starts_with("tex") && (name[3] == '0' || name[3] == '2') &&
			value.as<unsigned>() == unsigned(name[3]-'0'),"Original cloud sampler assignment differs");
		++count;
	}
	// Complete31657 removes the optimized-out 2D noise uniform and binding.
	Check(count == 2,"Original cloud sampler count differs");
	auto program = context.CreateProgram(options["vertex"].get<std::string>(),options["fragment"].get<std::string>());
	auto previous = context.CurrentProgram();
	context.UseProgram(program);
	try {
		const sol::table uniforms = options["uniformFloat"];
		unsigned fields = 0;
		for (const auto& [key,value]: uniforms) {
			const auto name = key.as<std::string>();
			if (name == "viewProjectionInv") {
				const auto table = value.as<sol::table>(); Check(table.size() == 16,"Original cloud initial inverse matrix size differs");
				std::array<float,16> matrix{}; for (unsigned i=0;i<16;++i) matrix[i] = table[i+1].get<float>(); context.UniformMatrix(matrix);
			} else {
				std::vector<float> values;
				if (value.is<sol::table>()) { const auto table = value.as<sol::table>(); for (unsigned i=1;i<=table.size();++i) values.push_back(table[i].get<float>()); }
				else values.push_back(value.as<float>());
				context.Uniform(name,values);
			}
			++fields;
		}
		Check(fields == 6,"Original cloud initial uniform fields differ");
	} catch (...) { context.UseProgram(previous); throw; }
	context.UseProgram(std::move(previous)); return program;
}
void Call(sol::protected_function function, sol::variadic_args args)
{
	const auto result = function(sol::as_args(args));
	if (!result.valid()) { const sol::error error = result; throw std::runtime_error(error.what()); }
}
}
void PushCloudContext(lua_State* L, std::shared_ptr<NativeVolumetricCloudContext> context, CloudCallbacks callbacks)
{
	Check(context && callbacks.stage && callbacks.inverseDepth01ViewProjection && callbacks.loadOriginalVolume &&
		callbacks.loadOriginalNoise2D && callbacks.registerImage,"Cloud context requires real source-stage/resource callbacks");
	auto hooks = std::make_shared<CloudCallbacks>(std::move(callbacks));
	sol::state_view lua(L); const auto original = sol::stack::get<sol::table>(L,-1);
	const sol::protected_function originalTexRect = original["TexRect"];
	const sol::protected_function originalTexture = original["Texture"];
	auto table = lua.create_table();
	auto bind = [L](sol::table& target,const char* name,auto function) {
		target.set_function(name,Protected(L,std::move(function),&decltype(function)::operator()));
	};
	auto metatable = lua.create_table(); metatable["__index"] = original; table[sol::metatable_key] = metatable;
	bind(table,"CreateTexture",[context,hooks](double width,double height,sol::table options) {
		const auto temporary = context->CreateTexture(width,height,Options(options));
		const auto common = hooks->registerImage(context->GetTexture(temporary));
		context->AliasTexture(temporary,common); return common;
	});
	bind(table,"DeleteTexture",[context](sol::optional<std::string> name) { if (name) context->DeleteTexture(*name); });
	// This private function really accepts only the paired complete source. Its
	// existence satisfies the original widget's local availability check; no
	// general CreateShader is added to the actual global gl table.
	bind(table,"CreateShader",[context](sol::table options) { return CreateProgram(*context,options); });
	bind(table,"LuaShader",[context,hooks,L,bind](sol::table options,sol::optional<std::string> label) {
		(void)label; const auto program = CreateProgram(*context,options); sol::state_view lua(L); auto object = lua.create_table();
		bind(object,"Initialize",[program](sol::object) { Check(!program->deleted,"Deleted original cloud program"); return IsNativeVolumetricCloudConsumerAvailable(); });
		bind(object,"Finalize",[context,program](sol::object) { context->DeleteProgram(program); });
		bind(object,"Activate",[context,program](sol::object) { context->UseProgram(program); });
		bind(object,"Deactivate",[context](sol::object) { context->UseProgram({}); });
		bind(object,"SetUniform",[context,program](sol::object,const std::string& name,sol::variadic_args args) {
			Check(context->CurrentProgram() == program,"Original cloud uniform requires its actual active program");
			std::vector<float> values; for (auto value: args) values.push_back(value.as<float>()); context->Uniform(name,values);
		});
		bind(object,"SetUniformMatrix",[context,program,hooks](sol::object,const std::string& name,const std::string& matrix) {
			Check(context->CurrentProgram() == program && name == "viewProjectionInv" && matrix == "viewprojectioninverse","Unknown original cloud matrix binding");
			context->UniformMatrix(hooks->inverseDepth01ViewProjection());
		});
		return object;
	});
	bind(table,"Texture",[context,hooks,originalTexture](sol::variadic_args args) {
		Check(args.size() == 1 || args.size() == 2,"Original cloud texture argument count differs");
		const unsigned unit = args.size() == 2 ? args[0].as<unsigned>() : 0;
		const auto value = args[args.size()-1];
		if (value.is<bool>()) {
			Check(!value.as<bool>(),"Original clouds cannot enable an unspecified shader binding");
			context->DisableTexture(unit); if (unit == 0) Call(originalTexture,args); return true;
		}
		const auto name = value.as<std::string>();
		if (context->CurrentProgram()) {
			if (unit == 0) { context->BindDepth(name); Call(originalTexture,args); }
			else if (unit == 1) { Check(name == ":l:LuaUI/Images/rgbnoise.png","Original cloud unit1 named texture differs"); hooks->loadOriginalNoise2D(name); }
			else if (unit == 2) { Check(name == "LuaUI/Images/noisetextures/worley_rgbnorm_01_asum_128_v1.dds","Original cloud volume path differs"); context->BindVolume(hooks->loadOriginalVolume(name)); }
			else throw std::runtime_error("Original cloud texture unit differs");
		} else {
			Check(unit == 0,"Original cloud composite uses texture unit0");
			const auto binding = context->GetTexture(name); Check(binding->current != nullptr,"Original cloud composite has no real version");
			Call(originalTexture,args);
		}
		return true;
	});
	bind(table,"CopyToTexture",[context,hooks](const std::string& name,int dx,int dy,int sx,int sy,int width,int height) {
		Check(width >= 0 && height >= 0,"Original cloud depth-copy size is negative");
		context->CopyDepth(name,{sx,sy,std::uint32_t(width),std::uint32_t(height)},{dx,dy,std::uint32_t(width),std::uint32_t(height)},hooks->stage());
	});
	bind(table,"RenderToTexture",[context](const std::string& name,sol::protected_function callback,sol::variadic_args args) {
		context->BeginTarget(name);
		try { Call(std::move(callback),args); } catch (...) { context->EndTarget(); throw; }
		context->EndTarget();
	});
	bind(table,"TexRect",[context,hooks,originalTexRect](sol::variadic_args args) {
		if (!context->CurrentProgram()) { Call(originalTexRect,args); return; }
		Check(args.size() == 8,"Original cloud shader quad arguments differ");
		std::array<float,4> clip{},uv{};
		for (unsigned i=0;i<4;++i) { clip[i] = args[i].as<float>(); uv[i] = args[i+4].as<float>(); }
		context->TexRect(clip,uv,hooks->stage());
	});
	sol::stack::push(L,table);
}
} // namespace LuaNativeVolumetricClouds
