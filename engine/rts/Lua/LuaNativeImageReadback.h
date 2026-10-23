/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaInclude.h"
#include "LuaIO.h"
#include "Rendering/Backend/NativeImageReadback.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/Bitmap.h"
#include "System/Log/ILog.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace LuaNativeImageReadback {
// Invoked inside LuaNativeUI's existing exception-to-Lua boundary.
inline int SaveImage(lua_State* L)
{
	const auto integer=[L](int index){
		const double value=luaL_checknumber(L,index);
		if(!std::isfinite(value) || value<std::numeric_limits<std::int32_t>::min() ||
			value>std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("SaveImage integer argument outside finite signed range");
		return std::int32_t(value);
	};
	const auto x=integer(1),y=integer(2),width=integer(3),height=integer(4);
	const std::string filename=luaL_checkstring(L,5);
	// Preserve original LuaOpenGL::SaveImage validation, diagnostic text and
	// no-result behavior. Do not run a GPU prefix for an invalid save request.
	if(!LuaIO::SafeWritePath(filename) || !LuaIO::IsSimplePath(filename)){
		LOG_L(L_WARNING,"gl.SaveImage: tried to write to illegal path localtion");return 0;
	}
	if(width<=0 || height<=0){
		LOG_L(L_WARNING,"gl.SaveImage: tried to write empty image");return 0;
	}
	bool alpha=false,yflip=true,grayscale16=false;int readBuffer=0;
	if(lua_istable(L,6)){
		lua_getfield(L,6,"alpha");alpha=luaL_optboolean(L,-1,alpha);lua_pop(L,1);
		lua_getfield(L,6,"yflip");yflip=luaL_optboolean(L,-1,yflip);lua_pop(L,1);
		lua_getfield(L,6,"grayscale16bit");grayscale16=luaL_optboolean(L,-1,grayscale16);lua_pop(L,1);
		lua_getfield(L,6,"readbuffer");readBuffer=luaL_optint(L,-1,readBuffer);lua_pop(L,1);
	}
	if(!globalRendering)throw std::runtime_error("Native SaveImage has no original rendering frame");
	auto pixels=rendering::ReadNativeImage(globalRendering->drawFrame,
		{x,y,std::uint32_t(width),std::uint32_t(height),std::uint32_t(readBuffer)});
	CBitmap bitmap;bitmap.Alloc(width,height);
	std::memcpy(bitmap.GetRawMem(),pixels.data(),pixels.size());
	if(yflip)bitmap.ReverseYAxis();
	// Encoding and filesystem result remain synchronous and use the original
	// bitmap implementation, including its grayscale16 channel conversion.
	lua_pushboolean(L,grayscale16?bitmap.SaveGrayScale(filename):bitmap.Save(filename,!alpha));
	return 1;
}
} // namespace LuaNativeImageReadback
