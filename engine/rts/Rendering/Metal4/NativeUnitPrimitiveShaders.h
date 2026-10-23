/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeUnitPrimitiveShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"DPATMSL(
// SPDX-License-Identifier: MIT
// Original DrawPrimitiveAtUnit shaders: Copyright (c) 2026 Beherith.
// Original GS strip topology, root transform interpolation and
// full fragment variants for building AO, feature AO, TeamPlatter and Selected Units.
#include <metal_stdlib>
using namespace metal;
struct PrimitiveInstance {
 packed_float4 dimensions;
 uint team,vertices;
 packed_float4 parameters,uv;
 packed_uint4 instanceData;
};
static_assert(sizeof(PrimitiveInstance)==72,"Original mixed point-stream ABI");
struct PrimitiveObject {
 float4 previousQuaternion,previousTranslationScale,currentQuaternion,currentTranslationScale;
 uint4 metadata;
 float4 teamColor;
};
static_assert(sizeof(PrimitiveObject)==96,"Published root/identity transport ABI");
struct PrimitiveUniform {
 float4x4 viewProjection,viewInverse;
 float4 time,parameters; // addRadius, iconDistance, captured opacity, alpha reference
 uint4 options; // family, original GL depth conversion, first row, atlas flip
 uint4 alpha; // enabled, compare, original USE_QUADS absent, reserved
 float4 ally; // actual line opacity, MAXVERTICES; reserved
 float4 selected; // actual INITIALSIZE, original POST_SHADING white-mix literal
};
struct PrimitiveOut {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]],uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
float3x3 PrimitiveRotationY(float a) {
 float s=sin(a),c=cos(a);return float3x3(float3(c,0,-s),float3(0,1,0),float3(s,0,c));
}
float3x3 PrimitiveTransformRotation(float4 q,float scale) {
 float qxx=q.x*q.x,qyy=q.y*q.y,qzz=q.z*q.z,qxz=q.x*q.z,qxy=q.x*q.y,qyz=q.y*q.z;
 float qrx=q.w*q.x,qry=q.w*q.y,qrz=q.w*q.z;
 float3x3 rot=float3x3(float3(1.0-2.0*(qyy+qzz),2.0*(qxy+qrz),2.0*(qxz-qry)),
  float3(2.0*(qxy-qrz),1.0-2.0*(qxx+qzz),2.0*(qyz+qrx)),
  float3(2.0*(qxz+qry),2.0*(qyz-qrx),1.0-2.0*(qxx+qyy)));
 return rot*scale;
}
struct PrimitivePoint {float3 position;float2 uv;float correction;};
PrimitivePoint PrimitiveGSPoint(uint shape,uint emitted,float length,float width,float cs) {
 PrimitivePoint p={};
 if(shape==3u){
  p.correction=2.000;
  if(emitted==0){p.position=float3(0,0,length);p.uv=float2(.5,1);}
  else if(emitted==1){p.position=float3(-.866*width,0,-.5*length);p.uv=float2(0,0);}
  else{p.position=float3(.866*width,0,-.5*length);p.uv=float2(1,0);}
 }else if(shape==4u){
  p.correction=1.414;
  if(emitted==0){p.position=float3(width*.5,0,length*.5);p.uv=float2(0,1);}
  else if(emitted==1){p.position=float3(width*.5,0,-length*.5);p.uv=float2(0,0);}
  else if(emitted==2){p.position=float3(-width*.5,0,length*.5);p.uv=float2(1,1);}
  else{p.position=float3(-width*.5,0,-length*.5);p.uv=float2(1,0);}
 }else if(shape==2u){
  p.correction=1.1;float cuv=(cs/(length+width))*2.0;
  switch(emitted){
   case 0:p.position=float3(-width*.5,0,-length*.5+cs);p.uv=float2(0,cuv);break;
   case 1:p.position=float3(-width*.5,0,length*.5-cs);p.uv=float2(0,1.0-cuv);break;
   case 2:p.position=float3(-width*.5+cs,0,-length*.5);p.uv=float2(cuv,0);break;
   case 3:p.position=float3(-width*.5+cs,0,length*.5);p.uv=float2(cuv,1);break;
   case 4:p.position=float3(width*.5-cs,0,-length*.5);p.uv=float2(1.0-cuv,0);break;
   case 5:p.position=float3(width*.5-cs,0,length*.5);p.uv=float2(1.0-cuv,1);break;
   case 6:p.position=float3(width*.5,0,-length*.5+cs);p.uv=float2(1,cuv);break;
   default:p.position=float3(width*.5,0,length*.5-cs);p.uv=float2(1.0-cuv,1);break;
  }
 }else{
  uint count=min(shape,64u);float internalAngle=float(count-2u)*3.14159265358979323846/float(count);p.correction=1.0/sin(internalAngle/2.0);
  if(emitted==0){p.position=float3(-width*.5,0,0);p.uv=float2(0,.5);}
  else if(emitted==count-1u){p.position=float3(width*.5,0,0);p.uv=float2(1,.5);}
  else{
   uint i=(emitted+1u)/2u,numSides=count/2u;float phi=((float(i)*3.141592)/float(numSides))-1.5707963;
   float sinphi=sin(phi),cosphi=cos(phi);bool positive=(emitted&1u)!=0;
   p.position=float3(width*.5*sinphi,0,(positive?length*.5:-length*.5)*cosphi);
   p.uv=float2(sinphi*.5+.5,cosphi*(positive?.5:-.5)+.5);
  }
 }
 return p;
}
vertex PrimitiveOut UnitPrimitiveVertex(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],
 const device PrimitiveInstance* instances [[buffer(0)]],const device PrimitiveObject* objects [[buffer(1)]],constant PrimitiveUniform& u [[buffer(2)]]) {
 PrimitiveOut out={};out.position=float4(2,2,2,1);
 bool icon=u.options.x>=7u && u.options.x<=13u;
 PrimitiveInstance row=instances[u.options.z+instanceID];PrimitiveObject object=objects[instanceID];
 float a=clamp(u.time.w,0.0,1.0);float4 qb=object.currentQuaternion;
 if(dot(object.previousQuaternion,qb)<0.0)qb=-qb;
 float4 q=normalize(mix(object.previousQuaternion,qb,a));
 float4 translationScale=mix(object.previousTranslationScale,object.currentTranslationScale,a);
 float3x3 modelRotation=PrimitiveTransformRotation(q,translationScale.w);
 float4 center=float4(translationScale.xyz,1.0);float4 centerClip=u.viewProjection*center;
 float tolerance=(u.options.x==1u || (icon && u.options.x!=9u && u.options.x!=13u))?1.2:1.1;
 bool clipped=any(centerClip.xyz < -centerClip.www*tolerance)||any(centerClip.xyz > centerClip.www*tolerance);
 if(clipped || length(u.viewInverse[3].xyz-center.xyz)>u.parameters.y || dot(center.xyz,center.xyz)<1.0 || (object.metadata.x&3u)<1u)return out;
 uint shape=row.vertices;uint count=shape==2u?8u:shape>5u?min(shape,64u):shape;
 if(shape==4u && u.alpha.z)return out;
 bool enabled=u.options.x==16u?shape==4u:icon?(shape==3u||shape==4u):u.options.x==0u?shape==4u:u.options.x==1u?(shape==3u||shape==4u):(shape==2u||shape==3u||shape==4u||shape>5u);
 if(!enabled || count<3u || vertexID>=3u*(count-2u))return out;
 if(u.options.x==6u && count>uint(u.ally.y))return out;
 uint triangle=vertexID/3u,corner=vertexID%3u;
 uint emitted=triangle+(corner==2u?2u:((triangle&1u)?1u-corner:corner));
 float4 dimensions=float4(row.dimensions),parameters=float4(row.parameters),uvoffsets=float4(row.uv);
 if(u.options.x>=3u && !icon && u.options.x!=14u && u.options.x!=15u && u.options.x!=16u){
  float animation=clamp(((u.time.x+u.time.w)-parameters.x)/(u.options.x==6u?8.0:4.0)+u.selected.x,u.selected.x,1.0)+sin(u.time.x/30.0)*.05;
  dimensions.xy*=animation;
 }
 if(icon){
  float initial=u.options.x==13u?.25:u.options.x==9u?.5:.22,growth=u.options.x==13u?16.0:u.options.x==9u?5.0:4.0;
  float breathe=u.options.x==7u?.1:(u.options.x==8u || u.options.x==11u)?.05:0.0;
  // The original group program sets both BREATHERATE and BREATHESIZE to0.
  // Its disabled breathing contribution is zero, without evaluating sin(x/0).
  float animation=clamp(((u.time.x+u.time.w)-parameters.x)/growth+initial,initial,1.0);
  if(breathe!=0.0)animation+=sin(u.time.x/30.0)*breathe;
  dimensions.xy*=animation;
 }
 if(u.options.x==13u)dimensions.xy*=parameters.y*4.5+(1250.0-abs(length(u.viewInverse[3].xyz-center.xyz)-1250.0))*.016;
 if(u.options.x==14u)dimensions.xy*=clamp(((u.time.x+u.time.w)-parameters.x)/15.0+.5,.5,1.0)+sin(u.time.x/15.0)*.075;
 if(u.options.x==0u)parameters.w=max(-.2,sin((u.time.x+u.time.w)*2.0/30.0+float(object.metadata.x>>16)*.1))+.2;
 center.y+=((u.options.x==14u || u.options.x==15u || u.options.x==16u)?3.99:icon?0.0:(u.options.x==6u?3.9:(u.options.x>=3u?4.0:(u.options.x==2u?3.99:0.0))));center.y+=dimensions.w;
 float3x3 rotation=u.options.x==1u?modelRotation:PrimitiveRotationY(-atan2(modelRotation[0][2],modelRotation[0][0]));
 if(u.options.x==6u && shape>5u)rotation=float3x3(1.0);
 if(icon)rotation=float3x3(u.viewInverse[0].xyz,u.viewInverse[2].xyz,u.viewInverse[1].xyz);
 PrimitivePoint point=PrimitiveGSPoint(shape,emitted,dimensions.x,dimensions.y,dimensions.z);
 float3 vecnorm=normalize(point.position);
 if(u.options.x==9u)point.position.xz+=float2(20,-5);
 out.position=u.viewProjection*float4(center.xyz+rotation*(u.parameters.x*point.correction*vecnorm+point.position),1.0);
 out.position.z-=((icon?(u.options.x==9u || u.options.x==12u || u.options.x==13u?256.0:512.0):(u.options.x>=2u?256.0:512.0))/out.position.w);
 if(u.options.x==12u)out.position.z-=512.0/out.position.w;
 if(u.options.y)out.position.z=(out.position.z+out.position.w)*.5;
 // Original GLSL swizzle: .stpq are x,y,z,w. The widget has already
 // supplied q,p,t,s from the atlas; do not flip or reorder them a second time.
 float av=uvoffsets.y-uvoffsets.x,bv=uvoffsets.w-uvoffsets.z;
 out.uv=float4(uvoffsets.x+av*point.uv.x,uvoffsets.z+bv*point.uv.y,parameters.zw);
 out.color=object.teamColor;return out;
}
bool PrimitiveAlpha(float value,constant PrimitiveUniform& u) {
 if(!u.alpha.x)return true;float r=clamp(u.parameters.w,0.0,1.0);
 switch(u.alpha.y){case 0:return false;case 1:return value<r;case 2:return value==r;case 3:return value<=r;case 4:return value>r;case 5:return value!=r;case 6:return value>=r;default:return true;}
}
fragment float4 UnitPrimitiveAtlasFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]],texture2d<float> atlas [[texture(0)]],sampler atlasSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.options.w)uv.y=1.0-uv.y;float4 texcolor=atlas.sample(atlasSampler,uv);
 float4 color=u.options.x==0u?float4(texcolor.rgb*(1.0+input.uv.w),texcolor.a*input.uv.z):float4(texcolor.rgb,pow(texcolor.a,.85)*input.uv.z);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
// Original tracker/enemy-spotter default DPAT fragment: texture * team RGB,
// captured profile transparency, no AO post-shading or selected-unit white mix.
fragment float4 UnitPrimitiveTrackerFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.options.w)uv.y=1.0-uv.y;
 float4 texcolor=image.sample(imageSampler,uv);
 float4 color=float4(input.color.rgb*texcolor.rgb+u.parameters.x,texcolor.a*u.parameters.z+u.parameters.x);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
fragment float4 UnitPrimitiveTeamFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 color=float4(input.color.rgb+u.parameters.x,u.parameters.z+u.parameters.x);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
fragment float4 UnitPrimitiveIconFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.options.w)uv.y=1.0-uv.y;
 float4 texcolor=image.sample(imageSampler,uv);
 float4 color=u.options.x==9u?float4(texcolor.rgb*float3(.8,1,.8),texcolor.a*u.parameters.z+u.parameters.x):float4(texcolor.rgb,texcolor.a*input.uv.z);
 if(u.options.x==13u){color=float4(texcolor.rgb*(1.0+input.uv.w),texcolor.a*input.uv.z);if(color.a<=0.0)discard_fragment();}
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
fragment float4 UnitPrimitiveHaloFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.options.w)uv.y=1.0-uv.y;float4 color=image.sample(imageSampler,uv);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
fragment float4 UnitPrimitiveAllyFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 color=float4(input.color.rgb,u.parameters.z+step(.01,u.parameters.x)*u.ally.x);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
fragment float4 UnitPrimitiveSelectedFragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.options.w)uv.y=1.0-uv.y;
 float4 texcolor=image.sample(imageSampler,uv);
 // Exact selected widget POST_SHADING; no guessed solid-color replacement.
 float4 color=float4(mix(input.color.rgb*texcolor.rgb+u.parameters.x,float3(1.0),u.selected.y),texcolor.a*u.parameters.z+u.parameters.x);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
// Exact current POST_SHADING with USETEXTURE=0; no image/sampler binding.
fragment float4 UnitPrimitiveSelected31657Fragment(PrimitiveOut input [[stage_in]],constant PrimitiveUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 color=float4(mix(input.color.rgb+u.parameters.x,float3(1.0),u.selected.y),u.parameters.z+u.parameters.x);
 if(!PrimitiveAlpha(color.a,u))discard_fragment();return color;
}
)DPATMSL";
}
