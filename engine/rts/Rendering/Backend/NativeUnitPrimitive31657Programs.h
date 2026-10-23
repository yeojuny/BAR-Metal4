/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitPrimitivePrograms.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rendering::unitPrimitive31657Program {
// Selected game31657/2f0a840 source identities. This profile covers only the
// building AO, feature AO, TeamPlatter and original UnitTracker producers, including the exact selected-units producer; SSAO is separate.
struct SourceIdentity { std::string_view path; std::uint64_t hash; bool lua; };
inline constexpr std::array<SourceIdentity,14> Sources{{
	{"LuaUI/Widgets/gui_selectedunits_gl4.lua",17182888598436896268ULL,true}, // SHA256 bce780e5aeb73b826f138323d05e846da45dd542d037ef280dfe43dd9d96cf79
	{"LuaUI/Widgets/gui_enemy_spotter.lua",13958551530279508913ULL,true}, // SHA256 a2be861d65be89d2c85041f2435da2b008fb46784633846e55136959cdc64d19
	{"LuaUI/Widgets/api_unit_tracker_gl4.lua",10173124021414281512ULL,true}, // SHA256 6d8b974b43ea8bd6fd8b868019dc68aa9078d3754d5464c1c0eddea555096e52
	{"LuaUI/Widgets/gui_ground_ao_plates_gl4.lua",16221502360500712451ULL,true}, // SHA256 290fdead016db18324a4d9d7d2c09104207bed84e6d097e4bcdb9eedbd613989
	{"LuaUI/Widgets/gui_ground_ao_plates_features_gl4.lua",6573443227322071357ULL,true}, // SHA256 238fedf8a0356ce863f77fd8ecc4aff3202fc59b6eb0f9b5336b5de54912a57e
	{"LuaUI/Widgets/gui_team_platter.lua",14437241877826152362ULL,true}, // SHA256 bd91351352122ad505394c0a3af6c68d73acf63bf609d4584784905cb1c96abf
	{"LuaUI/Include/DrawPrimitiveAtUnit.lua",12764826900055679525ULL,true}, // SHA256 1e2b85c34cd2e93f73d69a2d86bb6a0801df7d0a6fd65a2edb14afea31edfecd
	{"modules/graphics/instancevbotable.lua",10155353677033079655ULL,true}, // SHA256 a295511e741d2965647697fce3ad3839835ad172e56909937f2ccf9dbbf80e47
	{"modules/graphics/luashader.lua",15173167305575988042ULL,true}, // SHA256 61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23
	{"unittextures/decals/unitaoplates_atlas.lua",1437600297285110498ULL,true}, // SHA256 c6e5d72c467137ba872dbbfb4d3a37ba401a1313b4d53fba5a1af96b0eabf334
	{"unittextures/decals_features/featureaoplates_atlas.lua",13766119380244836037ULL,true}, // SHA256 bc68a17c525859c4d66b93b7e585f8e380445cf3feb280dbc255031b3b6d917f
	{"LuaUI/Shaders/DrawPrimitiveAtUnit.vert.glsl",3149931361356126697ULL,false}, // SHA256 2195d2d24431400a5b700db2ee96829628749209aebdc3a1f9ff318e9018f034
	{"LuaUI/Shaders/DrawPrimitiveAtUnit.geom.glsl",4116190329713313035ULL,false}, // SHA256 4b1d7527e4e3e9ebfc73866e24bf7ec27992957b6af17a4658707d62cc6aa258
	{"LuaUI/Shaders/DrawPrimitiveAtUnit.frag.glsl",5541454101484968067ULL,false}, // SHA256 b688ab7e44116a174c47d9d315fcfcd83841d30bd25f6615a1737042c1026e66
}};
inline constexpr std::uint64_t VertexHash=3149931361356126697ULL; // SHA256 2195d2d24431400a5b700db2ee96829628749209aebdc3a1f9ff318e9018f034
inline constexpr std::uint64_t GeometryHash=4116190329713313035ULL; // SHA256 4b1d7527e4e3e9ebfc73866e24bf7ec27992957b6af17a4658707d62cc6aa258
inline constexpr std::uint64_t FragmentHash=5541454101484968067ULL; // SHA256 b688ab7e44116a174c47d9d315fcfcd83841d30bd25f6615a1737042c1026e66
inline constexpr std::uint64_t EngineDefinitionsHash=80087823534942856ULL; // SHA256 8e70b17af4dd781ac46497120c735aca892cf74f2e143e9af5944021fafa2936
inline constexpr std::uint64_t QuaternionHash=12853114528900136756ULL; // SHA256 8ff34c8b6fec418d15ac9a6ff561d9e84a7238d4f1496cc724be5018e2d37899
// The 31657 engine definition contains the same UBO layout and no appended
// map-specific waterBlend block. AO POST_SHADING replaces the new generic
// transparency/teamcolorization fragment expression in both AO variants. The
// TeamPlatter consumer reads the actual per-draw transparency value instead.

// The selected team widget removes USE_QUADS and retains the other nineteen
// numeric definitions. TRANSPARENCY remains its original mutable initial value.
inline constexpr auto TeamNumbers=[] {
 std::array<unitPrimitiveProgram::NumberDefine,19> result{};
 std::size_t next=0;
 for(const auto& definition:unitPrimitiveProgram::TeamNumbers)
  if(definition.name!="USE_QUADS")result[next++]=definition;
 return result;
}();
// Exact original unitTracker initGL4 overrides the shared DPAT defaults.
inline constexpr auto UnitTrackerNumbers=[] {
	auto result=unitPrimitiveProgram::TeamNumbers;
	for(auto& definition:result){
		if(definition.name=="TRANSPARENCY")definition.value=.5f;
		if(definition.name=="USETEXTURE")definition.value=1.f;
	}
	return result;
}();

inline constexpr auto EnemySpotterNumbers=[] {
	std::array<unitPrimitiveProgram::NumberDefine,17> result{};size_t next=0;
	for(auto definition:UnitTrackerNumbers){
		if(definition.name=="USE_CIRCLES"||definition.name=="USE_CORNERRECT"||definition.name=="USE_TRIANGLES")continue;
		if(definition.name=="TRANSPARENCY")definition.value=.23f;
		if(definition.name=="MAXVERTICES")definition.value=4.f;
		result[next++]=definition;
	}
	return result;
}();

// Exact 31657 selection shader: no texture/quads; runtime opacity and team mix.
inline constexpr auto SelectedNumbers=[] {
 auto result=TeamNumbers;
 for(auto& d:result){
  if(d.name=="ANIMATION")d.value=1.f;
  if(d.name=="HEIGHTOFFSET")d.value=4.f;
  if(d.name=="INITIALSIZE")d.value=.75f;
 }
 return result;
}();
inline constexpr auto SelectedUnfinishedNumbers=[] {
 auto result=SelectedNumbers;
 for(auto& d:result)if(d.name=="INITIALSIZE")d.value=.9999f;
 return result;
}();
inline constexpr auto SelectedTexts=[] {
 auto result=unitPrimitiveProgram::TeamTexts;
 for(auto& d:result)if(d.name=="POST_SHADING")d.value="fragColor.rgba = vec4(mix(g_color.rgb + addRadius, vec3(1.0), 1.0 - teamcolorization), transparency + addRadius);";
 return result;
}();

}
