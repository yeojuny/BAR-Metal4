/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <array>
#include <string_view>
#include <stdexcept>
#include <cmath>
#include <cstddef>
namespace rendering {
enum class NativeDiffusePainterProgram : unsigned { Composite,Stamp,Channel,Copy };
inline std::weak_ptr<const void>& NativeDiffusePainterOwner(){static std::weak_ptr<const void> owner;return owner;}
struct NativeDiffusePainterUniforms {
	std::array<float,4> squareOrigin{};
	std::array<float,4> squareSize{};
	std::array<float,4> mapSize{};
	std::array<float,4> layerColor{};
	std::array<float,4> layerOpacity{};
	std::array<std::int32_t,4> blendMode{};
	std::array<std::int32_t,4> useLayerTex{};
	std::array<float,4> tileScale{};
	std::array<std::int32_t,4> altEnabled{};
	std::array<float,4> altMin{};
	std::array<float,4> altMax{};
	std::array<float,4> altFalloffLo{};
	std::array<float,4> altFalloffHi{};
	std::array<std::int32_t,4> slopeEnabled{};
	std::array<float,4> slopeMinCos{};
	std::array<float,4> slopeMaxCos{};
	std::array<float,4> slopeFalloffLo{};
	std::array<float,4> slopeFalloffHi{};
	std::array<std::int32_t,4> handPaintEnabled{};
	std::array<std::int32_t,4> hydroEnabled{};
	std::array<float,4> hydroStrength{};
	std::array<float,4> hydroFalloffLo{};
	std::array<float,4> hydroFalloffHi{};
	std::array<std::int32_t,4> thermoEnabled{};
	std::array<float,4> thermoAngle{};
	std::array<float,4> thermoFalloff{};
	std::array<float,4> brushPos{};
	std::array<float,4> brushRadius{};
	std::array<float,4> brushStrength{};
	std::array<float,4> brushCurve{};
	std::array<std::int32_t,4> brushErase{};
	std::array<std::int32_t,4> useNormalTex{};
	std::array<std::int32_t,4> useRoughTex{};
	std::array<float,4> pbrStrength{};
	std::array<float,4> fractalAmount{};
	std::array<float,4> fractalFreq{};
	std::array<float,4> dstUvOffset{};
	std::array<float,4> dstUvScale{};
	std::array<std::int32_t,4> channelMode{};
	std::array<float,4> glowStrength{};
	std::array<float,4> specIntensity{};
};
static_assert(sizeof(NativeDiffusePainterUniforms)==656);
static_assert(offsetof(NativeDiffusePainterUniforms,squareOrigin)==0);
static_assert(offsetof(NativeDiffusePainterUniforms,squareSize)==16);
static_assert(offsetof(NativeDiffusePainterUniforms,mapSize)==32);
static_assert(offsetof(NativeDiffusePainterUniforms,layerColor)==48);
static_assert(offsetof(NativeDiffusePainterUniforms,layerOpacity)==64);
static_assert(offsetof(NativeDiffusePainterUniforms,blendMode)==80);
static_assert(offsetof(NativeDiffusePainterUniforms,useLayerTex)==96);
static_assert(offsetof(NativeDiffusePainterUniforms,tileScale)==112);
static_assert(offsetof(NativeDiffusePainterUniforms,altEnabled)==128);
static_assert(offsetof(NativeDiffusePainterUniforms,altMin)==144);
static_assert(offsetof(NativeDiffusePainterUniforms,altMax)==160);
static_assert(offsetof(NativeDiffusePainterUniforms,altFalloffLo)==176);
static_assert(offsetof(NativeDiffusePainterUniforms,altFalloffHi)==192);
static_assert(offsetof(NativeDiffusePainterUniforms,slopeEnabled)==208);
static_assert(offsetof(NativeDiffusePainterUniforms,slopeMinCos)==224);
static_assert(offsetof(NativeDiffusePainterUniforms,slopeMaxCos)==240);
static_assert(offsetof(NativeDiffusePainterUniforms,slopeFalloffLo)==256);
static_assert(offsetof(NativeDiffusePainterUniforms,slopeFalloffHi)==272);
static_assert(offsetof(NativeDiffusePainterUniforms,handPaintEnabled)==288);
static_assert(offsetof(NativeDiffusePainterUniforms,hydroEnabled)==304);
static_assert(offsetof(NativeDiffusePainterUniforms,hydroStrength)==320);
static_assert(offsetof(NativeDiffusePainterUniforms,hydroFalloffLo)==336);
static_assert(offsetof(NativeDiffusePainterUniforms,hydroFalloffHi)==352);
static_assert(offsetof(NativeDiffusePainterUniforms,thermoEnabled)==368);
static_assert(offsetof(NativeDiffusePainterUniforms,thermoAngle)==384);
static_assert(offsetof(NativeDiffusePainterUniforms,thermoFalloff)==400);
static_assert(offsetof(NativeDiffusePainterUniforms,brushPos)==416);
static_assert(offsetof(NativeDiffusePainterUniforms,brushRadius)==432);
static_assert(offsetof(NativeDiffusePainterUniforms,brushStrength)==448);
static_assert(offsetof(NativeDiffusePainterUniforms,brushCurve)==464);
static_assert(offsetof(NativeDiffusePainterUniforms,brushErase)==480);
static_assert(offsetof(NativeDiffusePainterUniforms,useNormalTex)==496);
static_assert(offsetof(NativeDiffusePainterUniforms,useRoughTex)==512);
static_assert(offsetof(NativeDiffusePainterUniforms,pbrStrength)==528);
static_assert(offsetof(NativeDiffusePainterUniforms,fractalAmount)==544);
static_assert(offsetof(NativeDiffusePainterUniforms,fractalFreq)==560);
static_assert(offsetof(NativeDiffusePainterUniforms,dstUvOffset)==576);
static_assert(offsetof(NativeDiffusePainterUniforms,dstUvScale)==592);
static_assert(offsetof(NativeDiffusePainterUniforms,channelMode)==608);
static_assert(offsetof(NativeDiffusePainterUniforms,glowStrength)==624);
static_assert(offsetof(NativeDiffusePainterUniforms,specIntensity)==640);
struct NativeDiffusePainterParameters {
 NativeDiffusePainterUniforms uniforms;
 std::array<std::array<std::uint32_t,4>,4> sampling{};
};
static_assert(sizeof(NativeDiffusePainterParameters)==720);
static_assert(offsetof(NativeDiffusePainterParameters,sampling)==656);
struct NativeDiffusePainterUniformField {std::string_view name;unsigned components;bool integer;unsigned programs;};
inline constexpr std::array<NativeDiffusePainterUniformField,41> NativeDiffusePainterUniformFields{{
 {"squareOrigin",2,false,3},
 {"squareSize",2,false,3},
 {"mapSize",2,false,5},
 {"layerColor",3,false,7},
 {"layerOpacity",1,false,1},
 {"blendMode",1,true,1},
 {"useLayerTex",1,true,7},
 {"tileScale",1,false,7},
 {"altEnabled",1,true,1},
 {"altMin",1,false,1},
 {"altMax",1,false,1},
 {"altFalloffLo",1,false,1},
 {"altFalloffHi",1,false,1},
 {"slopeEnabled",1,true,1},
 {"slopeMinCos",1,false,1},
 {"slopeMaxCos",1,false,1},
 {"slopeFalloffLo",1,false,1},
 {"slopeFalloffHi",1,false,1},
 {"handPaintEnabled",1,true,1},
 {"hydroEnabled",1,true,1},
 {"hydroStrength",1,false,1},
 {"hydroFalloffLo",1,false,1},
 {"hydroFalloffHi",1,false,1},
 {"thermoEnabled",1,true,1},
 {"thermoAngle",1,false,1},
 {"thermoFalloff",1,false,1},
 {"brushPos",2,false,6},
 {"brushRadius",1,false,6},
 {"brushStrength",1,false,6},
 {"brushCurve",1,false,6},
 {"brushErase",1,true,6},
 {"useNormalTex",1,true,6},
 {"useRoughTex",1,true,6},
 {"pbrStrength",1,false,2},
 {"fractalAmount",1,false,6},
 {"fractalFreq",1,false,6},
 {"dstUvOffset",2,false,4},
 {"dstUvScale",2,false,4},
 {"channelMode",1,true,4},
 {"glowStrength",1,false,4},
 {"specIntensity",1,false,4},
}};
inline constexpr std::uint64_t NativeDiffuseVertexHash=17012528594867871139ull;
inline constexpr std::array<std::uint64_t,4> NativeDiffuseFragmentHashes{5560071221307988434ull,14835861997943966710ull,5561690818394770344ull,13663162182795998738ull};
struct NativeDiffusePainterInput {
 NativeDiffusePainterProgram program=NativeDiffusePainterProgram::Composite;
 NativeDiffusePainterUniforms uniforms;
 std::shared_ptr<const void> compiled;
 std::array<ScreenSecondaryTexture,4> images;
};
inline void ValidateNativeDiffusePainter(const NativeDiffusePainterInput& input){
 if(!input.compiled||input.program>NativeDiffusePainterProgram::Copy)throw std::runtime_error("Diffuse Painter actual compiled program missing");
 const unsigned count=input.program==NativeDiffusePainterProgram::Copy?1:4;
 for(unsigned i=0;i<count;++i){const auto& image=input.images[i];
  if(!image.texture||(i==0&&image.texture->incomplete)||image.uvMode!=ScreenUVMode::Normalized||image.texture->format==ScreenTextureFormat::Depth16Unorm||image.texture->format==ScreenTextureFormat::Depth24Unorm||image.texture->format==ScreenTextureFormat::Depth32Float)
   throw std::runtime_error("Diffuse Painter requires its actual color/height inputs and complete current canvas");
  ValidateScreenSampler(image.sampler);
 }
 for(float v:input.uniforms.squareOrigin)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform squareOrigin");
 for(float v:input.uniforms.squareSize)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform squareSize");
 for(float v:input.uniforms.mapSize)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform mapSize");
 for(float v:input.uniforms.layerColor)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform layerColor");
 for(float v:input.uniforms.layerOpacity)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform layerOpacity");
 for(float v:input.uniforms.tileScale)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform tileScale");
 for(float v:input.uniforms.altMin)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform altMin");
 for(float v:input.uniforms.altMax)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform altMax");
 for(float v:input.uniforms.altFalloffLo)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform altFalloffLo");
 for(float v:input.uniforms.altFalloffHi)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform altFalloffHi");
 for(float v:input.uniforms.slopeMinCos)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform slopeMinCos");
 for(float v:input.uniforms.slopeMaxCos)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform slopeMaxCos");
 for(float v:input.uniforms.slopeFalloffLo)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform slopeFalloffLo");
 for(float v:input.uniforms.slopeFalloffHi)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform slopeFalloffHi");
 for(float v:input.uniforms.hydroStrength)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform hydroStrength");
 for(float v:input.uniforms.hydroFalloffLo)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform hydroFalloffLo");
 for(float v:input.uniforms.hydroFalloffHi)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform hydroFalloffHi");
 for(float v:input.uniforms.thermoAngle)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform thermoAngle");
 for(float v:input.uniforms.thermoFalloff)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform thermoFalloff");
 for(float v:input.uniforms.brushPos)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform brushPos");
 for(float v:input.uniforms.brushRadius)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform brushRadius");
 for(float v:input.uniforms.brushStrength)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform brushStrength");
 for(float v:input.uniforms.brushCurve)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform brushCurve");
 for(float v:input.uniforms.pbrStrength)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform pbrStrength");
 for(float v:input.uniforms.fractalAmount)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform fractalAmount");
 for(float v:input.uniforms.fractalFreq)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform fractalFreq");
 for(float v:input.uniforms.dstUvOffset)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform dstUvOffset");
 for(float v:input.uniforms.dstUvScale)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform dstUvScale");
 for(float v:input.uniforms.glowStrength)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform glowStrength");
 for(float v:input.uniforms.specIntensity)if(!std::isfinite(v))throw std::runtime_error("Diffuse Painter nonfinite uniform specIntensity");
}
} // namespace rendering
