/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

namespace rendering::metal4 {
// Literal family translation of gfx_guishader.lua and gui_pausescreen.lua.
// ScreenGPU owns image production, pass order, target load/store and retirement.
inline constexpr const char* NativePanelMetalPrograms = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;

struct PanelUniform {
	float4x4 mvp, textureMatrix;
	float4 rect, uv, blur, view, effect, tint;
	uint4 mode, alphaTest;
	float4 alphaValue;
	uint4 image[3];
};
struct PanelVarying { float4 position [[position]]; float2 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]]; };

float4 PanelRead(texture2d<float> image, int2 texel, uint4 flags)
{
	if (flags.w) return float4(0,0,0,1);
	if (flags.x) texel.y=int(image.get_height())-1-texel.y;
	return image.read(uint2(texel),0);
}
float4 PanelSample(texture2d<float> image, sampler imageSampler, float2 uv, uint4 flags)
{
	if (flags.w) return float4(0,0,0,1);
	if (flags.x) uv.y=1.0f-uv.y;
	if (flags.y || flags.z) {
		// GL_CLAMP clamps normalized coordinates before its border filter;
		// its LOD derivatives still come from the unclamped coordinates.
		const float2 dx=dfdx(uv),dy=dfdy(uv);
		if (flags.y) uv.x=clamp(uv.x,0.0f,1.0f);
		if (flags.z) uv.y=clamp(uv.y,0.0f,1.0f);
		return image.sample(imageSampler,uv,gradient2d(dx,dy));
	}
	return image.sample(imageSampler,uv);
}
bool PanelAlphaPass(float value, constant PanelUniform& u)
{
	if (!u.alphaTest.x) return true;
	const float reference=u.alphaValue.x;
	switch (u.alphaTest.y) {
		case 0: return false; case 1: return value<reference; case 2: return value==reference;
		case 3: return value<=reference; case 4: return value>reference; case 5: return value!=reference;
		case 6: return value>=reference; case 7: return true;
	}
	return false;
}

vertex PanelVarying PanelVertex(uint vertexId [[vertex_id]], uint instanceId [[instance_id]],
	device const packed_float2* corners [[buffer(0)]], device const packed_float2* tiles [[buffer(1)]],
	constant PanelUniform& u [[buffer(2)]], texture2d<float> tileMask [[texture(1)]])
{
	PanelVarying result;
	const float2 corner=float2(corners[vertexId]);
	if (u.mode.x==0) {
		const float2 tile=float2(tiles[instanceId]);
		const int2 t=int2(tile);
		float covered=PanelRead(tileMask,t,u.image[1]).r;
		if (u.mode.y==0) {
			const int2 last=int2(tileMask.get_width(),tileMask.get_height())-1;
			for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x)
				covered=max(covered,PanelRead(tileMask,clamp(t+int2(x,y),int2(0),last),u.image[1]).r);
		}
		const float2 pos=min((tile+corner)*u.blur.w,u.view.xy);
		result.texCoord=pos/u.view.xy;
		// z=0 in GL becomes 0.5 in Metal. The collapsed tile remains beyond
		// both clip volumes (GL z=2, Metal z=1.5) and emits no fragments.
		result.position=covered>0.5f?float4(result.texCoord*2.0f-1.0f,0.5f,1.0f):float4(2,2,1.5f,1);
	} else if (u.mode.x==1) {
		result.position=float4(corner*2.0f-1.0f,0.5f,1.0f);
		result.texCoord=corner;
	} else {
		const float2 pos=mix(u.rect.xy,u.rect.zw,corner);
		result.position=u.mvp*float4(pos,0,1);
		result.position.z=(result.position.z+result.position.w)*0.5f;
		result.texCoord=(u.textureMatrix*float4(mix(u.uv.xy,u.uv.zw,corner),0,1)).xy;
	}
	return result;
}

// Metal fragment positions use a top-left origin. Reconstruct actual GL
// window coordinates; original derivatives are with respect to +Y upward.
float2 PanelGLFragCoord(float4 position, constant PanelUniform& u)
{
	return float2(position.x,u.view.z-position.y);
}
float2 PanelQuadVector(float2 screenCoords)
{
	float2 q=fract(floor(screenCoords)*0.5f)*4.0f-1.0f;
	const float2 oddStartMirror=0.5f*float2(dfdx(q.x),-dfdy(q.y));
	return sign(q*oddStartMirror);
}
float4 PanelBlur(PanelVarying input, constant PanelUniform& u,
	texture2d<float> source, sampler sourceSampler, texture2d<float> stencil, sampler stencilSampler)
{
	const float2 fragCoord=PanelGLFragCoord(input.position,u);
	float4 blurred;
	if (u.mode.w&1u) {
		// Exact original INTEL_GPU fallback, including its optional /17 path.
		const float mask=PanelSample(stencil,stencilSampler,input.texCoord,u.image[2]).a;
		if (mask<0.01f) {
			if (u.mode.y==1) discard_fragment();
			return PanelRead(source,int2(fragCoord),u.image[0]);
		}
		const float2 offset=u.blur.xy*6.0f*u.blur.z;
		float4 sum=PanelSample(source,sourceSampler,input.texCoord,u.image[0])*4.0f;
		sum+=PanelSample(source,sourceSampler,input.texCoord+float2(offset.x,0),u.image[0])*2.0f;
		sum+=PanelSample(source,sourceSampler,input.texCoord-float2(offset.x,0),u.image[0])*2.0f;
		sum+=PanelSample(source,sourceSampler,input.texCoord+float2(0,offset.y),u.image[0])*2.0f;
		sum+=PanelSample(source,sourceSampler,input.texCoord-float2(0,offset.y),u.image[0])*2.0f;
		sum+=PanelSample(source,sourceSampler,input.texCoord+offset,u.image[0]);
		sum+=PanelSample(source,sourceSampler,input.texCoord-offset,u.image[0]);
		sum+=PanelSample(source,sourceSampler,input.texCoord+float2(offset.x,-offset.y),u.image[0]);
		sum+=PanelSample(source,sourceSampler,input.texCoord+float2(-offset.x,offset.y),u.image[0]);
		blurred=sum/((u.mode.w&2u)?16.0f:17.0f);
	} else {
		// Original ordinary and dFdxFine/dFdyFine programs share this quad
		// derivative operation. All lanes sample before the region test.
		const float2 q=PanelQuadVector(fragCoord);
		const float2 subpixel=u.blur.xy*q;
		float4 sum=float4(0);
		for (int i=0;i<=1;++i) for (int j=0;j<=1;++j) {
			const float2 sampleCoords=input.texCoord+float2(i,j)*6.0f*u.blur.z*subpixel+subpixel;
			sum+=PanelSample(source,sourceSampler,sampleCoords,u.image[0]);
		}
		const float4 inputadjx=sum-dfdx(sum)*q.x;
		const float4 inputadjy=sum+dfdy(sum)*q.y;
		const float4 inputdiag=inputadjx+dfdy(inputadjx)*q.y;
		sum+=inputadjx+inputadjy+inputdiag;
		blurred=sum/16.0f;
		const float mask=PanelSample(stencil,stencilSampler,input.texCoord,u.image[2]).a;
		if (mask<0.01f)
			return u.mode.y==1?float4(0):PanelRead(source,int2(fragCoord),u.image[0]);
	}
	if (u.mode.y==1) return blurred;
	const float4 below=PanelRead(source,int2(fragCoord),u.image[0]);
	return float4(blurred.rgb*blurred.a+below.rgb*(1.0f-blurred.a),
		blurred.a*blurred.a+below.a*(1.0f-blurred.a));
}
float4 PanelMask(PanelVarying input, constant PanelUniform& u, texture2d<float> source)
{
	const int2 origin=int2(PanelGLFragCoord(input.position,u))*int(u.mode.z);
	const int2 limit=min(origin+int(u.mode.z),int2(source.get_width(),source.get_height()));
	float covered=0;
	for (int y=origin.y;y<limit.y && covered==0.0f;++y) {
		for (int x=origin.x;x<limit.x;++x) {
			if (PanelRead(source,int2(x,y),u.image[0]).a>=u.effect.x) {covered=1;break;}
		}
	}
	return float4(covered);
}
float4 PanelPause(PanelVarying input, constant PanelUniform& u, texture2d<float> source, sampler sourceSampler)
{
	const float2 uv=input.texCoord;
	const float3 original=PanelSample(source,sourceSampler,uv,u.image[0]).rgb;
	const float intensity=(u.mode.w&4u)?
		sqrt(dot(original*original,float3(0.299f,0.587f,0.114f)))*(0.66f*1.15f*1.7320508f):
		length(original*float3(0.66f))*1.15f;
	float level;
	if (u.mode.w&8u) {
		const float t=clamp(intensity,0.1f,0.875f);
		level=t<0.275f?mix(0.22f,0.32f,(t-0.1f)/0.175f):
			(t<0.425f?mix(0.32f,0.55f,(t-0.275f)/0.15f):
			(t<0.625f?mix(0.55f,0.7f,(t-0.425f)/0.2f):mix(0.7f,0.82f,(t-0.625f)/0.25f)));
	} else level=intensity<0.5f?(intensity<0.2f?0.22f:(intensity<0.35f?0.32f:0.55f)):
		(intensity<0.75f?0.7f:0.82f);
	float3 grey=float3(intensity*0.9f*level);
	float alpha=u.effect.y;
	if (u.effect.z>0) {
		const float2 size=float2(source.get_width(),source.get_height());
		const float2 d=(uv*2.0f-1.0f)*float2(size.x/size.y,1);
		const float v=smoothstep(0.3f,1.0f,length(d)/length(float2(size.x/size.y,1)))*u.effect.z;
		grey*=1.0f-0.35f*v;
		alpha=min(alpha*(1.0f+0.8f*v),1.0f);
	}
	if (u.mode.w&16u) grey=min(grey*u.tint.xyz,float3(1));
	return float4(grey,alpha);
}
fragment float4 PanelFragment(PanelVarying input [[stage_in]], constant PanelUniform& u [[buffer(2)]],
	texture2d<float> source [[texture(0)]], sampler sourceSampler [[sampler(0)]],
	texture2d<float> stencil [[texture(2)]], sampler stencilSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 result;
	if (u.mode.x==0) result=PanelBlur(input,u,source,sourceSampler,stencil,stencilSampler);
	else if (u.mode.x==1) result=PanelMask(input,u,source);
	else result=PanelPause(input,u,source,sourceSampler);
	if (!PanelAlphaPass(result.a,u)) discard_fragment();
	return result;
}
)MSL";
} // namespace rendering::metal4
