/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Source-specific opt-in consumer; gadget stays disabled by default.
// GLSL: BAR 2f0a8402a14a53a506cb438efc206dd829358577 gfx_unit_glass.lua (PD).
// Requires actual GL-compatible texture views/samplers and captured GL matrices.
#define RECOIL_NATIVE_UNIT_GLASS_MSL RECOIL_NATIVE_SAMPLE_SHADING_MSL R"GLASSMSL(
#include <metal_stdlib>
using namespace metal;
struct GlassVertex {
 float4 position;
 float4 normal;
 float4 tangent;
 float4 bitangent;
 float4 uv;
};
struct GlassUniforms {
 float4x4 modelView;
 float4x4 modelViewProjection;
 float4x4 viewInverse;
 float4x4 normalMatrix;
 float4 sunPos;
 float4 sunSpecular;
 float4 teamColor;
 float4 pbrA; // parameters 0..3
 float4 pbrB; // parameters 4..7
 float4 alpha; // reference, comparison, enabled
 uint4 flags; // x: captured projection is already zero-to-one; y: per-texture V flip bits
};
struct GlassOut {
 float4 position [[position]];
 float3 T [[RECOIL_SAMPLE_PERSPECTIVE]];
 float3 B [[RECOIL_SAMPLE_PERSPECTIVE]];
 float3 vertexN [[RECOIL_SAMPLE_PERSPECTIVE]];
 float3 L [[RECOIL_SAMPLE_PERSPECTIVE]];
 float3 viewCameraDir [[RECOIL_SAMPLE_PERSPECTIVE]];
 float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex GlassOut GlassVertexMain(uint id [[vertex_id]],
 device const GlassVertex* vertices [[buffer(0)]], constant GlassUniforms& u [[buffer(1)]]) {
 const GlassVertex v = vertices[id];
 GlassOut o;
 // normalMatrix is the inverse-transpose of the ACTUAL model-view upper 3x3,
 // not a guessed unit rotation. w=0 preserves the GLSL mat3 operations.
 const float3x3 vi(u.viewInverse[0].xyz,u.viewInverse[1].xyz,u.viewInverse[2].xyz);
 const float3x3 nm(u.normalMatrix[0].xyz,u.normalMatrix[1].xyz,u.normalMatrix[2].xyz);
 o.T = vi * (nm * v.tangent.xyz);
 o.B = vi * (nm * v.bitangent.xyz);
 o.vertexN = vi * (nm * v.normal.xyz);
 const float4 worldVertex = u.viewInverse * (u.modelView * v.position);
 const float4 worldCamera = u.viewInverse * float4(0,0,0,1);
 o.viewCameraDir = worldCamera.xyz - worldVertex.xyz;
 o.L = u.sunPos.xyz; // Source does NOT normalize L.
 o.uv = v.uv.xy;
 o.position = u.modelViewProjection * v.position;
 if (!u.flags.x) o.position.z = (o.position.z + o.position.w) * 0.5f;
 return o;
}
float3 GlassSpecular(float HdotN, float roughness, float3 sunSpecular) {
 const float power = 2.0f / max(roughness * 0.25f, 0.01f);
 const float powerNorm = (power + 8.0f) / 32.0f;
 return sunSpecular * pow(HdotN, power) * powerNorm;
}
float3 GlassTonemap(float3 x, constant GlassUniforms& u) {
 // Source does not clamp input/output or guard the rational denominator.
 return (x * (u.pbrA.x * x + u.pbrA.y)) /
        (x * (u.pbrA.z * x + u.pbrA.w) + u.pbrB.x);
}
float2 GlassUV(float2 uv,uint flags,uint slot){if(flags&(1u<<slot))uv.y=1.0f-uv.y;return uv;}
bool GlassAlphaPass(float a,constant GlassUniforms& u){if(!u.alpha.z)return true;float r=u.alpha.x;switch(uint(u.alpha.y)){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;default:return true;}}
fragment float4 GlassFragmentMain(GlassOut in [[stage_in]], bool front [[front_facing]],
 constant GlassUniforms& u [[buffer(1)]],
 texture2d<float> tex1 [[texture(0)]], texture2d<float> tex2 [[texture(1)]],
 texture2d<float> normalTex [[texture(2)]], texturecube<float> reflectTex [[texture(3)]],
 sampler s1 [[sampler(0)]], sampler s2 [[sampler(1)]],
 sampler sn [[sampler(2)]], sampler sr [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT) {
 const float4 c1 = tex1.sample(s1, GlassUV(in.uv,u.flags.y,0));
 const float4 c2 = tex2.sample(s2, GlassUV(in.uv,u.flags.y,1));
 const float3 normal = normalTex.sample(sn, GlassUV(in.uv,u.flags.y,2)).xyz * 2.0f - 1.0f;
 const float3 diffColor = mix(c1.rgb, u.teamColor.rgb, c1.a);
 float3 N = normalize(float3x3(in.T,in.B,in.vertexN) * normal);
 N = front ? N : -N;
 const float metalness = clamp(c2.g, 0.04f, 1.0f);
 const float roughness = clamp(c2.b, 0.04f, 1.0f);
 float roughness4 = roughness * roughness;
 roughness4 *= roughness4;
 const float R0 = ((1.0f - 1.5f) * (1.0f - 1.5f)) / ((1.0f + 1.5f) * (1.0f + 1.5f));
 const float R0v = mix(R0, 1.0f, metalness);
 const float3 V = normalize(in.viewCameraDir);
 const float3 H = normalize(in.L + V);
 float3 Rl = reflect(-V, N);
 Rl = mix(Rl, N, roughness4); // Preserve unnormalized source lookup vector.
 const float NdotV = clamp(dot(N,V),0.0f,1.0f);
 const float HdotN = clamp(dot(H,N),0.0f,1.0f);
 const float fresnel = R0v + (1.0f - R0v) * pow(1.0f - NdotV,5.0f);
 // GLSL texture(..., lodBias), NOT textureLod(...). Keep implicit derivatives.
 const float lodBias = log2(float(reflectTex.get_width(0))) * roughness;
 float3 reflColor = 1.35f * reflectTex.sample(sr,Rl,bias(lodBias)).rgb;
 reflColor *= fresnel;
 reflColor += u.pbrB.z * GlassSpecular(HdotN,roughness,u.sunSpecular.xyz);
 const float3 color = GlassTonemap(u.pbrB.w * (diffColor + reflColor),u);
 const float a=c2.a * 2.0f - 1.0f; if(!GlassAlphaPass(a,u))discard_fragment();
 return float4(color,a); // Preserve negative source alpha.
}
)GLASSMSL"
