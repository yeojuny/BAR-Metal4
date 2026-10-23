/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativePanelShader.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaNativeUI.h"
#include "LuaOpenGL.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativePanelShaderData.h"
#include "Rendering/Backend/NativePostprocessData.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace LuaNativePanelShader {
namespace {
using namespace rendering;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::uint64_t Hash(std::string_view text)
{
	std::uint64_t value=14695981039346656037ULL;
	for (unsigned char byte: text) value=(value^byte)*1099511628211ULL;
	return value;
}
template<typename Signature> struct Boundary;
template<typename R, typename... Args> struct Boundary<R(Args...)> {
	template<typename F> static auto Wrap(F function)
	{
		return [function=std::move(function)](sol::this_state L, Args... args)->R {
			try { return function(std::forward<Args>(args)...); }
			catch (const std::exception& failure) { lua_pushstring(L, failure.what()); }
			lua_error(L); std::terminate();
		};
	}
};
template<typename T, typename F> void Bind(T& table, const char* name, F function)
{
	table.set_function(name, Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
struct Program {
	std::shared_ptr<const int> domain;
	NativePanelShaderDraw uniforms;
	bool deleted=false;
};
struct Context {
	std::uint64_t generation=0;
	std::shared_ptr<const int> domain=std::make_shared<const int>(0);
	std::shared_ptr<Program> active;
	NativeTextureContext& Actual() const
	{
		auto& owner=OriginalNativeTextureContext();
		Check(owner.Initialized() && owner.Registry().ContextGeneration()==generation,
			"Panel shader crossed its actual texture context lifetime");
		return owner;
	}
	void Own(const std::shared_ptr<Program>& program) const
	{
		Actual();
		Check(program && !program->deleted && program->domain==domain, "Invalid/deleted private panel shader");
	}
	void Emit(NativePanelShaderDraw draw, NativePanelShaderKind expected)
	{
		auto& owner=Actual(); auto& recorder=ui::GetRecorder();
		Check(active && active->uniforms.kind==expected && recorder.IsActive() && !recorder.IsWorldStage(),
			"Original panel shader draw requires its actual screen/RTT stage and active paired program");
		const unsigned count=expected==NativePanelShaderKind::GUIBlur ? 3 : 1;
		for (unsigned unit=0; unit<count; ++unit) {
			// GLSL sampler bindings ignore the fixed-function texture enable.
			const auto binding=owner.History().Binding(unit,NativeCUSTextureTarget::Image2D);
			Check(binding.object && !ui::NativeUITextureHistory::SamplingIncomplete(binding),
				"Original panel shader requires its complete actual sampled image");
			const auto observed=owner.UI().DescribeBinding(binding);
			Check(bool(observed.image), "Original panel shader sampled producer has no immutable descriptor");
			draw.images[unit]=observed.image; draw.samplers[unit]=observed.sampler;
		}
		recorder.AppendPanelShader(std::move(draw));
	}
};
constexpr std::array<std::string_view,9> UniformNames{
	"ivsx", "ivsy", "blurScale", "viewSize", "tileSize", "toScreen", "blockSize", "threshold", "alpha"};
void Uniform(Program& program, std::string_view name, const std::vector<float>& values)
{
	auto& u=program.uniforms;
	Check(std::all_of(values.begin(),values.end(),[](float v){return std::isfinite(v);}), "Nonfinite panel shader uniform");
	Check(values.size()==(name=="viewSize" ? 2u : 1u), "Original panel shader uniform arity changed");
	const float value=values[0];
	if (u.kind==NativePanelShaderKind::GUIBlur) {
		if (name=="ivsx") u.ivsx=value;
		else if (name=="ivsy") u.ivsy=value;
		else if (name=="blurScale") u.blurScale=value;
		else if (name=="viewSize") u.viewSize={value,values[1]};
		else if (name=="tileSize") { Check(value==32, "Original GUI tile size changed"); u.tileSize=value; }
		else if (name=="toScreen") { Check(value==0 || value==1, "Invalid GUI destination selector"); u.toScreen=std::int32_t(value); }
		else throw std::runtime_error("Unknown original GUI blur uniform");
	} else if (u.kind==NativePanelShaderKind::GUIMask) {
		if (name=="blockSize") { Check(value==4 || value==8, "Original GUI mask reduction size changed"); u.blockSize=std::int32_t(value); }
		else if (name=="threshold") u.threshold=value;
		else throw std::runtime_error("Unknown original GUI mask uniform");
	} else {
		Check(name=="alpha", "Unknown original Pause shader uniform"); u.alpha=value;
	}
}
std::vector<float> Values(sol::variadic_args args)
{
	std::vector<float> result;
	for (auto value: args) result.push_back(value.as<float>());
	return result;
}
std::shared_ptr<Program> CreateProgram(const std::shared_ptr<Context>& context, const sol::table& options)
{
	context->Actual();
	for (const auto& [key,value]: options) {
		const auto name=key.as<std::string>();
		Check(name=="vertex" || name=="fragment" || name=="uniformInt" || name=="uniformFloat", "Unknown original panel shader option");
	}
	auto program=std::make_shared<Program>(); program->domain=context->domain;
	auto& u=program->uniforms;
	const auto fragment=options.get<std::string>("fragment");
	const auto vertex=options.get_or<std::string>("vertex", "");
	const auto fh=Hash(fragment), vh=Hash(vertex);
	std::map<std::string,int> samplers;
	if (vertex.size()==893 && vh==8690911374927095089ULL) {
		u.kind=NativePanelShaderKind::GUIBlur;
		const bool quad=fragment.size()==2831 && fh==8172927243376227428ULL;
		u.fineDerivatives=fragment.size()==2922 && fh==14625648938614272838ULL;
		const bool intel=fragment.size()==2849 && fh==17709729674167680059ULL;
		const bool normalized=fragment.size()==2874 && fh==4044404735850265353ULL;
		Check(quad || u.fineDerivatives || intel || normalized, "Original31657 GUI blur fragment identity differs");
		u.intelGPU=intel || normalized; u.intelNormalized=normalized;
		samplers={{"tex0",0},{"tileMask",1},{"tex2",2}};
	} else if (vertex.size()==123 && vh==1342451496132615029ULL) {
		Check(fragment.size()==504 && fh==10720764698117843641ULL, "Original31657 GUI mask fragment identity differs");
		u.kind=NativePanelShaderKind::GUIMask; samplers={{"source",0}};
	} else {
		Check(vertex.empty() && fragment.size()==1422 && fh==3895677703687703412ULL,
			"Original31657 Pause fragment/defines identity differs");
		u.kind=NativePanelShaderKind::Pause; u.perceivedLuma=true; u.smoothTone=false;
		u.vignette=1; u.tintEnabled=false; u.tint={1,1,1}; samplers={{"screencopy",0}};
	}
	const auto integers=options.get<sol::table>("uniformInt");
	for (const auto& [key,value]: integers) {
		const auto name=key.as<std::string>(); const auto found=samplers.find(name);
		if (found!=samplers.end()) {
			Check(value.as<int>()==found->second, "Original panel shader sampler unit changed"); samplers.erase(found);
		} else Uniform(*program,name,{value.as<float>()});
	}
	Check(samplers.empty(), "Original panel shader sampler declaration missing");
	const sol::optional<sol::table> floats=options["uniformFloat"];
	if (floats) for (const auto& [key,value]: *floats) Uniform(*program,key.as<std::string>(),{value.as<float>()});
	return program;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeScreenCopyConsumerAvailable(),
		"Original panels require the installed native screen/copy consumer");
	auto context=std::make_shared<Context>(); auto& owner=OriginalNativeTextureContext();
	Check(owner.Initialized() && owner.UIAttached(), "Original panel texture owner is unavailable");
	context->generation=owner.Registry().ContextGeneration();
	sol::state_view lua(L); auto table=lua.create_table();
	Bind(table,"CreateShader",[context](sol::table options){return CreateProgram(context,options);});
	Bind(table,"DeleteShader",[context](sol::optional<std::shared_ptr<Program>> program){
		if (!program || !*program) return;
		context->Own(*program); (*program)->deleted=true;
		// Already recorded operations retain their program/uniform snapshots.
	});
	Bind(table,"UseShader",[context](sol::object value){
		std::shared_ptr<Program> program;
		if (value.is<int>()) Check(value.as<int>()==0,"Unknown private panel shader name");
		else { program=value.as<std::shared_ptr<Program>>(); context->Own(program); }
		ui::GetRecorder().RecordOperation([context,program](ui::NativeUIRecorder& recorder){
			if (!recorder.IsCompiling()) { context->Actual(); context->active=program; }
		});
	});
	Bind(table,"GetUniformLocation",[context](std::shared_ptr<Program> program,const std::string& name){
		context->Own(program); const auto found=std::find(UniformNames.begin(),UniformNames.end(),name);
		Check(found!=UniformNames.end(),"Unknown private panel uniform location");
		return int(found-UniformNames.begin());
	});
	Bind(table,"ProgramUniform",[context](std::shared_ptr<Program> program,const std::string& name,sol::variadic_args args){
		context->Own(program); auto values=Values(args);
		ui::GetRecorder().RecordOperation([context,program,name,values=std::move(values)](ui::NativeUIRecorder& recorder){
			if (recorder.IsCompiling()) return;
			context->Actual(); Check(context->active==program,"Panel uniform updated another active program");
			Uniform(*program,name,values);
		});
	});
	auto uniform=[context](int location,sol::variadic_args args){
		if (location==-1) return;
		Check(location>=0 && std::size_t(location)<UniformNames.size(),"Unknown private panel uniform location");
		auto values=Values(args);
		ui::GetRecorder().RecordOperation([context,location,values=std::move(values)](ui::NativeUIRecorder& recorder){
			if (recorder.IsCompiling()) return;
			context->Actual(); Check(bool(context->active),"Panel uniform has no active program");
			Uniform(*context->active,UniformNames[location],values);
		});
	};
	Bind(table,"Uniform",uniform); Bind(table,"UniformInt",uniform);
	Bind(table,"Texture",[L,context](sol::object first,sol::optional<sol::object> second){
		Check(LuaOpenGL::IsDrawingEnabled(L),"Panel texture binding requires the original draw call-in");
		unsigned unit=0; sol::object value=first;
		if (first.is<double>()) {
			const auto number=first.as<double>();
			Check(std::isfinite(number) && std::trunc(number)>=0 && std::trunc(number)<3 && second.has_value(),
				"Original panel texture unit must be zero, one or two");
			unit=unsigned(std::trunc(number)); value=*second;
		}
		auto& recorder=ui::GetRecorder(); context->Actual();
		if (value.is<bool>()) { recorder.LuaTextureEnabled(value.as<bool>(),unit); return true; }
		const auto name=value.as<std::string>();
		if (recorder.IsCompiling()) {
			// Match original display-list bind semantics: capture the parsed
			// logical object now; resolve its surviving version at replay.
			const auto lookup=LuaNativeUI::CaptureTextureLookup(L,name);
			recorder.RecordOperation([context,unit,lookup](ui::NativeUIRecorder& replay){
				if (replay.IsCompiling()) return;
				const auto actual=context->Actual().UI().LuaLookup(unit,[lookup]{return lookup;});
				replay.SyncObservedTextureBinding(actual);
			});
			return lookup.parsed;
		}
		bool parsed=false;
		const auto actual=context->Actual().UI().LuaLookup(unit,[&]{auto lookup=LuaNativeUI::CaptureTextureLookup(L,name);parsed=lookup.parsed;return lookup;});
		recorder.SyncObservedTextureBinding(actual); return parsed;
	});
	Bind(table,"DrawTiles",[context](unsigned mode,unsigned count,unsigned first,unsigned instances,
		const sol::table& corners,const sol::table& indices,const sol::table& tiles){
		Check(mode==0x0004 && count==6 && first==0 && instances>0 && corners.size()==8 && indices.size()==6 && tiles.size()%2==0 && instances<=tiles.size()/2,
			"Original GUI tile draw topology changed");
		NativePanelShaderDraw geometry; geometry.instanceCount=instances;
		const std::array<float,8> expectedCorners{0,0,1,0,0,1,1,1};
		const std::array<unsigned,6> expectedIndices{0,1,2,2,1,3};
		for (unsigned i=0;i<8;++i) {
			const float value=corners.get<float>(i+1); Check(value==expectedCorners[i],"Original GUI corner upload changed"); geometry.corners[i/2][i%2]=value;
		}
		for (unsigned i=0;i<6;++i) {
			const unsigned value=indices.get<unsigned>(i+1); Check(value==expectedIndices[i],"Original GUI index upload changed"); geometry.indices[i]=value;
		}
		geometry.tiles.reserve(tiles.size()/2);
		for (unsigned i=1;i<=tiles.size();i+=2) {
			const float x=tiles.get<float>(i),y=tiles.get<float>(i+1);
			Check(std::isfinite(x) && std::isfinite(y) && x>=0 && y>=0 && std::trunc(x)==x && std::trunc(y)==y,"Invalid original GUI tile upload");
			geometry.tiles.push_back({x,y});
		}
		ui::GetRecorder().RecordOperation([context,geometry=std::move(geometry)](ui::NativeUIRecorder& recorder){
			if (recorder.IsCompiling()) return;
			Check(context->active && context->active->uniforms.kind!=NativePanelShaderKind::Pause,"GUI tile draw requires its paired program");
			auto draw=context->active->uniforms;
			draw.corners=geometry.corners; draw.indices=geometry.indices; draw.tiles=geometry.tiles; draw.instanceCount=geometry.instanceCount;
			context->Emit(std::move(draw),context->active->uniforms.kind);
		});
	});
	Bind(table,"DrawPause",[context](float x0,float y0,float x1,float y1){
		ui::GetRecorder().RecordOperation([context,x0,y0,x1,y1](ui::NativeUIRecorder& recorder){
			if (recorder.IsCompiling()) return;
			Check(context->active && context->active->uniforms.kind==NativePanelShaderKind::Pause,"Pause rectangle requires its exact paired fragment");
			auto draw=context->active->uniforms; draw.rect={x0,y0,x1,y1}; draw.uv={0,1,1,0};
			context->Emit(std::move(draw),NativePanelShaderKind::Pause);
			recorder.TexCoord(0,0); // Final original TexRect vertex leaves this current coordinate.
		});
	});
	return table;
}
}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl=sol::stack::get<sol::table>(L,-1); auto family=lua.create_table();
	family.new_usertype<Program>("PrivateProgram",sol::no_constructor); family.set("PrivateProgram",sol::lua_nil);
	Bind(family,"Available",[](){return IsNativeScreenCopyConsumerAvailable() && OriginalNativeTextureContext().Initialized() && OriginalNativeTextureContext().UIAttached();});
	Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original panel sources are unsynced only");
		const bool gui=path=="luaui/widgets/gfx_guishader.lua", pause=path=="luaui/widgets/gui_pausescreen.lua";
		Check(gui || pause,"Unknown original panel source path");
		CFileHandler file(path,SPRING_VFS_ZIP); std::string text;
		Check(file.FileExists() && file.LoadStringData(text),"Original31657 panel source missing");
		Check(gui ? text.size()==26148 && Hash(text)==16324400600168666664ULL : text.size()==11193 && Hash(text)==10149866307967708712ULL,
			"Original31657 panel archive identity differs");
		return text;
	});
	gl.set("NativePanelShader",family); return true;
}
}
#else
namespace LuaNativePanelShader { bool PushEntries(lua_State*) { return true; } }
#endif
