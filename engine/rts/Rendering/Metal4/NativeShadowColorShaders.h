/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Source-equivalent original ProjFXVertShadowProg / ProjFXFragShadowProg.
inline constexpr const char* NATIVE_SHADOW_COLOR_SHADER=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ShadowParticleVertex {packed_float3 position,uvw;packed_float4 uvInfo;packed_float3 animation;uchar4 color;};
struct ShadowColorUniform {float4x4 view,projection;uint4 options;};
struct ShadowColorVarying {float4 position [[position]];float4 color,uv;float page,blend;};
vertex ShadowColorVarying ShadowColorVertex(uint id [[vertex_id]],device const ShadowParticleVertex* vertices [[buffer(0)]],constant ShadowColorUniform& u [[buffer(1)]]) {
 const ShadowParticleVertex v=vertices[id];const float3 a=float3(v.animation),uvw=float3(v.uvw);const float4 ui=float4(v.uvInfo);
 ShadowColorVarying o;float ap=fract(a.z);float maximum=a.x*a.y-1;ap*=maximum;
 float i0=floor(ap),i1=i0+1;o.blend=fract(ap);
 if(maximum>1){const float2 diff=uvw.xy-ui.xy;o.uv=diff.xyxy+float4(floor(fmod(i0,a.x)),floor(i0/a.x),floor(fmod(i1,a.x)),floor(i1/a.x))*ui.zwzw;o.uv/=a.xyxy;o.uv+=ui.xyxy;}else o.uv=uvw.xyxy;
 o.page=uvw.z;o.color=float4(v.color)/255.f;
 float4 light=u.view*float4(float3(v.position),1);light.xy+=float2(.5f);o.position=u.projection*light;
 if(u.options.x==0)o.position.z=(o.position.z+o.position.w)*.5f;return o;
}
fragment float4 ShadowColorFragment(ShadowColorVarying in [[stage_in]],constant ShadowColorUniform& u [[buffer(1)]],texture2d_array<float> atlas [[texture(0)]]) {
 constexpr sampler source(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
 const float4 c0=atlas.sample(source,in.uv.xy,uint(in.page)),c1=atlas.sample(source,in.uv.zw,uint(in.page));
 float4 color=mix(c0,c1,in.blend)*in.color;
 color.rgb=mix(float3(dot(color.rgb,float3(.299f,.587f,.114f))),color.rgb,float(u.options.y));
 if(!(color.a>0.f))discard_fragment();return float4(1-color.a+color.rgb*color.a,0);
}
)MSL";
}
