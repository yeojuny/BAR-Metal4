/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr char ExactPiPLineShader[]=R"MSL(
#pragma clang fp contract(off)
struct PiPLineUniform123 {float4x4 matrix;float4 values[32];uint4 flipV;};
inline float4 pipLineColor123(thread const float* raw,constant PiPLineUniform123& u){
 float alpha=raw[3];
 if(u.values[6].x>.5f){
  float duration=min(1.f,u.values[6].z);
  float remaining=u.values[6].z-(u.values[6].y-raw[3]);
  alpha=.85f*clamp(remaining/duration,0.f,1.f);
 }
 return float4((D120::f(raw[0])*D120::f(u.values[5].x)).f(),(D120::f(raw[1])*D120::f(u.values[5].y)).f(),(D120::f(raw[2])*D120::f(u.values[5].z)).f(),(D120::f(alpha)*D120::f(u.values[5].w)).f());
}
// One invocation per original segment, not per expanded pixel.
kernel void PiPLineColors123(device const Line120Segment* input [[buffer(0)]],device Line120Segment* output [[buffer(1)]],constant PiPLineUniform123& uniforms [[buffer(2)]],uint id [[thread_position_in_grid]]){
 auto s=input[id];auto a=pipLineColor123(s.a.uv,uniforms),b=pipLineColor123(s.b.uv,uniforms);
 for(uint c=0;c<4;++c){s.a.color[c]=mix120(a[c],b[c],D120::bits(s.viewport[0]));s.b.color[c]=mix120(a[c],b[c],D120::bits(s.viewport[1]));s.a.uv[c]=0;s.b.uv[c]=0;}
 output[id]=s;
}
inline Line120Vertex pipLineClip123(Line120Vertex v,uint width,uint height){
 v.position[0]=2.f*v.position[0]/float(width)-1.f;
 v.position[1]=2.f*v.position[1]/float(height)-1.f;
 v.position[2]=.5f;v.position[3]=1.f;
 return v;
}
kernel void PiPLineGeometry123(device const Line120Segment* segments [[buffer(0)]],device const Line120Span* spans [[buffer(1)]],device Line120Vertex* vertices [[buffer(2)]],constant uint& spanCount [[buffer(3)]],device uint* indices [[buffer(4)]],uint id [[thread_position_in_grid]]){
 uint lo=0,hi=spanCount;while(lo+1<hi){uint mid=(lo+hi)/2;if(spans[mid].first<=id)lo=mid;else hi=mid;}
 auto span=spans[lo];auto s=segments[span.segment];auto q=pixelQuad122(s,span,id-span.first);
 for(uint i=0;i<4;++i)vertices[id*4+i]=pipLineClip123(q.v[i],s.width,s.height);
 const uint corners[6]={0,1,2,0,2,3};for(uint i=0;i<6;++i)indices[id*6+i]=id*4+corners[i];
}
)MSL";
} // namespace rendering::metal4
