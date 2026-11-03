/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// API diamond coverage;two minor replicas instead of O(width). Numeric proposal
// verified against the accepted128 width union in363840GPU conditions.
inline constexpr char SourceLineRasterShader130[]=R"MSL(
inline bool SourceLineLess(float3 a,float3 b){return a.x<b.x||(a.x==b.x&&(a.y<b.y||(a.y==b.y&&a.z<b.z)));}
inline bool SourceLineDiamond(float2 a,float2 b,float2 center){
 a-=center;b-=center;
 float2 ra=float2(a.x+a.y,a.x-a.y),d=float2((b.x+b.y)-ra.x,(b.x-b.y)-ra.y);
 float3 enter=float3(0),leave=float3(INFINITY,0,0);
 for(uint axis=0;axis<2;++axis){
  if(d[axis]==0){if(!(ra[axis]>-.5f&&ra[axis]<=.5f))return false;continue;}
  float inv=1.f/d[axis];float3 low=float3((-.5f-ra[axis])*inv,inv,axis?-inv:inv),high=float3((.5f-ra[axis])*inv,inv,axis?-inv:inv);
  if(d[axis]<0){float3 tmp=low;low=high;high=tmp;}
  if(SourceLineLess(enter,low))enter=low;if(SourceLineLess(high,leave))leave=high;
 }
 return SourceLineLess(enter,leave)&&SourceLineLess(leave,float3(1,0,0));
}
inline bool SourceLineCoverage130(float2 a,float2 b,float2 center,float width){
 float2 d=b-a;bool xm=abs(d.x)>=abs(d.y);if(all(d==0))return false;
 float major=xm?d.x:d.y,along=((xm?center.x:center.y)-(xm?a.x:a.y))/major;
 float minorAt=(xm?a.y:a.x)+along*(xm?d.y:d.x);
 uint w=max(1u,uint(floor(width+.5f)));float shift=(float(w)-1.f)*.5f;
 float f=(xm?center.y:center.x)+shift-minorAt;
 int low=int(floor(f)),high=int(ceil(f));float2 minor=xm?float2(0,1):float2(1,0);
 if(low>=0&&low<int(w)&&SourceLineDiamond(a-minor*shift,b-minor*shift,center-minor*float(low)))return true;
 return high!=low&&high>=0&&high<int(w)&&SourceLineDiamond(a-minor*shift,b-minor*shift,center-minor*float(high));
}

)MSL";
}
