/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr char ReclaimSourceLinesShader131[]=R"MSL(
struct ReclaimSourceRaster131 {float4 viewport,target;uint4 options;};
struct ReclaimSourceSegment131 {float4 a,b,colorA,colorB;bool valid;};
inline bool ReclaimSourceClip131(float a,float b,thread float& low,thread float& high){
 if(a<0&&b<0)return false;if(a<0||b<0){float t=a/(a-b);if(a<0)low=max(low,t);else high=min(high,t);}return low<=high;
}
inline ReclaimSourceSegment131 ReclaimSourceLine131(uint segment,device const float4* vertices,device const float4* params,constant Uniform& u,constant ReclaimSourceRaster131& raster){
 ReclaimSourceSegment131 s={};auto va=OriginalReclaimVertex131(2*segment,vertices,params,u),vb=OriginalReclaimVertex131(2*segment+1,vertices,params,u);float4 a=va.position,b=vb.position;
 if(!all(isfinite(a))||!all(isfinite(b)))return s;
 // Original vertex helper emits Metal0..w depth. Canonical clipping here uses
 // -w..w;depthClamp skips z clipping and keeps original x/y/w behavior.
 a.z=2*a.z-a.w;b.z=2*b.z-b.w;float low=0,high=1;
 for(uint axis=0;axis<(raster.options.y?2u:3u);++axis)if(!ReclaimSourceClip131(a.w+a[axis],b.w+b[axis],low,high)||!ReclaimSourceClip131(a.w-a[axis],b.w-b[axis],low,high))return s;
 s.a=mix(a,b,low);s.b=mix(a,b,high);s.colorA=mix(va.color,vb.color,low);s.colorB=mix(va.color,vb.color,high);s.valid=s.a.w>0&&s.b.w>0&&all(isfinite(s.a))&&all(isfinite(s.b));return s;
}
struct ReclaimSourceOut131 {float4 position [[position]];float4 endpoints [[flat]],colorA [[flat]],colorB [[flat]];float2 inverseW [[flat]];};
inline ReclaimSourceOut131 ReclaimSourceCorner131(ReclaimSourceSegment131 line,constant ReclaimSourceRaster131& r,uint corner){
 ReclaimSourceOut131 o={};o.position=float4(2,2,2,1);if(!line.valid)return o;
 float2 an=line.a.xy/line.a.w,bn=line.b.xy/line.b.w;
 float2 a=r.viewport.xy+(float2(an.x,-an.y)+1.f)*.5f*r.viewport.zw,b=r.viewport.xy+(float2(bn.x,-bn.y)+1.f)*.5f*r.viewport.zw,d=b-a;
 float lengthD=length(d);if(!(lengthD>0))return o;float2 axis=d/lengthD,normal=float2(-axis.y,axis.x);bool end=(corner&2)!=0,positive=(corner&1)!=0;
 float radius=r.options.x>1?r.target.z*.5f:floor(r.target.z+.5f)*.5f+1.5f,extension=r.options.x>1?0.f:1.f;
 float2 p=(end?b:a)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 // Admitted original outline states bypass depth or use ALWAYS/no-write.
 // The original fragment consumes color only. Other depth states use fallback.
 o.position=float4(2*p.x/r.target.x-1,1-2*p.y/r.target.y,.5f,1);o.endpoints=float4(a.x,r.target.y-a.y,b.x,r.target.y-b.y);o.inverseW=float2(1/line.a.w,1/line.b.w);o.colorA=line.colorA*o.inverseW.x;o.colorB=line.colorB*o.inverseW.y;return o;
}
vertex ReclaimSourceOut131 ReclaimSourceVertex131(uint corner [[vertex_id]],uint segment [[instance_id]],device const float4* vertices [[buffer(0)]],device const float4* params [[buffer(1)]],constant Uniform& u [[buffer(2)]],constant ReclaimSourceRaster131& raster [[buffer(3)]]){
 return ReclaimSourceCorner131(ReclaimSourceLine131(segment,vertices,params,u,raster),raster,corner);
}
inline Out ReclaimSourceMSAACorner131(ReclaimSourceSegment131 line,constant ReclaimSourceRaster131& raster,uint corner){
 Out out={};out.position=float4(2,2,2,1);if(!line.valid)return out;auto source=ReclaimSourceCorner131(line,raster,corner);auto endpoint=(corner&2)?line.b:line.a;out.position=source.position*endpoint.w;out.color=(corner&2)?line.colorB:line.colorA;return out;
}
vertex Out ReclaimSourceVertexMSAA131(uint corner [[vertex_id]],uint segment [[instance_id]],device const float4* vertices [[buffer(0)]],device const float4* params [[buffer(1)]],constant Uniform& u [[buffer(2)]],constant ReclaimSourceRaster131& raster [[buffer(3)]]){return ReclaimSourceMSAACorner131(ReclaimSourceLine131(segment,vertices,params,u,raster),raster,corner);}

inline bool ReclaimSourceAlpha131(float a,constant Uniform& u){
 if(!u.options.z)return true;float b=u.alpha.x;switch(u.options.w){case 0:return false;case 1:return a<b;case 2:return a==b;case 3:return a<=b;case 4:return a>b;case 5:return a!=b;case 6:return a>=b;default:return true;}
}
inline float4 ReclaimSourceColor131(ReclaimSourceOut131 in,float2 p){float2 d=in.endpoints.zw-in.endpoints.xy;float t=dot(p-in.endpoints.xy,d)/dot(d,d);return mix(in.colorA,in.colorB,t)/mix(in.inverseW.x,in.inverseW.y,t);}
fragment float4 ReclaimSourceFragment131(ReclaimSourceOut131 in [[stage_in]],constant Uniform& u [[buffer(2)]],constant ReclaimSourceRaster131& raster [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));float2 p=float2(in.position.x,raster.target.y-in.position.y);if(!SourceLineCoverage130(in.endpoints.xy,in.endpoints.zw,p,raster.target.z))discard_fragment();float4 color=ReclaimSourceColor131(in,p);if(!ReclaimSourceAlpha131(color.a,u))discard_fragment();return color;
}
// Existing CPU mesh fallback for unsupported original render states. No source
// endpoint evaluation occurs here;the old canonical mesh only converts depth.
struct ReclaimRasterVertexData131 {packed_float4 clip,color,uv;};
vertex Out ReclaimRasterVertex131(uint id [[vertex_id]],device const ReclaimRasterVertexData131* vertices [[buffer(0)]]){
 auto v=vertices[id];Out out;out.position=float4(v.clip);out.position.z=(out.position.z+out.position.w)*.5f;out.color=float4(v.color);return out;
}
)MSL";
}
