/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#define RECOIL_WORLD_FONT_MSL R"MSL(
float4 WorldFontTransform(device const packed_float4* columns,float4 p){
 #pragma clang fp contract(off)
 float4 result=float4(0);for(uint k=0;k<4;++k)result+=float4(columns[k])*p[k];return result;
}
float4 WorldFontPosition(device const packed_float4* packet,float4 p){
 #pragma clang fp contract(off)
 float4 clip=WorldFontTransform(packet+4,WorldFontTransform(packet,p));
 if(packet[8].x!=0)clip.z=2.0f*clip.z-clip.w;
 clip.z=(clip.z+clip.w)*.5f;return clip;
}
float4 WorldTextureCoordinate(device const packed_float4* packet,float4 uv){
 #pragma clang fp contract(off)
 const float4 p=float4(uv.xy,0,1);float4 result=uv;
 for(uint row=0;row<2;++row){float value=0;const float4 factors=float4(packet[9+row]);
  for(uint col=0;col<4;++col)value+=factors[col]*p[col];result[row]=value;}
 return result;
}
)MSL"
