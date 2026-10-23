/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Original ProjFXVertShadowProg / ProjFXFragShadowProg. The alpha component of
// the physical RGBA target is padding; the original RGB/R output never writes
// it and never consumes it as a source-alpha blend factor.
inline constexpr const char* NativeOriginalShadowParticlesShaders=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct OriginalColorVertex {packed_float3 position,uvw;packed_float4 uvInfo;packed_float3 animation;uchar4 color;};
struct OriginalColorUniform {float4x4 view,projection;float4 alphaControl,sampling;uint4 swizzle,image,options;};
struct OriginalColorVarying {float4 position [[position]];float4 color,uv;float page,blend;};
vertex OriginalColorVarying OriginalColorVertexMain(uint id [[vertex_id]],device const OriginalColorVertex* vertices [[buffer(0)]],constant OriginalColorUniform& u [[buffer(1)]]) {
 const auto v=vertices[id];const float3 a=float3(v.animation),uvw=float3(v.uvw);const float4 ui=float4(v.uvInfo);
 OriginalColorVarying o;float ap=fract(a.z);const float maximum=a.x*a.y-1;ap*=maximum;
 const float i0=floor(ap),i1=i0+1;o.blend=fract(ap);
 if(maximum>1){const float2 diff=uvw.xy-ui.xy;o.uv=diff.xyxy+float4(floor(i0-a.x*floor(i0/a.x)),floor(i0/a.x),floor(i1-a.x*floor(i1/a.x)),floor(i1/a.x))*ui.zwzw;o.uv/=a.xyxy;o.uv+=ui.xyxy;}else o.uv=uvw.xyxy;
 o.page=uvw.z;o.color=float4(v.color)/255.f;
 float4 light=u.view*float4(float3(v.position),1);light.xy+=.5f;o.position=u.projection*light;
 if(!u.options.x)o.position.z=(o.position.z+o.position.w)*.5f;if(u.options.z)o.position.y=-o.position.y;return o;
}
float OriginalColorChannel(float4 value,uint channel) {
 switch(channel){case 0x1903:return value.r;case 0x1904:return value.g;case 0x1905:return value.b;case 0x1906:return value.a;case 1:return 1.f;default:return 0.f;}
}
float4 OriginalColorChannels(float4 value,constant OriginalColorUniform& u) {
 if(u.image.x==2u){const float d=value.r;value=u.image.z==0x1909u?float4(d,d,d,1):u.image.z==0x8049u?float4(d):float4(0,0,0,d);}
 return float4(OriginalColorChannel(value,u.swizzle.x),OriginalColorChannel(value,u.swizzle.y),OriginalColorChannel(value,u.swizzle.z),OriginalColorChannel(value,u.swizzle.w));
}
float2 OriginalColorUV(float2 uv,constant OriginalColorUniform& u){if(u.image.y)uv.y=1-uv.y;return uv;}
// Resource declarations are specialized with the same image kind as the PSO.
#if ORIGINAL_IMAGE_KIND != 0
#if ORIGINAL_TEXTURE_ARRAY
float4 OriginalColorSample(float2 uv,float layer,constant OriginalColorUniform& u,texture2d_array<float> image,sampler state) {
 uv=OriginalColorUV(uv,u);
 const uint slice=uint(clamp(floor(layer+.5f),0.f,float(image.get_array_size()-1)));
 return OriginalColorChannels(image.sample(state,uv,slice,bias(u.sampling.x)),u);
}
#else
#if ORIGINAL_IMAGE_KIND == 2
float4 OriginalColorSample(float2 uv,constant OriginalColorUniform& u,depth2d<float> image,sampler state) {
 uv=OriginalColorUV(uv,u);
 const float4 value=float4(image.sample(state,uv,bias(u.sampling.x)));
#else
float4 OriginalColorSample(float2 uv,constant OriginalColorUniform& u,texture2d<float> image,sampler state) {
 uv=OriginalColorUV(uv,u);
 const float4 value=image.sample(state,uv,bias(u.sampling.x));
#endif
 return OriginalColorChannels(value,u);
}
#endif
#endif
float4 OriginalColorResult(float4 c0,float4 c1,OriginalColorVarying in,constant OriginalColorUniform& u) {
 float4 color=mix(c0,c1,in.blend)*in.color;
 color.rgb=mix(float3(dot(color.rgb,float3(.299f,.587f,.114f))),color.rgb,u.sampling.y);
 if(float(color.a>u.alphaControl.x)*u.alphaControl.y+float(color.a<u.alphaControl.x)*u.alphaControl.z+u.alphaControl.w==0.f)discard_fragment();
 if(u.options.y)discard_fragment();
 return float4(float3(1)-color.aaa+color.rgb*color.aaa,0);
}
fragment float4 OriginalColorFragmentMain(OriginalColorVarying in [[stage_in]],constant OriginalColorUniform& u [[buffer(1)]]
#if ORIGINAL_IMAGE_KIND != 0
#if ORIGINAL_TEXTURE_ARRAY
 ,texture2d_array<float> image [[texture(0)]]
#elif ORIGINAL_IMAGE_KIND == 2
 ,depth2d<float> image [[texture(1)]]
#else
 ,texture2d<float> image [[texture(0)]]
#endif
 ,sampler state [[sampler(0)]]
#endif
) {
#if ORIGINAL_IMAGE_KIND == 0
 return OriginalColorResult(float4(0,0,0,1),float4(0,0,0,1),in,u);
#elif ORIGINAL_TEXTURE_ARRAY
 return OriginalColorResult(OriginalColorSample(in.uv.xy,in.page,u,image,state),OriginalColorSample(in.uv.zw,in.page,u,image,state),in,u);
#else
 return OriginalColorResult(OriginalColorSample(in.uv.xy,u,image,state),OriginalColorSample(in.uv.zw,u,image,state),in,u);
#endif
}
)MSL";
}
