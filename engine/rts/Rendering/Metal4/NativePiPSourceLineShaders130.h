/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr char PiPSourceLinesShader130[]=R"MSL(
struct PiPSourceRaster130 {float4 viewport,target;uint4 options;};
struct PiPSourceSegment130 {PipLineStage a,b;bool valid;};
inline bool PiPSourceClip130(float da,float db,thread float& lo,thread float& hi){
 if(da<0&&db<0)return false;if(da<0||db<0){float t=da/(da-db);if(da<0)lo=max(lo,t);else hi=min(hi,t);}return lo<=hi;
}
inline PiPSourceSegment130 PiPSourceLine130(uint segment,device const float* vertices,constant PipUniformBlock& uniforms,constant PiPSourceRaster130& raster){
 PiPSourceSegment130 out={};auto a=PipOriginalLineVertex130(raster.options.y+2*segment,vertices,uniforms),b=PipOriginalLineVertex130(raster.options.y+2*segment+1,vertices,uniforms);
 if(!all(isfinite(a.position))||!all(isfinite(b.position)))return out;
 // The original PiP shader emits z=.5,w=1 in Metal. Only x/y can clip;
 // depthClamp cannot change that original constant in-range depth.
 float lo=0,hi=1;
 for(uint i=0;i<2;++i)if(!PiPSourceClip130(a.position.w+a.position[i],b.position.w+b.position[i],lo,hi)||!PiPSourceClip130(a.position.w-a.position[i],b.position.w-b.position[i],lo,hi))return out;
 out.a.position=mix(a.position,b.position,lo);out.b.position=mix(a.position,b.position,hi);out.a.f_color=mix(a.f_color,b.f_color,lo);out.b.f_color=mix(a.f_color,b.f_color,hi);
 out.valid=out.a.position.w>0&&out.b.position.w>0&&all(isfinite(out.a.position))&&all(isfinite(out.b.position));return out;
}
struct PiPSourceOut130 {float4 position [[position]];float4 endpoints [[flat]];float4 colorA [[flat]],colorB [[flat]];};
inline PiPSourceOut130 PiPSourceCorner130(PiPSourceSegment130 line,constant PiPSourceRaster130& u,uint corner){
 PiPSourceOut130 out={};out.position=float4(2,2,2,1);if(!line.valid)return out;
 float2 an=line.a.position.xy/line.a.position.w,bn=line.b.position.xy/line.b.position.w;
 float2 a=u.viewport.xy+(float2(an.x,-an.y)+1.f)*.5f*u.viewport.zw,b=u.viewport.xy+(float2(bn.x,-bn.y)+1.f)*.5f*u.viewport.zw;
 float2 d=b-a;float len=length(d);if(!(len>0))return out;float2 axis=d/len,normal=float2(-axis.y,axis.x);
 bool end=(corner&2)!=0,positive=(corner&1)!=0;
 float radius=u.options.x>1?u.target.z*.5f:floor(u.target.z+.5f)*.5f+1.5f,extension=u.options.x>1?0.f:1.f;
 float2 p=(end?b:a)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 out.position=float4(2*p.x/u.target.x-1,1-2*p.y/u.target.y,.5f,1);out.endpoints=float4(a.x,u.target.y-a.y,b.x,u.target.y-b.y);out.colorA=line.a.f_color;out.colorB=line.b.f_color;return out;
}
vertex PiPSourceOut130 PiPSourceVertex130(uint corner [[vertex_id]],uint segment [[instance_id]],device const float* vertices [[buffer(1)]],constant PipUniformBlock& uniforms [[buffer(3)]],constant PiPSourceRaster130& raster [[buffer(4)]]){
 return PiPSourceCorner130(PiPSourceLine130(segment,vertices,uniforms,raster),raster,corner);
}
vertex PipLineStage PiPSourceVertexMSAA130(uint corner [[vertex_id]],uint segment [[instance_id]],device const float* vertices [[buffer(1)]],constant PipUniformBlock& uniforms [[buffer(3)]],constant PiPSourceRaster130& raster [[buffer(4)]]){
 auto line=PiPSourceLine130(segment,vertices,uniforms,raster);auto source=PiPSourceCorner130(line,raster,corner);PipLineStage out;out.position=source.position;out.f_color=(corner&2)?line.b.f_color:line.a.f_color;return out;
}
inline float4 PiPSourceColorAt130(float4 endpoints,float4 a,float4 b,float2 p){float2 d=endpoints.zw-endpoints.xy;float t=dot(p-endpoints.xy,d)/dot(d,d);return mix(a,b,t);}
fragment float4 PiPSourceFragment130(PiPSourceOut130 in [[stage_in]],constant PiPSourceRaster130& raster [[buffer(4)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 p=float2(in.position.x,raster.target.y-in.position.y),a=in.endpoints.xy,b=in.endpoints.zw;
 if(!SourceLineCoverage130(a,b,p,raster.target.z))discard_fragment();float2 d=b-a;
 // Original w is1 at both ends. The API projected-dot parameter gives the
 // original smooth color;the old CPU major-axis interpolation is not copied.
 return PiPSourceColorAt130(in.endpoints,in.colorA,in.colorB,p);
}
)MSL";
}
