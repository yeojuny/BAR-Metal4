/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeGameDecalShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"GameDecalMSL(
// SPDX-License-Identifier: MIT
// Original exact30770 shader expressions: Copyright (c) 2026 Beherith.
// Typed Metal transport of the original small VS+GS, large VS and entire FS.
#include <metal_stdlib>
using namespace metal;
#define HEIGHTOFFSET 0.5
#define LOSDARKNESS 0.7
#define PARALLAX 0
#define AMBIENTOCCLUSION 0
#define USEGLOW 1
#define GLOWTHRESHOLD 0.99
#define FADEINTIME 20
#define SPECULAREXPONENT 5.0
#define SPECULARSTRENGTH 0.3
#define MINIMAPCOLORBLENDFACTOR 1.0
#define SINGLEQUADDECALSIZETHRESHOLD 24.0
struct DecalUniform {
 float4x4 cameraViewProj,cameraViewInv,shadowView;
 float4 timeInfo,mapSize,sunDir,sunAmbientMap,sunDiffuseMap;
 float4 waterAbsorb,waterMinimum,waterBase;
 uint4 options; // GL depth conversion, texture-flip mask, first row, alpha-test bits
 float4 alpha;
};
struct DecalInstance {
 float4 lengthwidthrotation,uvoffsets,alphastart_alphadecay_heatstart_heatdecay,worldPos,parameters;
};
struct DecalOut {
 float4 position [[position]];
 float4 g_uv [[RECOIL_SAMPLE_PERSPECTIVE]],g_position [[RECOIL_SAMPLE_PERSPECTIVE]],g_parameters [[RECOIL_SAMPLE_PERSPECTIVE]];
 float3 tangent [[RECOIL_SAMPLE_PERSPECTIVE]],bitangent [[RECOIL_SAMPLE_PERSPECTIVE]],normal [[RECOIL_SAMPLE_PERSPECTIVE]];
};
float2 ImageUV(float2 uv,uint image,uint flips) { if(flips & (1u<<image)) uv.y=1.0-uv.y;return uv; }
float2 HeightUV(float2 worldpos,float4 mapSize) {
 float2 inverseMapSize=float2(1.0)/mapSize.xy;
 float2 heightmaptexel=float2(8.0,8.0);
 worldpos+=float2(-8.0,-8.0)*(worldpos*inverseMapSize)+float2(4.0,4.0);
 float2 uvhm=clamp(worldpos,heightmaptexel,mapSize.xy-heightmaptexel);
 uvhm=uvhm*inverseMapSize;return uvhm;
}
float3x3 RotationY(float angle) {
 float s=sin(angle),c=cos(angle);
 return float3x3(float3(c,0.0,-s),float3(0.0,1.0,0.0),float3(s,0.0,c));
}
bool SphereOutsideXY(float4 wP,float wR,constant DecalUniform& u) {
 float3 ToCamera=wP.xyz-u.cameraViewInv[3].xyz;
 float ratio=wR*rsqrt(dot(ToCamera,ToCamera));
 float4 clip=u.cameraViewProj*wP;
 float2 clipVec=clip.ww*(1.0+ratio);
 return any(abs(clip.xy)>clipVec);
}
float Alpha(DecalInstance d,constant DecalUniform& u) {
 float currentFrame=u.timeInfo.x+u.timeInfo.w;
 float lifetonow=currentFrame-d.worldPos.w;
 float currentAlpha=min(1.0,(lifetonow/d.parameters.w))*d.alphastart_alphadecay_heatstart_heatdecay.x-lifetonow*d.alphastart_alphadecay_heatstart_heatdecay.y;
 return clamp(currentAlpha,0.0,d.lengthwidthrotation.w);
}
float Heat(DecalInstance d,constant DecalUniform& u) {
 float lifetonow=(u.timeInfo.x+u.timeInfo.w)-d.worldPos.w;
 float heatdecay=d.alphastart_alphadecay_heatstart_heatdecay.w;
 float heatstart=d.alphastart_alphadecay_heatstart_heatdecay.z;
 float heatsustain=d.parameters.y;
 return heatstart*exp(-0.033*step(heatsustain,lifetonow)*(lifetonow-heatsustain)*heatdecay);
}
DecalOut Emit(float2 local,float2 uv,DecalInstance d,constant DecalUniform& u,
 texture2d<float> heightmapTex,sampler heightSampler,float alpha,float heat) {
 DecalOut o;float3x3 rotY=RotationY(d.lengthwidthrotation.z);
 float3 dimensions=float3(d.lengthwidthrotation.x*.5,0.0,d.lengthwidthrotation.y*.5);
 float4 worldPos=float4(d.worldPos.xyz+rotY*(float3(local.x,0.0,local.y)*dimensions),1.0);
 worldPos.y=heightmapTex.sample(heightSampler,ImageUV(HeightUV(worldPos.xz,u.mapSize),0,u.options.y),level(0.0)).x+HEIGHTOFFSET;
 o.position=u.cameraViewProj*worldPos;
 o.position.z=o.position.z-512.0/o.position.w;
 if(u.options.x) o.position.z=(o.position.z+o.position.w)*0.5;
 float a=d.uvoffsets.y-d.uvoffsets.x,b=d.uvoffsets.w-d.uvoffsets.z;
 o.g_uv=float4(d.uvoffsets.x+a*uv.x,d.uvoffsets.z+b*uv.y,d.parameters.z,heat);
 o.g_position=float4(worldPos.xyz,alpha);o.g_parameters=d.parameters;o.g_parameters.w=heat;
 o.normal=float3(0,1,0);o.tangent=rotY*float3(1,0,0);o.bitangent=cross(o.normal,o.tangent);
 return o;
}
vertex DecalOut GameDecalGeometryVertex(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],
 const device DecalInstance* rows [[buffer(0)]],constant DecalUniform& u [[buffer(2)]],
 texture2d<float> heightmapTex [[texture(0)]],sampler heightSampler [[sampler(0)]]) {
 DecalInstance d=rows[u.options.z+instanceID];
 uint skip=0;
 if(d.lengthwidthrotation.x<SINGLEQUADDECALSIZETHRESHOLD && d.lengthwidthrotation.y<SINGLEQUADDECALSIZETHRESHOLD)skip=2;
 if(SphereOutsideXY(float4(d.worldPos.xyz,1.0),d.lengthwidthrotation.x+d.lengthwidthrotation.y,u))skip=1;
 float alpha=Alpha(d,u);if(alpha<0.01)skip=1;
 if(skip==1 || (skip==2 && vertexID>=6)) { DecalOut o={};o.position=float4(2,2,2,1);return o; }
 // Expand the original four independent 10-vertex strips, keeping every
 // emitted triangle's winding and the single-quad branch's two triangles.
 uint strip=vertexID/24,triangle=(vertexID%24)/3,corner=vertexID%3;
 uint emitted=triangle+(corner==2?2:((triangle&1)?1-corner:corner));
 float2 local,uv;
 if(skip==2) { uint column=emitted/2;bool bottom=emitted&1;local=float2(1.0-2.0*column,bottom?-1.0:1.0);uv=float2(1.0-column,bottom?0.0:1.0); }
 else { float v=float(strip)*.25;bool bottom=emitted&1;float column=float(emitted/2);local=float2(1.0-.5*column,2.0*v-(bottom?1.0:.5));uv=float2(1.0-.25*column,v+(bottom?0.0:.25)); }
 return Emit(local,uv,d,u,heightmapTex,heightSampler,alpha,Heat(d,u));
}
vertex DecalOut GameDecalMeshVertex(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],
 const device packed_float2* vertices [[buffer(0)]],const device DecalInstance* rows [[buffer(1)]],constant DecalUniform& u [[buffer(2)]],
 texture2d<float> heightmapTex [[texture(0)]],sampler heightSampler [[sampler(0)]]) {
 DecalInstance d=rows[u.options.z+instanceID];float2 local=vertices[vertexID];
 // Complete31657 performs the same early alpha/sphere rejection in its
 // large vertex stage before any height sampling or triangle rasterization.
 float alpha=Alpha(d,u);
 if(alpha<0.01 || SphereOutsideXY(float4(d.worldPos.xyz,1.0),d.lengthwidthrotation.x+d.lengthwidthrotation.y,u)) {
  DecalOut o={};o.position=float4(-2.0,-2.0,-2.0,1.0);return o;
 }
 return Emit(local,local*.5+.5,d,u,heightmapTex,heightSampler,alpha,Heat(d,u));
}
struct FragmentProgram {
 float4 g_uv,g_position,g_parameters,fragColor;
 float3x3 tbnmatrix;
 float4x4 cameraViewInv,shadowView;
 float4 mapSize,sunDir,sunAmbientMap,sunDiffuseMap;
 float3 waterAbsorb,waterMinimum,waterBase;
 uint flips;
 texture2d<float> miniMapTex,infoTex,mapNormalsTex,atlasColorAlpha,atlasNormals;
 depth2d<float> shadowTex;
 sampler minimapSampler,infoSampler,normalSampler,diffuseSampler,atlasNormalSampler,shadowSampler;
 float2 ImageUV(float2 uv,uint image) { return ::ImageUV(uv,image,flips); }
 float2 heightmapUVatWorldPos(float2 worldpos) { return HeightUV(worldpos,mapSize); }
 float4 waterBlend(float fragmentheight) {
  if(fragmentheight>=0) return float4(0.0);
  float4 result=float4(1.0,1.0,1.0,0.0);
  result.rgb=waterBase;result.rgb-=waterAbsorb*clamp(-fragmentheight,0.0,1023.0);
  result.rgb=max(result.rgb,waterMinimum);result.a=clamp(-fragmentheight*.1,0.0,1.0);return result;
 }
// This blends shadow amounts along the warp
float shadowAtWorldPosSoft(float3 worldPos){
		float4 shadowVertexPos = shadowView * float4(worldPos,1.0);
		shadowVertexPos.xy += float2(0.5);
		float localShadow = clamp(shadowTex.sample_compare(shadowSampler, ImageUV(shadowVertexPos.xy / shadowVertexPos.w, 3), shadowVertexPos.z / shadowVertexPos.w), 0.0, 1.0);
		float shadowDelta = 0.25 * (abs(dfdx(localShadow)) + abs(dfdy(localShadow)));
		localShadow = mix(shadowDelta, 1.0 - shadowDelta, localShadow);
		return localShadow;
}

float3 Temperature(float temperatureInKelvins)
{
	float3 retColor;
	
	float coldness = clamp((temperatureInKelvins - 300)* 0.0005, 0.0, 1.0) ;
	temperatureInKelvins = clamp(temperatureInKelvins, 1000.0, 40000.0) / 100.0;
	
	if (temperatureInKelvins <= 66.0)
	{
		retColor.r = 1.0;
		retColor.g = 0.39008157876901960784 * log(temperatureInKelvins) - 0.63184144378862745098;
	}
	else
	{
		float t = temperatureInKelvins - 60.0;
		retColor.r = 1.29293618606274509804 * pow(t, -0.1332047592);
		retColor.g = 1.12989086089529411765 * pow(t, -0.0755148492);
	}
	
	if (temperatureInKelvins >= 66.0)
		retColor.b = 1.0;
	else if(temperatureInKelvins <= 19.0)
		retColor.b = 0.0;
	else
		retColor.b = 0.54320678911019607843 * log(temperatureInKelvins - 10.0) - 1.19625408914;

	retColor = clamp(retColor,0.0,1.0);
	retColor = mix( float3(0.0),retColor, coldness);
	return retColor;
}

// RNM func from: https://blog.selfshadow.com/publications/blending-in-detail/
float3 ReOrientNormalPacked(float3 basenormal, float3 detailnormal){ // for normals packed to [0,1], z-up
	float3 t = basenormal.xyz   * float3( 2.0,  2.0, 2.0) + float3(-1.0, -1.0,  0.0);
	float3 u = detailnormal.xyz * float3(-2.0, -2.0, 2.0) + float3( 1.0,  1.0, -1.0);
	float3 r = t * dot(t, u) - u * t.z;
	return normalize(r);
}
float3 ReOrientNormalUnpacked(float3 basenormal, float3 detailnormal){ // for unpacked [-1,1] normals, only works when Z is up
	float3 t = basenormal.xyz + float3(0.0, 0.0,  1.0);
	float3 u = detailnormal.xyz * float3(-1.0, -1.0, 1.0);
	float3 r = t * dot(t, u)/t.z - u;
	return normalize(r);
}

float2 ParallaxMapping(float2 texCoords, float3 viewDir, float height)
{    
    float2 p = (viewDir.xz)* (height * 1);
    return p;    
} 

void Run()
{
	float3 campos = cameraViewInv[3].xyz;
	float3 camtoworld = normalize(g_position.xyz - campos);
	
	#if (PARALLAX == 1)
	float3 tangentviewpos = (tbnmatrix * campos.xzy).xyz; // Y points to camera
	float3 tangentfragpos = (tbnmatrix * g_position.xzy).xyz; // Y points up
	float3 tangentviewdir =  normalize(tangentfragpos - tangentviewpos); // Y points up!
		float4 tex3color = texture(atlasHeights, g_uv.xy);
		//float height = 
		
		// do parallax here !
		float2 parallaxUV = (tangentviewdir.xz) * (1.0 - tex3color.b )* 0.00002;
	#else
		float2 parallaxUV = float2(0.0);
		
	#endif

	
	float4 tex1color = atlasColorAlpha.sample(diffuseSampler, ImageUV(g_uv.xy - parallaxUV.xy, 5));
	if(tex1color.a*tex1color.a*g_position.w<0.0039 && g_parameters.w*tex1color.a<300.0) {
		discard_fragment();return;
	}
	float2 uvhm = heightmapUVatWorldPos(g_position.xz);
	float4 minimapcolor = miniMapTex.sample(minimapSampler, ImageUV(uvhm, 1), level(0.0));
	float3 mapnormal = mapNormalsTex.sample(normalSampler, ImageUV(uvhm, 4), level(0.0)).rgg; // seems to be in the [-1, 1] range!, raaa is its true return
	mapnormal.g = sqrt( 1.0 - dot( mapnormal.rb, mapnormal.rb)); // reconstruct Y	from it
	float offaxis = 1.0 - clamp(dot(mapnormal,-camtoworld), 0.0, 1.0);
	float bias = (offaxis*offaxis)*-2.0;
	
	if(bias<-0.01)tex1color = atlasColorAlpha.sample(diffuseSampler, ImageUV(g_uv.xy - parallaxUV.xy, 5), metal::bias(bias));
	tex1color.rgb = mix (tex1color.rgb, float3(dot(tex1color.rgb, float3(0.299, 0.587, 0.114))), g_parameters.x);
	
	float4 tex2color = atlasNormals.sample(atlasNormalSampler, ImageUV(g_uv.xy - parallaxUV.xy, 6), metal::bias(bias));
	float3 fragNormal = tex2color.rgb * 2.0 - 1.0;
	if (dot(tex2color.rgb, float3(1.0)) < 0.001) fragNormal = float3(0.0, 0.0, 1.0);// check if the normals are missing, if yes, then sub with Z up. 
	
	// Ambient color calculation, tint with minimap, and with sunAmbientMap
	float3 blendedcolor = tex1color.rgb * (minimapcolor.rgb * 2.0); // just your basic color modulation
	blendedcolor.rgb = mix(tex1color.rgb, blendedcolor.rgb, MINIMAPCOLORBLENDFACTOR);
	fragColor.rgb = blendedcolor * (sunAmbientMap.rgb * 2.0); 
	
	// take the square of the alpha, this is not ideal, but works well in practice
	fragColor.a = tex1color.a * tex1color.a * 0.999; 
	
	// Reorient the normals of the decal according to what we sampled from mapnormals and decal
	float3 worldspacenormal = tbnmatrix * fragNormal.rgb;
	float3 reorientedNormal = ReOrientNormalUnpacked(mapnormal.xzy, worldspacenormal.xzy).xzy;
	
	//debug the normals:
	/* 
		if (fract(gl_FragCoord.x * 0.01)>0.5)
			fragColor.rgb = (worldspacenormal + 1.0) * 0.5;
		else
			fragColor.rgb = (reorientedNormal + 1.0) * 0.5;
		fragColor.a = 1.0;
		return;
	*/
	// diffuse lighting, apply global sunDiffuseMap
	float diffuselight = clamp(dot(sunDir.xyz, reorientedNormal), -1.0, 1.0);
	fragColor.rgb += float3(diffuselight) * ( fragColor.rgb * sunDiffuseMap.rgb * 2.0);
	
	// Specular Color
	float3 reflvect = reflect(normalize(sunDir.xyz), reorientedNormal);
	float specular = clamp(pow(clamp(dot(camtoworld, reflvect), 0.0, 1.0), SPECULAREXPONENT), 0.0, 1.0) * SPECULARSTRENGTH;// * shadow;
	fragColor.rgb += fragColor.rgb * specular;
	
	// Apply darkening based on LOS texture
	float2 losUV = clamp(g_position.xz, float2(0.0), mapSize.xy ) / mapSize.zw;
	float loslevel = dot(float3(0.33), infoTex.sample(infoSampler, ImageUV(losUV, 2)).rgb) ; // lostex is PO2
	loslevel = clamp(loslevel * 4.0 - 1.0, LOSDARKNESS, 1.0);
	fragColor.rgb *= loslevel;
	
	// Blend underwater parts of the decals correctly
	float4 waterblendfactors = waterBlend(g_position.y);
	fragColor.rgb = mix(fragColor.rgb, fragColor.rgb * waterblendfactors.rgb, waterblendfactors.a);
	#if (AMBIENTOCCLUSION == 1) 
		float4 tex4color = texture(atlasORM, g_uv.xy  - parallaxUV.xy);
	#else
		float4 tex4color = float4 (0.0);
	#endif
	
	// Darken the whole goddamned decal:
	fragColor.rgb *= 0.80; 
	
	// calculate the shadow factor, note that this is partially smoothed
	float shadow = shadowAtWorldPosSoft(g_position.xyz);
	fragColor.rgb *= clamp(shadow, 0.5, 1.0);
	
	// Fade the decal out with time
	fragColor.a *= g_position.w;
	//fragColor.a = fract(g_position.w*10);
	
	// add emissive heat, if required
	#if (USEGLOW == 1) 
	if(g_parameters.w*tex1color.a>=300.0) {
		float glowChannel = tex2color.a * tex1color.a; // Could use a power operator here?

		float hotness = max(0.0,g_parameters.w);
		float3 heatColor = Temperature(hotness*glowChannel);
		//fragColor.rgb += heatColor * pow(glowChannel.r, 2) * hotness ;
		//fragColor.rgb = float3(fract(g_uv.w*20));
		fragColor.rgb += (heatColor.rgb * max(glowChannel,0.0)*12); //was *10 - icex increase to more brightness
		
		//experiment with glowadd:
		// we kinda need to additively blend here... 
		float heatalpha = dot (float3(1.0),heatColor);
		fragColor.rgba +=  g_parameters.z * heatalpha * float4(heatColor.rgb ,g_position.w); 
	}
	#endif 
	
	//fragColor.a = 1.0;
	// plenty of debug outputs for your viewing pleasure
	//fragColor.a *= g_parameters.z ;
	//fragColor.r += g_parameters.w;
	//fragColor.rgb += heatColor;
	//fragColor.rgb = tbnmatrix[2] * 0.5 + 0.5;
	//fragColor.rgb = tangentviewdir.yyy * 0.5 + 0.5;
	//fragColor.rgb = tex1color.rgb;
	//fragColor.rgb = tex3color.bbb;
	//fragColor.rgb = abs((parallaxUV.xyx * 10) + 0.5);
	//fragColor.rgb = fract(tspace.xyz* 0.01);
	//fragColor.rgb = reorientedNormal.rgb * 0.5 + 0.5;
	//fragColor.rgb = max(float3(parallaxUV.x, - parallaxUV.x, 0.0) * 10, 0.0);
	//fragColor.a = 1.0;
	//fragColor.rgb = g_uv.zyy;
	//fragColor.rgba = float4(0.5);
	
	//if (fract(gl_FragCoord.x * 0.01) > 0.5 ) fragColor.rgba = tex1color.rgba;
	//else fragColor.rgba = tex2color.rgba;
}
};
fragment float4 GameDecalFragment(DecalOut in [[stage_in]],constant DecalUniform& u [[buffer(2)]],
 texture2d<float> minimap [[texture(1)]],texture2d<float> info [[texture(2)]],depth2d<float> shadow [[texture(3)]],
 texture2d<float> normals [[texture(4)]],texture2d<float> diffuse [[texture(5)]],texture2d<float> atlasNormals [[texture(6)]],
 sampler minimapSampler [[sampler(1)]],sampler infoSampler [[sampler(2)]],sampler shadowSampler [[sampler(3)]],
 sampler normalSampler [[sampler(4)]],sampler diffuseSampler [[sampler(5)]],sampler atlasNormalSampler [[sampler(6)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 FragmentProgram p;p.g_uv=in.g_uv;p.g_position=in.g_position;p.g_parameters=in.g_parameters;
 p.tbnmatrix=float3x3(in.tangent,in.bitangent,in.normal);p.cameraViewInv=u.cameraViewInv;p.shadowView=u.shadowView;
 p.mapSize=u.mapSize;p.sunDir=u.sunDir;p.sunAmbientMap=u.sunAmbientMap;p.sunDiffuseMap=u.sunDiffuseMap;
 p.waterAbsorb=u.waterAbsorb.xyz;p.waterMinimum=u.waterMinimum.xyz;p.waterBase=u.waterBase.xyz;p.flips=u.options.y;
 p.miniMapTex=minimap;p.infoTex=info;p.shadowTex=shadow;p.mapNormalsTex=normals;p.atlasColorAlpha=diffuse;p.atlasNormals=atlasNormals;
 p.minimapSampler=minimapSampler;p.infoSampler=infoSampler;p.shadowSampler=shadowSampler;p.normalSampler=normalSampler;p.diffuseSampler=diffuseSampler;p.atlasNormalSampler=atlasNormalSampler;
 p.Run();
 if(u.options.w & 256u) {
  float a=p.fragColor.a,r=u.alpha.x;bool pass=false;
  switch(u.options.w & 255u) {case 0:pass=false;break;case 1:pass=a<r;break;case 2:pass=a==r;break;case 3:pass=a<=r;break;case 4:pass=a>r;break;case 5:pass=a!=r;break;case 6:pass=a>=r;break;case 7:pass=true;break;}
  if(!pass)discard_fragment();
 }
 return p.fragColor;
}
)GameDecalMSL";
}
