/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <array>
#include <string_view>
#include <stdexcept>
#include <cmath>
#include <cstddef>
namespace rendering {
enum class NativeSplatPainterProgram : unsigned { Paint,Copy,Overlay };
inline std::weak_ptr<const void>& NativeSplatPainterOwner(){static std::weak_ptr<const void> owner;return owner;}
inline std::weak_ptr<const void>& NativeSplatOverlayOwner(){static std::weak_ptr<const void> owner;return owner;}
struct NativeSplatPainterUniforms {
	std::array<float,4> brushPos{};
	std::array<float,4> brushRadius{};
	std::array<float,4> brushStrength{};
	std::array<std::int32_t,4> brushChannel{};
	std::array<std::int32_t,4> brushErase{};
	std::array<float,4> texSize{};
	std::array<float,4> mapSize{};
	std::array<float,4> brushCurve{};
	std::array<std::int32_t,4> brushShape{};
	std::array<float,4> brushRotation{};
	std::array<float,4> fractalAmount{};
	std::array<float,4> fractalFreq{};
	std::array<std::int32_t,4> sfEnabled{};
	std::array<std::int32_t,4> sfAvoidWater{};
	std::array<std::int32_t,4> sfAvoidCliffs{};
	std::array<float,4> sfSlopeMax{};
	std::array<std::int32_t,4> sfPreferSlopes{};
	std::array<float,4> sfSlopeMin{};
	std::array<std::int32_t,4> sfAltMinEnable{};
	std::array<float,4> sfAltMin{};
	std::array<std::int32_t,4> sfAltMaxEnable{};
	std::array<float,4> sfAltMax{};
	std::array<std::int32_t,4> infAltOn{};
	std::array<float,4> infAlt{};
	std::array<std::int32_t,4> infSlopeOn{};
	std::array<float,4> infSlope{};
};
static_assert(sizeof(NativeSplatPainterUniforms)==416);
static_assert(offsetof(NativeSplatPainterUniforms,brushPos)==0);
static_assert(offsetof(NativeSplatPainterUniforms,brushRadius)==16);
static_assert(offsetof(NativeSplatPainterUniforms,brushStrength)==32);
static_assert(offsetof(NativeSplatPainterUniforms,brushChannel)==48);
static_assert(offsetof(NativeSplatPainterUniforms,brushErase)==64);
static_assert(offsetof(NativeSplatPainterUniforms,texSize)==80);
static_assert(offsetof(NativeSplatPainterUniforms,mapSize)==96);
static_assert(offsetof(NativeSplatPainterUniforms,brushCurve)==112);
static_assert(offsetof(NativeSplatPainterUniforms,brushShape)==128);
static_assert(offsetof(NativeSplatPainterUniforms,brushRotation)==144);
static_assert(offsetof(NativeSplatPainterUniforms,fractalAmount)==160);
static_assert(offsetof(NativeSplatPainterUniforms,fractalFreq)==176);
static_assert(offsetof(NativeSplatPainterUniforms,sfEnabled)==192);
static_assert(offsetof(NativeSplatPainterUniforms,sfAvoidWater)==208);
static_assert(offsetof(NativeSplatPainterUniforms,sfAvoidCliffs)==224);
static_assert(offsetof(NativeSplatPainterUniforms,sfSlopeMax)==240);
static_assert(offsetof(NativeSplatPainterUniforms,sfPreferSlopes)==256);
static_assert(offsetof(NativeSplatPainterUniforms,sfSlopeMin)==272);
static_assert(offsetof(NativeSplatPainterUniforms,sfAltMinEnable)==288);
static_assert(offsetof(NativeSplatPainterUniforms,sfAltMin)==304);
static_assert(offsetof(NativeSplatPainterUniforms,sfAltMaxEnable)==320);
static_assert(offsetof(NativeSplatPainterUniforms,sfAltMax)==336);
static_assert(offsetof(NativeSplatPainterUniforms,infAltOn)==352);
static_assert(offsetof(NativeSplatPainterUniforms,infAlt)==368);
static_assert(offsetof(NativeSplatPainterUniforms,infSlopeOn)==384);
static_assert(offsetof(NativeSplatPainterUniforms,infSlope)==400);
struct NativeSplatPainterUniformField {std::string_view name;unsigned components;bool integer;};
inline constexpr std::array<NativeSplatPainterUniformField,26> NativeSplatPainterUniformFields{{
	{"brushPos",2,false},
	{"brushRadius",1,false},
	{"brushStrength",1,false},
	{"brushChannel",1,true},
	{"brushErase",1,true},
	{"texSize",2,false},
	{"mapSize",2,false},
	{"brushCurve",1,false},
	{"brushShape",1,true},
	{"brushRotation",1,false},
	{"fractalAmount",1,false},
	{"fractalFreq",1,false},
	{"sfEnabled",1,true},
	{"sfAvoidWater",1,true},
	{"sfAvoidCliffs",1,true},
	{"sfSlopeMax",1,false},
	{"sfPreferSlopes",1,true},
	{"sfSlopeMin",1,false},
	{"sfAltMinEnable",1,true},
	{"sfAltMin",1,false},
	{"sfAltMaxEnable",1,true},
	{"sfAltMax",1,false},
	{"infAltOn",1,true},
	{"infAlt",4,false},
	{"infSlopeOn",1,true},
	{"infSlope",4,false},
}};
inline constexpr std::uint64_t NativeSplat_PAINT_VERT_SRC=17012528594867871139ull;
inline constexpr std::uint64_t NativeSplat_PAINT_FRAG_SRC=1590739467352899321ull;
inline constexpr std::uint64_t NativeSplat_COPY_FRAG_SRC=14073356825308877593ull;
inline constexpr std::uint64_t NativeSplat_OVERLAY_FRAG_SRC=6924347526663188269ull;
struct NativeSplatPainterInput {
	NativeSplatPainterProgram program=NativeSplatPainterProgram::Paint;
	NativeSplatPainterUniforms uniforms;
	std::shared_ptr<const void> compiled;
	std::array<ScreenSecondaryTexture,2> images;
};
inline void ValidateNativeSplatPainter(const NativeSplatPainterInput& input){
	if(!input.compiled || input.program>NativeSplatPainterProgram::Overlay)throw std::runtime_error("Original Splat Painter compiled program missing");
	const unsigned count=input.program==NativeSplatPainterProgram::Paint?2:1;
	for(unsigned i=0;i<count;++i){const auto& image=input.images[i];
		if(!image.texture || image.texture->incomplete || image.uvMode!=ScreenUVMode::Normalized ||
			image.texture->format==ScreenTextureFormat::Depth16Unorm || image.texture->format==ScreenTextureFormat::Depth24Unorm || image.texture->format==ScreenTextureFormat::Depth32Float)
			throw std::runtime_error("Original Splat Painter requires its actual complete color/height input");
		ValidateScreenSampler(image.sampler);
	}
	for(float v:input.uniforms.brushPos)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform brushPos");
	for(float v:input.uniforms.brushRadius)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform brushRadius");
	for(float v:input.uniforms.brushStrength)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform brushStrength");
	for(float v:input.uniforms.texSize)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform texSize");
	for(float v:input.uniforms.mapSize)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform mapSize");
	for(float v:input.uniforms.brushCurve)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform brushCurve");
	for(float v:input.uniforms.brushRotation)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform brushRotation");
	for(float v:input.uniforms.fractalAmount)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform fractalAmount");
	for(float v:input.uniforms.fractalFreq)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform fractalFreq");
	for(float v:input.uniforms.sfSlopeMax)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform sfSlopeMax");
	for(float v:input.uniforms.sfSlopeMin)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform sfSlopeMin");
	for(float v:input.uniforms.sfAltMin)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform sfAltMin");
	for(float v:input.uniforms.sfAltMax)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform sfAltMax");
	for(float v:input.uniforms.infAlt)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform infAlt");
	for(float v:input.uniforms.infSlope)if(!std::isfinite(v))throw std::runtime_error("Splat Painter nonfinite uniform infSlope");
	if(input.program==NativeSplatPainterProgram::Paint && (input.uniforms.brushChannel[0]<0 || input.uniforms.brushChannel[0]>3))
		throw std::runtime_error("Original Splat Painter channel is outside RGBA");
}
} // namespace rendering
