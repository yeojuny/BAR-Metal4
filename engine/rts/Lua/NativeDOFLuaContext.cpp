/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDOFLuaContext.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lib/sol2/sol.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace LuaNativePostprocess {
namespace {
using namespace rendering;
void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
constexpr std::array<std::string_view,10> names{"projectionMat","resolution","autofocus","autofocusFudgeFactor",
	"autofocusPower","autofocusFocalLength","mousefocus","manualFocusDepth","mouseDepthCoord","fStop"};
std::string_view Name(int location)
{
	Check(location >= 0 && std::size_t(location) < names.size(), "Unknown private DOF uniform binding"); return names[location];
}
std::uint32_t Size(float value)
{
	Check(std::isfinite(value) && value >= 1 && value <= 32768, "Original DOF image extent is invalid");
	return std::uint32_t(value); // Original luaL_checknumber -> GLsizei truncates half extents.
}
NativePostprocessFilter Filter(unsigned value)
{
	Check(value == 0x2600 || value == 0x2601, "Unsupported original DOF min/mag filter");
	return value == 0x2600 ? NativePostprocessFilter::Nearest : NativePostprocessFilter::Linear;
}
NativePostprocessWrap Wrap(unsigned value)
{
	switch (value) { case 0x2901: return NativePostprocessWrap::Repeat; case 0x812f: return NativePostprocessWrap::ClampEdge;
		case 0x2900: return NativePostprocessWrap::LegacyClamp; default: throw std::runtime_error("Unsupported original DOF wrap mode"); }
}
NativeDOFTextureOptions Options(const sol::table& source)
{
	NativeDOFTextureOptions out;
	for (const auto& [key, value]: source) {
		Check(key.is<std::string>(), "DOF texture option name must be a string"); const auto name = key.as<std::string>();
		if (name == "format") {
			const auto f = value.as<unsigned>();
			Check(f == 0x8058 || f == 0x881a || f == 0x81a7 || f == 0x822b, "Unsupported original DOF image format");
			out.format = f == 0x822b ? NativePostprocessFormat::RG8 : f == 0x881a ? NativePostprocessFormat::RGBA16Float : f == 0x81a7 ? NativePostprocessFormat::Depth32 : NativePostprocessFormat::RGBA8;
		} else if (name == "fbo") out.renderToTexture = value.as<bool>();
		else if (name == "border") Check(value.is<bool>() && !value.as<bool>(), "Original DOF only uses border0 images");
		else if (name == "min_filter") out.sampler.minFilter = Filter(value.as<unsigned>());
		else if (name == "mag_filter") out.sampler.magFilter = Filter(value.as<unsigned>());
		else if (name == "wrap_s") out.sampler.wrapS = Wrap(value.as<unsigned>());
		else if (name == "wrap_t") out.sampler.wrapT = Wrap(value.as<unsigned>());
		else throw std::runtime_error("Unsupported original DOF texture option: " + name);
	}
	return out;
}

}
void PushDOFContext(lua_State* L, std::shared_ptr<NativeDOFContext> context, DOFCallbacks callbacks)
{
	Check(context && callbacks.screenCommandOffset && callbacks.viewExtent && callbacks.projection && callbacks.viewport && callbacks.blending && callbacks.textureUnit0Bound && callbacks.textureUnit0Disabled && callbacks.currentTexCoord,
		"Private DOF context requires real screen/matrix/geometry callbacks");
	sol::state_view lua(L); auto table = lua.create_table();
	table.new_usertype<NativeDOFProgram>("Program", sol::no_constructor);
	table.new_usertype<NativeDOFFBO>("FBO", sol::no_constructor);
	table.set("Program", sol::lua_nil, "FBO", sol::lua_nil);
	table.set_function("CreateTexture", [context](float width, float height, sol::table options) { return context->CreateTexture(Size(width), Size(height), Options(options)); });
	table.set_function("DeleteTexture", [context](sol::optional<std::string> name) { if (name) context->DeleteTexture(*name); });
	table.set_function("CreateFBO", [context](sol::table options) {
		std::map<unsigned, std::string> attachments; sol::optional<sol::table> buffers;
		for (const auto& [key, value]: options) {
			Check(key.is<std::string>(), "DOF FBO option must be a name"); const auto name = key.as<std::string>();
			if (name == "drawbuffers") buffers = value.as<sol::table>();
			else {
				Check(name.size() == 6 && name.starts_with("color") && name[5] >= '0' && name[5] <= '3', "Unsupported original DOF FBO attachment");
				attachments.emplace(unsigned(name[5]-'0'), value.as<std::string>());
			}
		}
		Check(!attachments.empty() && buffers && buffers->size() == attachments.size(), "DOF FBO requires its original draw buffers");
		std::vector<std::string> names;
		for (unsigned i = 0; i < attachments.size(); ++i) {
			Check(attachments.contains(i) && (*buffers)[i+1].get<unsigned>() == 0x8ce0+i, "Original DOF MRT order changed"); names.push_back(attachments.at(i));
		}
		return context->CreateFBO(names);
	});
	table.set_function("DeleteFBO", [context](sol::optional<std::shared_ptr<NativeDOFFBO>> fbo) { context->DeleteFBO(fbo ? *fbo : nullptr); });
	table.set_function("CreateShader",[context](sol::table options) {
		for (const auto& [key,value]: options) { Check(key.is<std::string>(),"DOF shader option name must be a string");
			const auto name=key.as<std::string>();Check(name=="vertex" || name=="fragment" || name=="defines" || name=="uniformInt","Unsupported original31657 DOF shader option"); }
		const sol::table defines=options["defines"],samplers=options["uniformInt"];
		const std::array<std::string_view,14> expected{
			"#version 150 compatibility\n","","#define FILTER_SIZE_PASS 0\n","#define INITIAL_BLUR_PASS 1\n",
			"#define FINAL_BLUR_PASS 2\n","#define INITIAL_NEAR_BLUR_PASS 3\n","#define FINAL_NEAR_BLUR_PASS 4\n",
			"#define COMPOSITION_PASS 5\n","#define TILE_FLAG_PASS 6\n","#define TILE_MASK_PASS 7\n","",
			"#define BLUR_START_DIST 10000\n","#define TILE_SIZE 16\n",""};
		Check(defines.size()==expected.size(),"Original DOF shader defines count changed");
		for(unsigned i=0;i<expected.size();++i) if(!expected[i].empty()) Check(defines[i+1].get<std::string>()==expected[i],"Original DOF shader define changed");
		const std::string clip=defines[2];Check(clip=="#define DEPTH_CLIP01 0\n" || clip=="#define DEPTH_CLIP01 1\n","Original DOF clip-depth define changed");
		const std::string passDefine=defines[11],qualityDefine=defines[14];int pass=-1;
		for(int i=0;i<=7;++i) if(passDefine=="#define PASS "+std::to_string(i)+"\n") pass=i;
		Check(pass>=0 && (qualityDefine=="#define HIGH_QUALITY 1\n" || (pass==5 && qualityDefine=="#define HIGH_QUALITY 0\n")),
			"Original DOF compiled pass/quality define changed");
		unsigned count=0;
		for(const auto& [key,value]:samplers) {
			Check(key.is<std::string>(),"Original DOF sampler name must be a string");const auto name=key.as<std::string>();unsigned unit=6;
			if(name=="origTex") unit=0;
			else if(name=="tileMask") unit=5;
			else if(name.size()==8 && name.starts_with("blurTex") && name[7]>='0' && name[7]<='3') unit=1+unsigned(name[7]-'0');
			Check(unit<6 && value.as<unsigned>()==unit,"Original DOF sampler-unit assignment changed");++count;
		}
		Check(count==6,"Original DOF sampler declarations changed");
		return context->CreateProgram(options["vertex"].get<std::string>(),options["fragment"].get<std::string>(),
			clip=="#define DEPTH_CLIP01 1\n",pass,qualityDefine=="#define HIGH_QUALITY 1\n"?1:0);
	});
	table.set_function("DeleteShader", [context](sol::optional<std::shared_ptr<NativeDOFProgram>> program) { context->DeleteProgram(program ? *program : nullptr); });
	table.set_function("UseShader", [context](sol::object program) {
		if (program.is<int>()) { Check(program.as<int>() == 0, "Unknown private DOF program"); context->UseProgram({}); }
		else context->UseProgram(program.as<std::shared_ptr<NativeDOFProgram>>());
	});
	table.set_function("GetUniformLocation", [](std::shared_ptr<NativeDOFProgram> program, const std::string& name) {
		Check(program && !program->deleted, "DOF uniform lookup uses deleted program");
		auto found = std::find(names.begin(), names.end(), name); Check(found != names.end(), "Unknown original DOF uniform");
		// Original GL optimizes unused per-program uniforms away. Each pass
		// retains its own state; only FILTER_SIZE has the focus/projection inputs.
		const auto pass=program->uniforms.pass;
		if (name=="resolution") { if(pass==5 || pass==6) return -1; }
		else if (pass!=0) return -1;
		return int(found-names.begin());
	});
	auto uniform = [context](int location, sol::variadic_args args) {
		if (location == -1) return;
		const auto name=Name(location);const auto program=context->CurrentProgram();
		Check(program && !program->deleted && (name=="resolution" || program->uniforms.pass==0),"DOF uniform belongs to another active program");
		std::vector<float> values;for(auto arg:args) values.push_back(arg.as<float>());context->Uniform(name,values);
	};
	table.set_function("Uniform", uniform); table.set_function("UniformInt", uniform);
	table.set_function("UniformMatrix", [context, callbacks](int location, const std::string& name) {
		if (location==-1) return;
		Check(Name(location) == "projectionMat" && name == "projection", "Original DOF only captures the actual projection matrix"); context->Projection(callbacks.projection());
	});
	table.set_function("Texture", [context, callbacks](unsigned unit, sol::object name) {
		if (name.is<bool>()) { Check(!name.as<bool>(), "DOF cannot bind an unspecified image"); context->Unbind(unit); if(unit==0)callbacks.textureUnit0Disabled(); }
		else { context->Texture(unit, name.as<std::string>()); if(unit==0)callbacks.textureUnit0Bound(); } return true;
	});
	table.set_function("CopyToTexture", [context, callbacks](const std::string& name, int xoff, int yoff, int x, int y, int width, int height) {
		Check(width > 0 && height > 0, "DOF copy size is invalid");
		context->Copy(name, {x,y,unsigned(width),unsigned(height)}, {xoff,yoff,unsigned(width),unsigned(height)}, callbacks.screenCommandOffset());
		callbacks.textureUnit0Bound();
	});
	table.set_function("RawBindFBO",[L,context,callbacks](sol::object fbo,sol::optional<unsigned> target,sol::optional<sol::object> restore) -> sol::object {
		callbacks.screenCommandOffset(); // This original callback runs on the root screen framebuffer.
		Check(!target || *target==0x8d40,"Original DOF framebuffer target changed");
		if(fbo==sol::lua_nil) {
			std::shared_ptr<NativeDOFFBO> previous;
			if(restore && *restore!=sol::lua_nil) {
				if(restore->is<int>()) Check(restore->as<int>()==0,"Unknown private DOF framebuffer restore token");
				else previous=restore->as<std::shared_ptr<NativeDOFFBO>>();
			}
			context->BindFBO(std::move(previous));return sol::make_object(L,sol::lua_nil);
		}
		auto previous=context->BindFBO(fbo.as<std::shared_ptr<NativeDOFFBO>>());
		// The original uses this only as an opaque restore token; never expose a
		// fabricated GL name or lose the previous native attachment ownership.
		return previous?sol::make_object(L,previous):sol::make_object(L,0);
	});
	table.set_function("Viewport",[context,callbacks](int x,int y,int width,int height) {
		Check(width>=0 && height>=0,"Original DOF viewport size is negative");
		const NativePostprocessRect rectangle{x,y,unsigned(width),unsigned(height)};
		context->Viewport(rectangle);callbacks.viewport(rectangle);
	});
	table.set_function("TexRect", [context, callbacks](float x1, float y1, float x2, float y2, sol::optional<bool> flipS, sol::optional<bool> flipT) {
		const auto extent = callbacks.viewExtent(); context->TexRect({x1,y1,x2,y2}, {flipS.value_or(false)?1.f:0.f,flipT.value_or(false)?0.f:1.f,
			flipS.value_or(false)?0.f:1.f,flipT.value_or(false)?1.f:0.f}, callbacks.screenCommandOffset(), extent[0], extent[1]);
		callbacks.currentTexCoord(flipS.value_or(false)?1.f:0.f,flipT.value_or(false)?1.f:0.f);
	});
	table.set_function("Blending", [callbacks](bool enabled) { callbacks.blending(enabled); });
	table.set_function("GetShaderLog", []() { return "Original DOF requires its exact compiled paired native shader; no arbitrary GLSL compiler is exposed"; });
	sol::stack::push(L, table);
}
}
