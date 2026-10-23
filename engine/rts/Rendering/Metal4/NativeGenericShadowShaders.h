/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Exact original legacy ShadowGen vertex expressions for raw generic input.
// A separate entry preserves gl_Vertex.w and current gl_Normal; model-piece
// geometry cannot substitute for an immediate/list vertex stream.
inline constexpr const char* NativeGenericShadowShaders=R"MSL(
struct GenericShadowVertex {float4 position,normal,color,uv;};
struct GenericShadowUniform {float4x4 modelView,projection;float4 clips[3];uint4 flags;};
struct GenericShadowVarying {float4 position [[position]];float clipDistance [[clip_distance]] [3];};
vertex GenericShadowVarying OriginalGenericShadowVertex(uint id [[vertex_id]],device const GenericShadowVertex* vertices [[buffer(0)]],constant GenericShadowUniform& u [[buffer(1)]]) {
 const auto v=vertices[id];GenericShadowVarying o;
 float4 light=u.modelView*v.position;const float3 normal=normalize((u.modelView*float4(v.normal.xyz,0)).xyz);
 const float ndotl=clamp(normal.z,0.f,1.f);const float bias=clamp(1e-6f*tan(acos(ndotl)),0.f,100e-6f);
 light.xy+=.5f;light.z+=bias;o.position=u.projection*light;
 for(uint n=0;n<3;++n)o.clipDistance[n]=(u.flags.y&(1u<<n))?dot(v.position,u.clips[n]):1.f;
 if(u.flags.x&1u)o.position.z=(o.position.z+o.position.w)*.5f;if(u.flags.x&2u)o.position.y=-o.position.y;return o;
}
// This is the selected original legacy ShadowGen fragment's empty body.
// The generic consumer accepts only disabled fixed alpha and no color writes;
// the separate built-in model alpha acceptance is not extended here.
fragment void OriginalGenericShadowFragment() {}
)MSL";
}
