/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

// Source: exact Recoil e9d199329538cd9078a621c5e0f9ada416e1229f
// BumpWaterVS/FS, BumpWaterCoastBlurVS/FS and VA_TYPE_2DTC render-buffer shaders.
// Native coordinates/resources replace compatibility builtins; the original
// wave, coast, lighting, refraction and reflection equations remain separate.
// NATIVE_WATER_FLAGS specializes precisely the original opt_* branches.
namespace rendering::metal4 {
inline constexpr const char* NATIVE_BUMP_WATER_SHADERS=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
#ifndef NATIVE_WATER_FLAGS
#define NATIVE_WATER_FLAGS 0
#endif
struct WaterUniforms {
	float4x4 viewProjection,view,shadow;
	float4 eyeFrame,mapMidSpecularPower,screenInverseTextureInverse;
	float4 viewportOriginProjection,texGenPlane,shadingPlane,surfaceColor;
	float4 diffuseColorFactor,specularColorFactor,sunDirectionAmbient;
	float4 fresnelDistortion,blurPerlin,perlinWave,waveCaustic;
	float4 fogColor,fogParameters,fragmentToLogical;
	uint4 flags;
};
struct WaterVarying {
	float4 position [[position]];
	float3 eyeVec [[RECOIL_SAMPLE_PERSPECTIVE]],ligVec [[RECOIL_SAMPLE_PERSPECTIVE]],worldPos [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 tc0 [[RECOIL_SAMPLE_PERSPECTIVE]],tc1 [[RECOIL_SAMPLE_PERSPECTIVE]],tc2 [[RECOIL_SAMPLE_PERSPECTIVE]],tc3 [[RECOIL_SAMPLE_PERSPECTIVE]],tc4 [[RECOIL_SAMPLE_PERSPECTIVE]];
	float2 shading [[RECOIL_SAMPLE_PERSPECTIVE]];
	float fog [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex WaterVarying BumpWaterVertex(uint id [[vertex_id]],
	device const packed_float3* positions [[buffer(0)]],constant WaterUniforms& u [[buffer(1)]])
{
	const float3 pos=positions[id];const float frame=u.eyeFrame.w;
	WaterVarying o;
	o.tc0=u.texGenPlane*pos.xzxz;o.shading=u.shadingPlane.xy*pos.xz;
	const float fstart=u.blurPerlin.z,f=u.blurPerlin.w;const float2 wind=float2(15.0);
	o.tc1.xy=(float2(-1,-1)+o.tc0.zw+0.75)*fstart+frame*wind;
	o.tc1.zw=(float2(-1,1)+o.tc0.zw+0.50)*fstart*f-frame*wind;
	o.tc2.xy=(float2(1,-1)+o.tc0.zw+0.25)*fstart*f*f+frame*wind;
	o.tc2.zw=(float2(1,1)+o.tc0.zw+0.00)*fstart*f*f*f+frame*wind;
	o.tc3.xy=o.tc0.zw*160.0+frame*2.5;o.tc3.zw=o.tc0.zw*90.0-frame*2.0;
	o.tc4.xy=o.tc0.zw*2.0;o.tc4.zw=o.tc0.zw*6.0+frame*0.37;
	float4 wave=float4(pos,1);wave.y=3.0*(cos(frame*500.0+pos.z)*sin(frame*500.0+pos.x/1000.0));
	o.eyeVec=u.eyeFrame.xyz-wave.xyz;o.ligVec=normalize(u.sunDirectionAmbient.xyz*20000.0+u.mapMidSpecularPower.xyz-wave.xyz);
	o.worldPos=wave.xyz;o.fog=(u.view*wave).z;o.position=u.viewProjection*wave;
	if(u.flags.x&256u)o.position.z=(o.position.z+o.position.w)*0.5;
	return o;
}
float2 WaterUV(float2 uv,uint unit,constant WaterUniforms& u) {
	if(u.flags.y&(1u<<unit))uv.y=1.0-uv.y;return uv;
}
float4 WaterSample(texture2d<float> image,sampler filter,float2 uv,uint unit,constant WaterUniforms& u) {
	return image.sample(filter,WaterUV(uv,unit,u));
}
float WaterDepth(depth2d<float> image,sampler filter,float2 uv,constant WaterUniforms& u) {
	return image.sample(filter,WaterUV(uv,7,u));
}
float WaterEyeZ(float d,constant WaterUniforms& u) {
	return u.viewportOriginProjection.z/(d*-2.0+1.0-u.viewportOriginProjection.w);
}
float WaterSmoothLimit(float x,float edge) {
	// GLSL mod(x,y) is x-y*floor(x/y), not MSL fmod for negative inputs.
	float limitcurv=edge-((x-edge*floor(x/edge))*edge)/(1.0-edge);
	return mix(x,limitcurv,step(edge,x));
}
float4 WaterWaveIntensity(float4 v) {
	float4 front=float4(1.0)-abs(v-float4(0.85))/float4(1.0-0.85);
	return max(front,select(float4(0),float4(1),v<float4(0.85))*v*0.5);
}
fragment float4 BumpWaterFragment(WaterVarying i [[stage_in]],constant WaterUniforms& u [[buffer(1)]],
	texture2d<float> normalmap [[texture(0)]],texture2d<float> heightmap [[texture(1)]],
	texture2d<float> caustic [[texture(2)]],texture2d<float> foam [[texture(3)]],
	texture2d<float> reflection [[texture(4)]],texture2d<float> refraction [[texture(5)]],
	texture2d<float> coastmap [[texture(6)]],depth2d<float> depthmap [[texture(7)]],
	texture2d<float> waverand [[texture(8)]],depth2d<float> shadowmap [[texture(9)]],
	texture2d<float> infotex [[texture(10)]],texture2d<float> shadowColor [[texture(11)]],
	sampler s0 [[sampler(0)]],sampler s1 [[sampler(1)]],sampler s2 [[sampler(2)]],sampler s3 [[sampler(3)]],
	sampler s4 [[sampler(4)]],sampler s5 [[sampler(5)]],sampler s6 [[sampler(6)]],sampler s7 [[sampler(7)]],
	sampler s8 [[sampler(8)]],sampler s9 [[sampler(9)]],sampler s10 [[sampler(10)]],sampler s11 [[sampler(11)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(recoilOriginalSampleID));
	const float frame=u.eyeFrame.w;
	const float2 clampedWorldPos=clamp(i.worldPos.xz,float2(0),float2(1)/u.texGenPlane.xy);
	const float2 window=i.position.xy*u.fragmentToLogical.xy+u.fragmentToLogical.zw;
	const float2 screenPos=window-u.viewportOriginProjection.xy;
	const float2 screencoord=screenPos*u.screenInverseTextureInverse.zw;
	float2 reftexcoord=screenPos*u.screenInverseTextureInverse.xy;
	float outside=0,invwaterdepth=0;float2 coast=float2(0);
#if (NATIVE_WATER_FLAGS & 4)
	float3 coastSample=WaterSample(coastmap,s6,i.tc0.xy,6,u).rgb;coast=coastSample.rg;invwaterdepth=coastSample.b;
#else
	invwaterdepth=WaterSample(heightmap,s1,i.tc0.xy,1,u).a;
#endif
#if (NATIVE_WATER_FLAGS & 32)
	outside=min(1.0,distance(clampedWorldPos,i.worldPos.xz));invwaterdepth=mix(invwaterdepth,1.0,step(0.5,outside));
#endif
	float waterdepth=1.0-invwaterdepth;float shallowScale=waterdepth;
#if (NATIVE_WATER_FLAGS & 8)
	float texZ=WaterEyeZ(WaterDepth(depthmap,s7,screencoord,u),u),fragZ=WaterEyeZ(i.position.z,u);
	shallowScale=clamp(abs(texZ-fragZ)*0.333,0.0,1.0);
#endif
	const float a=u.perlinWave.x;
	float3 octave1=(WaterSample(normalmap,s0,i.tc1.xy,0,u).rgb*2.0-1.0)*a;
	float3 octave2=(WaterSample(normalmap,s0,i.tc1.zw,0,u).rgb*2.0-1.0)*a*a;
	float3 octave3=(WaterSample(normalmap,s0,i.tc2.xy,0,u).rgb*2.0-1.0)*a*a*a;
	float3 octave4=(WaterSample(normalmap,s0,i.tc2.zw,0,u).rgb*2.0-1.0)*a*a*a*a;
	float3 normal=normalize(octave1+octave2+octave3+octave4).xzy;
	float3 eVec=normalize(i.eyeVec);float eyeNormalCos=dot(-eVec,normal),angle=1.0-abs(eyeNormalCos);
	float3 shadowOcc=float3(1);
#if (NATIVE_WATER_FLAGS & 64)
	float4 shadowPos=u.shadow*float4(i.worldPos+float3(normal.x,0,normal.z)*10.0,1);shadowPos.xy+=float2(0.5);
	float sh=shadowmap.sample_compare(s9,WaterUV(shadowPos.xy/shadowPos.w,9,u),shadowPos.z/shadowPos.w);
	float3 shColor=WaterSample(shadowColor,s11,shadowPos.xy,11,u).rgb;
	shadowOcc=mix(1.0,sh,u.waveCaustic.w)*shColor;
#endif
	float3 reflectDir=reflect(normalize(-i.ligVec),normal);
	float specular=angle*pow(max(dot(reflectDir,eVec),0.0),u.mapMidSpecularPower.w)*u.specularColorFactor.w*shallowScale;
	float3 sunLow=u.sunDirectionAmbient.xyz*float3(1,0.1,1);
	float diffuse=pow(max(dot(normal,sunLow),0.0),3.0)*u.diffuseColorFactor.w;
	float ambient=smoothstep(-1.3,0.0,eyeNormalCos)*u.sunDirectionAmbient.w;
	float3 waterSurface=u.surfaceColor.rgb+u.diffuseColorFactor.rgb*diffuse+float3(ambient);
	float surfaceMix=(u.surfaceColor.a+diffuse)*shallowScale;
	float refractDistortion=60.0*(1.0-pow(i.position.z,80.0))*shallowScale;
#if (NATIVE_WATER_FLAGS & 128)
	float3 info=(WaterSample(infotex,s10,u.shadingPlane.xy*clampedWorldPos,10,u).rgb-0.5)*0.5;
	waterSurface+=mix(info,float3(0),outside);
#endif
	float4 color=float4(0,0,0,1);
#if (NATIVE_WATER_FLAGS & 2)
	float2 refrUV=screencoord+normal.xz*refractDistortion*u.screenInverseTextureInverse.xy;
	float3 refrColor=WaterSample(refraction,s5,refrUV,5,u).rgb;
	float refrZ=WaterEyeZ(WaterDepth(depthmap,s7,refrUV,u),u),fragmentZ=WaterEyeZ(i.position.z,u);
	float mixback=clamp(refrZ-fragmentZ,0.0,1.0);
	float3 origRefrColor=WaterSample(refraction,s5,screencoord,5,u).rgb;
	refrColor=mix(refrColor,origRefrColor,mixback);
	color.rgb=mix(refrColor,waterSurface,0.1+surfaceMix*0.100);
#else
	color.rgb=waterSurface;color.a=surfaceMix+specular;
#endif
	if(waterdepth>0.0){
		float3 caust=WaterSample(caustic,s2,i.tc0.zw*u.waveCaustic.y,2,u).rgb;
#if (NATIVE_WATER_FLAGS & 2)
		// Preserve the original reversed-edge smoothstep's source arithmetic.
		float t=clamp((abs(waterdepth-0.5)-0.45)/(0.0-0.45),0.0,1.0);
		float caustBlend=t*t*(3.0-2.0*t);color.rgb+=caust*caustBlend*u.waveCaustic.z;
#else
		color.a*=min(waterdepth*4.0,1.0);color.rgb+=caust*(1.0-waterdepth)*7.5*u.waveCaustic.z;
#endif
	}
	float3 shore=float3(0);
#if (NATIVE_WATER_FLAGS & 4)
	float coastdist=coast.g+octave3.x*0.1;
	if(coastdist>0.0){
		float underground=1.0-step(1.0,invwaterdepth);
		float3 wavefoam=WaterSample(foam,s3,i.tc3.xy+octave3.xy*u.perlinWave.w,3,u).rgb;
		wavefoam+=WaterSample(foam,s3,i.tc3.zw+octave3.xy*u.perlinWave.w,3,u).rgb;wavefoam*=u.waveCaustic.x;
		float4 waverands=WaterSample(waverand,s8,i.tc4.zw,8,u);
		float4 f=fract(float4(0.25,0.50,0.75,1)+frame*50.0+(i.tc1.x+i.tc1.y)*u.perlinWave.y);
		f=f*1.4-float4(coastdist);f=float4(1)-f*(1.0/u.perlinWave.z);f=clamp(f,0.0,1.0);f=WaterWaveIntensity(f);
		float intensity=dot(f,waverands)*coastdist;intensity*=WaterSmoothLimit(invwaterdepth,0.8)*1.5;
		shore+=wavefoam*underground*intensity;
		shore+=(wavefoam*wavefoam)*(underground*5.5*(coast.r*coast.r*coast.r)*(coastdist*coastdist*coastdist*coastdist));
	}
#endif
	color.rgb+=shadowOcc*shore;
	float fresnel=0;float3 reflColor=float3(0);
#if (NATIVE_WATER_FLAGS & 1)
	reftexcoord=float2(reftexcoord.x,1.0-reftexcoord.y);
	reftexcoord+=float2(0,3.0*u.screenInverseTextureInverse.y)+normal.xz*0.09*u.fresnelDistortion.w;
	reflColor=WaterSample(reflection,s4,reftexcoord,4,u).rgb;
#if (NATIVE_WATER_FLAGS & 16)
	float2 v=float2(0,u.blurPerlin.x);float s=u.blurPerlin.y;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s*s,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s*s*s,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s*s*s*s,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s*s*s*s*s,4,u).rgb;
	reflColor+=WaterSample(reflection,s4,reftexcoord+v*s*s*s*s*s*s,4,u).rgb;reflColor*=0.125;
#endif
	fresnel=u.fresnelDistortion.x+u.fresnelDistortion.y*pow(angle,u.fresnelDistortion.z);
#endif
	color.rgb=mix(color.rgb,reflColor,fresnel*shallowScale);color.rgb+=shadowOcc*specular*u.specularColorFactor.rgb;
	float fog=clamp((u.fogParameters.y-abs(i.fog))*u.fogParameters.w,0.0,1.0);
	color.rgb=mix(u.fogColor.rgb,color.rgb,fog);return color;
}

struct WaterUpdateVertex {packed_float2 position;packed_float4 uv;};
struct WaterMipVarying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex WaterMipVarying BumpWaterMipVertex(uint id [[vertex_id]]) {
	// Mesa u_gen_mipmap -> original MoltenVK vkCmdBlitImage uses one sampled
	// strip per level. Preserve that conversion into RGB5; Metal's opaque
	// generateMipmaps operation rounds exact half steps differently.
	const float2 positions[4]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(1,1)};
	const float2 uv[4]={float2(0,1),float2(1,1),float2(0,0),float2(1,0)};
	WaterMipVarying o;o.position=float4(positions[id],0,1);o.uv=uv[id];return o;
}
fragment float4 BumpWaterMipFragment(WaterMipVarying i [[stage_in]],constant uint& sourceLevel [[buffer(0)]],
	texture2d<float> source [[texture(0)]],sampler linearClamp [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(recoilOriginalSampleID));
	return source.sample(linearClamp,i.uv,level(float(sourceLevel)));
}
struct WaterUpdateVarying {float4 position [[position]];float4 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex WaterUpdateVarying BumpWaterUpdateVertex(uint id [[vertex_id]],device const WaterUpdateVertex* v [[buffer(0)]]) {
	// Store producer rows in original GL order (row0=bottom). This maps source
	// ortho0..1 y directly to increasing Metal image rows, preserving dFdy.
	WaterUpdateVarying o;o.position=float4(float2(v[id].position)*float2(2,-2)+float2(-1,1),0.5,1);o.uv=v[id].uv;return o;
}
fragment float4 BumpWaterNormalFragment(WaterUpdateVarying i [[stage_in]],texture2d<float> original [[texture(0)]],sampler nearest [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(recoilOriginalSampleID));
	// VA_TYPE_2DTC default SColor=255 and shader ucolor=1, alpha test=always.
	return original.sample(nearest,i.uv.xy);
}
struct CoastParameters {int radius;int renderToAtlas;uint2 reserved;};
float4 CoastTex(float2 offset,WaterUpdateVarying i,constant CoastParameters& p,
	texture2d<float> coast,texture2d<float> atlas,sampler nearest,float2 texel0,float2 texelScissor) {
	if(p.renderToAtlas)return coast.sample(nearest,i.uv.xy+offset*texel0);
	float2 scissor=i.uv.zw+offset*texelScissor;
	if(any(scissor>float2(1))||any(scissor<float2(0)))return atlas.sample(nearest,i.uv.xy);
	return atlas.sample(nearest,i.uv.xy+offset*texel0);
}
float2 CoastDistRect(float d,float2 offset) {
	float minDist=15.0-d*15.0;float2 dist;dist.x=floor(minDist);float iDist=dist.x*dist.x;minDist*=minDist;
	dist.y=sqrt(minDist-iDist);return dist+offset;
}
void CoastLoop(thread float& maxDist,thread float3& minDist,float n,WaterUpdateVarying i,constant CoastParameters& p,
	texture2d<float> coast,texture2d<float> atlas,sampler nearest,float2 texel0,float2 texelScissor) {
	float r=float(p.radius);float4 v1,v2;
	v1.x=CoastTex(float2(-n,r),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v1.y=CoastTex(float2(n,r),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v1.z=CoastTex(float2(-n,-r),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v1.w=CoastTex(float2(n,-r),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v2.x=CoastTex(float2(r,n),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v2.y=CoastTex(float2(r,-n),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v2.z=CoastTex(float2(-r,n),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v2.w=CoastTex(float2(-r,-n),i,p,coast,atlas,nearest,texel0,texelScissor).g;
	v1=max(v1,v2);v1.xy=max(v1.xy,v1.zw);v1.x=max(v1.x,v1.y);v1.x=max(v1.x,maxDist);
	float2 dist=CoastDistRect(v1.x,float2(r,n));if(dot(dist,dist)<minDist.z){maxDist=v1.x;minDist=float3(dist,dot(dist,dist));}
}
fragment float4 BumpWaterCoastFragment(WaterUpdateVarying i [[stage_in]],constant CoastParameters& p [[buffer(1)]],
	texture2d<float> coast [[texture(0)]],texture2d<float> atlas [[texture(1)]],sampler nearest [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(recoilOriginalSampleID));
	float2 texelScissor=float2(dfdx(i.uv.z),dfdy(i.uv.w)),texel0=float2(dfdx(i.uv.x),dfdy(i.uv.y));float4 out;
	if(p.radius==0)out=atlas.sample(nearest,i.uv.xy);
	else if(p.radius==10){
		float2 ground=CoastTex(float2(1,1),i,p,coast,atlas,nearest,texel0,texelScissor).rb;
		ground+=CoastTex(float2(-1,1),i,p,coast,atlas,nearest,texel0,texelScissor).rb;
		ground+=CoastTex(float2(-1,-1),i,p,coast,atlas,nearest,texel0,texelScissor).rb;
		ground+=CoastTex(float2(1,-1),i,p,coast,atlas,nearest,texel0,texelScissor).rb;
		out=atlas.sample(nearest,i.uv.xy);out.r=(ground.x+out.r==5.0)?1.0:0.93-(ground.y+out.b)/5.0;
	}else if(p.radius==9){
		float2 blur=coast.sample(nearest,i.uv.xy+float2(1,1)*texel0).rg;
		blur+=coast.sample(nearest,i.uv.xy+float2(-1,1)*texel0).rg;
		blur+=coast.sample(nearest,i.uv.xy+float2(-1,-1)*texel0).rg;
		blur+=coast.sample(nearest,i.uv.xy+float2(1,-1)*texel0).rg;
		out=coast.sample(nearest,i.uv.xy);out.r=step(5.0,blur.x+out.r);out.g=mix(out.g,blur.y*0.25,0.4);
	}else{
		float maxValue=0;float3 minDist=float3(1e9);
		// Exact original eight conditionals: radius8 still samples i0..7.
		for(uint n=0;n<8;++n)if(float(n)<=float(p.radius))CoastLoop(maxValue,minDist,float(n),i,p,coast,atlas,nearest,texel0,texelScissor);
		float fDist=1.0-min(15.0,sqrt(minDist.z))/15.0;
		out=p.renderToAtlas?coast.sample(nearest,i.uv.xy):atlas.sample(nearest,i.uv.xy);out.g=max(out.g,fDist*fDist*fDist);
	}
	// RGB5 has no alpha channel; Metal's equivalent BGR5A1 stores a constant1.
	// Atlas remains RGBA8 and retains the original sampled alpha.
	if(!p.renderToAtlas)out.a=1.0;return out;
}
)MSL";
} // namespace rendering::metal4
