/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitPrimitive31657Programs.h"
namespace rendering::allySelectedProgram {
inline constexpr std::string_view Name="allySelectedUnitsGroundShader GL4";
inline constexpr std::array<unitPrimitive31657Program::SourceIdentity,7> Sources{{
 {"LuaUI/Widgets/gui_allyselectedunits.lua",12804267815257305312ULL,true}, // 142bd41d3592398d3507fd26effe496824f6de6347f93409ea8a2f12c9ba010d
 {"LuaUI/Include/DrawPrimitiveAtUnit.lua",12764826900055679525ULL,true}, // 1e2b85c34cd2e93f73d69a2d86bb6a0801df7d0a6fd65a2edb14afea31edfecd
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL,true}, // a295511e741d2965647697fce3ad3839835ad172e56909937f2ccf9dbbf80e47
 {"modules/graphics/luashader.lua",15173167305575988042ULL,true}, // 61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.vert.glsl",3149931361356126697ULL,false}, // 2195d2d24431400a5b700db2ee96829628749209aebdc3a1f9ff318e9018f034
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.geom.glsl",4116190329713313035ULL,false}, // 4b1d7527e4e3e9ebfc73866e24bf7ec27992957b6af17a4658707d62cc6aa258
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.frag.glsl",5541454101484968067ULL,false}, // b688ab7e44116a174c47d9d315fcfcd83841d30bd25f6615a1737042c1026e66
}};
inline constexpr auto Numbers=[] {
 std::array<unitPrimitiveProgram::NumberDefine,20> result{};
 std::size_t i=0;
 for(auto definition:unitPrimitive31657Program::TeamNumbers){
  if(definition.name=="ANIMATION")definition.value=1;
  if(definition.name=="INITIALSIZE")definition.value=.75f;
  if(definition.name=="GROWTHRATE")definition.value=8;
  if(definition.name=="HEIGHTOFFSET")definition.value=3.9f;
  if(definition.name=="ROTATE_CIRCLES")definition.value=0;
  result[i++]=definition;
 }
 result[i]={"LINETRANSPARANCY",0};return result;
}();
inline constexpr auto Texts=[] {
 auto result=unitPrimitiveProgram::TeamTexts;
 for(auto& definition:result)if(definition.name=="POST_SHADING")definition.value="fragColor.rgba = vec4(g_color.rgb, TRANSPARENCY + step( 0.01, addRadius) * LINETRANSPARANCY);";
 return result;
}();
}
