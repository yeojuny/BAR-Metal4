/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitPrimitive31657Programs.h"
namespace rendering::unitIconsProgram {
inline constexpr std::array<std::string_view,6> Names{"energy iconsShader GL4","hold fire iconsShader GL4","unitGroupsShader GL4","energy iconsShader GL4","repeat iconsShader GL4","energy iconsShader GL4"};
inline constexpr std::array<unitPrimitive31657Program::SourceIdentity,12> Sources{{
 {"LuaUI/Widgets/gui_unit_energy_icons.lua",4379934148737016315ULL,true}, // 0a0385dcd25b68520daa6644032cf202c2fa03d983bf4d0bcaf05528415e5c27
 {"LuaUI/Widgets/gui_unit_firestate_icons.lua",440017858587459803ULL,true}, // 660e3423f0eacaf8d36ec1dfd0bb1e71f2934da1ee676517f2af90a44fc9f2bc
 {"LuaUI/Widgets/gui_unit_group_number.lua",5134929377185385754ULL,true}, // f5d70f97bbebca864bbfa92b45f83cf16e566dfb319ac0db0c8e2c740d92ec20
 {"LuaUI/Widgets/gui_unit_idlebuilder_icons.lua",11223233514266317116ULL,true}, // bd87c802c7b8592b9b4cb6c4fdf134aa683fbe35cf605ee0453b418a3bd155c8
 {"LuaUI/Widgets/gui_unit_repeat_icon.lua",10225232575887199586ULL,true}, // 58c60c64ad3bbfe2839ae9ec6db1301ef258d3bd3d56f12a4670e8114032831c
 {"LuaUI/Widgets/gui_unit_wait_icons.lua",7005480600645722925ULL,true}, // e24d0f51287f8b892740593d764261c33cfff91106df0e4db0ac10d8a53624dd
 {"LuaUI/Include/DrawPrimitiveAtUnit.lua",12764826900055679525ULL,true}, // 1e2b85c34cd2e93f73d69a2d86bb6a0801df7d0a6fd65a2edb14afea31edfecd
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL,true}, // a295511e741d2965647697fce3ad3839835ad172e56909937f2ccf9dbbf80e47
 {"modules/graphics/luashader.lua",15173167305575988042ULL,true}, // 61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.vert.glsl",3149931361356126697ULL,false}, // 2195d2d24431400a5b700db2ee96829628749209aebdc3a1f9ff318e9018f034
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.geom.glsl",4116190329713313035ULL,false}, // 4b1d7527e4e3e9ebfc73866e24bf7ec27992957b6af17a4658707d62cc6aa258
 {"LuaUI/Shaders/DrawPrimitiveAtUnit.frag.glsl",5541454101484968067ULL,false}, // b688ab7e44116a174c47d9d315fcfcd83841d30bd25f6615a1737042c1026e66
}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> EnergyIconNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.1f},{"CLIPTOLERANCE",1.2f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.22f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.75f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> EnergyIconTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, texcolor.a * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> FireStateIconNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.05f},{"CLIPTOLERANCE",1.2f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.22f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.85f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> FireStateIconTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, texcolor.a * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> GroupNumberNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",0.0f},{"BREATHESIZE",0.0f},{"CLIPTOLERANCE",1.1f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",5.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.5f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",1.0f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",256.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> GroupNumberTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb* vec3(0.8, 1.0, 0.8), fragColor.a);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET","primitiveCoords.xz += vec2(20, -5);"},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> IdleBuilderIconNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.0f},{"CLIPTOLERANCE",1.2f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.22f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.75f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> IdleBuilderIconTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, texcolor.a * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> RepeatIconNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.05f},{"CLIPTOLERANCE",1.2f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.22f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.85f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> RepeatIconTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, texcolor.a * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<unitPrimitiveProgram::NumberDefine,18> WaitIconNumbers{{{"ANIMATION",1.0f},{"BILLBOARD",1.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.0f},{"CLIPTOLERANCE",1.2f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.22f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.75f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",256.0f}}};
inline constexpr std::array<unitPrimitiveProgram::TextDefine,6> WaitIconTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY"," gl_Position.z = (gl_Position.z) - 512.0 / (gl_Position.w); // send 16 elmos forward in depth buffer"},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, texcolor.a * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline bool ImageMatches(NativeUnitPrimitiveKind kind,std::string_view source) {
 switch(kind){
 case NativeUnitPrimitiveKind::EnergyIcon:return source=="LuaUI/Images/energy-red.png";
 case NativeUnitPrimitiveKind::FireStateIcon:return source=="LuaUI/Images/holdfire.png" || source=="LuaUI/Images/returnfire.png";
 case NativeUnitPrimitiveKind::GroupNumber:return source=="LuaUI/Images/healtbars_exo4.tga";
 case NativeUnitPrimitiveKind::RepeatIcon:return source=="LuaUI/Images/repeat.png";
 default:break;
 }
 if(kind==NativeUnitPrimitiveKind::IdleBuilderIcon){
  constexpr std::string_view prefix="Luaui/Images/idleicon/idlecon_";
  if(!source.starts_with(prefix) || !source.ends_with(".png"))return false;
  source.remove_prefix(prefix.size());source.remove_suffix(4);
  if(source.size()!=3 || source[0]!='0' || source[1]<'0' || source[1]>'5' || source[2]<'0' || source[2]>'9')return false;
  const unsigned value=(source[1]-'0')*10+source[2]-'0';return value>=1 && value<=59;
 }
 if(kind==NativeUnitPrimitiveKind::WaitIcon){
  constexpr std::string_view prefix="anims/icexuick_200/cursorwait_";
  if(!source.starts_with(prefix) || !source.ends_with(".png"))return false;
  source.remove_prefix(prefix.size());source.remove_suffix(4);
  if(source.empty() || source.size()>2 || source[0]<'1' || source[0]>'9')return false;
  unsigned value=0;for(char c:source){if(c<'0' || c>'9')return false;value=value*10+c-'0';}return value<=44;
 }
 return false;
}
}
