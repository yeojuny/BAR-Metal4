/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldOverlayRenderer.h"
#include "NativeSampleShadingShaders.h"
#include "NativeWaterOverlayShaders.h"
#include "Rendering/Backend/NativeWaterOverlayProgram.h"
#include "Rendering/Backend/NativeSplatPainterProgram.h"
#include <tuple>
#include "Metal4UnitEffects.h"
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
namespace rendering::metal4 {
namespace {
const char* SOURCE=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
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
struct V {packed_float4 clip,color;packed_float4 uv;};
struct U {float4 scale;uint4 options;float4 parameters;float4 scale1;uint4 options1;float4 parameters1;float4 arrowShape,arrowDetail,arrowFill,arrowOutline;float4 hullColor,hullStripe,hullCentroid,hullAnimation;float4x4 overlayViewProjInv;float4 overlayPlacement,overlayFit,overlayMapSize;float4 losImage,losRaster;};
struct Out {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float4 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex Out WorldOverlayVertex(uint id [[vertex_id]],device const V* vertices [[buffer(0)]]) {
 const V v=vertices[id];Out o;o.position=float4(v.clip);o.position.z=(o.position.z+o.position.w)*.5f;o.color=float4(v.color);o.uv=float4(v.uv);return o;
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
fragment OUTPUT_TYPE WorldOverlayFragment(Out in [[stage_in]],constant U& u [[buffer(1)]],texture2d<float> image [[texture(0)]],sampler source [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}
fragment OUTPUT_TYPE WorldOverlayFragment1(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> image [[texture(0)]],sampler source [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}
fragment OUTPUT_TYPE WorldOverlayFragment2(Out in [[stage_in]],constant U& u [[buffer(1)]],texture2d<float> image [[texture(0)]],sampler source [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
}
fragment OUTPUT_TYPE WorldOverlayFragment3(Out in [[stage_in]],constant U& u [[buffer(1)]],depth2d<float> image [[texture(0)]],sampler source [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler source1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x && (u.options.x==2 || u.parameters.w==0)){float2 uv=in.uv.xy*u.scale.xy;if(u.options.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters.w)texel=float4(0,0,0,1);else if(u.parameters.y || u.parameters.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image,source,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image,source,uv);
  if(u.options.x==2)result.a*=texel.r;else result*=texel;}
 if(u.options1.x && u.options.x!=2 && u.parameters1.w==0){float2 uv=in.uv.zw*u.scale1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
  if(u.parameters1.w)texel=float4(0,0,0,1);else if(u.parameters1.y||u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,source1,uv,gradient2d(dx,dy));}else texel=SampleOriginalUI(image1,source1,uv);result*=texel;}
 if(u.options.z&&!Alpha(result.a,u.parameters.x,u.options.w))discard_fragment();return OUTPUT_VALUE(result);
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
struct Uniform{std::array<float,4> scale;std::array<uint32_t,4> options;std::array<float,4> parameters;std::array<float,4> scale1;std::array<uint32_t,4> options1;std::array<float,4> parameters1;std::array<float,4> arrowShape,arrowDetail,arrowFill,arrowOutline;std::array<float,4> hullColor,hullStripe,hullCentroid,hullAnimation;std::array<float,16> overlayViewProjInv;std::array<float,4> overlayPlacement,overlayFit,overlayMapSize;std::array<float,4> losImage,losRaster;};
static_assert(sizeof(Uniform)==368);
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
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size)
{
	Require(size&&size<=lease.Device().maxBufferLength,"Native world overlay buffer extent invalid");auto value=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];Require(value!=nil,"Native world overlay allocation failed");lease.AddAllocation(value);return value;
}
}
struct PreparedWorldOverlayFrame::Impl {
	struct Draw {
		uint64_t sequence=0;NativeWorldDrawStage stage=NativeWorldDrawStage::World;
  std::shared_ptr<PreparedUnitEffect> unitEffect;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
		NativeWorldDrawState state;MTLViewport viewport{};MTLScissorRect scissor{};uint32_t count=0;
		bool polygon=false;
		std::vector<PreparedScreenExternalImage> externalImages;
 std::vector<PreparedPiPSourceImage> typedImages;
	};
	std::vector<Draw> draws;
};
PreparedWorldOverlayFrame::PreparedWorldOverlayFrame():impl(std::make_unique<Impl>()){}
PreparedWorldOverlayFrame::~PreparedWorldOverlayFrame()=default;
void PreparedWorldOverlayFrame::EncodeDependencies(id<MTL4CommandBuffer> command)const{for(const auto& d:impl->draws)for(const auto& image:d.typedImages)if(image.encodeDependencies)image.encodeDependencies(command);}
void PreparedWorldOverlayFrame::DidComplete()const{for(const auto& d:impl->draws)for(const auto& image:d.typedImages)if(image.didComplete)image.didComplete();}
size_t PreparedWorldOverlayFrame::DrawCount()const{return impl->draws.size();}
uint64_t PreparedWorldOverlayFrame::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
NativeWorldDrawStage PreparedWorldOverlayFrame::StageAt(size_t i)const{return impl->draws.at(i).stage;}
void PreparedWorldOverlayFrame::EncodeDraw(size_t i,id<MTL4RenderCommandEncoder> encoder)const
{
	const auto& d=impl->draws.at(i);if(d.unitEffect){d.unitEffect->Encode(encoder);return;}if(!d.count)return;
	for(const auto& image:d.externalImages)image.requireReadable();
 for(const auto& image:d.typedImages)image.requireReadable();
	[encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];
	[encoder setViewport:d.viewport];[encoder setScissorRect:d.scissor];[encoder setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setCullMode:d.polygon&&d.state.cullEnabled?(d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack):MTLCullModeNone];
	[encoder setDepthBias:d.polygon&&d.state.polygonOffset?d.state.offsetUnits:0 slopeScale:d.polygon&&d.state.polygonOffset?d.state.offsetFactor:0 clamp:0];
	[encoder setStencilFrontReferenceValue:uint32_t(std::clamp(d.state.frontStencil.reference,0,255)) backReferenceValue:uint32_t(std::clamp(d.state.backStencil.reference,0,255))];
	[encoder setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
}
struct SquadHullCompiledOwner {__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> pipeline=nil;};
struct WorldOverlayGPU::Impl {
 std::shared_ptr<const SquadHullCompiledOwner> squadHullCompiled,terraformOverlayCompiled,losViewCompiled,splatOverlayCompiled;
 std::array<std::shared_ptr<const SquadHullCompiledOwner>,2> waterOverlayCompiled;
 UnitEffectsGPU unitEffects;
	std::array<__strong id<MTLLibrary>,3> libraries{};
	std::map<std::tuple<uint64_t,bool,unsigned,bool,unsigned,unsigned>,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const ScreenBlendState& blend,const std::array<bool,4>& mask,MTLPixelFormat format,bool validation,double& shaderMs,unsigned sampledDepthMask,bool mapMRTBroadcast,bool alphaToCoverage,unsigned samples,bool combined=false,unsigned combinedMask=15)
	{
		const uint64_t key=uint64_t(format)<<32|uint64_t(validation)<<31|uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24|uint64_t(mapMRTBroadcast)<<30;
		const auto pipelineKey=std::make_tuple(key,alphaToCoverage,lease.SampleShadingKey(samples),combined,combinedMask,sampledDepthMask);
		if(auto found=pipelines.find(pipelineKey);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;
		auto& library=libraries[combined?2:mapMRTBroadcast?1:0];
  if(library==nil){auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(SOURCE);auto compile=[MTLCompileOptions new];compile.preprocessorMacros=@{@"MAP_MRT_BROADCAST":@(mapMRTBroadcast),@"MAP_MRT_COMBINED":@(combined)};descriptor.options=compile;descriptor.name=@"Original common Lua world primitives";library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile native world overlay shader: "+Error(error));}
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
void WorldOverlayGPU::ClearResources(){impl->unitEffects.ClearResources();} // Immutable images belong to the shared ScreenGPU; compiled programs remain valid.
std::unique_ptr<PreparedWorldOverlayFrame> WorldOverlayGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawFrame& frame,
	ScreenGPU& images,const PreparedScreenFrame* targetProducers,const WorldCamera& camera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,
	MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& shaderMs,bool mapMRTBroadcast,unsigned samples,bool mapCombinedColor)
{
	Require(!mapMRTBroadcast||(colorFormat==MTLPixelFormatRGBA8Unorm&&stencilFormat==MTLPixelFormatInvalid),"Map generic broadcast requires its actual five-color/no-stencil target");
 Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Native world overlay requires actual scene depth");
	auto result=std::make_unique<PreparedWorldOverlayFrame>();
	uint64_t lastSequence=0;
	for(const auto& source:frame.draws) {
		ValidateNativeWorldDrawCommand(source);Require(source.sequence>lastSequence,"Native world draw ordinal must retain actual call order");lastSequence=source.sequence;
  if(source.unitEffect){Require(!mapMRTBroadcast,"Airjets cannot execute in a terrain MRT callback");PreparedWorldOverlayFrame::Impl::Draw draw;draw.sequence=source.sequence;draw.stage=source.stage;draw.unitEffect=impl->unitEffects.Prepare(lease,source,images,targetProducers,camera,width,height,logicalWidth,logicalHeight,colorFormat,depthFormat,stencilFormat,validation,shaderMs,samples);result->impl->draws.push_back(std::move(draw));continue;}
		if(source.distortion)continue; // Paired command-level original Distortion consumer.
		if(source.objectSpotlight)continue; // Dedicated original instanced-cylinder vertex/fragment consumer.
		if(source.grass||source.resourceSpots||source.sensorLOS)continue; // Actual indexed Grass buffers belong to their paired GPU consumer.
		if(source.reclaimField&&!source.reclaimField->edges)continue; // Consumed by the paired original-buffer fill stage.
		auto rectangle=camera.viewport;if(source.surface.viewport){const auto& v=*source.surface.viewport;Require(v.width<=INT32_MAX && v.height<=INT32_MAX,"Original world viewport exceeds GLsizei");rectangle={v.x,v.y,std::int32_t(v.width),std::int32_t(v.height)};}
		if(rectangle[2]==0 || rectangle[3]==0)continue;
		auto mapped=MapWorldViewport(rectangle,logicalWidth,logicalHeight,width,height);if(!mapped)continue;
		auto mesh=BuildNativeWorldDrawGeometry(source,mapped->viewport,width,height,samples);if(mesh.indices.empty())continue;
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
		unsigned combinedMask=0;for(unsigned i=0;i<4;++i)if(s.combinedColorWrite[i])combinedMask|=1u<<i;
		Require(!mapCombinedColor||mapMRTBroadcast,"Optional combined color requires actual Map MRT broadcast");
		draw.pipeline=impl->Pipeline(lease,source.surface.blend,s.colorWrite,colorFormat,validation,shaderMs,sampledDepthMask,mapMRTBroadcast,s.alphaToCoverage,samples,mapCombinedColor,combinedMask);
		const auto retainExternal=[&](PreparedScreenExternalImage image){draw.externalImages.push_back(std::move(image));};
		auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=s.depthTest?Compare(s.depthCompare):MTLCompareFunctionAlways;depth.depthWriteEnabled=s.depthTest&&s.depthWrite;
		if(stencilEnabled)for(bool front:{true,false}){const auto& original=front?s.frontStencil:s.backStencil;auto face=[MTLStencilDescriptor new];face.stencilCompareFunction=Compare(original.compare);face.stencilFailureOperation=Operation(original.fail);face.depthFailureOperation=Operation(original.depthFail);face.depthStencilPassOperation=Operation(original.pass);face.readMask=original.readMask;face.writeMask=original.writeMask;if(front)depth.frontFaceStencil=face;else depth.backFaceStencil=face;}
		draw.depth=[lease.Device() newDepthStencilStateWithDescriptor:depth];Require(draw.depth!=nil,"Cannot create native original depth/stencil state");lease.Retain(draw.depth);
		auto vertices=Buffer(lease,mesh.vertices.data(),mesh.vertices.size()*sizeof(NativeWorldDrawVertex));draw.indices=Buffer(lease,mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));draw.count=uint32_t(mesh.indices.size());
		const auto consumer=NativePostprocessConsumer{source.stage==NativeWorldDrawStage::PreDecals?NativePostprocessStage::PreDecals:(source.stage==NativeWorldDrawStage::PreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World),source.sequence,0};
		const auto& surface=source.surface;auto sampled=images.PrepareImage(lease,surface.material==ScreenMaterial::Solid?nullptr:surface.texture,surface.sampler,targetProducers,consumer,retainExternal);
		Uniform uniform{{1,1,0,0},{uint32_t(surface.material),uint32_t(surface.texture&&surface.texture->flipV),uint32_t(surface.alphaTest.enabled),uint32_t(surface.alphaTest.compare)},{surface.alphaTest.reference,float(surface.sampler.wrapS==ScreenWrap::LegacyClamp),float(surface.sampler.wrapT==ScreenWrap::LegacyClamp),float(surface.texture&&surface.texture->incomplete)}};
		if(surface.texture&&!surface.texture->incomplete&&surface.uvMode==ScreenUVMode::Texel){uniform.scale[0]=1.f/surface.texture->width;uniform.scale[1]=1.f/surface.texture->height;}
		const auto& second=surface.secondary;const bool hasSecondary=second.texture&&surface.material!=ScreenMaterial::FontCoverage;
		auto sampled1=images.PrepareImage(lease,hasSecondary?second.texture:nullptr,second.sampler,targetProducers,consumer,retainExternal);
		if(hasSecondary){uniform.scale1={1,1,0,0};uniform.options1={1,uint32_t(second.texture->flipV),0,0};uniform.parameters1={0,float(second.sampler.wrapS==ScreenWrap::LegacyClamp),float(second.sampler.wrapT==ScreenWrap::LegacyClamp),float(second.texture->incomplete)};if(second.uvMode==ScreenUVMode::Texel&&!second.texture->incomplete){Require(second.texture->width&&second.texture->height,"Empty secondary texel-coordinate image");uniform.scale1[0]=1.f/second.texture->width;uniform.scale1[1]=1.f/second.texture->height;}}
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
  auto uniforms=source.waterOverlay?Buffer(lease,&water,sizeof(water)):Buffer(lease,&uniform,sizeof(uniform));auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=source.waterOverlay?6:3;descriptor.maxSamplerStateBindCount=source.waterOverlay?6:3;descriptor.initializeBindings=YES;NSError* error=nil;
		draw.arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];Require(draw.arguments!=nil,"Cannot create original world argument table: "+Error(error));[draw.arguments setAddress:vertices.gpuAddress atIndex:0];[draw.arguments setAddress:uniforms.gpuAddress atIndex:1];[draw.arguments setTexture:sampled.texture.gpuResourceID atIndex:0];[draw.arguments setSamplerState:sampled.sampler.gpuResourceID atIndex:0];[draw.arguments setTexture:sampled1.texture.gpuResourceID atIndex:1];[draw.arguments setSamplerState:sampled1.sampler.gpuResourceID atIndex:1];if(source.losView){[draw.arguments setTexture:losImage.texture.gpuResourceID atIndex:2];[draw.arguments setSamplerState:losImage.sampler.gpuResourceID atIndex:2];}if(source.waterOverlay)for(unsigned n=0;n<(source.waterOverlay->fog?3u:6u);++n){[draw.arguments setTexture:waterImages[n].texture.gpuResourceID atIndex:n];[draw.arguments setSamplerState:waterImages[n].sampler.gpuResourceID atIndex:n];}lease.Retain(draw.arguments);result->impl->draws.push_back(std::move(draw));
	}
	return result;
}
}
