/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativePlacementShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct PlacementRasterVertex {float4 clip,color,world;};
struct PlacementGridUniform {float4 mouse,camera,alphaTest;};
struct PlacementGridVarying {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float3 world [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex PlacementGridVarying PlacementGridVertex(uint id [[vertex_id]],device const PlacementRasterVertex* v [[buffer(0)]]){
 PlacementGridVarying out;out.position=v[id].clip;out.position.z=(out.position.z+out.position.w)*.5f;out.color=v[id].color;out.world=v[id].world.xyz;return out;
}
fragment float4 PlacementGridFragment(PlacementGridVarying in [[stage_in]],constant PlacementGridUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float dist=distance(in.world,u.mouse.xyz);
 float alpha=smoothstep(0.f,1.f,(480.f/(dist/2.5f))-2.5f);
 float camDist=distance(u.camera.xyz,u.mouse.xyz);
 float distAlpha=smoothstep(0.f,1.f,1.f-(3000.f/camDist));
 float cameraFade=u.camera.w!=0.f?u.mouse.w:(distAlpha*1.75f);
 float resultAlpha=(alpha-cameraFade)*in.color.a;
 if(u.alphaTest.x!=0){float reference=u.alphaTest.z;uint comparison=uint(u.alphaTest.y);bool pass=false;
  switch(comparison){case 0:break;case 1:pass=resultAlpha<reference;break;case 2:pass=resultAlpha==reference;break;case 3:pass=resultAlpha<=reference;break;case 4:pass=resultAlpha>reference;break;case 5:pass=resultAlpha!=reference;break;case 6:pass=resultAlpha>=reference;break;case 7:pass=true;break;}if(!pass)discard_fragment();}
 return float4(in.color.rgb,resultAlpha);
}
)MSL";
}
