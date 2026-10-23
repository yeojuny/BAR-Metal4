/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeAirjetsPrograms.h"
namespace rendering::paralyze {
inline constexpr uint64_t VertexHash=829430707067506089ULL,FragmentHash=17707575499206544934ULL;
inline constexpr airjets::SourceIdentity Sources[]={
 {"LuaUI/Widgets/gfx_paralyze_effect.lua",11319453836141454234ULL},
 {"modules/graphics/luashader.lua",15173167305575988042ULL},
 {"modules/graphics/instancevboidtable.lua",12052612169495720187ULL},
};
inline bool ValidateProgram(std::string_view v,std::string_view f,std::string_view engine,std::string_view quaternion,const std::string& defines){
 if(NativeUnitPrimitiveSourceHash(v)!=VertexHash||NativeUnitPrimitiveSourceHash(f)!=FragmentHash)throw std::runtime_error("Original Paralyze source/quaternion differs");unitEffectsProgram::ValidateHelpers(engine,quaternion);
 std::istringstream input(defines);std::string directive,key,value;std::map<std::string,std::string> actual;while(input>>directive){if(directive!="#define"||!(input>>key>>value)||!actual.emplace(key,value).second)throw std::runtime_error("Paralyze definitions malformed");}
 if(actual.size()!=3||actual["USEQUATERNIONS"]!="1"||actual["PARALYZE_NOISE_OCTAVES"]!="4"||(actual["EMPREWORK"]!="0"&&actual["EMPREWORK"]!="1"))throw std::runtime_error("Original Paralyze definitions differ");return actual["EMPREWORK"]=="1";
}
}
