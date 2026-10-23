/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// VS/GS/fragment translation of original HealthbarsGL4 (MIT), Copyright (c)
// 2026 Beherith (mysterme@gmail.com), Beyond All Reason. CPU publishes the
// original point/model/UBO bytes; all per-unit shader work remains on GPU.
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* HealthBarsShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
struct HealthBarsPoint {
 float4 heightTimers; uint4 typeIndexUniform; float4 minColor,maxColor; uint4 instanceData;
};
struct HealthBarsModel { uint4 metadata; float4 health,drawPos,speed; float4 user[4]; };
struct HealthBarsUniforms {
 float4x4 viewProjection,viewInverse;
 float4 time,parameters,shape;
 uint4 options,sampling;
};
struct HealthBarsVarying { float4 position [[position]]; float4 color [[RECOIL_SAMPLE_PERSPECTIVE]]; float3 uv [[RECOIL_SAMPLE_PERSPECTIVE]]; };
// Three independent 8-vertex strips (18 vertices each) and up to four glyph
// strips (6 each): 78 triangle vertices per original point, in source order.
// Every triangle preserves GL's alternating strip winding and last vertex.
uint HealthBarsStripIndex(uint id) {
 const uint triangle=id/3,corner=id%3;
 return corner==2?triangle+2:triangle+(corner^(triangle&1u));
}
float HealthBarsMod(float x,float y){return x-y*floor(x/y);}
float4 HealthBarsPosition(float3 center,float2 pos,float zOffset,float size,float depth,constant HealthBarsUniforms& u){
 const float3 primitive=float3(pos.x,0,pos.y-zOffset)*4.0f*size;
 const float3x3 rotation=float3x3(u.viewInverse[0].xyz,u.viewInverse[2].xyz,u.viewInverse[1].xyz);
 float4 clip=u.viewProjection*float4(center+rotation*primitive,1);
 clip.z+=depth;
 if(u.options.x)clip.z=(clip.z+clip.w)*.5f;
 return clip;
}
vertex HealthBarsVarying HealthBarsVertexMain(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 device const HealthBarsPoint* points [[buffer(0)]],device const HealthBarsModel* models [[buffer(1)]],
 constant HealthBarsUniforms& u [[buffer(2)]]) {
 HealthBarsVarying output;output.position=float4(0,0,2,1);output.color=float4(0);output.uv=float3(0);
 const HealthBarsPoint row=points[u.options.y+instanceId];
 // models[instanceId] is the exact immutable128-byte row addressed by this
 // point's instData.y. No health, animation or visibility was computed on CPU.
 const HealthBarsModel model=models[instanceId];
 const uint flags=row.typeIndexUniform.x,loc=row.typeIndexUniform.z;
 const bool debugShow=u.shape.w>.5f;
 float3 center=model.drawPos.xyz;
 const float4 originalClip=u.viewProjection*float4(center,1);
 bool visible=!(any(originalClip.xyz < -originalClip.www*1.2f)||any(originalClip.xyz > originalClip.www*1.2f));
 const float distance=length(u.viewInverse[3].xyz-center);
 float barAlpha=1.0f-clamp((clamp(distance*u.parameters.y,3200.0f,3800.0f)-3200.0f)/600.0f,0.0f,1.0f);
 float glyphAlpha=1.0f-clamp((clamp(distance*u.parameters.y*u.parameters.z,3200.0f,3800.0f)-3200.0f)/600.0f,0.0f,1.0f);
 if(debugShow){barAlpha=1;glyphAlpha=1;}
 // iconDistance is intentionally unused by the pinned original full shader.
 if(dot(center,center)<1.0f)visible=false;
 center.y+=3.0f;center.y+=row.heightTimers.x;
 float health=model.health.y/model.health.x;
 if(loc<20u)health=model.user[0].y;
 else if(!debugShow && abs(model.user[0].x-health)<.03f)visible=false;
 if(loc<4u)health=model.user[0][loc];
 if(loc==4u)health=model.user[1].x;
 if(loc==5u)health=model.user[1].y;
 if(flags&32u)health=clamp(((u.time.x+u.time.w)-model.user[0].z)/(model.user[0].w-model.user[0].z),0.0f,1.0f);
 if(barAlpha<.2f || !visible)return output;
 if(!debugShow){
  if(health<.00001f)return output;
  if((flags&4u) && health>.999f)return output;
  if(!(flags&4u) && (flags&32u) && health>.999f)return output;
 }
 float stockpiled=0;
 if(flags&16u){const float whole=floor(health);health=fract(health);stockpiled=floor(HealthBarsMod(whole,128.0f));}
 const float width=2.56f,height=u.shape.x,corner=u.shape.y,small=u.shape.z,atlas=.0625f;
 const float zOffset=1.15f*height*float(row.typeIndexUniform.y);
 if(vertexId<54u){
  const uint strip=vertexId/18u,k=HealthBarsStripIndex(vertexId%18u);
  if(strip==0u){
   const float2 positions[8]={float2(-width,corner),float2(-width,height-corner),float2(-width+corner,0),float2(-width+corner,height),
    float2(width-corner,0),float2(width-corner,height),float2(width,corner),float2(width,height-corner)};
   const float2 pos=positions[k];
   const float flash=((flags&64u) && HealthBarsMod(u.time.x,10.0f)>4.0f)?.5f:0.0f;
   output.color=mix(float4(.25f,.25f,.25f,.8f)+flash,float4(.1f,.1f,.1f,.8f)+flash,pos.y);
   output.color.a*=barAlpha;
   output.position=HealthBarsPosition(center,pos,zOffset,row.heightTimers.y,.001f,u);
   return output;
  }
  float4 trueColor=mix(row.minColor,row.maxColor,health);trueColor.a=.2f;
  float4 color;float2 pos;float textureOffset=0,depth=0;
  if(strip==1u){
   const float2 positions[8]={float2(-width+corner,small+corner),float2(-width+corner,height-small-corner),
    float2(-width+small+corner,corner),float2(-width+small+corner,height-corner),
    float2(width-small-corner,corner),float2(width-small-corner,height-corner),
    float2(width-corner,small+corner),float2(width-corner,height-small-corner)};
   pos=positions[k];color=trueColor;if(k&1u)color.rgb*=.5f;
  }else{
   float fill=(2.0f*(width-corner)-2.0f*small)*health;
   if(flags&8u)fill=2.0f*(width-corner)-2.0f*small;
   if(flags&128u)trueColor.rgb/=max(trueColor.r,trueColor.g);
   trueColor.a=1; color=trueColor;if(!(k&1u))color.rgb*=.5f;
   const float2 positions[8]={float2(-width+corner,small+corner),float2(-width+corner,height-corner-small),
    float2(-width+corner+small,corner),float2(-width+corner+small,height-corner),
    float2(-width+corner+small+fill,corner),float2(-width+corner+small+fill,height-corner),
    float2(-width+corner+2.0f*small+fill,corner+small),float2(-width+corner+2.0f*small+fill,height-corner-small)};
   pos=positions[k];depth=-.001f;if(flags&1u)textureOffset=row.heightTimers.w;
  }
  const float2 barUV=float2(pos.x/(2.0f*(width-corner))+.5f,(pos.y-corner)/(height-2.0f*corner))*float2(9.0f*atlas,atlas)+float2(3.0f*atlas,textureOffset);
  output.uv=float3(barUV.x,-barUV.y,clamp(10000.0f*textureOffset,0.0f,1.0f));
  output.color=color;output.color.a*=barAlpha;
  output.position=HealthBarsPosition(center,pos,zOffset,row.heightTimers.y,depth,u);return output;
 }
 if(glyphAlpha<.2f || u.parameters.w>1.5f)return output;
 // The pinned barTypeMap never combines integer stockpile and percentage/time
 // flags, so its original max_vertices40 permits at most four glyph quads.
 float3 glyphs[4];uint glyphCount=0;float glyphPosition=1;
 if(u.parameters.w<.5f){
  if(flags&2u)glyphs[glyphCount++]=float3(-width-glyphPosition*height,atlas,row.heightTimers.w);
 }else glyphPosition=0;
 if(flags&16u){
  const float units=floor(HealthBarsMod(stockpiled,10.0f));
  const float tens=floor(HealthBarsMod(stockpiled*.1f,10.0f));
  glyphs[glyphCount++]=float3(-width-(glyphPosition+1.0f)*height,0,units*atlas);
  if(tens>0)glyphs[glyphCount++]=float3(-width-(glyphPosition+2.0f)*height+height*.4f,0,tens*atlas);
 }
 if(flags&(8u|4u)){
  float units,tens,symbol;
  if(flags&8u){health=(health-1.0f)/(1.0f/40.0f);units=abs(floor(HealthBarsMod(health,10.0f)));tens=abs(floor(HealthBarsMod(health*.1f,10.0f)));symbol=14;}
  else{units=floor(HealthBarsMod(health*100.0f,10.0f));tens=floor(HealthBarsMod(health*10.0f,10.0f));symbol=11;}
  glyphs[glyphCount++]=float3(-width-(glyphPosition+1.0f)*height,0,symbol*atlas);
  glyphs[glyphCount++]=float3(-width-(glyphPosition+2.0f)*height+height*.2f,0,units*atlas);
  if(tens>0)glyphs[glyphCount++]=float3(-width-(glyphPosition+3.0f)*height+height*.5f,0,tens*atlas);
 }
 const uint glyphIndex=(vertexId-54u)/6u;
 if(glyphIndex>=glyphCount)return output;
 const uint k=HealthBarsStripIndex((vertexId-54u)%6u);
 const bool right=k>=2,top=(k&1u)!=0;const float halfPixel=.0019765625f;
 const float3 glyph=glyphs[glyphIndex];
 const float2 pos=float2(glyph.x+(right?height:0.0f),top?height:0.0f);
 const float2 uv=float2(glyph.y+(right?atlas-halfPixel:halfPixel),glyph.z+(top?atlas-halfPixel:halfPixel));
 output.position=HealthBarsPosition(center,pos,zOffset,row.heightTimers.y,0,u);
 output.color=float4(1,1,1,glyphAlpha);output.uv=float3(uv.x,1.0f-uv.y,1);return output;
}
fragment float4 HealthBarsFragmentMain(HealthBarsVarying input [[stage_in]],constant HealthBarsUniforms& u [[buffer(2)]],
 texture2d<float> atlas [[texture(0)]],sampler atlasSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=input.uv.xy;if(u.sampling.x)uv.y=1-uv.y;
 const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(u.sampling.y)uv.x=clamp(uv.x,0.0f,1.0f);
 if(u.sampling.z)uv.y=clamp(uv.y,0.0f,1.0f);
 float4 texel=atlas.sample(atlasSampler,uv,gradient2d(dx,dy));
 texel.a*=input.color.a;
 const float4 color=mix(input.color,texel,input.uv.z);
 if(color.a<.05f)discard_fragment();
 return color;
}
)MSL";
}
