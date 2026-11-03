/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr const char* NativeScreenGeometryShaderSource=R"MSL(
#pragma clang fp contract(off)
struct ScreenGeometryUniform {
 float4 modelView[4]; float4 projection[4]; float4 cofactors[3];
 float4 normal; uint4 options;
};
float4 ScreenGeometryTransform(constant float4* m,float4 p){
 float4 result=float4(0);for(uint k=0;k<4;++k)result+=m[k]*p[k];return result;
}
ScreenVarying ScreenGeometryEvaluate(uint vertexId,uint instanceId,device const float* vertices,
 device const float* instances,constant ScreenUniform& u,constant ScreenGeometryUniform& g){
 ScreenVarying out;out.uv=float2(0);out.uv1=float2(0);
 if(g.options.x==0){
  const uint v=vertexId*4,i=instanceId*8;
  const float4 pos=float4(vertices[v],vertices[v+1],vertices[v+2],vertices[v+3])+float4(instances[i],instances[i+1],instances[i+2],instances[i+3]);
  out.position=float4(pos.xy,(pos.z+pos.w)*.5f,pos.w);
  out.color=float4(instances[i+4],instances[i+5],instances[i+6],instances[i+7]);out.uv.x=pos.w;
 }else if(g.options.x==1){
  const uint v=vertexId*4,i=instanceId*4;
  const float4 t=float4(instances[i],instances[i+1],instances[i+2],instances[i+3]);const bool sim=abs(t.z-1.0f)<.01f;
  const float gap=min(1.0f,t.y*.35f);
  const float width=max(t.y-gap,sim?4.0f:1.0f)/u.dimensions.x;
  const float height=max(u.dimensions1.y*t.y,sim?10.0f:1.0f)/u.dimensions.y;
  const float right=1.0f-(u.dimensions1.x*1.0f-t.x)/u.dimensions.x;
  out.position=float4(right-vertices[v]*width,-1.0f+vertices[v+1]*height,(.5f+.1f*t.z+t.w*.1f+1.0f)*.5f,1);
  if(right<0.0f||t.y>200.0f)out.position.xy=float2(-1.0f);out.color=t;
 }else if(g.options.x==6){
  const uint v=vertexId*12;
  // Match AppendPhysicalPixels then ScreenVertexMain, in binary32 order.
  const float px=vertices[v]*g.normal.y/g.projection[0].x,py=vertices[v+1]*g.normal.z/g.projection[1].y;
  out.position=float4(2.0f*px*u.dimensions.x-1.0f,2.0f*py*u.dimensions.y-1.0f,(vertices[v+2]+1.0f)*.5f,1);
  out.color=float4(vertices[v+4],vertices[v+5],vertices[v+6],vertices[v+7]);
 }else if(g.options.x==5){
  const uint v=vertexId*12;
  // Match AppendScreenRaster's binary32 clip -> pixel conversion followed by
  // ScreenVertexMain. The generated line already has the exact120 coverage.
  const float px=(vertices[v]+1.0f)*g.normal.y*.5f,py=(vertices[v+1]+1.0f)*g.normal.z*.5f;
  out.position=float4(2.0f*px*u.dimensions.x-1.0f,2.0f*py*u.dimensions.y-1.0f,(vertices[v+2]+1.0f)*.5f,1);
  out.color=float4(vertices[v+4],vertices[v+5],vertices[v+6],vertices[v+7]);
  out.uv=float2(vertices[v+8],vertices[v+9]);out.uv1=float2(vertices[v+10],vertices[v+11]);
 }else if(g.options.x==3||g.options.x==4){
  const uint v=vertexId*7,t=vertexId*4;
  const float4 clip=ScreenGeometryTransform(g.projection,ScreenGeometryTransform(g.modelView,float4(vertices[v],vertices[v+1],vertices[v+2],1)));
  // Match the existing Project -> ScreenVertexMain pixel conversion, including
  // its rounding and GL [-1,1] depth. Clipping stays on the native GPU.
  const float2 pixel=(clip.xy+1.0f)*g.normal.yz*.5f;
  out.position=float4(2.0f*pixel*u.dimensions.xy-1.0f,(clip.z+1.0f)*.5f,1);
  out.color=float4(vertices[v+3],vertices[v+4],vertices[v+5],vertices[v+6]);
  out.uv=float2(instances[t],instances[t+1]);out.uv1=float2(instances[t+2],instances[t+3]);
  if(g.options.y){
   // Same ordered sums as ProjectUV; q=1 was proven at packet creation.
   // Normal cofactor lanes are unused by ScreenUI and carry the two UV rows.
   const float4 uv=float4(out.uv,0,1);float2 transformed=float2(0);
   for(uint col=0;col<4;++col)transformed+=float2(g.cofactors[0][col],g.cofactors[1][col])*uv[col];
   out.uv=transformed;
  }
 }else{
  const uint v=vertexId*19;
  const float4 pos=float4(vertices[v],vertices[v+1],vertices[v+2],1);
  const float3 n=float3(vertices[v+3],vertices[v+4],vertices[v+5]);
  const float4 clip=ScreenGeometryTransform(g.projection,ScreenGeometryTransform(g.modelView,pos));
  out.position=float4(clip.xy,(clip.z+clip.w)*.5f,clip.w);
  out.color=float4((g.cofactors[0].xyz*n.x+g.cofactors[1].xyz*n.y+g.cofactors[2].xyz*n.z)/g.normal.x,1);
  out.uv=float2(vertices[v+12],vertices[v+13]);out.uv1=float2(clip.w,0);
 }
 return out;
}
vertex ScreenVarying ScreenGeometryVertexMain(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 device const float* vertices [[buffer(0)]],device const float* instances [[buffer(1)]],
 constant ScreenUniform& u [[buffer(2)]],constant ScreenGeometryUniform& g [[buffer(4)]]){
 return ScreenGeometryEvaluate(vertexId,instanceId,vertices,instances,u,g);
}
)MSL";
}
