/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NATIVE_SEPIA_SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"SEPIAMSL(
struct SepiaUniforms { float4 rows[3]; float4 target; int4 viewPos; uint4 alpha; float4 parameters; };
struct SepiaVarying {float4 position [[position]];};
vertex SepiaVarying SepiaVertex(uint id [[vertex_id]],constant SepiaUniforms& u [[buffer(1)]]) {
 const float2 vertices[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(vertices[id],u.parameters.z,1)};
}
fragment float4 SepiaFragment(SepiaVarying input [[stage_in]],texture2d<float,access::read> source [[texture(0)]],constant SepiaUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 int2 p=int2(float2(input.position.x*u.target.z,(u.target.y-input.position.y)*u.target.w))-u.viewPos.xy;
 float3 color=pow(source.read(uint2(p.x,int(source.get_height())-1-p.y)).rgb,float3(u.parameters.x));
 float4 result=float4(dot(u.rows[0].xyz,color),dot(u.rows[1].xyz,color),dot(u.rows[2].xyz,color),1.0);
 if(u.alpha.x){float r=clamp(u.parameters.y,0.0f,1.0f);bool pass=false;switch(u.alpha.y){case 0:break;case 1:pass=result.a<r;break;case 2:pass=result.a==r;break;case 3:pass=result.a<=r;break;case 4:pass=result.a>r;break;case 5:pass=result.a!=r;break;case 6:pass=result.a>=r;break;default:pass=true;}if(!pass)discard_fragment();}
 return result;
}
)SEPIAMSL";
}
