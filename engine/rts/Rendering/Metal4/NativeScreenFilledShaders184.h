/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

// Appended to the SAME library as ScreenFragmentMain. Input/uniform types,
// samplers, AlphaPass, arithmetic mode and source expression order are shared.
// Only the bounded root1x filled classification may select this entry point.
// Pass185 keeps material/alpha dispatch draw-uniform so these states share a PSO.
inline constexpr const char* NativeScreenFilledShader184=R"FILLED184(
fragment float4 ScreenFilledFragment184(ScreenVarying input [[stage_in]], constant ScreenUniform& u [[buffer(2)]],
 texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]]) {
 float4 output = input.color;
 if (u.options.x != 0 && u.options.x < 4 && (u.options.x == 2 || u.parameters.w == 0)) {
  float2 uv = input.uv*u.dimensions.zw;
  if (u.options.y) uv.y = 1-uv.y;
  float4 texel;
  if (u.parameters.w != 0) texel=float4(0,0,0,1);
  else if (u.parameters.y != 0 || u.parameters.z != 0) {
   // Derivatives precede legacy-clamp coordinates exactly as in the fallback.
   const float2 dx=dfdx(uv),dy=dfdy(uv);
   if (u.parameters.y != 0) uv.x=clamp(uv.x,0.0f,1.0f);
   if (u.parameters.z != 0) uv.y=clamp(uv.y,0.0f,1.0f);
   texel=SampleOriginalUI(image,imageSampler,uv,gradient2d(dx,dy));
  } else texel=SampleOriginalUI(image,imageSampler,uv);
  if (u.options.x == 2) output.a *= texel.r;
  else output *= texel;
 }
 if (u.options.z && !AlphaPass(output.a,u.parameters.x,u.options.w)) discard_fragment();
 // Absent point metadata gives ~0u in the generic shader. At this root1x
 // route there is no point/A2C/sample-shading contract to synthesize.
 return output;
}
)FILLED184";
