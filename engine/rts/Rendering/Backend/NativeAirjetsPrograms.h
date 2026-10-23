/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitPrimitivePrograms.h"
#include "NativeUnitEffectsPrograms.h"
#include "NativeUnitPrimitiveData.h"
#include <map>
#include <sstream>
namespace rendering::airjets {
inline constexpr uint64_t VertexHash=4813630176280487185ULL,FragmentHash=10000695880555119744ULL;
struct SourceIdentity {std::string_view path;uint64_t hash;};
inline constexpr SourceIdentity Sources[]={
 {"LuaUI/Widgets/gfx_airjets_gl4.lua",16164021094436186303ULL},
 {"modules/graphics/luashader.lua",15173167305575988042ULL},
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL},
 {"luaui/configs/airjet_effects.lua",8160320817351791880ULL},
};
inline void ValidateProgram(std::string_view vertex,std::string_view fragment,std::string_view engine,std::string_view quaternion,const std::string& defines){
 if(NativeUnitPrimitiveSourceHash(vertex)!=VertexHash||NativeUnitPrimitiveSourceHash(fragment)!=FragmentHash)throw std::runtime_error("Original Airjets shader/quaternion source differs");
 unitEffectsProgram::ValidateHelpers(engine,quaternion);std::istringstream lines(defines);std::string directive,key,value;std::map<std::string,std::string> actual;
 while(lines>>directive){if(directive!="#define"||!(lines>>key>>value)||!actual.emplace(key,value).second)throw std::runtime_error("Original Airjets defines malformed");}
 if(actual!=std::map<std::string,std::string>{{"DEBUG","0"},{"USEQUATERNIONS","1"}})throw std::runtime_error("Original Airjets compiled defines differ");
}
}
