/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitIconsPrograms.h"
namespace rendering::rankHaloProgram {
inline constexpr std::array<unitPrimitive31657Program::SourceIdentity,8> Sources{{
 {"LuaUI/Widgets/gui_rank_icons_gl4.lua",1722491373225177981ULL,true},
 {"LuaUI/Widgets/gui_resurrection_halos_gl4.lua",5618749545081933215ULL,true},
 {"LuaUI/Include/DrawPrimitiveAtUnit.lua",12764826900055679525ULL,true},
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL,true},
 {"modules/graphics/luashader.lua",15173167305575988042ULL,true},
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.vert.glsl",3149931361356126697ULL,false},
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.geom.glsl",4116190329713313035ULL,false},
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.frag.glsl",5541454101484968067ULL,false},
}};
inline constexpr auto RankNumbers=[] {
 auto result=unitIconsProgram::GroupNumberNumbers;
 for(auto& d:result){if(d.name=="INITIALSIZE")d.value=.25f;if(d.name=="GROWTHRATE")d.value=16;}
 return result;
}();
inline constexpr auto RankTexts=[] {
 auto result=unitIconsProgram::GroupNumberTexts;
 for(auto& d:result){
  if(d.name=="PRE_OFFSET")d.value="";
  if(d.name=="POST_VERTEX")d.value="v_lengthwidthcornerheight.xy *= parameters.y * 4.5 + (1250 - abs(cameraDistance-1250))*0.016;";
  if(d.name=="POST_SHADING")d.value="fragColor.rgba = vec4(texcolor.rgb* (1.0 + g_uv.w), texcolor.a * g_uv.z); if (fragColor.a <= 0.0) discard;";
 }
 return result;
}();
inline constexpr auto HaloNumbers=[] {
 auto result=unitPrimitiveProgram::TeamNumbers;
 for(auto& d:result){
  if(d.name=="TRANSPARENCY")d.value=.5f;if(d.name=="ANIMATION")d.value=1;
  if(d.name=="BREATHERATE")d.value=15;if(d.name=="BREATHESIZE")d.value=.075f;
  if(d.name=="MAXVERTICES")d.value=4;if(d.name=="INITIALSIZE")d.value=.5f;
  if(d.name=="GROWTHRATE")d.value=15;if(d.name=="USETEXTURE")d.value=1;
 }
 return result;
}();
inline constexpr auto HaloTexts=[] {auto result=unitPrimitiveProgram::TeamTexts;for(auto& d:result)if(d.name=="POST_SHADING")d.value="fragColor.rgba = texcolor;";return result;}();
}
