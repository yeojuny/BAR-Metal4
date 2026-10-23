/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeUIDrawState.h"
#include "LuaInclude.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rendering;
using namespace rendering::ui;
namespace {
void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int NativeGLInt(lua_State* L,int index)
{
	const double value=luaL_checknumber(L,index);
	Require(std::isfinite(value)&&value>=double(std::numeric_limits<int>::min())&&value<=double(std::numeric_limits<int>::max()),"GL integer argument outside finite signed range");
	return static_cast<int>(value); // Original Lua integer conversion truncates.
}
int PushPopMatrix(lua_State* L,NativeUIRecorder& recorder)
{
	Require(recorder.IsActive()||recorder.IsCompiling(),"PushPopMatrix outside a drawing/list stage");
	int error=0;
	{
	std::vector<NativeUIMatrixMode> modes;
	int callback=1;
	for(;lua_isnumber(L,callback);++callback){
		const int value=NativeGLInt(L,callback);
		if(value==GL_MODELVIEW)modes.push_back(NativeUIMatrixMode::ModelView);
		else if(value==GL_PROJECTION)modes.push_back(NativeUIMatrixMode::Projection);
		else if(value==GL_TEXTURE)modes.push_back(NativeUIMatrixMode::Texture);
		else throw std::runtime_error("Incorrect matrix mode in gl.PushPopMatrix()");
	}
	Require(lua_isfunction(L,callback),"Incorrect arguments to gl.PushPopMatrix()");
	if(modes.empty())recorder.PushMatrix();
	else for(auto mode:modes){recorder.MatrixMode(mode);recorder.PushMatrix();}
	const int args=lua_gettop(L);
	error=lua_pcall(L,args-callback,0,0);
	// The original pops the CURRENT mode if no mode list was passed, and
	// otherwise visits the explicit modes in their original order. It does
	// not restore the caller's matrix mode. Preserve this also on Lua error.
	if(modes.empty())recorder.PopMatrix();
	else for(auto mode:modes){recorder.MatrixMode(mode);recorder.PopMatrix();}
	} // Destroy C++ mode storage before Lua propagates its error object.
	if(error)return lua_error(L);
	return 0;
}
NativeUIMatrixMode MatrixMode(int mode)
{
	switch (mode) {
		case GL_MODELVIEW: return NativeUIMatrixMode::ModelView;
		case GL_PROJECTION: return NativeUIMatrixMode::Projection;
		case GL_TEXTURE: return NativeUIMatrixMode::Texture;
		default: throw std::runtime_error("invalid matrix mode");
	}
}
ScreenBlendFactor BlendFactor(int value)
{
	switch (value) {
		case GL_ZERO: return ScreenBlendFactor::Zero;
		case GL_ONE: return ScreenBlendFactor::One;
		case GL_SRC_ALPHA: return ScreenBlendFactor::SourceAlpha;
		case GL_ONE_MINUS_SRC_ALPHA: return ScreenBlendFactor::OneMinusSourceAlpha;
		case GL_DST_ALPHA: return ScreenBlendFactor::DestinationAlpha;
		case GL_ONE_MINUS_DST_ALPHA: return ScreenBlendFactor::OneMinusDestinationAlpha;
		case GL_SRC_COLOR: return ScreenBlendFactor::SourceColor;
		case GL_ONE_MINUS_SRC_COLOR: return ScreenBlendFactor::OneMinusSourceColor;
		case GL_DST_COLOR: return ScreenBlendFactor::DestinationColor;
		case GL_ONE_MINUS_DST_COLOR: return ScreenBlendFactor::OneMinusDestinationColor;
		case GL_SRC_ALPHA_SATURATE: return ScreenBlendFactor::SourceAlphaSaturate;
		default: throw std::runtime_error("unsupported blend factor");
	}
}
std::array<float,4> Values(lua_State* L, int offset, unsigned count, std::array<float,4> value)
{
	if (lua_istable(L,offset)) {
		for (unsigned k=0; k<count; ++k) {
			lua_rawgeti(L,offset,k+1);
			if (!lua_isnil(L,-1)) value[k]=luaL_checknumber(L,-1);
			else Require(k >= 2,"missing vector coordinate");
			lua_pop(L,1);
		}
	} else {
		for (unsigned k=0; k<count; ++k) value[k]=luaL_optnumber(L,offset+k,value[k]);
	}
	return value;
}
NativeUIMatrix ReadMatrix(lua_State* L)
{
	NativeUIMatrix result{};
	if (lua_istable(L,1)) {
		for (int k=0;k<16;++k) { lua_rawgeti(L,1,k+1); result[k]=luaL_checknumber(L,-1); lua_pop(L,1); }
	} else {
		Require(lua_gettop(L)==16,"expected sixteen matrix values");
		for (int k=0;k<16;++k) result[k]=luaL_checknumber(L,k+1);
	}
	return result;
}
}
std::optional<int> LuaNativeUIDrawState::Dispatch(lua_State* L,NativeUIRecorder& r,std::string_view op)
{
 const int args=lua_gettop(L);
	if (op=="Translate" || op=="Scale") { auto v=Values(L,1,3,op=="Scale"?std::array<float,4>{1,1,1,1}:std::array<float,4>{0,0,0,1}); r.MultMatrix(op=="Scale"?ScaleMatrix(v[0],v[1],v[2]):TranslationMatrix(v[0],v[1],v[2])); return 0; }
	if (op=="Rotate") { r.MultMatrix(RotationMatrix(luaL_checknumber(L,1),luaL_checknumber(L,2),luaL_checknumber(L,3),luaL_checknumber(L,4))); return 0; }
	if (op=="MatrixMode") { r.MatrixMode(MatrixMode(luaL_checkint(L,1))); return 0; }
	if (op=="LoadIdentity") { r.LoadMatrix(IdentityMatrix()); return 0; }
	if (op=="LoadMatrix") { r.LoadMatrix(ReadMatrix(L)); return 0; }
	if (op=="MultMatrix") { r.MultMatrix(ReadMatrix(L)); return 0; }
	if (op=="Ortho") { r.MultMatrix(OrthoMatrix(luaL_checknumber(L,1),luaL_checknumber(L,2),luaL_checknumber(L,3),luaL_checknumber(L,4),luaL_checknumber(L,5),luaL_checknumber(L,6))); return 0; }
	if (op=="PushPopMatrix") return PushPopMatrix(L,r);
	if (op=="PushMatrix") { r.PushMatrix(); return 0; }
	if (op=="PopMatrix") { r.PopMatrix(); return 0; }
	if (op=="Blending" || op=="BlendFunc" || op=="BlendFuncSeparate") {
		if (op=="Blending" && args==1 && lua_isboolean(L,1)) {
			const bool enabled=lua_toboolean(L,1);r.UpdateBlend([enabled](ScreenBlendState& state){state.enabled=enabled;});
		} else if (op=="Blending" && args==1 && lua_isstring(L,1)) {
			const std::string name=lua_tostring(L,1);ScreenBlendState factors;
			if(name=="add")factors.sourceColor=factors.sourceAlpha=factors.destinationColor=factors.destinationAlpha=ScreenBlendFactor::One;
			else if(name=="alpha_add")factors.destinationColor=factors.destinationAlpha=ScreenBlendFactor::One;
			else if(name=="color"){factors.sourceColor=factors.sourceAlpha=ScreenBlendFactor::SourceColor;factors.destinationColor=factors.destinationAlpha=ScreenBlendFactor::OneMinusSourceColor;}
			else if(name=="modulate"){factors.sourceColor=factors.sourceAlpha=ScreenBlendFactor::DestinationColor;factors.destinationColor=factors.destinationAlpha=ScreenBlendFactor::Zero;}
			else Require(name=="alpha" || name=="reset" || name=="disable","unknown blending mode");
			factors.enabled=name!="disable";
			r.UpdateBlend([factors](ScreenBlendState& state){state.enabled=factors.enabled;state.sourceColor=factors.sourceColor;state.destinationColor=factors.destinationColor;state.sourceAlpha=factors.sourceAlpha;state.destinationAlpha=factors.destinationAlpha;});
		} else {
			Require(args==(op=="BlendFuncSeparate"?4:2),"BlendFunc requires two factors; BlendFuncSeparate requires four");
			const auto source=BlendFactor(luaL_checkint(L,1)),destination=BlendFactor(luaL_checkint(L,2));
			const auto sourceAlpha=op=="BlendFuncSeparate"?BlendFactor(luaL_checkint(L,3)):source;
			const auto destinationAlpha=op=="BlendFuncSeparate"?BlendFactor(luaL_checkint(L,4)):destination;
			const bool enable=op=="Blending";
			r.UpdateBlend([source,destination,sourceAlpha,destinationAlpha,enable](ScreenBlendState& state){state.sourceColor=source;state.destinationColor=destination;state.sourceAlpha=sourceAlpha;state.destinationAlpha=destinationAlpha;if(enable)state.enabled=true;});
		}
		return 0;
	}
	if (op=="BlendEquation" || op=="BlendEquationSeparate") {
		auto equation=[](int value) {switch(value){case GL_FUNC_ADD:return ScreenBlendEquation::Add;case GL_FUNC_SUBTRACT:return ScreenBlendEquation::Subtract;case GL_FUNC_REVERSE_SUBTRACT:return ScreenBlendEquation::ReverseSubtract;case GL_MIN:return ScreenBlendEquation::Minimum;case GL_MAX:return ScreenBlendEquation::Maximum;}throw std::runtime_error("Unsupported original blend equation");};
		Require(args==(op=="BlendEquation"?1:2),"BlendEquation expects one equation; BlendEquationSeparate expects RGB and alpha");
		const auto color=equation(luaL_checkint(L,1)),alpha=op=="BlendEquationSeparate"?equation(luaL_checkint(L,2)):color;
		r.UpdateBlend([color,alpha](ScreenBlendState& state){state.colorEquation=color;state.alphaEquation=alpha;});return 0;
	}
	if(op=="DepthMask") {Require(lua_isboolean(L,1),"DepthMask expects boolean");const bool value=lua_toboolean(L,1);r.UpdateWorldState([value](NativeWorldDrawState& state){state.depthWrite=value;},false,true);return 0;}
	if(op=="DepthTest") {
		if(lua_isboolean(L,1)){const bool value=lua_toboolean(L,1);r.UpdateWorldState([value](NativeWorldDrawState& state){state.depthTest=value;},false,true);}
		else {const int compare=luaL_checkint(L,1);Require(compare>=GL_NEVER&&compare<=GL_ALWAYS,"invalid depth comparison");r.UpdateWorldState([compare](NativeWorldDrawState& state){state.depthTest=true;state.depthCompare=ScreenAlphaCompare(compare-GL_NEVER);},false,true);}
		return 0;
	}
 return std::nullopt;
}
