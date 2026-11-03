/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSourceLineMath133.h"
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr char SourceLineStages133[]=R"MSL(
struct CommonVertex133 {packed_float4 position,color,uv;};
struct CommonTransform133 {float4x4 modelView,projection,texture;float4 clipPlane;uint4 flags;};
struct CommonSegment133 {uint a,b,transform,head;float width;uint pattern,factor,stipple;};
struct CommonRaster133 {float4 viewport,target;uint4 options;}; // samples,scan,count
struct CommonPrefix133 {ulong sum;uint head,pad;};
struct CommonClipped133 {float4 a,b,colorA,colorB,uvA,uvB;float2 pixelA,pixelB;bool valid;};
inline bool CommonClipInterval133(float a,float b,thread float& low,thread float& high){
 if(a<0&&b<0)return false;
 if(a<0||b<0){float t=a/(a-b);if(a<0)low=max(low,t);else high=min(high,t);}
 return low<=high;
}
inline CommonClipped133 CommonClip133(uint index,device const CommonVertex133* vertices,
 device const CommonSegment133* segments,device const CommonTransform133* transforms,constant CommonRaster133& raster){
 CommonClipped133 o={};const auto s=segments[index];const auto t=transforms[s.transform];
 const auto va=vertices[s.a],vb=vertices[s.b];
 float4 a=t.projection*(t.modelView*float4(va.position)),b=t.projection*(t.modelView*float4(vb.position));
 if(t.flags.x){a.z=2*a.z-a.w;b.z=2*b.z-b.w;}
 if(!all(isfinite(a))||!all(isfinite(b)))return o;
 float low=0,high=1;
 if(t.flags.z&&!CommonClipInterval133(dot(t.clipPlane,a),dot(t.clipPlane,b),low,high))return o;
 for(uint axis=0;axis<(t.flags.y?2u:3u);++axis)
  if(!CommonClipInterval133(a.w+a[axis],b.w+b[axis],low,high)||!CommonClipInterval133(a.w-a[axis],b.w-b[axis],low,high))return o;
 o.a=mix(a,b,low);o.b=mix(a,b,high);
 if(!(o.a.w>0&&o.b.w>0)||!all(isfinite(o.a))||!all(isfinite(o.b)))return o;
 o.colorA=mix(float4(va.color),float4(vb.color),low);o.colorB=mix(float4(va.color),float4(vb.color),high);
 float4 ua=float4(va.uv),ub=float4(vb.uv);
 if(t.flags.w){ua.xy=(t.texture*float4(ua.xy,0,1)).xy;ub.xy=(t.texture*float4(ub.xy,0,1)).xy;}
 o.uvA=mix(ua,ub,low);o.uvB=mix(ua,ub,high);
 const float2 bottom=float2(raster.viewport.x,raster.target.y-raster.viewport.y-raster.viewport.w);
 o.pixelA=bottom+(o.a.xy/o.a.w+1.f)*.5f*raster.viewport.zw;
 o.pixelB=bottom+(o.b.xy/o.b.w+1.f)*.5f*raster.viewport.zw;
 o.valid=all(isfinite(o.pixelA))&&all(isfinite(o.pixelB))&&any(o.pixelA!=o.pixelB);
 return o;
}
inline int2 CommonRange133(CommonClipped133 line,float width,uint samples){
 if(!line.valid)return int2(0);
 if(samples>1)return int2(0,int(ceil(length(line.pixelB-line.pixelA))));
 return CommonUnitRange133(line.pixelA,line.pixelB,width);
}
kernel void CommonCount133(uint id [[thread_position_in_grid]],device const CommonVertex133* vertices [[buffer(0)]],
 device const CommonSegment133* segments [[buffer(1)]],device const CommonTransform133* transforms [[buffer(2)]],
 constant CommonRaster133& raster [[buffer(3)]],device CommonPrefix133* output [[buffer(4)]]){
 if(id>=raster.options.z)return;
 const auto s=segments[id];const auto line=CommonClip133(id,vertices,segments,transforms,raster);
 output[id]={ulong(CommonRange133(line,s.width,raster.options.x).y),s.head,0};
}
inline CommonPrefix133 CommonCombine133(CommonPrefix133 a,CommonPrefix133 b){return {b.head?b.sum:a.sum+b.sum,a.head|b.head,0};}
// Associative segmented prefix. Local scans and recursively scanned block
// totals are O(N); no per-pixel records and no CPU result readback.
kernel void CommonScan133(uint local [[thread_index_in_threadgroup]],uint group [[threadgroup_position_in_grid]],
 device const CommonPrefix133* input [[buffer(0)]],device CommonPrefix133* output [[buffer(1)]],
 device CommonPrefix133* blocks [[buffer(2)]],constant uint& count [[buffer(3)]]){
 threadgroup CommonPrefix133 a[256],b[256];uint id=group*256+local;
 a[local]=id<count?input[id]:CommonPrefix133{0,0,0};threadgroup_barrier(mem_flags::mem_threadgroup);
 for(uint stride=1;stride<256;stride*=2){b[local]=local>=stride?CommonCombine133(a[local-stride],a[local]):a[local];threadgroup_barrier(mem_flags::mem_threadgroup);a[local]=b[local];threadgroup_barrier(mem_flags::mem_threadgroup);}
 if(id<count)output[id]=a[local];if(local==255)blocks[group]=a[255];
}
kernel void CommonCarry133(uint id [[thread_position_in_grid]],device CommonPrefix133* values [[buffer(0)]],
 device const CommonPrefix133* blocks [[buffer(1)]],constant uint& count [[buffer(2)]]){
 if(id>=count||id<256)return;auto value=values[id];
 if(!value.head)value.sum+=blocks[id/256-1].sum;
 values[id]=value;
}
struct CommonOut133 {
 float4 position [[position]];
 float4 endpoints [[flat]],colorA [[flat]],colorB [[flat]],uvA [[flat]],uvB [[flat]];
 float4 depthW [[flat]],raster [[flat]];uint4 stipple [[flat]];
};
inline CommonOut133 CommonCorner133(CommonClipped133 line,CommonSegment133 s,constant CommonRaster133& u,uint corner,ulong inclusive){
 CommonOut133 out={};out.position=float4(2,2,2,1);if(!line.valid)return out;
 float2 delta=line.pixelB-line.pixelA;float len=length(delta);if(!(len>0))return out;
 float2 axis=delta/len,normal=float2(-axis.y,axis.x);bool end=corner&2,positive=corner&1;
 float radius=u.options.x>1?s.width*.5f:max(1.f,floor(s.width+.5f))*.5f+1.5f;
 float extension=u.options.x>1?0.f:1.f;
 float2 p=(end?line.pixelB:line.pixelA)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 // A screen-linear depth plane retains per-sample hardware depth even when
 // color shading is per-pixel. The caller uses depthClipMode=Clamp: original
 // z clipping was already applied to the endpoints, while padded corners
 // must not clip away valid diamond coverage. GL depth-clamp inputs keep
 // their unclamped plane until the hardware evaluates each actual sample.
 float z0=(line.a.z/line.a.w+1)*.5f,z1=(line.b.z/line.b.w+1)*.5f;
 float along=end?1.f+extension/len:-extension/len;
 out.position=float4(2*p.x/u.target.x-1,2*p.y/u.target.y-1,mix(z0,z1,along),1);
 out.endpoints=float4(line.pixelA,line.pixelB);float2 iw=1.f/float2(line.a.w,line.b.w);
 out.colorA=line.colorA*iw.x;out.colorB=line.colorB*iw.y;out.uvA=line.uvA*iw.x;out.uvB=line.uvB*iw.y;
 out.depthW=float4(iw,(line.a.z*iw.x+1)*.5f,(line.b.z*iw.y+1)*.5f);
 int2 range=s.stipple?CommonRange133(line,s.width,u.options.x):int2(0);
 const ulong phase=u.options.y?inclusive-ulong(range.y):0;
 out.stipple=uint4(uint(range.x),uint(phase%ulong(16*s.factor)),s.pattern,s.factor);
 out.raster=float4(s.width,s.stipple,abs(delta.x)>=abs(delta.y),(abs(delta.x)>=abs(delta.y)?delta.x:delta.y)>=0?1.f:-1.f);
 return out;
}
vertex CommonOut133 CommonVertexMain133(uint corner [[vertex_id]],uint segment [[instance_id]],
 device const CommonVertex133* vertices [[buffer(0)]],device const CommonSegment133* segments [[buffer(1)]],
 device const CommonTransform133* transforms [[buffer(2)]],constant CommonRaster133& raster [[buffer(3)]],
 device const CommonPrefix133* prefix [[buffer(4)]]){
 return CommonCorner133(CommonClip133(segment,vertices,segments,transforms,raster),segments[segment],raster,corner,raster.options.y?prefix[segment].sum:0);
}
inline bool CommonStipple133(CommonOut133 in,float2 p,uint samples){
 if(in.raster.y==0)return true;float2 a=in.endpoints.xy,d=in.endpoints.zw-a;int offset;
 if(samples>1)offset=max(0,int(floor(dot(p-a,d)/length(d))));
 else offset=max(0,int(in.raster.w)*(int(floor(in.raster.z?p.x:p.y))-int(in.stipple.x)));
 uint bit=((in.stipple.y+uint(offset))/in.stipple.w)&15;
 return (in.stipple.z&(1u<<bit))!=0;
}
struct CommonValue133 {float4 color,uv;float depth;};
inline CommonValue133 CommonInterpolate133(CommonOut133 in,float2 p){
 float2 d=in.endpoints.zw-in.endpoints.xy;float t=dot(p-in.endpoints.xy,d)/dot(d,d);
 float inverseW=mix(in.depthW.x,in.depthW.y,t);
 CommonValue133 value;value.color=mix(in.colorA,in.colorB,t)/inverseW;value.uv=mix(in.uvA,in.uvB,t)/inverseW;
 value.depth=clamp(mix(in.depthW.z,in.depthW.w,t),0.f,1.f);return value;
}
)MSL";
inline constexpr char SourceLineFragment133[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
struct CommonSurface133 {float4 scale;uint4 options;float4 parameters;float4 scale1;uint4 options1;float4 parameters1;};
struct CommonResult133 {float4 color [[color(0)]];uint mask [[sample_mask]];
#if COMMON_MRT133
 float4 c1 [[color(1)]],c2 [[color(2)]],c3 [[color(3)]],c4 [[color(4)]];
#if COMMON_COMBINED133
 float4 c5 [[color(5)]];
#endif
#endif
};
#if COMMON_DEPTH0_133
#define COMMON_IMAGE0_133 depth2d<float>
#else
#define COMMON_IMAGE0_133 texture2d<float>
#endif
#if COMMON_DEPTH1_133
#define COMMON_IMAGE1_133 depth2d<float>
#else
#define COMMON_IMAGE1_133 texture2d<float>
#endif
inline float4 CommonTexture133(texture2d<float> t,sampler s,float2 uv,gradient2d g){return t.sample(s,uv,g);}
inline float4 CommonTexture133(depth2d<float> t,sampler s,float2 uv,gradient2d g){float d=t.sample(s,uv,g);return float4(d,d,d,1);}
inline bool CommonAlpha133(float a,float r,uint c){switch(c){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;default:return true;}}
fragment CommonResult133 CommonFragmentMain133(CommonOut133 in [[stage_in]],constant CommonRaster133& raster [[buffer(3)]],
 constant CommonSurface133& u [[buffer(5)]],COMMON_IMAGE0_133 image [[texture(0)]],sampler sampler0 [[sampler(0)]],
 COMMON_IMAGE1_133 image1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 const float2 pixel=floor(in.position.xy);uint mask=~0u;
 if(raster.options.x>1&&in.raster.y!=0){mask=0;for(uint s=0;s<raster.options.x;++s){float2 p=pixel+get_sample_position(s);p.y=raster.target.y-p.y;if(CommonStipple133(in,p,raster.options.x))mask|=1u<<s;}if(!mask)discard_fragment();}
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 p=float2(in.position.x,raster.target.y-in.position.y);
 if(raster.options.x==1&&(!CommonLineCoverage133(in.endpoints.xy,in.endpoints.zw,p,in.raster.x)||!CommonStipple133(in,p,1)))discard_fragment();
 const auto value=CommonInterpolate133(in,p);float4 result=value.color;
 if(u.options.x&&(u.options.x==2||u.parameters.w==0)){
  float2 uv=value.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);
  texel=u.parameters.w?float4(0,0,0,1):CommonTexture133(image,sampler0,uv,gradient2d(dx,dy));if(u.options.x==2)result.a*=texel.r;else result*=texel;
 }
 if(u.options1.x&&u.options.x!=2&&u.parameters1.w==0){float2 uv=value.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);result*=CommonTexture133(image1,sampler1,uv,gradient2d(dx,dy));}
 if(u.options.z&&!CommonAlpha133(result.a,u.parameters.x,u.options.w))discard_fragment();
 CommonResult133 out;out.color=result;out.mask=mask;
#if COMMON_MRT133
 out.c1=out.c2=out.c3=out.c4=result;
#if COMMON_COMBINED133
 out.c5=result;
#endif
#endif
 return out;
}
)MSL";
}
