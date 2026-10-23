/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4::unitStencil {
inline constexpr const char* SHADER=R"UNITSTENCIL(
#include <metal_stdlib>
using namespace metal;
struct StencilInstance {float4 mins;float4 maxs;uint4 instData;};
struct StencilMatrices {float4x4 values[26];};
struct StencilObject {uint4 composite;float4 health;float4 drawPos;float4 speed;float4 userDefined[4];};
struct StencilUniforms {float addRadius;float stencilColor;uint firstRecord;uint clipZeroToOne;float4 alpha;};
struct StencilVertexOut {float4 position [[position]];};
vertex StencilVertexOut UnitStencilVertex(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],
 device const StencilInstance* records [[buffer(0)]],constant StencilMatrices& matrices [[buffer(1)]],
 device const StencilObject* objects [[buffer(2)]],constant StencilUniforms& uniforms [[buffer(3)]])
{
 const StencilInstance instance=records[uniforms.firstRecord+instanceID];
 const StencilObject object=objects[instance.instData.y];
 float4 center=object.drawPos;float4 mins=instance.mins,maxs=instance.maxs;
 float3 toCamera=center.xyz-matrices.values[7][3].xyz;
 float radius=uniforms.addRadius+maxs.x+maxs.z;
 float ratio=radius*rsqrt(dot(toCamera,toCamera));
 float4 clipCenter=matrices.values[5]*float4(center.xyz,1.0f);
 bool outside=any(abs(clipCenter.xy)>clipCenter.ww*(1.0f+ratio));
 StencilVertexOut result;
 if(outside||((object.composite.x&0x3u)<1u)){result.position=float4(2,2,2,1);return result;}
 float3 cameraDirection=normalize(matrices.values[7][3].xyz-center.xyz);
 float s=sin(center.w),c=cos(center.w);
 float3x3 rotation=float3x3(float3(c,0,-s),float3(0,1,0),float3(s,0,c));
 // Same primitive order and winding as each original4-vertex GS strip.
 constexpr uint stripTriangles[6]={0,1,2,2,1,3};
 uint face=vertexID/6u,corner=stripTriangles[vertexID%6u];float3 p;
 if(face==0u){p=float3((corner&1u)?maxs.x:mins.x,maxs.y,corner<2u?mins.z:maxs.z);}
 else if(face==1u){float side=dot(float3(c,0,-s),cameraDirection)<0?mins.x:maxs.x;
  p=float3(side,corner<2u?maxs.y:mins.y,(corner&1u)?maxs.z:mins.z);}
 else{float side=dot(float3(s,0,c),cameraDirection)>0?maxs.z:mins.z;
  p=float3((corner&1u)?maxs.x:mins.x,corner<2u?maxs.y:mins.y,side);}
 float4 position=matrices.values[5]*float4(center.xyz+rotation*(float3(uniforms.addRadius,0,uniforms.addRadius)*sign(p)+p),1.0f);
 if(!uniforms.clipZeroToOne)position.z=(position.z+position.w)*0.5f;
 result.position=position;return result;
}
bool StencilCompare(float value,float reference,uint comparison){
 switch(comparison){case 0:return false;case 1:return value<reference;case 2:return value==reference;case 3:return value<=reference;case 4:return value>reference;case 5:return value!=reference;case 6:return value>=reference;default:return true;}
}
fragment float4 UnitStencilFragment(constant StencilUniforms& uniforms [[buffer(3)]])
{
 // Original gl_FragColor alpha1 passes the actual inherited alpha test.
 if(uniforms.alpha.x!=0&&!StencilCompare(1.0f,uniforms.alpha.y,uint(uniforms.alpha.z)))discard_fragment();
 return float4(uniforms.stencilColor,uniforms.stencilColor,uniforms.stencilColor,1);
}
struct ClearOut {float4 position [[position]];};
vertex ClearOut UnitStencilClearVertex(uint vertexID [[vertex_id]]){
 constexpr float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};ClearOut v;v.position=float4(p[vertexID],0,1);return v;
}
fragment float4 UnitStencilClearFragment(constant float4& color [[buffer(3)]]){return color;}
)UNITSTENCIL";
}
