#include "Rendering/Backend/NativePreparationTrace.h"
#include "Metal4RetiredArguments.h"
#include "Rendering/Backend/NativeExactWorldLine.h"
#include "NativeExactWorldLineShaders.h"
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldOverlayRenderer.h"
#include "System/Log/ILog.h"
#include "NativeSampleShadingShaders.h"
#include "NativePointCoverageShaders.h"
#include "NativeWorldFontShaders.h"
#include "Rendering/Backend/NativeWorldFillGeometry.h"
#include "NativeWaterOverlayShaders.h"
#include "Rendering/Backend/NativeWaterOverlayProgram.h"
#include "Rendering/Backend/NativeSplatPainterProgram.h"
#include <tuple>
#include "Metal4UnitEffects.h"
#include "Metal4UnitGlass.h"
#include "Rendering/Backend/NativeEasyFacingProgram.h"
#include "Rendering/Backend/NativeSquadHullProgram.h"
#include "Rendering/Backend/NativeTerraformOverlayProgram.h"
#include "Rendering/Backend/NativeLOSViewProgram.h"
#include "Rendering/UniformConstants.h"
#include "Rendering/Backend/NativeReclaimFieldData.h"
#include "Metal4WorldViewport.h"
#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include "Metal4SharedUploadPool.h"
#include "Metal4SourceLines133.h"
@interface M4WorldFontUploadLifetime : NSObject {
@public
 std::shared_ptr<rendering::metal4::ScreenUploadStorage> storage;
 std::vector<std::shared_ptr<rendering::metal4::ScreenUploadStorage>> lineStorage;
 std::vector<std::shared_ptr<rendering::metal4::PreparedSourceLines133>> sourceLines;
}
@end
@implementation M4WorldFontUploadLifetime
@end
namespace rendering::metal4 {
namespace {
// Optional bounded counters, written once at normal process exit. No GPU
// reads, screenshots, per-frame I/O or default-on measurement overhead.
struct WorldFontProbe {
 const char* path;
 explicit WorldFontProbe(const char* environment):path(std::getenv(environment)){}
 std::atomic<uint64_t> batches{0},draws{0},vertices{0},bytes{0},allocations{0},uvDraws{0};
 bool Enabled()const{return path&&*path;}
 ~WorldFontProbe(){if(!Enabled())return;if(auto f=std::fopen(path,"w")){std::fprintf(f,"batches,draws,vertices,bytes,newAllocations,affineUVDraws\n%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)batches.load(),(unsigned long long)draws.load(),(unsigned long long)vertices.load(),(unsigned long long)bytes.load(),(unsigned long long)allocations.load(),(unsigned long long)uvDraws.load());std::fclose(f);}}
};
WorldFontProbe& FontProbe(){static WorldFontProbe probe("BAR_NATIVE_WORLD_FONT_PROBE_FILE");return probe;}
struct ExactLineProbe {
 const char* path=std::getenv("BAR_NATIVE_WORLD_LINE_PROBE_FILE");std::atomic<uint64_t> draws{0},vertices{0},bytes{0},allocations{0},hits{0};
 bool Enabled()const{return path&&*path;}
 ~ExactLineProbe(){if(!Enabled())return;if(auto f=std::fopen(path,"w")){std::fprintf(f,"gpuDraws,gpuVertices,sharedPacketBytes,newBuffers,completedCacheHits\n%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)draws.load(),(unsigned long long)vertices.load(),(unsigned long long)bytes.load(),(unsigned long long)allocations.load(),(unsigned long long)hits.load());std::fclose(f);}}
};
ExactLineProbe& LineProbe(){static ExactLineProbe probe;return probe;}
struct StageLineProbe124 {
 const char* path=std::getenv("BAR_NATIVE_STAGE_LINE_PROBE_FILE");
 struct Row {std::atomic<uint64_t> draws{0},vertices{0},reclaim{0};};
 std::array<Row,size_t(NativeWorldDrawStage::TerrainReflection)+1> rows;
 bool Enabled()const{return path&&*path;}
 ~StageLineProbe124(){if(!Enabled())return;if(auto f=std::fopen(path,"w")){std::fprintf(f,"stage,gpuDraws,gpuVertices,reclaimDraws\n");for(size_t i=0;i<rows.size();++i)if(rows[i].draws.load())std::fprintf(f,"%zu,%llu,%llu,%llu\n",i,(unsigned long long)rows[i].draws.load(),(unsigned long long)rows[i].vertices.load(),(unsigned long long)rows[i].reclaim.load());std::fclose(f);}}
};
StageLineProbe124& StageProbe124(){static StageLineProbe124 probe;return probe;}

WorldFontProbe& FillProbe(){static WorldFontProbe probe("BAR_NATIVE_WORLD_FILL_PROBE_FILE");return probe;}
const char* SOURCE=RECOIL_NATIVE_SAMPLE_SHADING_MSL RECOIL_NATIVE_POINT_COVERAGE_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
#if MAP_MRT_BROADCAST
struct MRTOutput {
 float4 c0 [[color(0)]];
 float4 c1 [[color(1)]];
 float4 c2 [[color(2)]];
 float4 c3 [[color(3)]];
 float4 c4 [[color(4)]];
#if MAP_MRT_COMBINED
 float4 c5 [[color(5)]];
#endif
};
MRTOutput Broadcast(float4 c){MRTOutput out;out.c0=out.c1=out.c2=out.c3=out.c4=c;
#if MAP_MRT_COMBINED
 out.c5=c;
#endif
 return out;}
#define OUTPUT_TYPE MRTOutput
#define OUTPUT_VALUE(c) Broadcast(c)
#else
#define OUTPUT_TYPE float4
#define OUTPUT_VALUE(c) c
#endif
)MSL" RECOIL_WORLD_FONT_MSL R"MSL(
struct V {packed_float4 clip,color;packed_float4 uv;};
struct U {float4 scale;uint4 options;float4 parameters;float4 scale1;uint4 options1;float4 parameters1;float4 arrowShape,arrowDetail,arrowFill,arrowOutline;float4 hullColor,hullStripe,hullCentroid,hullAnimation;float4x4 overlayViewProjInv;float4 overlayPlacement,overlayFit,overlayMapSize;float4 losImage,losRaster;uint4 pointRaster;};
struct Out {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float4 uv [[RECOIL_SAMPLE_PERSPECTIVE]];float4 pointDisk [[flat]];};
vertex Out WorldOverlayVertex(uint id [[vertex_id]],device const V* vertices [[buffer(0)]],constant U& u [[buffer(1)]],device const packed_float4* pointDisks [[buffer(2)]]) {
 const V v=vertices[id];Out o;o.position=float4(v.clip);if(u.pointRaster.y)o.position=WorldFontPosition(reinterpret_cast<device const packed_float4*>(vertices),o.position);else o.position.z=(o.position.z+o.position.w)*.5f;o.color=float4(v.color);o.uv=float4(v.uv);if(u.pointRaster.z)o.uv=WorldTextureCoordinate(reinterpret_cast<device const packed_float4*>(vertices),o.uv);o.pointDisk=u.pointRaster.x?float4(pointDisks[id]):float4(0.0f);return o;
}
struct PointOutput {
 float4 c0 [[color(0)]];
#if MAP_MRT_BROADCAST
 float4 c1 [[color(1)]];float4 c2 [[color(2)]];float4 c3 [[color(3)]];float4 c4 [[color(4)]];
#if MAP_MRT_COMBINED
 float4 c5 [[color(5)]];
#endif
#endif
 uint sampleMask [[sample_mask]];
};
PointOutput PointResult(float4 color,uint mask) {
 PointOutput out;out.c0=color;out.sampleMask=mask;
#if MAP_MRT_BROADCAST
 out.c1=out.c2=out.c3=out.c4=color;
#if MAP_MRT_COMBINED
 out.c5=color;
#endif
#endif
 return out;
}
bool Alpha(float a,float r,uint c){switch(c){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;case 7:return true;}return false;}
float4 SampleOriginalUI(texture2d<float> image,sampler s,float2 uv) { return image.sample(s,uv); }
float4 SampleOriginalUI(texture2d<float> image,sampler s,float2 uv,gradient2d g) { return image.sample(s,uv,g); }
float4 SampleOriginalUI(depth2d<float> image,sampler s,float2 uv) { const float d=image.sample(s,uv);return float4(d,d,d,1); }
float4 SampleOriginalUI(depth2d<float> image,sampler s,float2 uv,gradient2d g) { const float d=image.sample(s,uv,g);return float4(d,d,d,1); }
// Original Splat Painter channel colors and alpha; fixed gl.Color is unused.
fragment OUTPUT_TYPE WorldSplatOverlayFragment(Out in [[stage_in]],constant U& u [[buffer(1)]],texture2d<float> image [[texture(0)]],sampler source [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=in.uv.xy;if(u.options.y)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);
 const float4 sp=image.sample(source,uv,gradient2d(dx,dy));
 const float3 c=sp.r*float3(1.f,.15f,.15f)+sp.g*float3(.15f,1.f,.15f)+sp.b*float3(.2f,.45f,1.f)+sp.a*float3(1.f,.85f,.1f);
 const float4 result=float4(c,clamp(sp.r+sp.g+sp.b+sp.a,0.f,1.f)*.55f);
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}
fragment PointOutput WorldOverlayFragment(Out in [[stage_in]],constant U& u [[buffer(1)]],texture2d<float> image [[texture(0)]],sampler source [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	const uint pointMask=RecoilPointSampleMask(in.position.xy,in.pointDisk);
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return PointResult(result,pointMask);
}
fragment PointOutput WorldOverlayFragment1(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> image [[texture(0)]],sampler source [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	const uint pointMask=RecoilPointSampleMask(in.position.xy,in.pointDisk);
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return PointResult(result,pointMask);
}
fragment PointOutput WorldOverlayFragment2(Out in [[stage_in]],constant U& u [[buffer(1)]],texture2d<float> image [[texture(0)]],sampler source [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	const uint pointMask=RecoilPointSampleMask(in.position.xy,in.pointDisk);
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return PointResult(result,pointMask);
}
fragment PointOutput WorldOverlayFragment3(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> image [[texture(0)]],sampler source [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	const uint pointMask=RecoilPointSampleMask(in.position.xy,in.pointDisk);
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return PointResult(result,pointMask);
}
constant float2 A = float2(0.0, -32.0);
constant float2 B = float2(0.0, 32.0);
constant float2 C = float2(24.0, 0.0);
// Incenter and inradius of the triangle; the incircle touches the base so the incenter is at (inradius, 0).
constant float INRADIUS = 32.0 / 3.0;
constant float2 INCENTER = float2(INRADIUS, 0.0);
// Inward normals of the two leading edges (A->C and B->C).
constant float2 N_AC = float2(-0.8, 0.6);
constant float2 N_BC = float2(-0.8, -0.6);

float sdTriangle(float2 p, float2 p0, float2 p1, float2 p2) {
	float2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
	float2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
	float2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
	float2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
	float2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
	float s = sign(e0.x * e2.y - e0.y * e2.x);
	float2 d = min(min(float2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
	                 float2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
	                 float2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
	return -sqrt(d.x) * sign(d.y);
}

float3 LOSViewHSVToRGB(float3 c){const float4 K=float4(1.,2./3.,1./3.,3.);return c.z*mix(K.xxx,clamp(abs(fract(c.x+K.xyz)*6.-K.w)-K.x,0.,1.),c.y);}
float3 LOSViewRGBToHSV(float3 c){const float4 K=float4(0.,-1./3.,2./3.,-1.);const float4 p=mix(float4(c.bg,K.wz),float4(c.gb,K.xy),step(c.b,c.g));const float4 q=mix(float4(p.xyw,c.r),float4(c.r,p.yzx),step(p.x,c.r));const float d=q.x-min(q.w,q.y),e=1e-10;return float3(abs(q.z+(q.w-q.y)/(6.*d+e)),d/(q.x+e),q.x);}
float LOSViewFilteredStep(float p,float resolution){
 resolution*=1.1f;const float dpdx=dfdx(p),dpdy=dfdy(p),dpddiag=dfdy(dpdx);const float w=max(max(abs(dpdx),abs(dpdy)),abs(dpddiag));
 if(w<1e-6f)return 0;const float a=p+.5f*w,b=p-.5f*w;
 const float i=(floor(a)+min(fract(a)*resolution,10.1f)-floor(b)-min(fract(b)*resolution,10.1f))/(resolution*w);return clamp(i-1,0.f,1.f);
}
fragment OUTPUT_TYPE WorldLOSViewFragment(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> mapDepths [[texture(0)]],sampler depthSampler [[sampler(0)]],texture2d<float> screenCopy [[texture(1)]],sampler copySampler [[sampler(1)]],texture2d<float> losTex [[texture(2)]],sampler losSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float2 screenUV=in.uv.xy;
 float2 copyUV=screenUV;if(u.options1.y)copyUV.y=1-copyUV.y;const float2 copyDx=dfdx(copyUV),copyDy=dfdy(copyUV);if(u.parameters1.y)copyUV.x=clamp(copyUV.x,0.f,1.f);if(u.parameters1.z)copyUV.y=clamp(copyUV.y,0.f,1.f);
 float4 screenColor=screenCopy.sample(copySampler,copyUV,gradient2d(copyDx,copyDy));
 float2 depthUV=screenUV;if(u.options.y)depthUV.y=1-depthUV.y;const float2 depthDx=dfdx(depthUV),depthDy=dfdy(depthUV);if(u.parameters.y)depthUV.x=clamp(depthUV.x,0.f,1.f);if(u.parameters.z)depthUV.y=clamp(depthUV.y,0.f,1.f);
 const float worlddepth=mapDepths.sample(depthSampler,depthUV,gradient2d(depthDx,depthDy));
 // Original PREUNIT=1 excludes modelDepths even though Lua binds unit1.
 float4 world=u.overlayViewProjInv*float4(screenUV*2.0f-1.0f,worlddepth,1);world.xyz/=world.w;
 float2 losUV=clamp(world.xz/u.overlayMapSize.xy,0.f,1.f);if(u.losImage.x)losUV.y=1-losUV.y;
 const float2 losDx=dfdx(losUV),losDy=dfdy(losUV);if(u.losImage.y)losUV.x=clamp(losUV.x,0.f,1.f);if(u.losImage.z)losUV.y=clamp(losUV.y,0.f,1.f);
 const float4 los=losTex.sample(losSampler,losUV,gradient2d(losDx,losDy));const float3 current=smoothstep(.227f,.973f,los.rgb);
 float3 hsv=LOSViewRGBToHSV(screenColor.rgb);hsv=mix(hsv*float3(1,.5,1),hsv,current.g);hsv=mix(hsv*float3(1,0,1),hsv,current.r);screenColor.rgb=LOSViewHSVToRGB(hsv);
 const float3 scanlineColor=screenColor.rgb*.75f;const float glY=u.losRaster.x-in.position.y;
 screenColor.rgb=mix(screenColor.rgb,scanlineColor,(1-current.b)*step(.5f,fract(glY/4.f)));
 float timeCorrection=u.overlayPlacement.w;if(timeCorrection<0)timeCorrection=0;if(timeCorrection>1)timeCorrection=0;
 const float losEdge=LOSViewFilteredStep(current.b-.5f-timeCorrection*.00f,.5f);screenColor.g+=.2f*losEdge;
 const float4 result=float4(screenColor.rgb,1);if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}

fragment OUTPUT_TYPE WorldTerraformOverlayFragment(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> mapDepths [[texture(0)]],sampler depthSampler [[sampler(0)]],texture2d<float> overlayTex [[texture(1)]],sampler imageSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float2 screenUV=in.uv.xy;float2 depthUV=screenUV;if(u.options.y)depthUV.y=1-depthUV.y;
 const float2 depthDx=dfdx(depthUV),depthDy=dfdy(depthUV);if(u.parameters.y)depthUV.x=clamp(depthUV.x,0.f,1.f);if(u.parameters.z)depthUV.y=clamp(depthUV.y,0.f,1.f);
 const float mapdepth=mapDepths.sample(depthSampler,depthUV,gradient2d(depthDx,depthDy));if(mapdepth>=.999999f)discard_fragment();
 float4 worldPos=u.overlayViewProjInv*float4(screenUV*2.0f-1.0f,mapdepth,1.0f);worldPos.xyz/=worldPos.w;
 const float2 mapUV=worldPos.xz/u.overlayMapSize.xy;
 float2 p=(mapUV-.5f-u.overlayPlacement.zw)/max(u.overlayPlacement.y,1e-4f);p/=u.overlayFit.xy;
 float2 uv=p+.5f;if(any(uv<float2(0))||any(uv>float2(1)))discard_fragment();
 if(u.overlayFit.z>.5f)uv.x=1-uv.x;if(u.overlayFit.w>.5f)uv.y=1-uv.y;
 if(u.options1.y)uv.y=1-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);
 const float4 img=overlayTex.sample(imageSampler,uv,gradient2d(dx,dy));const float4 result=float4(img.rgb,img.a*u.overlayPlacement.x);
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}

fragment OUTPUT_TYPE WorldSquadHullFragment(Out in [[stage_in]],constant U& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float a=u.hullColor.a;
 if(u.hullStripe.x>0.0f){const float band=step(.5f,fract((in.uv.x+in.uv.y+u.hullStripe.z)/u.hullStripe.x));a*=mix(u.hullStripe.y,1.0f,band);}
 const float2 toCenter=in.uv.xy-u.hullCentroid.xy;
 const float dist=length(toCenter)/max(u.hullCentroid.z,1.0f);
 a*=mix(u.hullAnimation.y,1.0f,smoothstep(0.0f,1.0f,dist));
 a*=u.hullAnimation.x;
 const float4 result=float4(u.hullColor.rgb,a);
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}

fragment OUTPUT_TYPE WorldEasyFacingFragment(Out in [[stage_in]],constant U& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	float2 p = in.uv.xy;
	// Round the corners without changing the outer extent: shrink the triangle towards its incenter by the
	// radius, then grow the distance field back out by the same amount.
	float radiusLocal = min(u.arrowShape.y / u.arrowShape.x, INRADIUS * 0.9);
	float shrink = 1.0 - radiusLocal / INRADIUS;
	float2 a = INCENTER + (A - INCENTER) * shrink;
	float2 b = INCENTER + (B - INCENTER) * shrink;
	float2 c = INCENTER + (C - INCENTER) * shrink;
	float edgeDistance = (sdTriangle(p, a, b, c) - radiusLocal) * u.arrowShape.x; // elmos, negative inside
	float antialias = fwidth(edgeDistance);
	float coverage = 1.0 - smoothstep(0.0, antialias, edgeDistance);
	float outline = smoothstep(-u.arrowShape.z - antialias, -u.arrowShape.z + antialias, edgeDistance);

	// Inner chevron: a band parallel to the two leading edges, measured in local units so it scales with the arrow.
	float leadingDistance = min(dot(p - A, N_AC), dot(p - B, N_BC));
	float maxLeadingDistance = dot(-A, N_AC); // at the base centre
	float chevronCentre = u.arrowShape.w * maxLeadingDistance;
	float chevronDistance = abs(leadingDistance - chevronCentre) * u.arrowShape.x;
	float chevronAntialias = fwidth(chevronDistance);
	float chevron = 1.0 - smoothstep(u.arrowDetail.x * 0.5 - chevronAntialias, u.arrowDetail.x * 0.5 + chevronAntialias, chevronDistance);
	// Keep the chevron clear of the outline band so they don't merge into a blob at the base corners.
	chevron *= 1.0 - smoothstep(-u.arrowShape.z * 2.0 - antialias, -u.arrowShape.z * 2.0 + antialias, edgeDistance);
	// ...and stop its legs short of the flat side so the arrow keeps a clean base.
	float baseDistance = p.x * u.arrowShape.x;
	float baseAntialias = fwidth(baseDistance);
	chevron *= smoothstep(u.arrowDetail.y - baseAntialias, u.arrowDetail.y + baseAntialias, baseDistance);

	// The fill gets a touch denser towards the tip so the direction reads even without the outline.
	float tipMix = clamp(p.x / C.x, 0.0, 1.0);
	float fillAlpha = u.arrowFill.a * mix(0.85, 1.15, tipMix);

	float detail = max(outline, chevron * 0.85);
	float3 color = mix(u.arrowFill.rgb, u.arrowOutline.rgb, detail);
	float alpha = mix(fillAlpha, u.arrowOutline.a, detail);
	float4 result=float4(color, alpha * coverage * u.arrowDetail.z); if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}

)MSL" RECOIL_NATIVE_WATER_OVERLAY_MSL;
struct Uniform{std::array<float,4> scale;std::array<uint32_t,4> options;std::array<float,4> parameters;std::array<float,4> scale1;std::array<uint32_t,4> options1;std::array<float,4> parameters1;std::array<float,4> arrowShape,arrowDetail,arrowFill,arrowOutline;std::array<float,4> hullColor,hullStripe,hullCentroid,hullAnimation;std::array<float,16> overlayViewProjInv;std::array<float,4> overlayPlacement,overlayFit,overlayMapSize;std::array<float,4> losImage,losRaster;std::array<uint32_t,4> pointRaster{};};
static_assert(sizeof(Uniform)==384);
static_assert(offsetof(Uniform,pointRaster)==368);
struct WaterUniform {
 std::array<float,16> viewProj,viewProjInv,shadowView;
 std::array<float,4> camera,mapSize,time,sun,view,parameters;
 std::array<uint32_t,4> flags;
 std::array<std::array<uint32_t,4>,6> images;
 std::array<float,4> alpha;
};
static_assert(sizeof(WaterUniform)==416);
MTLBlendOperation Equation(ScreenBlendEquation value) {
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Unsupported original blend equation");
}
MTLBlendFactor Factor(ScreenBlendFactor x)
{
	switch(x){case ScreenBlendFactor::Zero:return MTLBlendFactorZero;case ScreenBlendFactor::One:return MTLBlendFactorOne;case ScreenBlendFactor::SourceAlpha:return MTLBlendFactorSourceAlpha;case ScreenBlendFactor::OneMinusSourceAlpha:return MTLBlendFactorOneMinusSourceAlpha;case ScreenBlendFactor::DestinationAlpha:return MTLBlendFactorDestinationAlpha;case ScreenBlendFactor::OneMinusDestinationAlpha:return MTLBlendFactorOneMinusDestinationAlpha;case ScreenBlendFactor::SourceColor:return MTLBlendFactorSourceColor;case ScreenBlendFactor::OneMinusSourceColor:return MTLBlendFactorOneMinusSourceColor;case ScreenBlendFactor::DestinationColor:return MTLBlendFactorDestinationColor;case ScreenBlendFactor::OneMinusDestinationColor:return MTLBlendFactorOneMinusDestinationColor;case ScreenBlendFactor::SourceAlphaSaturate:return MTLBlendFactorSourceAlphaSaturated;}
	throw std::runtime_error("Native world blend factor unsupported");
}
MTLCompareFunction Compare(ScreenAlphaCompare value)
{
	switch(value){case ScreenAlphaCompare::Never:return MTLCompareFunctionNever;case ScreenAlphaCompare::Less:return MTLCompareFunctionLess;case ScreenAlphaCompare::Equal:return MTLCompareFunctionEqual;case ScreenAlphaCompare::LessEqual:return MTLCompareFunctionLessEqual;case ScreenAlphaCompare::Greater:return MTLCompareFunctionGreater;case ScreenAlphaCompare::NotEqual:return MTLCompareFunctionNotEqual;case ScreenAlphaCompare::GreaterEqual:return MTLCompareFunctionGreaterEqual;case ScreenAlphaCompare::Always:return MTLCompareFunctionAlways;}
	throw std::runtime_error("Native world comparison unsupported");
}
MTLStencilOperation Operation(NativeWorldStencilOp value)
{
	switch(value){case NativeWorldStencilOp::Keep:return MTLStencilOperationKeep;case NativeWorldStencilOp::Zero:return MTLStencilOperationZero;case NativeWorldStencilOp::Replace:return MTLStencilOperationReplace;case NativeWorldStencilOp::IncrementClamp:return MTLStencilOperationIncrementClamp;case NativeWorldStencilOp::DecrementClamp:return MTLStencilOperationDecrementClamp;case NativeWorldStencilOp::Invert:return MTLStencilOperationInvert;case NativeWorldStencilOp::IncrementWrap:return MTLStencilOperationIncrementWrap;case NativeWorldStencilOp::DecrementWrap:return MTLStencilOperationDecrementWrap;}
	throw std::runtime_error("Native world stencil operation unsupported");
}
 #include "Metal4WorldGeometryView.inc"
#include "Metal4WorldGeometryProbe174.inc"
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size)
{
 PreparationTraceScope bufferProbe91(32);CountPreparationTrace(12,1);CountPreparationTrace(13,size);
	Require(size&&size<=lease.Device().maxBufferLength,"Native world overlay buffer extent invalid");auto value=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];Require(value!=nil,"Native world overlay allocation failed");lease.AddAllocation(value);return value;
}
}
struct ExactLineJob {
 __strong id<MTLComputePipelineState> pipeline=nil;
 __strong id<MTL4ArgumentTable> arguments=nil;
 std::shared_ptr<std::atomic<bool>> completed;
 uint32_t quads=0;
 bool encoded=false;
};
struct PreparedWorldOverlayFrame::Impl {
 RetiredArgumentOwners argumentOwners;
 __strong M4WorldFontUploadLifetime* fontUploadLifetime=nil;
	struct Draw {
		uint64_t sequence=0;NativeWorldDrawStage stage=NativeWorldDrawStage::World;
  std::shared_ptr<ExactLineJob> line;
  std::shared_ptr<PreparedSourceLines133> sourceLines;
		std::shared_ptr<PreparedUnitEffect> unitEffect;
  std::shared_ptr<PreparedUnitGlass> unitGlass;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
		NativeWorldDrawState state;MTLViewport viewport{};MTLScissorRect scissor{};uint32_t count=0;
		bool polygon=false;size_t indexOffset=0,indexLength=0;
		std::vector<PreparedScreenExternalImage> externalImages;
 std::vector<PreparedPiPSourceImage> typedImages;
	};
	std::vector<Draw> draws;
 bool hasLineJobs=false;
};
PreparedWorldOverlayFrame::PreparedWorldOverlayFrame():impl(std::make_unique<Impl>()){}
PreparedWorldOverlayFrame::~PreparedWorldOverlayFrame()=default;
void PreparedWorldOverlayFrame::EncodeLineDependencies(id<MTL4CommandBuffer> command)const{
 for(const auto& d:impl->draws)if(d.sourceLines)d.sourceLines->EncodeDependencies(command);
 if(!impl->hasLineJobs)return;
 id<MTL4ComputeCommandEncoder> lines=nil;
 for(const auto& d:impl->draws)if(d.line){auto& job=*d.line;Require(!job.encoded,"Exact world lines were encoded twice");if(!lines){lines=[command computeCommandEncoder];Require(lines,"Cannot encode exact world lines");}[lines setComputePipelineState:job.pipeline];[lines setArgumentTable:job.arguments];[lines dispatchThreads:MTLSizeMake(job.quads,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];job.encoded=true;}
 if(lines){[lines barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];[lines endEncoding];}
}
void PreparedWorldOverlayFrame::EncodeDependencies(id<MTL4CommandBuffer> command)const{
 EncodeLineDependencies(command);
for(const auto& d:impl->draws)if(d.unitGlass)d.unitGlass->EncodeDependencies(command);for(const auto& d:impl->draws)for(const auto& image:d.typedImages)if(image.encodeDependencies)image.encodeDependencies(command);}
void PreparedWorldOverlayFrame::CompleteLineDependencies()const{if(!impl->hasLineJobs)return;for(const auto& d:impl->draws)if(d.line){Require(d.line->encoded,"Unencoded exact lines cannot enter immutable cache");d.line->completed->store(true,std::memory_order_release);}
}
void PreparedWorldOverlayFrame::DidComplete()const{
 CompleteLineDependencies();
for(const auto& d:impl->draws)if(d.unitGlass)d.unitGlass->DidComplete();for(const auto& d:impl->draws)for(const auto& image:d.typedImages)if(image.didComplete)image.didComplete();}
size_t PreparedWorldOverlayFrame::DrawCount()const{return impl->draws.size();}
uint64_t PreparedWorldOverlayFrame::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
NativeWorldDrawStage PreparedWorldOverlayFrame::StageAt(size_t i)const{return impl->draws.at(i).stage;}
void PreparedWorldOverlayFrame::EncodeDraw(size_t i,id<MTL4RenderCommandEncoder> encoder)const
{
	const auto& d=impl->draws.at(i);if(d.unitGlass){d.unitGlass->Encode(encoder);return;}if(d.unitEffect){d.unitEffect->Encode(encoder);return;}if(!d.count)return;
 if(d.sourceLines)d.sourceLines->RequireReady();
	if(d.line)Require(d.line->encoded,"Exact world line producer must precede its draw");
	for(const auto& image:d.externalImages)image.requireReadable();
 for(const auto& image:d.typedImages)image.requireReadable();
	[encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];
	[encoder setViewport:d.viewport];[encoder setScissorRect:d.scissor];[encoder setDepthClipMode:(d.sourceLines||d.state.depthClamp)?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setCullMode:d.polygon&&d.state.cullEnabled?(d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack):MTLCullModeNone];
	[encoder setDepthBias:d.polygon&&d.state.polygonOffset?d.state.offsetUnits:0 slopeScale:d.polygon&&d.state.polygonOffset?d.state.offsetFactor:0 clamp:0];
	[encoder setStencilFrontReferenceValue:uint32_t(std::clamp(d.state.frontStencil.reference,0,255)) backReferenceValue:uint32_t(std::clamp(d.sourceLines?d.state.frontStencil.reference:d.state.backStencil.reference,0,255))];
	[encoder setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 if(d.sourceLines)[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:d.count];
 else [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress+d.indexOffset indexBufferLength:d.indexLength?d.indexLength:d.indices.length];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
}
struct SquadHullCompiledOwner {__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> pipeline=nil;};
#include "Metal4WorldLineGeometryCache.inc"
struct WorldOverlayGPU::Impl {
 SourceLinesGPU133 sourceLines;
 RetiredArguments argumentPool;
 WorldLineGeometryCache lineGeometry;
 __strong id<MTLLibrary> lineLibrary=nil;
 __strong id<MTLComputePipelineState> linePipeline=nil;
 bool lineValidation=false;
 void CompileLines(RuntimeLease& lease,bool validation,double& shaderMs){
  if(linePipeline&&lineValidation==validation)return;
  const auto start=Clock::now();NSError* error=nil;auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  auto library=lease.NewLibraryWithSource(String(ExactWorldLineShader),options,&error);Require(library,"Cannot compile exact world line arithmetic: "+Error(error));
  auto function=[MTL4LibraryFunctionDescriptor new];function.library=library;function.name=@"ExactWorldLineGeometry";auto descriptor=[MTL4ComputePipelineDescriptor new];descriptor.computeFunctionDescriptor=function;descriptor.options=[MTL4PipelineOptions new];descriptor.options.shaderValidation=ShaderValidationMode(validation);
  auto pipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];Require(pipeline,"Cannot compile exact world line geometry: "+Error(error));lineLibrary=library;linePipeline=pipeline;lineValidation=validation;shaderMs+=Milliseconds(start);
 }

 ScreenUploadPool fontUploadPool;
 std::shared_ptr<const SquadHullCompiledOwner> squadHullCompiled,terraformOverlayCompiled,losViewCompiled,splatOverlayCompiled;
 std::array<std::shared_ptr<const SquadHullCompiledOwner>,2> waterOverlayCompiled;
 UnitEffectsGPU unitEffects;
 UnitGlassGPU unitGlass;
	std::array<__strong id<MTLLibrary>,3> libraries{},fontLibraries{};
	std::map<std::tuple<uint64_t,bool,unsigned,bool,unsigned,unsigned>,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const ScreenBlendState& blend,const std::array<bool,4>& mask,MTLPixelFormat format,bool validation,double& shaderMs,unsigned sampledDepthMask,bool mapMRTBroadcast,bool alphaToCoverage,unsigned samples,bool combined=false,unsigned combinedMask=15,bool gpuFont=false)
	{
		const uint64_t key=uint64_t(gpuFont)<<29|uint64_t(format)<<32|uint64_t(validation)<<31|uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24|uint64_t(mapMRTBroadcast)<<30;
		const auto pipelineKey=std::make_tuple(key,alphaToCoverage,lease.SampleShadingKey(samples),combined,combinedMask,sampledDepthMask);
		if(auto found=pipelines.find(pipelineKey);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;
		auto& library=(gpuFont?fontLibraries:libraries)[combined?2:mapMRTBroadcast?1:0];
  if(library==nil){auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(SOURCE);auto compile=[MTLCompileOptions new];if(gpuFont){compile.mathMode=MTLMathModeSafe;compile.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;}compile.preprocessorMacros=@{@"MAP_MRT_BROADCAST":@(mapMRTBroadcast),@"MAP_MRT_COMBINED":@(combined)};descriptor.options=compile;descriptor.name=@"Original common Lua world primitives";library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile native world overlay shader: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new];vs.library=library;vs.name=sampledDepthMask==8?@"WaterSurfaceVertex":sampledDepthMask==9?@"WaterFogVertex":@"WorldOverlayVertex";d.vertexFunctionDescriptor=vs;auto fs=[MTL4LibraryFunctionDescriptor new];fs.library=library;fs.name=sampledDepthMask==10?@"WorldSplatOverlayFragment":sampledDepthMask==8?@"WaterSurfaceFragment":sampledDepthMask==9?@"WaterFogFragment":sampledDepthMask==7?@"WorldLOSViewFragment":sampledDepthMask==6?@"WorldTerraformOverlayFragment":sampledDepthMask==5?@"WorldSquadHullFragment":sampledDepthMask==4?@"WorldEasyFacingFragment":String(std::string("WorldOverlayFragment")+(sampledDepthMask?std::to_string(sampledDepthMask):""));d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		d.rasterSampleCount=samples;
		d.alphaToCoverageState=alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		for(unsigned n=0;n<(combined?6u:mapMRTBroadcast?5u:1u);++n){		auto color=d.colorAttachments[n];color.pixelFormat=format;color.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;color.sourceRGBBlendFactor=Factor(blend.sourceColor);color.destinationRGBBlendFactor=Factor(blend.destinationColor);color.sourceAlphaBlendFactor=Factor(blend.sourceAlpha);color.destinationAlphaBlendFactor=Factor(blend.destinationAlpha);color.rgbBlendOperation=Equation(blend.colorEquation);color.alphaBlendOperation=Equation(blend.alphaEquation);color.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
   if(n==5)color.writeMask=MTLColorWriteMask((combinedMask&1?MTLColorWriteMaskRed:0)|(combinedMask&2?MTLColorWriteMaskGreen:0)|(combinedMask&4?MTLColorWriteMaskBlue:0)|(combinedMask&8?MTLColorWriteMaskAlpha:0));
  }
		auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile native world overlay pipeline: "+Error(error));pipelines.emplace(pipelineKey,pipeline);shaderMs+=Milliseconds(start);return pipeline;
	}
};
WorldOverlayGPU::WorldOverlayGPU():impl(std::make_unique<Impl>()){}
WorldOverlayGPU::~WorldOverlayGPU(){for(unsigned n=0;n<2;++n)if(NativeWaterOverlayConsumer(n)==impl->waterOverlayCompiled[n])SetNativeWaterOverlayConsumer(n,{});if(NativeLOSViewConsumer()==impl->losViewCompiled)SetNativeLOSViewConsumer({});if(NativeTerraformOverlayConsumer()==impl->terraformOverlayCompiled)SetNativeTerraformOverlayConsumer({});if(NativeSquadHullConsumer()==impl->squadHullCompiled)SetNativeSquadHullConsumer({});SetNativeEasyFacingConsumer({});}
void WorldOverlayGPU::InitializeReclaimLines(RuntimeLease& lease,MTLPixelFormat color,bool validation,double& shaderMs,unsigned samples)
{
 ScreenBlendState b;b.enabled=true;b.sourceColor=b.sourceAlpha=ScreenBlendFactor::SourceAlpha;b.destinationColor=b.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
 impl->unitEffects.Initialize(lease,color,validation,shaderMs,samples);
 impl->unitGlass.Initialize(lease,color,validation,shaderMs,samples);
 impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,0,false,false,samples);
 impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,4,false,false,samples);SetNativeEasyFacingConsumer(std::make_shared<const unsigned char>(1));
 auto hull=std::make_shared<SquadHullCompiledOwner>();hull->pipeline=impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,5,false,false,samples);hull->library=impl->libraries[0];
 Require(hull->pipeline!=nil&&hull->library!=nil,"Squad Hull original consumer failed to compile");impl->squadHullCompiled=hull;SetNativeSquadHullConsumer(std::move(hull));
 auto overlay=std::make_shared<SquadHullCompiledOwner>();overlay->pipeline=impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,6,false,false,samples);overlay->library=impl->libraries[0];
 Require(overlay->pipeline!=nil&&overlay->library!=nil,"Terraform Overlay original consumer failed to compile");impl->terraformOverlayCompiled=overlay;SetNativeTerraformOverlayConsumer(std::move(overlay));
 auto los=std::make_shared<SquadHullCompiledOwner>();los->pipeline=impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,7,false,false,samples);los->library=impl->libraries[0];
 Require(los->pipeline!=nil&&los->library!=nil,"LOS View original consumer failed to compile");impl->losViewCompiled=los;SetNativeLOSViewConsumer(std::move(los));
 auto splat=std::make_shared<SquadHullCompiledOwner>();splat->pipeline=impl->Pipeline(lease,b,{true,true,true,true},color,validation,shaderMs,10,false,false,samples);splat->library=impl->libraries[0];
 Require(splat->pipeline!=nil&&splat->library!=nil,"Splat Painter actual world overlay unavailable");impl->splatOverlayCompiled=splat;NativeSplatOverlayOwner()=splat;
 for(unsigned n=0;n<2;++n){auto w=std::make_shared<SquadHullCompiledOwner>();auto blend=b;if(n)blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::One;
  w->pipeline=impl->Pipeline(lease,blend,{true,true,true,true},color,validation,shaderMs,8+n,false,false,samples);w->library=impl->libraries[0];
  Require(w->pipeline!=nil&&w->library!=nil,"Water Overlay actual surface/fog program unavailable");impl->waterOverlayCompiled[n]=w;SetNativeWaterOverlayConsumer(n,std::move(w));
 }
}
void WorldOverlayGPU::ClearResources(){impl->sourceLines.Clear();impl->argumentPool.Clear();impl->unitGlass.ClearResources();impl->unitEffects.ClearResources();} // Immutable images belong to the shared ScreenGPU; compiled programs remain valid.
std::unique_ptr<PreparedWorldOverlayFrame> WorldOverlayGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawSelection167& frame,
	ScreenGPU& images,const PreparedScreenFrame* targetProducers,const WorldCamera& camera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,
	MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& shaderMs,bool mapMRTBroadcast,unsigned samples,bool mapCombinedColor,const CUSSourceImageProvider& glassImages)
{
 frame.RequireOverlay();
 std::unique_ptr<WorldGeometryProbe174> observed174;
 if(worldPlan167::Observe174(frame.SourceDrawFrame()))observed174=std::make_unique<WorldGeometryProbe174>();
 PreparationTraceScope overlayProbe91(26);
	Require(!mapMRTBroadcast||(colorFormat==MTLPixelFormatRGBA8Unorm&&stencilFormat==MTLPixelFormatInvalid),"Map generic broadcast requires its actual five-color/no-stencil target");
 Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Native world overlay requires actual scene depth");
// Preflight the entire marked frame before allocations or any early-out.
	// Source-white complement blending is algebraically byte inversion on
	// linear UNORM8 only. Exact hardware bytes require the dedicated GPU gate.
	for (const auto& source : frame) if (source.world.invertColorUnorm8) {
		Require(colorFormat == MTLPixelFormatBGRA8Unorm || colorFormat == MTLPixelFormatRGBA8Unorm,
			"Rectangle inversion requires a linear eight-bit UNORM target");
		Require(!mapMRTBroadcast && !mapCombinedColor && source.stage == NativeWorldDrawStage::World,
			"Rectangle inversion requires the main world color attachment");
		Require(stencilFormat == MTLPixelFormatStencil8 || stencilFormat == MTLPixelFormatDepth32Float_Stencil8,
			"Rectangle inversion requires its actual paired stencil attachment");
		Require(source.world.stencilTest && !source.world.alphaToCoverage && !source.surface.alphaTest.enabled,
			"Rectangle inversion requires stencil and unmodified fragment coverage");
		Require(source.surface.material == ScreenMaterial::Solid && !source.surface.secondary.texture && !source.surface.splatPainter &&
			!source.originalShadowInput && !source.easyFacing && !source.squadHull && !source.terraformOverlay && !source.losView &&
			!source.waterOverlay && !source.unitEffect && !source.distortion && !source.objectSpotlight && !source.grass &&
			!source.resourceSpots && !source.sensorLOS && !source.reclaimField,
			"Rectangle inversion requires the solid common world program");
		for (const auto& vertex : source.vertices)
			Require(vertex.color == ScreenColor{1,1,1,1}, "Rectangle inversion requires exact opaque white source vertices");
	}
 impl->sourceLines.BeginBatch(lease);
	impl->argumentPool.BeginBatch(lease);auto result=std::make_unique<PreparedWorldOverlayFrame>();
 // Font matrices, vertices, indices and uniforms share one CPU-visible GPU
 // allocation per Prepare. No GPU copy or extra submission is introduced.
 // Both the prepared frame and completion/poison graph own the reuse token.
 impl->fontUploadPool.BeginBatch(lease.Device());size_t fontBytes=0;
 const size_t fontMaximum=lease.Device().maxBufferLength;
 const auto aligned=[](size_t n){return (n+255)&~size_t(255);};
 bool packFonts=true;
 for(const auto& source:frame)if(IsWorldMatrixGPU(source.sourcePrimitive)){
  for(size_t length:{source.vertices.size()*sizeof(NativeWorldDrawVertex),source.indices.size()*sizeof(uint32_t),sizeof(Uniform)}){
   if(length>fontMaximum||fontBytes>fontMaximum-std::min(fontMaximum,size_t(255))||aligned(fontBytes)>fontMaximum-length){packFonts=false;break;}
   fontBytes=aligned(fontBytes)+length;
  }
  if(!packFonts)break;
 }
 id<MTLBuffer> fontBuffer=nil;size_t fontCursor=0;
 if(packFonts&&fontBytes){
  result->impl->fontUploadLifetime=[M4WorldFontUploadLifetime new];lease.Retain(result->impl->fontUploadLifetime);
  auto& slot=result->impl->fontUploadLifetime->storage;const auto cachedBefore=impl->fontUploadPool.CachedCount();slot=impl->fontUploadPool.Acquire(lease.Device(),fontBytes);
  fontBuffer=slot?slot->buffer:[lease.Device() newBufferWithLength:fontBytes options:MTLResourceStorageModeShared];
  if(observed174&&fontBuffer){observed174->matrixBufferRequest=fontBytes;if(!slot||impl->fontUploadPool.CachedCount()>cachedBefore)observed174->matrixNewBufferBytes=fontBuffer.length;}
  if(fontBuffer&&FontProbe().Enabled()){FontProbe().batches.fetch_add(1,std::memory_order_relaxed);FontProbe().bytes.fetch_add(fontBytes,std::memory_order_relaxed);if(!slot||impl->fontUploadPool.CachedCount()>cachedBefore)FontProbe().allocations.fetch_add(1,std::memory_order_relaxed);}
  if(fontBuffer){Require(fontBuffer.contents,"World font shared buffer has no CPU address");lease.AddAllocation(fontBuffer);CountPreparationTrace(12,1);CountPreparationTrace(13,fontBytes);}
 }
 const auto packFont=[&](const void* data,size_t length){
  fontCursor=aligned(fontCursor);Require(fontBuffer&&length&&fontCursor<=fontBytes&&length<=fontBytes-fontCursor,"World font shared upload bounds differ");
  const auto offset=fontCursor;std::memcpy(static_cast<uint8_t*>(fontBuffer.contents)+offset,data,length);fontCursor+=length;return offset;
 };
 WorldBorrowMetrics168 borrowWork168;
	uint64_t lastSequence=0;std::size_t sourceOrdinal92=0;
	for(const auto& source:frame) {
  const auto lineOrdinal92=sourceOrdinal92++;
  auto* row174=observed174?&observed174->Select(source):nullptr;
  const auto validationStart174=row174?WorldGeometryProbe174::Clock::now():WorldGeometryProbe174::Clock::time_point{};
		const ScopedWorldBorrow168 validatedBorrow168(source,borrowWork168);Require(source.sequence>lastSequence,"Native world draw ordinal must retain actual call order");lastSequence=source.sequence;
  if(row174)row174->validationNS+=WorldGeometryProbe174::Since(validationStart174);
  if(source.unitGlass){Require(!mapMRTBroadcast,"Glass cannot execute in terrain MRT");PreparedWorldOverlayFrame::Impl::Draw draw;draw.sequence=source.sequence;draw.stage=source.stage;draw.unitGlass=impl->unitGlass.Prepare(lease,source,glassImages,camera,width,height,logicalWidth,logicalHeight,colorFormat,depthFormat,stencilFormat,validation,shaderMs,samples);result->impl->draws.push_back(std::move(draw));continue;}
  if(source.unitEffect){Require(!mapMRTBroadcast,"Airjets cannot execute in a terrain MRT callback");PreparedWorldOverlayFrame::Impl::Draw draw;draw.sequence=source.sequence;draw.stage=source.stage;draw.unitEffect=impl->unitEffects.Prepare(lease,source,images,targetProducers,camera,width,height,logicalWidth,logicalHeight,colorFormat,depthFormat,stencilFormat,validation,shaderMs,samples);result->impl->draws.push_back(std::move(draw));continue;}
		if(source.distortion)continue; // Paired command-level original Distortion consumer.
		if(source.objectSpotlight)continue; // Dedicated original instanced-cylinder vertex/fragment consumer.
		if(source.grass||source.resourceSpots||source.sensorLOS)continue; // Actual indexed Grass buffers belong to their paired GPU consumer.
		if(source.reclaimField)continue; // Paired original-buffer consumer owns fill and outline, including fallback/order.
		auto rectangle=camera.viewport;if(source.surface.viewport){const auto& v=*source.surface.viewport;Require(v.width<=INT32_MAX && v.height<=INT32_MAX,"Original world viewport exceeds GLsizei");rectangle={v.x,v.y,std::int32_t(v.width),std::int32_t(v.height)};}
		if(rectangle[2]==0 || rectangle[3]==0)continue;
		auto mapped=MapWorldViewport(rectangle,logicalWidth,logicalHeight,width,height);if(!mapped)continue;
  const unsigned geometryCategory91=source.primitive==NativeDrawPrimitive::Lines?29:source.primitive==NativeDrawPrimitive::Points?30:
   (source.world.frontPolygonMode!=NativePolygonMode::Fill||source.world.backPolygonMode!=NativePolygonMode::Fill)?31:source.world.inheritedClipPlane?28:27;
  const auto geometryStart91=StartPreparationScope();
  const auto meshStart174=row174?WorldGeometryProbe174::Clock::now():WorldGeometryProbe174::Clock::time_point{};
  const auto cachedLines=source.sourceLines?nullptr:impl->lineGeometry.Find(lease,source,lineOrdinal92,mapped->viewport,width,height,samples);
  if(cachedLines&&cachedLines->completed&&LineProbe().Enabled())LineProbe().hits.fetch_add(1,std::memory_order_relaxed);
  std::optional<exactWorldLine::Packet> linePacket;
  if(!source.sourceLines&&!cachedLines)linePacket=exactWorldLine::TryBuild(source,mapped->viewport,width,height,samples);
  NativeWorldRasterMesh mesh;
  if(source.sourceLines)mesh.fullViewport=true;
  else if(linePacket)mesh.fullViewport=samples>1;
  const bool borrowed=!source.sourceLines&&!linePacket&&!cachedLines&&validatedBorrow168.Borrow(mapped->viewport,width,height,samples);
  if(!source.sourceLines&&!cachedLines&&!borrowed&&!linePacket)mesh=BuildNativeWorldDrawGeometry(source,mapped->viewport,width,height,samples);
  const auto& meshVertices=borrowed?source.vertices:mesh.vertices;
  const auto& meshIndices=borrowed?source.indices:mesh.indices;
  if(row174){
   row174->meshNS+=WorldGeometryProbe174::Since(meshStart174);
   if(source.sourceLines)++row174->sourceLines;else if(cachedLines)++row174->hit;else if(linePacket)++row174->exact;
   else if(borrowed)++row174->borrow;else ++row174->rebuild;
   row174->meshV+=cachedLines?cachedLines->vertices.length/sizeof(NativeWorldDrawVertex):linePacket?size_t(linePacket->quads)*4:meshVertices.size();
   row174->meshI+=cachedLines?cachedLines->count:linePacket?size_t(linePacket->quads)*6:meshIndices.size();
  }
  EndPreparationScope(geometryCategory91,geometryStart91);CountPreparationTrace(14+geometryCategory91-27,1);
  CountPreparationTrace(19+geometryCategory91-27,source.vertices.size()*sizeof(NativeWorldDrawVertex)+source.indices.size()*sizeof(uint32_t));
  CountPreparationTrace(24+geometryCategory91-27,(cachedLines?cachedLines->vertices.length+cachedLines->indices.length:linePacket?uint64_t(linePacket->quads)*(4*sizeof(NativeWorldDrawVertex)+6*sizeof(uint32_t)):meshVertices.size()*sizeof(NativeWorldDrawVertex)+meshIndices.size()*sizeof(uint32_t)));
  if(!source.sourceLines && !cachedLines && !linePacket && meshIndices.empty())continue;
		const auto& s=source.world;
		if(s.cullEnabled&&s.cull==NativeWorldCull::FrontAndBack&&source.primitive==NativeDrawPrimitive::Triangles)continue;
		const bool stencilEnabled=s.stencilTest&&stencilFormat!=MTLPixelFormatInvalid;
		if(stencilEnabled)Require(stencilFormat==MTLPixelFormatStencil8||stencilFormat==MTLPixelFormatDepth32Float_Stencil8,"Native world stencil requires actual paired attachment");
		PreparedWorldOverlayFrame::Impl::Draw draw;draw.sequence=source.sequence;draw.stage=source.stage;draw.state=s;draw.polygon=source.primitive==NativeDrawPrimitive::Triangles;
		// Mixed polygon modes have already performed original-face culling and
		// retain that winding for stencil, including generated line/point quads.
		draw.polygon=draw.polygon&&s.frontPolygonMode==NativePolygonMode::Fill&&s.backPolygonMode==NativePolygonMode::Fill;
		draw.viewport=mesh.fullViewport?MTLViewport{0,0,double(width),double(height),0,1}:MTLViewport{mapped->viewport[0],mapped->viewport[1],mapped->viewport[2],mapped->viewport[3],0,1};draw.scissor={0,0,width,height};
		if(source.surface.scissor){const auto& sc=*source.surface.scissor;const double sx=double(width)/logicalWidth,sy=double(height)/logicalHeight;
			const auto x0=NSUInteger(std::clamp(std::floor(double(sc.x)*sx),0.,double(width))),x1=NSUInteger(std::clamp(std::ceil((double(sc.x)+sc.width)*sx),0.,double(width)));
			const auto y0=NSUInteger(std::clamp(std::floor((double(logicalHeight)-sc.y-sc.height)*sy),0.,double(height))),y1=NSUInteger(std::clamp(std::ceil((double(logicalHeight)-sc.y)*sy),0.,double(height)));
			const auto left=std::max(draw.scissor.x,x0),top=std::max(draw.scissor.y,y0),right=std::min(draw.scissor.x+draw.scissor.width,x1),bottom=std::min(draw.scissor.y+draw.scissor.height,y1);if(right<=left||bottom<=top)continue;draw.scissor={left,top,right-left,bottom-top};}
		const auto depthImage=[](const auto& image){return image&&!image->incomplete&&(image->format==ScreenTextureFormat::Depth32Float||image->format==ScreenTextureFormat::Depth24Unorm||image->format==ScreenTextureFormat::Depth16Unorm);};
		const unsigned sampledDepthMask=source.surface.splatPainter?10:source.waterOverlay?(source.waterOverlay->fog?9:8):source.losView?7:source.terraformOverlay?6:source.squadHull?5:source.easyFacing?4:unsigned(source.surface.material!=ScreenMaterial::Solid&&depthImage(source.surface.texture))|(unsigned(source.surface.material!=ScreenMaterial::FontCoverage&&depthImage(source.surface.secondary.texture))<<1);
		Require(mesh.pointDisks.empty() || sampledDepthMask<=3,"Point disk coverage requires the generic fixed-function world consumer");
		unsigned combinedMask=0;for(unsigned i=0;i<4;++i)if(s.combinedColorWrite[i])combinedMask|=1u<<i;
		Require(!mapCombinedColor||mapMRTBroadcast,"Optional combined color requires actual Map MRT broadcast");
		auto blend = source.surface.blend;
		if (s.invertColorUnorm8) {
			blend.enabled = true;
			blend.colorEquation = blend.alphaEquation = ScreenBlendEquation::Add;
			blend.sourceColor = ScreenBlendFactor::OneMinusDestinationColor;
			blend.sourceAlpha = ScreenBlendFactor::OneMinusDestinationAlpha;
			blend.destinationColor = blend.destinationAlpha = ScreenBlendFactor::Zero;
		}
		draw.pipeline=source.sourceLines?impl->sourceLines.Pipeline(lease,blend,s.colorWrite,colorFormat,samples,s.alphaToCoverage,sampledDepthMask,validation,shaderMs,mapMRTBroadcast,mapCombinedColor,combinedMask):impl->Pipeline(lease,blend,s.colorWrite,colorFormat,validation,shaderMs,sampledDepthMask,mapMRTBroadcast,s.alphaToCoverage,samples,mapCombinedColor,combinedMask,IsWorldMatrixGPU(source.sourcePrimitive));
		const auto retainExternal=[&](PreparedScreenExternalImage image){draw.externalImages.push_back(std::move(image));};
		const auto depthStart91=StartPreparationScope();
		auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=s.depthTest?Compare(s.depthCompare):MTLCompareFunctionAlways;depth.depthWriteEnabled=s.depthTest&&s.depthWrite;
		if(stencilEnabled)for(bool front:{true,false}){const auto& original=(front||source.sourceLines)?s.frontStencil:s.backStencil;auto face=[MTLStencilDescriptor new];face.stencilCompareFunction=Compare(original.compare);face.stencilFailureOperation=Operation(original.fail);face.depthFailureOperation=Operation(original.depthFail);face.depthStencilPassOperation=Operation(original.pass);face.readMask=original.readMask;face.writeMask=original.writeMask;if(front)depth.frontFaceStencil=face;else depth.backFaceStencil=face;}
		draw.depth=lease.CachedDepthState(depth);Require(draw.depth!=nil,"Cannot create native original depth/stencil state");EndPreparationScope(33,depthStart91);CountPreparationTrace(29,1);
  const bool packedFont=fontBuffer&&IsWorldMatrixGPU(source.sourcePrimitive);
  id<MTLBuffer> vertices=nil;uint64_t vertexAddress=0;
  if(source.sourceLines){/* Original raw inputs are packed after image resolution below. */}
  else if(linePacket){
   const auto& packet=*linePacket;impl->CompileLines(lease,validation,shaderMs);lease.Retain(impl->lineLibrary);lease.Retain(impl->linePipeline);
   const size_t spanOffset=aligned(packet.segments.size()*sizeof(exactWorldLine::Line120Segment));
   const size_t paramsOffset=aligned(spanOffset+packet.spans.size()*sizeof(exactWorldLine::Line120Span));const size_t packetBytes=paramsOffset+sizeof(uint32_t);
   if(!result->impl->fontUploadLifetime){result->impl->fontUploadLifetime=[M4WorldFontUploadLifetime new];lease.Retain(result->impl->fontUploadLifetime);}
   const auto cachedUploadCount=impl->fontUploadPool.CachedCount();auto packetSlot=impl->fontUploadPool.Acquire(lease.Device(),packetBytes);
   auto upload=packetSlot?packetSlot->buffer:[lease.Device() newBufferWithLength:packetBytes options:MTLResourceStorageModeShared];Require(upload&&upload.contents,"Cannot allocate exact world line packet");
   if(packetSlot)result->impl->fontUploadLifetime->lineStorage.push_back(packetSlot);
   std::memcpy(upload.contents,packet.segments.data(),packet.segments.size()*sizeof(exactWorldLine::Line120Segment));std::memcpy(static_cast<uint8_t*>(upload.contents)+spanOffset,packet.spans.data(),packet.spans.size()*sizeof(exactWorldLine::Line120Span));const uint32_t spanCount=uint32_t(packet.spans.size());std::memcpy(static_cast<uint8_t*>(upload.contents)+paramsOffset,&spanCount,sizeof(spanCount));lease.AddAllocation(upload);
   const size_t vertexBytes=size_t(packet.quads)*4*sizeof(NativeWorldDrawVertex),indexBytes=size_t(packet.quads)*6*sizeof(uint32_t);
   vertices=[lease.Device() newBufferWithLength:vertexBytes options:MTLResourceStorageModePrivate];draw.indices=[lease.Device() newBufferWithLength:indexBytes options:MTLResourceStorageModePrivate];Require(vertices&&draw.indices,"Cannot allocate exact world line GPU geometry");lease.AddAllocation(vertices);lease.AddAllocation(draw.indices);vertexAddress=vertices.gpuAddress;draw.count=packet.quads*6;
   if(row174){row174->packet+=packet.segments.size()*sizeof(exactWorldLine::Line120Segment)+packet.spans.size()*sizeof(exactWorldLine::Line120Span)+sizeof(spanCount);row174->privateMesh+=vertexBytes+indexBytes;row174->privateAllocs+=2;}
   result->impl->hasLineJobs=true;
   draw.line=std::make_shared<ExactLineJob>();draw.line->pipeline=impl->linePipeline;draw.line->quads=packet.quads;draw.line->completed=std::make_shared<std::atomic<bool>>(false);
   auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=5;descriptor.initializeBindings=YES;NSError* error=nil;draw.line->arguments=impl->argumentPool.Acquire(lease,descriptor,result->impl->argumentOwners,&error);Require(draw.line->arguments,"Cannot allocate exact world line arguments: "+Error(error));lease.Retain(draw.line->arguments);
   [draw.line->arguments setAddress:upload.gpuAddress atIndex:0];[draw.line->arguments setAddress:upload.gpuAddress+spanOffset atIndex:1];[draw.line->arguments setAddress:vertices.gpuAddress atIndex:2];[draw.line->arguments setAddress:upload.gpuAddress+paramsOffset atIndex:3];[draw.line->arguments setAddress:draw.indices.gpuAddress atIndex:4];
   impl->lineGeometry.StoreGenerated(lease,source,lineOrdinal92,mapped->viewport,width,height,samples,draw.count,vertices,draw.indices,draw.line->completed);
   if(StageProbe124().Enabled()){auto& row=StageProbe124().rows.at(size_t(source.stage));row.draws.fetch_add(1,std::memory_order_relaxed);row.vertices.fetch_add(uint64_t(packet.quads)*4,std::memory_order_relaxed);if(source.reclaimField)row.reclaim.fetch_add(1,std::memory_order_relaxed);}
   if(LineProbe().Enabled()){LineProbe().draws.fetch_add(1,std::memory_order_relaxed);LineProbe().vertices.fetch_add(uint64_t(packet.quads)*4,std::memory_order_relaxed);LineProbe().bytes.fetch_add(packetBytes,std::memory_order_relaxed);LineProbe().allocations.fetch_add(2+unsigned(!packetSlot||impl->fontUploadPool.CachedCount()>cachedUploadCount),std::memory_order_relaxed);}
  }else if(packedFont){
   auto& probe=source.sourcePrimitive==NativeDrawPrimitive::GPUFillTriangles?FillProbe():FontProbe();
   if(probe.Enabled()){probe.draws.fetch_add(1,std::memory_order_relaxed);probe.vertices.fetch_add(meshVertices.size()-WorldMatrixHeaderVertices(source),std::memory_order_relaxed);if(WorldFillTextureGPU(source))probe.uvDraws.fetch_add(1,std::memory_order_relaxed);}
   vertices=fontBuffer;vertexAddress=fontBuffer.gpuAddress+packFont(meshVertices.data(),meshVertices.size()*sizeof(NativeWorldDrawVertex));
   draw.indices=fontBuffer;draw.indexOffset=packFont(meshIndices.data(),meshIndices.size()*sizeof(uint32_t));draw.indexLength=meshIndices.size()*sizeof(uint32_t);draw.count=uint32_t(meshIndices.size());
   if(row174){++row174->sharedPacked;row174->uploadV+=meshVertices.size()*sizeof(NativeWorldDrawVertex);row174->uploadI+=draw.indexLength;}
  }else{
   vertices=cachedLines?cachedLines->vertices:Buffer(lease,meshVertices.data(),meshVertices.size()*sizeof(NativeWorldDrawVertex));vertexAddress=vertices.gpuAddress;
   draw.indices=cachedLines?cachedLines->indices:Buffer(lease,meshIndices.data(),meshIndices.size()*sizeof(uint32_t));
   draw.count=cachedLines?cachedLines->count:uint32_t(meshIndices.size());
   if(row174&&!cachedLines){row174->directAllocs+=2;row174->uploadV+=meshVertices.size()*sizeof(NativeWorldDrawVertex);row174->uploadI+=meshIndices.size()*sizeof(uint32_t);}
   if(cachedLines){lease.AddAllocation(vertices);lease.AddAllocation(draw.indices);}
   else if(!borrowed)impl->lineGeometry.Store(lease,source,lineOrdinal92,mapped->viewport,width,height,samples,mesh,vertices,draw.indices);
  }
		Require(mesh.pointDisks.empty() || mesh.pointDisks.size()==mesh.vertices.size(),"World point metadata extent differs");
		auto pointDisks=mesh.pointDisks.empty()?vertices:Buffer(lease,mesh.pointDisks.data(),mesh.pointDisks.size()*sizeof(std::array<float,4>));
		if(row174&&!mesh.pointDisks.empty()){row174->disks+=mesh.pointDisks.size()*sizeof(std::array<float,4>);++row174->directAllocs;}
		const auto consumer=NativePostprocessConsumer{source.stage==NativeWorldDrawStage::PreDecals?NativePostprocessStage::PreDecals:(source.stage==NativeWorldDrawStage::PreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World),source.sequence,0};
		const auto& surface=source.surface;auto sampled=images.PrepareImage(lease,surface.material==ScreenMaterial::Solid?nullptr:surface.texture,surface.sampler,targetProducers,consumer,retainExternal);
		Uniform uniform{{1,1,0,0},{uint32_t(surface.material),uint32_t(surface.texture&&surface.texture->flipV),uint32_t(surface.alphaTest.enabled),uint32_t(surface.alphaTest.compare)},{surface.alphaTest.reference,float(surface.sampler.wrapS==ScreenWrap::LegacyClamp),float(surface.sampler.wrapT==ScreenWrap::LegacyClamp),float(surface.texture&&surface.texture->incomplete)}};
		uniform.pointRaster[0]=!mesh.pointDisks.empty();uniform.pointRaster[1]=IsWorldMatrixGPU(source.sourcePrimitive);uniform.pointRaster[2]=WorldFillTextureGPU(source);
		if(surface.texture&&!surface.texture->incomplete&&surface.uvMode==ScreenUVMode::Texel){uniform.scale[0]=1.f/surface.texture->width;uniform.scale[1]=1.f/surface.texture->height;}
		const auto& second=surface.secondary;const bool hasSecondary=second.texture&&surface.material!=ScreenMaterial::FontCoverage;
		auto sampled1=images.PrepareImage(lease,hasSecondary?second.texture:nullptr,second.sampler,targetProducers,consumer,retainExternal);
		if(hasSecondary){uniform.scale1={1,1,0,0};uniform.options1={1,uint32_t(second.texture->flipV),0,0};uniform.parameters1={0,float(second.sampler.wrapS==ScreenWrap::LegacyClamp),float(second.sampler.wrapT==ScreenWrap::LegacyClamp),float(second.texture->incomplete)};if(second.uvMode==ScreenUVMode::Texel&&!second.texture->incomplete){Require(second.texture->width&&second.texture->height,"Empty secondary texel-coordinate image");uniform.scale1[0]=1.f/second.texture->width;uniform.scale1[1]=1.f/second.texture->height;}}
  if(source.sourceLines){
   draw.sourceLines=impl->sourceLines.Prepare(lease,source.sourceLines,mapped->viewport,width,height,samples,surface,sampled.texture,sampled.sampler,sampled1.texture,sampled1.sampler,validation,shaderMs);draw.arguments=draw.sourceLines->arguments;draw.count=source.sourceLines->segments.size();
   if(row174){const auto& in=*source.sourceLines;row174->packet+=in.vertices.size()*sizeof(NativeWorldDrawVertex)+in.segments.size()*sizeof(NativeSourceLineSegment133)+in.transforms.size()*sizeof(NativeSourceLineTransform133)+sizeof(SourceLineRaster133)+sizeof(SourceLineSurface133);}
   if(!result->impl->fontUploadLifetime){result->impl->fontUploadLifetime=[M4WorldFontUploadLifetime new];lease.Retain(result->impl->fontUploadLifetime);}
   result->impl->fontUploadLifetime->sourceLines.push_back(draw.sourceLines);result->impl->draws.push_back(std::move(draw));continue;
  }
		PreparedScreenImage losImage;
		if(source.losView){const auto& los=*source.losView;
			std::memcpy(uniform.overlayViewProjInv.data(),los.engine->matrices->bytes.data()+offsetof(UniformMatricesBuffer,cameraViewProjInv),64);
			std::memcpy(uniform.overlayMapSize.data(),los.engine->params->bytes.data()+offsetof(UniformParamsBuffer,mapSize),16);
			std::memcpy(uniform.overlayPlacement.data(),los.engine->params->bytes.data()+offsetof(UniformParamsBuffer,timeInfo),16);
			uniform.losImage={float(los.los->flipV),float(los.losSampler.wrapS==ScreenWrap::LegacyClamp),float(los.losSampler.wrapT==ScreenWrap::LegacyClamp),0};uniform.losRaster={float(height),0,0,0};
			losImage=images.PrepareImage(lease,los.los,los.losSampler,targetProducers,consumer,retainExternal);
		}
		if(source.terraformOverlay){const auto& overlay=*source.terraformOverlay;uniform.overlayPlacement=overlay.placement;uniform.overlayFit=overlay.fit;
			static_assert(offsetof(UniformMatricesBuffer,cameraViewProjInv)==9*64);static_assert(offsetof(UniformParamsBuffer,mapSize)==48);
			std::memcpy(uniform.overlayViewProjInv.data(),overlay.engine->matrices->bytes.data()+offsetof(UniformMatricesBuffer,cameraViewProjInv),64);
			std::memcpy(uniform.overlayMapSize.data(),overlay.engine->params->bytes.data()+offsetof(UniformParamsBuffer,mapSize),16);
		}
		if(source.squadHull){const auto& hull=*source.squadHull;uniform.hullColor=hull.color;uniform.hullStripe=hull.stripe;uniform.hullCentroid=hull.centroidRadius;uniform.hullAnimation=hull.animation;}
		if(source.easyFacing){const auto& arrow=*source.easyFacing;uniform.arrowShape=arrow.shape;uniform.arrowDetail=arrow.detail;uniform.arrowFill=arrow.fill;uniform.arrowOutline=arrow.outline;}
		WaterUniform water{};std::array<PreparedScreenImage,6> waterImages;
  if(source.waterOverlay){const auto& w=*source.waterOverlay;const auto& m=w.engine->matrices->bytes;const auto& p=w.engine->params->bytes;
   std::memcpy(water.viewProj.data(),m.data()+offsetof(UniformMatricesBuffer,cameraViewProj),64);
   std::memcpy(water.viewProjInv.data(),m.data()+offsetof(UniformMatricesBuffer,cameraViewProjInv),64);
   std::memcpy(water.shadowView.data(),m.data()+offsetof(UniformMatricesBuffer,shadowView),64);
   std::memcpy(water.camera.data(),m.data()+offsetof(UniformMatricesBuffer,cameraViewInv)+48,16);
   std::memcpy(water.mapSize.data(),p.data()+offsetof(UniformParamsBuffer,mapSize),16);
   std::memcpy(water.time.data(),p.data()+offsetof(UniformParamsBuffer,timeInfo),16);
   std::memcpy(water.sun.data(),p.data()+offsetof(UniformParamsBuffer,sunDir),16);
   std::memcpy(water.view.data(),p.data()+offsetof(UniformParamsBuffer,viewGeometry),16);
   water.parameters={w.parameters[0],w.parameters[1],w.parameters[2],float(w.acid)};
   water.flags={uint32_t(w.clipZeroToOne),uint32_t(w.clampShadowReference),0,0};
   water.alpha={surface.alphaTest.reference,float(surface.alphaTest.compare),float(surface.alphaTest.enabled),0};
   for(unsigned slot=0;slot<(w.fog?3u:4u);++slot){const auto& t=w.images[slot];waterImages[slot]=images.PrepareImage(lease,t.texture,t.sampler,targetProducers,consumer,retainExternal);water.images[slot]={uint32_t(t.texture->flipV),uint32_t(t.sampler.wrapS==ScreenWrap::LegacyClamp),uint32_t(t.sampler.wrapT==ScreenWrap::LegacyClamp),0};}
   if(!w.fog)for(unsigned n=0;n<2;++n){auto t=images.ResolveOriginalSourceImage(lease,w.typedImages[n],w.typedSamplers[n],validation,shaderMs);
    Require(t.texture&&t.sampler&&t.storageOwner&&t.requireReadable,"Water Overlay typed image lost actual producer ownership");
    const auto& sampler=w.typedImages[n].object->sampling;water.images[4+n]={uint32_t(t.flipV),uint32_t(sampler.wrapS==0x2900),uint32_t(sampler.wrapT==0x2900),0};
    waterImages[4+n]={t.texture,t.sampler};draw.typedImages.push_back(std::move(t));
   }
  }
  id<MTLBuffer> uniforms=nil;uint64_t uniformAddress=0;
  if(packedFont){uniforms=fontBuffer;uniformAddress=fontBuffer.gpuAddress+packFont(&uniform,sizeof(uniform));}
  else{uniforms=source.waterOverlay?Buffer(lease,&water,sizeof(water)):Buffer(lease,&uniform,sizeof(uniform));uniformAddress=uniforms.gpuAddress;}
  if(row174){row174->uniforms+=(!packedFont&&source.waterOverlay)?sizeof(water):sizeof(uniform);row174->directAllocs+=!packedFont;}
  const auto argumentsStart91=StartPreparationScope();auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;descriptor.maxTextureBindCount=source.waterOverlay?6:3;descriptor.maxSamplerStateBindCount=source.waterOverlay?6:3;descriptor.initializeBindings=YES;NSError* error=nil;
		draw.arguments=impl->argumentPool.Acquire(lease,descriptor,result->impl->argumentOwners,&error);Require(draw.arguments!=nil,"Cannot create original world argument table: "+Error(error));[draw.arguments setAddress:vertexAddress atIndex:0];[draw.arguments setAddress:uniformAddress atIndex:1];[draw.arguments setAddress:pointDisks.gpuAddress atIndex:2];[draw.arguments setTexture:sampled.texture.gpuResourceID atIndex:0];[draw.arguments setSamplerState:sampled.sampler.gpuResourceID atIndex:0];[draw.arguments setTexture:sampled1.texture.gpuResourceID atIndex:1];[draw.arguments setSamplerState:sampled1.sampler.gpuResourceID atIndex:1];if(source.losView){[draw.arguments setTexture:losImage.texture.gpuResourceID atIndex:2];[draw.arguments setSamplerState:losImage.sampler.gpuResourceID atIndex:2];}if(source.waterOverlay)for(unsigned n=0;n<(source.waterOverlay->fog?3u:6u);++n){[draw.arguments setTexture:waterImages[n].texture.gpuResourceID atIndex:n];[draw.arguments setSamplerState:waterImages[n].sampler.gpuResourceID atIndex:n];}lease.Retain(draw.arguments);EndPreparationScope(34,argumentsStart91);CountPreparationTrace(30,1);result->impl->draws.push_back(std::move(draw));
	}
 #if defined(RECOIL_METAL4_LIVE_SCENE)
 if(observed174)observed174->Report(frame,samples);
 static bool worldBorrowReceipt168=false;
 if(!worldBorrowReceipt168&&frame.SourceDrawFrame()>=300&&borrowWork168.reusedFullValidations){
  worldBorrowReceipt168=true;const auto& m=borrowWork168;
  LOG("[NativeWorldBorrow168] source_frame=%llu serial=%llu selected_commands=%llu first_validations=%llu borrow_candidates=%llu eliminated_full_validations=%llu typed_revalidations=%llu source_vertex_elements=%llu source_index_elements=%llu source_payload_extent_bytes=%llu scope_bytes=%llu metrics_bytes=%llu owner_gate_calls=%llu scope_heap_allocations=0 preparation_only=1 cross_frame_reuse=0 target_checks_retained=1 timing=0",
   (unsigned long long)frame.SourceDrawFrame(),(unsigned long long)frame.Serial(),(unsigned long long)frame.size(),
   (unsigned long long)m.firstValidations,(unsigned long long)m.borrowCandidates,(unsigned long long)m.reusedFullValidations,
   (unsigned long long)m.typedRevalidations,(unsigned long long)m.vertexElementsCovered,(unsigned long long)m.indexElementsCovered,
   (unsigned long long)m.payloadBytesCovered,(unsigned long long)sizeof(ScopedWorldBorrow168),(unsigned long long)sizeof(WorldBorrowMetrics168),(unsigned long long)m.borrowCandidates);
 }
 #endif
	return result;
}
}
