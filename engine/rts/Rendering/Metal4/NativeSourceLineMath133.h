/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr char SourceLineMath133[]=R"MSL(
#include <metal_stdlib>
using namespace metal;

// Compare symbolic rational bounds before division: rounded reciprocal
// products can turn an exact corner tangency into a covered fragment.
inline bool CommonLineLess133(float4 a,float4 b){
 float3 x=a.xyz*b.w,y=b.xyz*a.w;
 return x.x<y.x||(x.x==y.x&&(x.y<y.y||(x.y==y.y&&x.z<y.z)));
}
inline bool CommonLineDiamond133(float2 a,float2 b,float2 center){
 a-=center;b-=center;
 float2 ra=float2(a.x+a.y,a.x-a.y),d=float2((b.x+b.y)-ra.x,(b.x-b.y)-ra.y);
 float4 enter=float4(0,0,0,1),leave=float4(1,0,0,0);
 for(uint axis=0;axis<2;++axis){
  if(d[axis]==0){if(!(ra[axis]>-.5f&&ra[axis]<=.5f))return false;continue;}
  float4 low=float4(-.5f-ra[axis],1,axis?-1.f:1.f,d[axis]),high=float4(.5f-ra[axis],1,axis?-1.f:1.f,d[axis]);
  if(d[axis]<0){float4 tmp=-low;low=-high;high=tmp;}
  if(CommonLineLess133(enter,low))enter=low;if(CommonLineLess133(high,leave))leave=high;
 }
 return CommonLineLess133(enter,leave)&&CommonLineLess133(leave,float4(1,0,0,1));
}
inline bool CommonLineCoverage133(float2 a,float2 b,float2 center,float width){
 float2 origin=floor(center);a-=origin;b-=origin;center-=origin;
 float2 d=b-a;bool xm=abs(d.x)>=abs(d.y);if(all(d==0))return false;
 float major=xm?d.x:d.y,along=((xm?center.x:center.y)-(xm?a.x:a.y))/major;
 float minorAt=(xm?a.y:a.x)+along*(xm?d.y:d.x);
 uint w=max(1u,uint(floor(width+.5f)));float shift=(float(w)-1.f)*.5f;
 float f=(xm?center.y:center.x)+shift-minorAt;
 int low=int(floor(f)),high=int(ceil(f));float2 minor=xm?float2(0,1):float2(1,0);
 if(low>=0&&low<int(w)&&CommonLineDiamond133(a-minor*shift,b-minor*shift,center-minor*float(low)))return true;
 return high!=low&&high>=0&&high<int(w)&&CommonLineDiamond133(a-minor*shift,b-minor*shift,center-minor*float(high));
}

// Common line proposal133: constant work per segment, no pixel stream.
inline bool CommonColumn133(float2 a,float2 b,int major,bool xm){
 float along=(float(major)+.5f-(xm?a.x:a.y))/(xm?b.x-a.x:b.y-a.y);
 int row=int(floor((xm?a.y:a.x)+along*(xm?b.y-a.y:b.x-a.x)));
 for(int offset=-1;offset<=1;++offset){float2 p=xm?float2(float(major)+.5f,float(row+offset)+.5f):float2(float(row+offset)+.5f,float(major)+.5f);if(CommonLineDiamond133(a,b,p))return true;}
 return false;
}
inline int2 CommonUnitRange133(float2 a,float2 b,float width){
 float2 d=b-a;if(all(d==0))return int2(0,0);bool xm=abs(d.x)>=abs(d.y);
 // Integer translation before half-pixel width shift avoids losing a
 // representable fraction when a large negative endpoint crosses a binade.
 float2 origin=floor(a);a-=origin;b-=origin;
 float shift=(max(1.f,floor(width+.5f))-1.f)*.5f;
 if(xm){a.y-=shift;b.y-=shift;}else{a.x-=shift;b.x-=shift;}
 int low=int(floor(min(xm?a.x:a.y,xm?b.x:b.y)))-1,high=int(floor(max(xm?a.x:a.y,xm?b.x:b.y)))+1;
 int first=low+3,last=high-3;
 for(int i=0;i<3;++i){if(CommonColumn133(a,b,low+i,xm)){first=low+i;break;}}
 for(int i=0;i<3;++i){if(CommonColumn133(a,b,high-i,xm)){last=high-i;break;}}
 if(first>last)return int2(0,0);
 return int2(((xm?d.x:d.y)>=0?first:last)+int(xm?origin.x:origin.y),last-first+1);
}
)MSL";
}
