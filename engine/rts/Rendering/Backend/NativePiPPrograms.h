/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePiPData.h"
namespace rendering {
inline constexpr NativePiPUniformSpec PiPLOSUniforms[]{
	{"losScale", 1, 0, false, true},
	{"showRadar", 1, 1, false, true},
	{"losTex", 1, 0, true, true},
	{"radarTex", 1, 1, true, true},
};
inline constexpr NativePiPUniformSpec PiPDecalBlitUniforms[]{
	{"strength", 1, 0, false, true},
	{"decalTex", 1, 0, true, true},
};
inline constexpr NativePiPUniformSpec PiPMinimapShadingUniforms[]{
	{"minimapTex", 1, 0, true, true},
	{"shadingTex", 1, 1, true, true},
	{"infoTex", 1, 2, true, true},
	{"infoUvMult", 2, 0, false, true},
	{"infoTexMul", 1, 1, false, true},
};
inline constexpr NativePiPUniformSpec PiPDecalUniforms[]{
	{"gameFrame", 1, 0, false, true},
	{"invMapSize", 2, 1, false, true},
	{"atlasTex", 1, 0, true, true},
};
inline constexpr NativePiPUniformSpec PiPWaterUniforms[]{
	{"waterColor", 4, 0, false, true},
	{"waterLevel", 1, 1, false, true},
	{"isLava", 1, 2, false, true},
	{"hasLavaTex", 1, 3, false, true},
	{"hasDistortTex", 1, 4, false, true},
	{"gameFrames", 1, 5, false, true},
	{"lavaCoastColor", 3, 6, false, true},
	{"lavaHighlightColor", 3, 7, false, true},
	{"colorCorrection", 3, 8, false, true},
	{"lavaCoastWidth", 1, 9, false, true},
	{"lavaUvScale", 1, 10, false, true},
	{"lavaSwirlFreq", 1, 11, false, false},
	{"lavaSwirlAmp", 1, 12, false, false},
	{"mapRatio", 1, 13, false, true},
	{"sunDirY", 1, 14, false, true},
	{"heatDistortX", 1, 15, false, true},
	{"heatDistortZ", 1, 16, false, true},
	{"wSurfColor", 3, 17, false, true},
	{"wSurfAlpha", 1, 18, false, true},
	{"wAbsorbColor", 3, 19, false, true},
	{"wBaseColor", 3, 20, false, true},
	{"wMinColor", 3, 21, false, true},
	{"wCausticsStr", 1, 22, false, true},
	{"wPerlinStart", 1, 23, false, true},
	{"wPerlinLacun", 1, 24, false, true},
	{"wPerlinAmp", 1, 25, false, true},
	{"wFresnelMin", 1, 26, false, true},
	{"wDiffuseFactor", 1, 27, false, true},
	{"heightTex", 1, 0, true, true},
	{"lavaDiffuseTex", 1, 1, true, true},
	{"lavaDistortTex", 1, 2, true, true},
};
inline constexpr NativePiPUniformSpec PiPIconUniforms[]{
	{"wtp_scale", 2, 0, false, true},
	{"wtp_offset", 2, 1, false, true},
	{"ndcScale", 2, 2, false, true},
	{"rotSC", 2, 3, false, true},
	{"rotCenter", 2, 4, false, true},
	{"iconBaseSize", 1, 5, false, true},
	{"gameTime", 1, 6, false, true},
	{"wallClockTime", 1, 7, false, true},
	{"outlinePass", 1, 8, false, true},
	{"healthDarkenMax", 1, 9, false, true},
	{"iconAtlas", 1, 0, true, true},
};
inline constexpr NativePiPUniformSpec PiPCircleUniforms[]{
	{"wtp_scale", 2, 0, false, true},
	{"wtp_offset", 2, 1, false, true},
	{"ndcScale", 2, 2, false, true},
	{"rotSC", 2, 3, false, true},
	{"rotCenter", 2, 4, false, true},
};
inline constexpr NativePiPUniformSpec PiPQuadUniforms[]{
	{"wtp_scale", 2, 0, false, true},
	{"wtp_offset", 2, 1, false, true},
	{"ndcScale", 2, 2, false, true},
	{"rotSC", 2, 3, false, true},
	{"rotCenter", 2, 4, false, true},
	{"atlasTex", 1, 0, true, true},
};
inline constexpr NativePiPUniformSpec PiPNanoStreamUniforms[]{
	{"wtp_scale", 2, 0, false, true},
	{"wtp_offset", 2, 1, false, true},
	{"ndcScale", 2, 2, false, true},
	{"rotSC", 2, 3, false, true},
	{"rotCenter", 2, 4, false, true},
	{"gameTime", 1, 5, false, true},
	{"uiScale", 1, 6, false, true},
	{"cameraZoom", 1, 7, false, true},
	{"nanoTex", 1, 0, true, true},
};
inline constexpr NativePiPUniformSpec PiPLineUniforms[]{
	{"wtp_scale", 2, 0, false, true},
	{"wtp_offset", 2, 1, false, true},
	{"ndcScale", 2, 2, false, true},
	{"rotSC", 2, 3, false, true},
	{"rotCenter", 2, 4, false, true},
	{"colorMul", 4, 5, false, true},
	{"mapLineFade", 3, 6, false, true},
};
inline constexpr NativePiPProgramSpec NativePiPPrograms[]{
	{"LOS", 0x287c2e72e56ac9ccULL, 0x16fc191934ede212ULL, NativePiPGeometry::Immediate, 0, PiPLOSUniforms},
	{"DecalBlit", 0xab13fd53e27cc0bcULL, 0x8dbf7fc9720f1531ULL, NativePiPGeometry::Immediate, 0, PiPDecalBlitUniforms},
	{"MinimapShading", 0xab13fd53e27cc0bcULL, 0x4452fb9dc6bba526ULL, NativePiPGeometry::Immediate, 0, PiPMinimapShadingUniforms},
	{"Decal", 0x3eb94846382f0c24ULL, 0x6bcae5fe679d3f25ULL, NativePiPGeometry::Instanced, 5, PiPDecalUniforms},
	{"Water", 0xab13fd53e27cc0bcULL, 0x8e0af95bf6399b3cULL, NativePiPGeometry::Immediate, 0, PiPWaterUniforms},
	{"Icon", 0x315ec2544e35a22dULL, 0x0b91a2be6a9fb2b3ULL, NativePiPGeometry::Instanced, 3, PiPIconUniforms},
	{"Circle", 0xce8f73f97bf85b33ULL, 0xfdb43a8ef7ce51f4ULL, NativePiPGeometry::Instanced, 3, PiPCircleUniforms},
	{"Quad", 0x6be3857a68c2c850ULL, 0xf42926e04657fa7fULL, NativePiPGeometry::Instanced, 4, PiPQuadUniforms},
	{"Line", 0x1c2c10728a72e6caULL, 0x6deb43e24f84c16dULL, NativePiPGeometry::Line, 0, PiPLineUniforms},
	{"NanoStream", 0x65206cd2bc657ff6ULL, 0x0cdb1a814218c3ecULL, NativePiPGeometry::Instanced, 2, PiPNanoStreamUniforms},
};
}
