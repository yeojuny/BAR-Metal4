/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
// Appended to NativeEngineDeferredShaders: one shared original vertex/sample
// implementation, full original legacy/GL4 FORWARD fragments below.
inline constexpr const char* NativeEngineForwardShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
struct OriginalModelLight {float4 position,halfVector,directionCutoff,ambient,diffuse,specular,attenuation;};
struct OriginalModelLights {uint4 header;};
bool OriginalAlpha(float a,float b,uint op){
 switch(op){case 0:return false;case 1:return a<b;case 2:return a==b;case 3:return a<=b;case 4:return a>b;case 5:return a!=b;case 6:return a>=b;default:return true;}
}
float3 OriginalDynamicLights(float3 normal,float3 diffuse,float3 specular,float3 position,device const OriginalModelLights& source){
 float3 rgb=0;device const OriginalModelLight* records=reinterpret_cast<device const OriginalModelLight*>(&source+1);
 for(uint i=0;i<source.header.x;++i){auto light=records[i];
  float3 vector=light.position.xyz-position;float distance=length(vector),radius=light.attenuation.x;
  float scale=float(distance<=radius);
  float diff=clamp(dot(normal,vector/distance),0.f,1.f),spec=clamp(dot(normal,normalize(light.halfVector.xyz)),0.f,1.f);
  // Original selected ModelDrawerState extraDefs does not define
  // OGL_SPEC_ATTENUATION: retain the actual radius-squared branch.
  float attenuation=1-min(1.f,distance*distance/(radius*radius));
  scale*=float(dot(-vector/distance,light.directionCutoff.xyz)>=light.directionCutoff.w);
  rgb+=scale*light.ambient.rgb;
  rgb+=scale*attenuation*(diffuse*light.diffuse.rgb*diff);
  rgb+=scale*attenuation*(specular*light.specular.rgb*pow(spec,4.f));
 }
 return rgb;
}
float4 OriginalForward(EngineFragmentInput in,constant EngineUniform& u
 ENGINE_IMAGE0_PARAMS ENGINE_IMAGE1_PARAMS ENGINE_IMAGE2_PARAMS
 ENGINE_IMAGE3_PARAMS ENGINE_IMAGE4_PARAMS ENGINE_IMAGE5_PARAMS,
 device const OriginalModelLights& lights,bool modern){
 float4 tex1=ENGINE_SAMPLE0(in.uv);
 float4 tex2=ENGINE_SAMPLE1(in.uv);
 float alpha=u.team.a*(modern?float(tex2.a>=.5f):tex2.a);
 if(modern&&(float(alpha>u.alphaControl.x)*u.alphaControl.y+float(alpha<u.alphaControl.x)*u.alphaControl.z+u.alphaControl.w)==0)discard_fragment();
 float3 normal=normalize(in.normal),diffuse=mix(tex1.rgb,u.team.rgb,tex1.a),finalColor=diffuse;
 if(!modern||u.lightingFlags.w==0){
  float3 light,specular,reflected;float ndotl;
  if(modern){
   float3 L=normalize(u.sunDirection.xyz),V=normalize(in.cameraDirection),H=normalize(L+V),R=-reflect(V,normal);
   ndotl=clamp(dot(normal,L),0.f,1.f);float hdotn=clamp(dot(H,normal),0.f,1.f);
   reflected=(u.flags.w&4u)?ENGINE_REFLECTION_SAMPLE(R):float3(0);
   specular=u.sunSpecular.rgb*min(pow(hdotn,2.5f*u.sunSpecular.a)+.3f*pow(hdotn,6.f),1.f);
   light=u.sunAmbient.rgb+ndotl*u.sunDiffuse.rgb;
  }else{
   ndotl=max(dot(normal,u.sunDirection.xyz),.001f);float3 R=reflect(in.cameraDirection,normal);
   specular=(u.flags.w&8u)?ENGINE_SPECULAR_SAMPLE(R)*u.sunSpecular.rgb:float3(0);
   reflected=(u.flags.w&4u)?ENGINE_REFLECTION_SAMPLE(R):float3(0);
   light=ndotl*u.sunDiffuse.rgb+u.sunAmbient.rgb;
  }
  float3 shadowMult=1;
  if(u.lightingFlags.x){
   float3 coord=in.shadowPosition.xyz/in.shadowPosition.w;
   float2 uv=coord.xy;if(u.lightSampler.w)uv.y=1-uv.y;
   // An incomplete comparison texture has GL's incomplete sample red=0.
   const float reference=(u.flags.w&16u)?clamp(coord.z,0.f,1.f):coord.z;
   float sh=(u.flags.w&1u)?ENGINE_SHADOW_SAMPLE(uv,reference):0.f;
   sh=min(sh,smoothstep(0.f,.35f,ndotl));
   float3 color=ENGINE_SAMPLE3(coord.xy).rgb;
   shadowMult=mix(float3(1),float3(sh),u.lightingParameters.x)*color;
  }
  specular*=tex2.g*4*shadowMult;
  light=mix(u.sunAmbient.rgb,light,shadowMult);reflected=mix(light,reflected,tex2.g)+tex2.rrr;
  finalColor=diffuse*reflected+specular;
  if(!modern)finalColor+=OriginalDynamicLights(normal,tex1.rgb,specular,in.worldPosition.xyz,lights);
 }
 finalColor=mix(u.fogColor.rgb,finalColor,in.fogFactor);
 finalColor=mix(finalColor,u.nano.rgb,u.nano.a);
 float4 result=float4(finalColor,alpha)*(modern?u.colorMultiplier:float4(1));
 if(!modern&&u.lightingFlags.y&&!OriginalAlpha(result.a,u.lightingParameters.y,u.lightingFlags.z))discard_fragment();
 return result;
}
fragment float4 NativeEngineForwardLegacy(EngineFragmentInput in [[stage_in]],constant EngineUniform& u [[buffer(1)]],device const OriginalModelLights& lights [[buffer(2)]]
 ENGINE_IMAGE0_ARGS ENGINE_IMAGE1_ARGS ENGINE_IMAGE2_ARGS
 ENGINE_IMAGE3_ARGS ENGINE_IMAGE4_ARGS ENGINE_IMAGE5_ARGS RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 return OriginalForward(in,u ENGINE_IMAGE0_PASS ENGINE_IMAGE1_PASS ENGINE_IMAGE2_PASS
 ENGINE_IMAGE3_PASS ENGINE_IMAGE4_PASS ENGINE_IMAGE5_PASS,lights,false);
}
fragment float4 NativeEngineForwardGL4(EngineFragmentInput in [[stage_in]],constant EngineUniform& u [[buffer(1)]],device const OriginalModelLights& lights [[buffer(2)]]
 ENGINE_IMAGE0_ARGS ENGINE_IMAGE1_ARGS ENGINE_IMAGE2_ARGS
 ENGINE_IMAGE3_ARGS ENGINE_IMAGE4_ARGS ENGINE_IMAGE5_ARGS RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 return OriginalForward(in,u ENGINE_IMAGE0_PASS ENGINE_IMAGE1_PASS ENGINE_IMAGE2_PASS
 ENGINE_IMAGE3_PASS ENGINE_IMAGE4_PASS ENGINE_IMAGE5_PASS,lights,true);
}
)MSL";
} // namespace rendering::metal4
