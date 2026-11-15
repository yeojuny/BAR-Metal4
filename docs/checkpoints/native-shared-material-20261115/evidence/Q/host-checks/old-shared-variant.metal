
#ifndef RECOIL_NATIVE_SAMPLE_SHADING_PRELUDE
#define RECOIL_NATIVE_SAMPLE_SHADING_PRELUDE 1
#define RECOIL_NATIVE_SAMPLE_SHADING_ANNOTATED 1
#include <metal_stdlib>
using namespace metal;
#ifndef RECOIL_NATIVE_SAMPLE_SHADING
#define RECOIL_NATIVE_SAMPLE_SHADING 0
#endif
#if RECOIL_NATIVE_SAMPLE_SHADING
#define RECOIL_SAMPLE_PERSPECTIVE sample_perspective
#define RECOIL_SAMPLE_NOPERSPECTIVE sample_no_perspective
#define RECOIL_SAMPLE_ID_ARGUMENT , uint recoilOriginalSampleID [[sample_id]]
#define RECOIL_SAMPLE_POSITION_ARGUMENT
// Metal's built-in position remains center_no_perspective even for a sample
// function. Its depth and reciprocal-W are screen-linear planes. Evaluate those
// planes at the actual sample; xy uses the hardware sample position explicitly.
inline float4 RecoilOriginalSamplePosition(float4 center, float2 samplePosition) {
	const float2 offset=samplePosition-float2(0.5f);
	float4 result=center+dfdx(center)*offset.x+dfdy(center)*offset.y;
	result.xy=floor(center.xy)+samplePosition;
	return result;
}
#define RECOIL_SAMPLE_FIX_POSITION(value, samplePosition) value=RecoilOriginalSamplePosition(value,samplePosition)
#else
#define RECOIL_SAMPLE_PERSPECTIVE center_perspective
#define RECOIL_SAMPLE_NOPERSPECTIVE center_no_perspective
#define RECOIL_SAMPLE_ID_ARGUMENT
#define RECOIL_SAMPLE_POSITION_ARGUMENT
#define RECOIL_SAMPLE_FIX_POSITION(value, samplePosition)
#endif
#endif

struct SepiaUniforms { float4 rows[3]; float4 target; int4 viewPos; uint4 alpha; float4 parameters; };
struct SepiaVarying {float4 position [[position]];};
vertex SepiaVarying SepiaVertex(uint id [[vertex_id]],constant SepiaUniforms& u [[buffer(1)]]) {
 const float2 vertices[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(vertices[id],u.parameters.z,1)};
}
fragment float4 SepiaFragment(SepiaVarying input [[stage_in]],texture2d<float,access::read> source [[texture(0)]],constant SepiaUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 int2 p=int2(float2(input.position.x*u.target.z,(u.target.y-input.position.y)*u.target.w))-u.viewPos.xy;
 float3 color=pow(source.read(uint2(p.x,int(source.get_height())-1-p.y)).rgb,float3(u.parameters.x));
 float4 result=float4(dot(u.rows[0].xyz,color),dot(u.rows[1].xyz,color),dot(u.rows[2].xyz,color),1.0);
 if(u.alpha.x){float r=clamp(u.parameters.y,0.0f,1.0f);bool pass=false;switch(u.alpha.y){case 0:break;case 1:pass=result.a<r;break;case 2:pass=result.a==r;break;case 3:pass=result.a<=r;break;case 4:pass=result.a>r;break;case 5:pass=result.a!=r;break;case 6:pass=result.a>=r;break;default:pass=true;}if(!pass)discard_fragment();}
 return result;
}
struct SharedSepiaUniforms {float4 target;uint4 alpha;float4 raster;
 int4 viewPos;
 float4 exposure;
 float4 bias;
 float4 gamma;
 float4 colorR;
 float4 colorG;
 float4 colorB;
};
static_assert(sizeof(SharedSepiaUniforms)==160);
static_assert(__builtin_offsetof(SharedSepiaUniforms,viewPos)==48);
static_assert(__builtin_offsetof(SharedSepiaUniforms,exposure)==64);
static_assert(__builtin_offsetof(SharedSepiaUniforms,bias)==80);
static_assert(__builtin_offsetof(SharedSepiaUniforms,gamma)==96);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorR)==112);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorG)==128);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorB)==144);
struct SharedSepiaVarying {float4 position [[position]];};
vertex SharedSepiaVarying SharedSepiaVertex(uint id [[vertex_id]],constant SharedSepiaUniforms& u [[buffer(1)]]) {
 const float2 vertices[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(vertices[id],u.raster.y,1)};
}
float4 pilotFetch(texture2d<float,access::read> source,int2 p){return source.read(uint2(p.x,int(source.get_height())-1-p.y));}
fragment float4 SharedSepiaFragment(SharedSepiaVarying input [[stage_in]],texture2d<float,access::read> screenCopyTex [[texture(0)]],constant SharedSepiaUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 pilotFragCoord=float4(input.position.x*u.target.z,(u.target.y-input.position.y)*u.target.w,input.position.z,input.position.w);
 float4 pilotResult;
 float3 color=((pow((pilotFetch(screenCopyTex,(int2((pilotFragCoord).xy)-u.viewPos.xy))).rgb,float3(u.gamma.x))*u.exposure.x)+u.bias.xyz);
 pilotResult=float4(dot(u.colorR.xyz,color),dot(u.colorG.xyz,color),dot(u.colorB.xyz,color),1.0f);
 if(u.alpha.x){float r=clamp(u.raster.x,0.0f,1.0f);bool pass=false;switch(u.alpha.y){case 0:break;case 1:pass=pilotResult.a<r;break;case 2:pass=pilotResult.a==r;break;case 3:pass=pilotResult.a<=r;break;case 4:pass=pilotResult.a>r;break;case 5:pass=pilotResult.a!=r;break;case 6:pass=pilotResult.a>=r;break;default:pass=true;}if(!pass)discard_fragment();}
 return pilotResult;
}
