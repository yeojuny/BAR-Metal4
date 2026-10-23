/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ScreenNative.h"
#include "NativeSampleShadingShaders.h"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
#include "Rendering/Backend/NativeDeferredVisualizerProgram.h"
#include "Rendering/Backend/NativeDecalCaptureProgram.h"
#include "Rendering/Backend/NativeSplatPainterProgram.h"
#include "NativeSplatPainterShaders.h"
#include "Rendering/Backend/NativeDiffusePainterProgram.h"
#include "NativeDiffusePainterShaders.h"
#include "Rendering/Backend/NativeTerraformCaptureProgram.h"
#include "NativeTerraformCaptureShaders.h"
#include "Metal4UnitPrimitive.h"
#include "Metal4WorldBCDecode.h"
#include "Metal4WorldViewport.h"
#include "Metal4MiniMapNative.h"
#include "Metal4PiPNative.h"
#include "Metal4BuildSquareNative.h"
#include "Metal4HealthBarsShaders.h"
#include "Rendering/Backend/NativeHealthBarsData.h"
#include "Metal4PanelShader.h"
#include "Rendering/Backend/NativeCUSTextureHistory.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <bit>
#include <limits>

// Keep the readback/source owner alive even if encoding throws after Begin
// and the common runtime retains this uncertain submission until teardown.
@interface M4ScreenReadbackLifetime : NSObject {
@public
	std::shared_ptr<const void> retained;
}
@end
@implementation M4ScreenReadbackLifetime
@end

namespace rendering::metal4 {
namespace {
MTLPixelFormat ColorTargetFormat(ScreenTextureFormat format)
{
	switch (format) {
		case ScreenTextureFormat::RG8Unorm: return MTLPixelFormatRG8Unorm;
		case ScreenTextureFormat::RG16Float: return MTLPixelFormatRG16Float;
		case ScreenTextureFormat::RG11B10Float: return MTLPixelFormatRG11B10Float;
		case ScreenTextureFormat::R8Unorm: return MTLPixelFormatR8Unorm;
		case ScreenTextureFormat::R16Float: return MTLPixelFormatR16Float;
		case ScreenTextureFormat::R32Float: return MTLPixelFormatR32Float;
		case ScreenTextureFormat::RGBA8Unorm: return MTLPixelFormatRGBA8Unorm;
		case ScreenTextureFormat::RGBA16Float: return MTLPixelFormatRGBA16Float;
		case ScreenTextureFormat::Depth16Unorm: return MTLPixelFormatDepth16Unorm;
		default: throw std::runtime_error("Native UI RTT has no actual color attachment format");
	}
}
std::uint32_t AttachmentPixelBytes(ScreenTextureFormat format){return format==ScreenTextureFormat::Depth16Unorm?2:ScreenRenderTargetPixelBytes(format);}
struct HealthBarsUniform {
 std::array<float,16> viewProjection,viewInverse;
 std::array<float,4> time,parameters,shape;
 std::array<std::uint32_t,4> options,sampling;
};
static_assert(sizeof(HealthBarsUniform)==208);
const char* SHADER = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ScreenVertexData { packed_float3 position; packed_float4 color; };
struct ScreenUniform { float4 dimensions; uint4 options; float4 parameters; float4 dimensions1; uint4 options1; float4 parameters1; float4x4 captureViewProjection; float4 captureOriginExtent; float4 captureMapFeather; };
struct ScreenVarying { float4 position [[position]]; float4 color [[RECOIL_SAMPLE_PERSPECTIVE]]; float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]]; float2 uv1 [[RECOIL_SAMPLE_PERSPECTIVE]]; };
vertex ScreenVarying ScreenVertexMain(uint vertexId [[vertex_id]], device const ScreenVertexData* vertices [[buffer(0)]],
	device const packed_float4* coords [[buffer(1)]], constant ScreenUniform& u [[buffer(2)]]) {
	const ScreenVertexData item = vertices[vertexId];
	ScreenVarying output;
	output.position = float4(2*item.position.x*u.dimensions.x-1,2*item.position.y*u.dimensions.y-1,(item.position.z+1)*0.5f,1);
	if(u.options.x==9||u.options.x==10||u.options.x==11){const float w=float4(coords[vertexId]).z;output.position=float4(item.position.x,item.position.y,(item.position.z+w)*.5f,w);}
	if(u.options.x==8)output.position=float4(item.position.x,item.position.y,(item.position.z+1.f)*.5f,1);
	if(u.options.x==6)output.position=float4(float3(item.position.x,item.position.y,.5f),1);
 if(u.options.x==7){const float w=float4(coords[vertexId]).x;output.position=float4(item.position.x,item.position.y,(item.position.z+w)*.5f,w);}
	if(u.options.x==4)output.position=float4(float2(item.position.x,item.position.y)/u.dimensions.xy*2.0f-1.0f,.5f,1);
	if(u.options.x==5){
		const float4 t=float4(item.color);const bool sim=abs(t.z-1.0f)<.01f;
		const float gap=min(1.0f,t.y*.35f);
		const float width=max(t.y-gap,sim?4.0f:1.0f)/u.dimensions.x;
		const float height=max(u.dimensions1.y*t.y,sim?10.0f:1.0f)/u.dimensions.y;
		const float right=1.0f-(u.dimensions1.x*1.0f-t.x)/u.dimensions.x;
		output.position=float4(right-item.position.x*width,-1.0f+item.position.y*height,(.5f+.1f*t.z+t.w*.1f+1.0f)*.5f,1);
		if(right<0.0f||t.y>200.0f)output.position.xy=float2(-1.0f);
	}
	output.color = float4(item.color); output.uv = float4(coords[vertexId]).xy;output.uv1=float4(coords[vertexId]).zw; return output;
}
bool AlphaPass(float alpha, float reference, uint comparison) {
	switch (comparison) {
		case 0: return false; case 1: return alpha < reference; case 2: return alpha == reference;
		case 3: return alpha <= reference; case 4: return alpha > reference; case 5: return alpha != reference;
		case 6: return alpha >= reference; case 7: return true;
	}
	return false;
}
float4 SampleOriginalUI(texture2d<float> image,sampler s,float2 uv) { return image.sample(s,uv); }
float4 SampleOriginalUI(texture2d<float> image,sampler s,float2 uv,gradient2d g) { return image.sample(s,uv,g); }
float4 SampleOriginalUI(depth2d<float> image,sampler s,float2 uv) { const float d=image.sample(s,uv);return float4(d,d,d,1); }
float4 SampleOriginalUI(depth2d<float> image,sampler s,float2 uv,gradient2d g) { const float d=image.sample(s,uv,g);return float4(d,d,d,1); }

float4 VisualizerSample(texture2d<float> t,sampler s,float2 uv,uint4 flags){
 if(flags.x)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(flags.y)uv.x=clamp(uv.x,0.f,1.f);if(flags.z)uv.y=clamp(uv.y,0.f,1.f);
 return t.sample(s,uv,gradient2d(dx,dy));
}
float4 VisualizerSample(depth2d<float> t,sampler s,float2 uv,uint4 flags){
 if(flags.x)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(flags.y)uv.x=clamp(uv.x,0.f,1.f);if(flags.z)uv.y=clamp(uv.y,0.f,1.f);
 return float4(t.sample(s,uv,gradient2d(dx,dy)));
}
fragment float4 DeferredVisualizerFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& u [[buffer(2)]],constant uint4* flags [[buffer(3)]],
texture2d<float> image0 [[texture(0)]],sampler sampler0 [[sampler(0)]],
texture2d<float> image1 [[texture(1)]],sampler sampler1 [[sampler(1)]],
texture2d<float> image2 [[texture(2)]],sampler sampler2 [[sampler(2)]],
texture2d<float> image3 [[texture(3)]],sampler sampler3 [[sampler(3)]],
texture2d<float> image4 [[texture(4)]],sampler sampler4 [[sampler(4)]],
depth2d<float> image5 [[texture(5)]],sampler sampler5 [[sampler(5)]],
texture2d<float> image6 [[texture(6)]],sampler sampler6 [[sampler(6)]],
texture2d<float> image7 [[texture(7)]],sampler sampler7 [[sampler(7)]],
texture2d<float> image8 [[texture(8)]],sampler sampler8 [[sampler(8)]],
texture2d<float> image9 [[texture(9)]],sampler sampler9 [[sampler(9)]],
texture2d<float> image10 [[texture(10)]],sampler sampler10 [[sampler(10)]],
depth2d<float> image11 [[texture(11)]],sampler sampler11 [[sampler(11)]],
depth2d<float> image12 [[texture(12)]],sampler sampler12 [[sampler(12)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float2 uv=in.uv;float4 o=float4(0);bool linearDepth=false;
 if(uv.x>.875f){
  if(uv.y>.75f){o=VisualizerSample(image0,sampler0,uv,flags[0]);}
  else if(uv.y>.5f){o=VisualizerSample(image6,sampler6,uv,flags[6]);}
  else if(uv.y>.25f){o=VisualizerSample(image1,sampler1,uv,flags[1]);}
  else{o=VisualizerSample(image7,sampler7,uv,flags[7]);}
 }
 else if(uv.x>.75f){
  if(uv.y>.75f){o=VisualizerSample(image2,sampler2,uv,flags[2]);}
  else if(uv.y>.5f){o=VisualizerSample(image3,sampler3,uv,flags[3]);}
  else if(uv.y>.25f){o=VisualizerSample(image8,sampler8,uv,flags[8]);}
  else{o=VisualizerSample(image9,sampler9,uv,flags[9]);}
 }
 else if(uv.x>.625f){
  if(uv.y>.75f){o=VisualizerSample(image12,sampler12,uv,flags[12]);}
  else if(uv.y>.5f){o=VisualizerSample(image12,sampler12,uv,flags[12]);linearDepth=true;}
  else if(uv.y>.25f){o=VisualizerSample(image4,sampler4,uv,flags[4]);}
  else{o=VisualizerSample(image10,sampler10,uv,flags[10]);}
 }
 else {
  if(uv.y>.75f){o=VisualizerSample(image5,sampler5,uv,flags[5]);}
  else if(uv.y>.5f){o=VisualizerSample(image11,sampler11,uv,flags[11]);}
  else if(uv.y>.25f){o=VisualizerSample(image5,sampler5,uv,flags[5]);linearDepth=true;}
  else{o=VisualizerSample(image11,sampler11,uv,flags[11]);linearDepth=true;}
 }
 if(linearDepth)o=fract(.2f*(1.f/(1.f-o)));o.a=1.f;
 if(u.options.z&&!AlphaPass(o.a,u.parameters.x,u.options.w))discard_fragment();return o;
}

fragment float4 ScreenFragmentMain(ScreenVarying input [[stage_in]], constant ScreenUniform& u [[buffer(2)]],
	texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler imageSampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output = input.color;
	if(u.options.x==4)output.a*=1.0f-smoothstep(.5f,1.0f,abs(input.uv.x));
	if(u.options.x==5){
		const float4 t=input.color;float green=clamp(t.y/16.6f,.5f,1.0f),red=clamp((t.y-16.6f)/16.6f,0.0f,1.0f);
		if(t.y>16.6f)green=clamp(1.0f-(t.y-16.6f)/16.6f,0.0f,1.0f);
		output=float4(red,green,0,1);if(t.z>.5f)output=float4(0,0,1,1);
		if(t.w>2.0f)output=float4(1,0,1,1.1f);else output.rgb=mix(output.rgb,float3(0),t.w);
		if(abs(t.z-1.0f)<.01f)output=float4(1,0,0,1);
		else if(abs(t.z-2.0f)<.01f)output=float4(1,1,0,1);
		else if(abs(t.z-3.0f)<.01f)output=float4(0,1,0,1);
		else if(abs(t.z-4.0f)<.01f)output=float4(0,0,1,1);
		else output=float4(1,0,1,1);
	}
	if(u.options.x==8){
		const float2 wxz=u.captureOriginExtent.xy+input.uv*u.captureOriginExtent.zw;
		float2 huv=clamp(wxz/u.captureMapFeather.xy,0.f,1.f);if(u.options1.y)huv.y=1.f-huv.y;
		const float wy=SampleOriginalUI(image1,imageSampler1,huv).x;
		const float4 clip=u.captureViewProjection*float4(wxz.x,wy,wxz.y,1);
		output=float4(0);
		if(clip.w>0.f){
			float2 suv=clip.xy/clip.w*.5f+.5f;
			if(all(suv>=0.f)&&all(suv<=1.f)&&all(wxz>=0.f)&&all(wxz<=u.captureMapFeather.xy)){
				if(u.options.y)suv.y=1.f-suv.y;
				const float3 rgb=SampleOriginalUI(image,imageSampler,suv).rgb;
				output=float4(rgb,1.f-smoothstep(u.captureMapFeather.z,1.f,length(input.uv*2.f-1.f)));
			}
		}
	}
	if (u.options.x != 0 && u.options.x < 4 && (u.options.x == 2 || u.parameters.w == 0)) {
		float2 uv = input.uv*u.dimensions.zw;
		if (u.options.y) uv.y = 1-uv.y;
		float4 texel;
		if (u.parameters.w != 0) texel=float4(0,0,0,1);
		else if (u.parameters.y != 0 || u.parameters.z != 0) {
			// GL derivatives belong to the original normalized coordinates,
			// before wrapping/clamping, including the real atlas flip/texel scale.
			const float2 dx=dfdx(uv),dy=dfdy(uv);
			if (u.parameters.y != 0) uv.x=clamp(uv.x,0.0f,1.0f);
			if (u.parameters.z != 0) uv.y=clamp(uv.y,0.0f,1.0f);
			texel=SampleOriginalUI(image,imageSampler,uv,gradient2d(dx,dy));
		} else texel=SampleOriginalUI(image,imageSampler,uv);
		if (u.options.x == 2) output.a *= texel.r;
		else output *= texel;
	}
	if (u.options1.x && u.options.x!=2 && u.options.x<4 && u.parameters1.w==0) {
		float2 uv=input.uv1*u.dimensions1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
		if(u.parameters1.w)texel=float4(0,0,0,1);
		else if(u.parameters1.y || u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,imageSampler1,uv,gradient2d(dx,dy));}
		else texel=SampleOriginalUI(image1,imageSampler1,uv);
		output*=texel;
	}
	if (u.options.z && !AlphaPass(output.a,u.parameters.x,u.options.w)) discard_fragment();
	return output;
}
fragment float4 ScreenFragmentMain1(ScreenVarying input [[stage_in]], constant ScreenUniform& u [[buffer(2)]],
	depth2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]],texture2d<float> image1 [[texture(1)]],sampler imageSampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output = input.color;
	if(u.options.x==4)output.a*=1.0f-smoothstep(.5f,1.0f,abs(input.uv.x));
	if(u.options.x==5){
		const float4 t=input.color;float green=clamp(t.y/16.6f,.5f,1.0f),red=clamp((t.y-16.6f)/16.6f,0.0f,1.0f);
		if(t.y>16.6f)green=clamp(1.0f-(t.y-16.6f)/16.6f,0.0f,1.0f);
		output=float4(red,green,0,1);if(t.z>.5f)output=float4(0,0,1,1);
		if(t.w>2.0f)output=float4(1,0,1,1.1f);else output.rgb=mix(output.rgb,float3(0),t.w);
		if(abs(t.z-1.0f)<.01f)output=float4(1,0,0,1);
		else if(abs(t.z-2.0f)<.01f)output=float4(1,1,0,1);
		else if(abs(t.z-3.0f)<.01f)output=float4(0,1,0,1);
		else if(abs(t.z-4.0f)<.01f)output=float4(0,0,1,1);
		else output=float4(1,0,1,1);
	}
	if(u.options.x==8){
		const float2 wxz=u.captureOriginExtent.xy+input.uv*u.captureOriginExtent.zw;
		float2 huv=clamp(wxz/u.captureMapFeather.xy,0.f,1.f);if(u.options1.y)huv.y=1.f-huv.y;
		const float wy=SampleOriginalUI(image1,imageSampler1,huv).x;
		const float4 clip=u.captureViewProjection*float4(wxz.x,wy,wxz.y,1);
		output=float4(0);
		if(clip.w>0.f){
			float2 suv=clip.xy/clip.w*.5f+.5f;
			if(all(suv>=0.f)&&all(suv<=1.f)&&all(wxz>=0.f)&&all(wxz<=u.captureMapFeather.xy)){
				if(u.options.y)suv.y=1.f-suv.y;
				const float3 rgb=SampleOriginalUI(image,imageSampler,suv).rgb;
				output=float4(rgb,1.f-smoothstep(u.captureMapFeather.z,1.f,length(input.uv*2.f-1.f)));
			}
		}
	}
	if (u.options.x != 0 && u.options.x < 4 && (u.options.x == 2 || u.parameters.w == 0)) {
		float2 uv = input.uv*u.dimensions.zw;
		if (u.options.y) uv.y = 1-uv.y;
		float4 texel;
		if (u.parameters.w != 0) texel=float4(0,0,0,1);
		else if (u.parameters.y != 0 || u.parameters.z != 0) {
			// GL derivatives belong to the original normalized coordinates,
			// before wrapping/clamping, including the real atlas flip/texel scale.
			const float2 dx=dfdx(uv),dy=dfdy(uv);
			if (u.parameters.y != 0) uv.x=clamp(uv.x,0.0f,1.0f);
			if (u.parameters.z != 0) uv.y=clamp(uv.y,0.0f,1.0f);
			texel=SampleOriginalUI(image,imageSampler,uv,gradient2d(dx,dy));
		} else texel=SampleOriginalUI(image,imageSampler,uv);
		if (u.options.x == 2) output.a *= texel.r;
		else output *= texel;
	}
	if (u.options1.x && u.options.x!=2 && u.options.x<4 && u.parameters1.w==0) {
		float2 uv=input.uv1*u.dimensions1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
		if(u.parameters1.w)texel=float4(0,0,0,1);
		else if(u.parameters1.y || u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,imageSampler1,uv,gradient2d(dx,dy));}
		else texel=SampleOriginalUI(image1,imageSampler1,uv);
		output*=texel;
	}
	if (u.options.z && !AlphaPass(output.a,u.parameters.x,u.options.w)) discard_fragment();
	return output;
}
fragment float4 ScreenFragmentMain2(ScreenVarying input [[stage_in]], constant ScreenUniform& u [[buffer(2)]],
	texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler imageSampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output = input.color;
	if(u.options.x==4)output.a*=1.0f-smoothstep(.5f,1.0f,abs(input.uv.x));
	if(u.options.x==5){
		const float4 t=input.color;float green=clamp(t.y/16.6f,.5f,1.0f),red=clamp((t.y-16.6f)/16.6f,0.0f,1.0f);
		if(t.y>16.6f)green=clamp(1.0f-(t.y-16.6f)/16.6f,0.0f,1.0f);
		output=float4(red,green,0,1);if(t.z>.5f)output=float4(0,0,1,1);
		if(t.w>2.0f)output=float4(1,0,1,1.1f);else output.rgb=mix(output.rgb,float3(0),t.w);
		if(abs(t.z-1.0f)<.01f)output=float4(1,0,0,1);
		else if(abs(t.z-2.0f)<.01f)output=float4(1,1,0,1);
		else if(abs(t.z-3.0f)<.01f)output=float4(0,1,0,1);
		else if(abs(t.z-4.0f)<.01f)output=float4(0,0,1,1);
		else output=float4(1,0,1,1);
	}
	if(u.options.x==8){
		const float2 wxz=u.captureOriginExtent.xy+input.uv*u.captureOriginExtent.zw;
		float2 huv=clamp(wxz/u.captureMapFeather.xy,0.f,1.f);if(u.options1.y)huv.y=1.f-huv.y;
		const float wy=SampleOriginalUI(image1,imageSampler1,huv).x;
		const float4 clip=u.captureViewProjection*float4(wxz.x,wy,wxz.y,1);
		output=float4(0);
		if(clip.w>0.f){
			float2 suv=clip.xy/clip.w*.5f+.5f;
			if(all(suv>=0.f)&&all(suv<=1.f)&&all(wxz>=0.f)&&all(wxz<=u.captureMapFeather.xy)){
				if(u.options.y)suv.y=1.f-suv.y;
				const float3 rgb=SampleOriginalUI(image,imageSampler,suv).rgb;
				output=float4(rgb,1.f-smoothstep(u.captureMapFeather.z,1.f,length(input.uv*2.f-1.f)));
			}
		}
	}
	if (u.options.x != 0 && u.options.x < 4 && (u.options.x == 2 || u.parameters.w == 0)) {
		float2 uv = input.uv*u.dimensions.zw;
		if (u.options.y) uv.y = 1-uv.y;
		float4 texel;
		if (u.parameters.w != 0) texel=float4(0,0,0,1);
		else if (u.parameters.y != 0 || u.parameters.z != 0) {
			// GL derivatives belong to the original normalized coordinates,
			// before wrapping/clamping, including the real atlas flip/texel scale.
			const float2 dx=dfdx(uv),dy=dfdy(uv);
			if (u.parameters.y != 0) uv.x=clamp(uv.x,0.0f,1.0f);
			if (u.parameters.z != 0) uv.y=clamp(uv.y,0.0f,1.0f);
			texel=SampleOriginalUI(image,imageSampler,uv,gradient2d(dx,dy));
		} else texel=SampleOriginalUI(image,imageSampler,uv);
		if (u.options.x == 2) output.a *= texel.r;
		else output *= texel;
	}
	if (u.options1.x && u.options.x!=2 && u.options.x<4 && u.parameters1.w==0) {
		float2 uv=input.uv1*u.dimensions1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
		if(u.parameters1.w)texel=float4(0,0,0,1);
		else if(u.parameters1.y || u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,imageSampler1,uv,gradient2d(dx,dy));}
		else texel=SampleOriginalUI(image1,imageSampler1,uv);
		output*=texel;
	}
	if (u.options.z && !AlphaPass(output.a,u.parameters.x,u.options.w)) discard_fragment();
	return output;
}
fragment float4 ScreenFragmentMain3(ScreenVarying input [[stage_in]], constant ScreenUniform& u [[buffer(2)]],
	depth2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]],depth2d<float> image1 [[texture(1)]],sampler imageSampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output = input.color;
	if(u.options.x==4)output.a*=1.0f-smoothstep(.5f,1.0f,abs(input.uv.x));
	if(u.options.x==5){
		const float4 t=input.color;float green=clamp(t.y/16.6f,.5f,1.0f),red=clamp((t.y-16.6f)/16.6f,0.0f,1.0f);
		if(t.y>16.6f)green=clamp(1.0f-(t.y-16.6f)/16.6f,0.0f,1.0f);
		output=float4(red,green,0,1);if(t.z>.5f)output=float4(0,0,1,1);
		if(t.w>2.0f)output=float4(1,0,1,1.1f);else output.rgb=mix(output.rgb,float3(0),t.w);
		if(abs(t.z-1.0f)<.01f)output=float4(1,0,0,1);
		else if(abs(t.z-2.0f)<.01f)output=float4(1,1,0,1);
		else if(abs(t.z-3.0f)<.01f)output=float4(0,1,0,1);
		else if(abs(t.z-4.0f)<.01f)output=float4(0,0,1,1);
		else output=float4(1,0,1,1);
	}
	if(u.options.x==8){
		const float2 wxz=u.captureOriginExtent.xy+input.uv*u.captureOriginExtent.zw;
		float2 huv=clamp(wxz/u.captureMapFeather.xy,0.f,1.f);if(u.options1.y)huv.y=1.f-huv.y;
		const float wy=SampleOriginalUI(image1,imageSampler1,huv).x;
		const float4 clip=u.captureViewProjection*float4(wxz.x,wy,wxz.y,1);
		output=float4(0);
		if(clip.w>0.f){
			float2 suv=clip.xy/clip.w*.5f+.5f;
			if(all(suv>=0.f)&&all(suv<=1.f)&&all(wxz>=0.f)&&all(wxz<=u.captureMapFeather.xy)){
				if(u.options.y)suv.y=1.f-suv.y;
				const float3 rgb=SampleOriginalUI(image,imageSampler,suv).rgb;
				output=float4(rgb,1.f-smoothstep(u.captureMapFeather.z,1.f,length(input.uv*2.f-1.f)));
			}
		}
	}
	if (u.options.x != 0 && u.options.x < 4 && (u.options.x == 2 || u.parameters.w == 0)) {
		float2 uv = input.uv*u.dimensions.zw;
		if (u.options.y) uv.y = 1-uv.y;
		float4 texel;
		if (u.parameters.w != 0) texel=float4(0,0,0,1);
		else if (u.parameters.y != 0 || u.parameters.z != 0) {
			// GL derivatives belong to the original normalized coordinates,
			// before wrapping/clamping, including the real atlas flip/texel scale.
			const float2 dx=dfdx(uv),dy=dfdy(uv);
			if (u.parameters.y != 0) uv.x=clamp(uv.x,0.0f,1.0f);
			if (u.parameters.z != 0) uv.y=clamp(uv.y,0.0f,1.0f);
			texel=SampleOriginalUI(image,imageSampler,uv,gradient2d(dx,dy));
		} else texel=SampleOriginalUI(image,imageSampler,uv);
		if (u.options.x == 2) output.a *= texel.r;
		else output *= texel;
	}
	if (u.options1.x && u.options.x!=2 && u.options.x<4 && u.parameters1.w==0) {
		float2 uv=input.uv1*u.dimensions1.xy;if(u.options1.y)uv.y=1-uv.y;float4 texel;
		if(u.parameters1.w)texel=float4(0,0,0,1);
		else if(u.parameters1.y || u.parameters1.z){const float2 dx=dfdx(uv),dy=dfdy(uv);if(u.parameters1.y)uv.x=clamp(uv.x,0.f,1.f);if(u.parameters1.z)uv.y=clamp(uv.y,0.f,1.f);texel=SampleOriginalUI(image1,imageSampler1,uv,gradient2d(dx,dy));}
		else texel=SampleOriginalUI(image1,imageSampler1,uv);
		output*=texel;
	}
	if (u.options.z && !AlphaPass(output.a,u.parameters.x,u.options.w)) discard_fragment();
	return output;
}
)MSL";
struct Uniform { std::array<float,4> dimensions; std::array<uint32_t,4> options; std::array<float,4> parameters;std::array<float,4> dimensions1;std::array<uint32_t,4> options1;std::array<float,4> parameters1; NativeDecalCaptureUniforms capture; };
static_assert(sizeof(Uniform)==192 && sizeof(Uniform)<=256);
static_assert(offsetof(Uniform,capture)==96);
MTLBlendFactor Factor(ScreenBlendFactor value)
{
	switch(value) {
		case ScreenBlendFactor::Zero:return MTLBlendFactorZero; case ScreenBlendFactor::One:return MTLBlendFactorOne;
		case ScreenBlendFactor::SourceAlpha:return MTLBlendFactorSourceAlpha;
		case ScreenBlendFactor::OneMinusSourceAlpha:return MTLBlendFactorOneMinusSourceAlpha;
		case ScreenBlendFactor::DestinationAlpha:return MTLBlendFactorDestinationAlpha;
		case ScreenBlendFactor::OneMinusDestinationAlpha:return MTLBlendFactorOneMinusDestinationAlpha;
		case ScreenBlendFactor::SourceColor:return MTLBlendFactorSourceColor;
		case ScreenBlendFactor::OneMinusSourceColor:return MTLBlendFactorOneMinusSourceColor;
		case ScreenBlendFactor::DestinationColor:return MTLBlendFactorDestinationColor;
		case ScreenBlendFactor::OneMinusDestinationColor:return MTLBlendFactorOneMinusDestinationColor;
		case ScreenBlendFactor::SourceAlphaSaturate:return MTLBlendFactorSourceAlphaSaturated;
	}
	throw std::runtime_error("Unsupported native UI blend factor");
}
MTLBlendOperation Equation(ScreenBlendEquation value) {
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Unsupported original blend equation");
}
MTLCompareFunction Compare(ScreenAlphaCompare value)
{
	switch(value) {
		case ScreenAlphaCompare::Never:return MTLCompareFunctionNever;case ScreenAlphaCompare::Less:return MTLCompareFunctionLess;
		case ScreenAlphaCompare::Equal:return MTLCompareFunctionEqual;case ScreenAlphaCompare::LessEqual:return MTLCompareFunctionLessEqual;
		case ScreenAlphaCompare::Greater:return MTLCompareFunctionGreater;case ScreenAlphaCompare::NotEqual:return MTLCompareFunctionNotEqual;
		case ScreenAlphaCompare::GreaterEqual:return MTLCompareFunctionGreaterEqual;case ScreenAlphaCompare::Always:return MTLCompareFunctionAlways;
	}
	throw std::runtime_error("Unsupported original screen depth/stencil comparison");
}
MTLStencilOperation Operation(NativeWorldStencilOp value)
{
	switch(value) {
		case NativeWorldStencilOp::Keep:return MTLStencilOperationKeep;case NativeWorldStencilOp::Zero:return MTLStencilOperationZero;
		case NativeWorldStencilOp::Replace:return MTLStencilOperationReplace;case NativeWorldStencilOp::IncrementClamp:return MTLStencilOperationIncrementClamp;
		case NativeWorldStencilOp::DecrementClamp:return MTLStencilOperationDecrementClamp;case NativeWorldStencilOp::Invert:return MTLStencilOperationInvert;
		case NativeWorldStencilOp::IncrementWrap:return MTLStencilOperationIncrementWrap;case NativeWorldStencilOp::DecrementWrap:return MTLStencilOperationDecrementWrap;
	}
	throw std::runtime_error("Unsupported original screen stencil operation");
}
uint32_t BlendKey(const ScreenBlendState& s)
{
	Factor(s.sourceColor);Factor(s.destinationColor);Factor(s.sourceAlpha);Factor(s.destinationAlpha);
	Equation(s.colorEquation);Equation(s.alphaEquation);
	return uint32_t(s.enabled)|(uint32_t(s.sourceColor)<<1)|(uint32_t(s.destinationColor)<<5)|(uint32_t(s.sourceAlpha)<<9)|(uint32_t(s.destinationAlpha)<<13)|(uint32_t(s.colorEquation)<<17)|(uint32_t(s.alphaEquation)<<20);
}
MTLSamplerAddressMode Wrap(ScreenWrap mode,ScreenFilter filter)
{
	switch(mode) {
		case ScreenWrap::ClampEdge:return MTLSamplerAddressModeClampToEdge;
		case ScreenWrap::TransparentBorder:return MTLSamplerAddressModeClampToBorderColor;
		case ScreenWrap::Repeat:return MTLSamplerAddressModeRepeat;
		// GL_CLAMP nearest clamps texel indices to the interior; linear
		// includes the default border's half tap at normalized0 or1.
		case ScreenWrap::LegacyClamp:return filter==ScreenFilter::Nearest?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeClampToBorderColor;
	}
	throw std::runtime_error("Unsupported native UI sampler wrap");
}
id<MTLBuffer> Buffer(RuntimeLease& lease, const void* bytes, size_t length)
{
	Require(length && length<=lease.Device().maxBufferLength,"Native UI buffer exceeds device bounds");
	id<MTLBuffer> result=[lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate native UI buffer");lease.AddAllocation(result);return result;
}
std::optional<MTLScissorRect> Scissor(const std::optional<ScreenScissor>& source, const std::array<double,4>& viewport,
	uint32_t basisWidth,uint32_t basisHeight,uint32_t width,uint32_t height)
{
	const double bottom=double(height)-viewport[1]-viewport[3];
	double left=0,right=double(width),low=0,top=double(height);
	if(source) {
		const double sx=viewport[2]/basisWidth,sy=viewport[3]/basisHeight;
		left=viewport[0]+double(source->x)*sx;right=viewport[0]+(double(source->x)+source->width)*sx;
		low=bottom+double(source->y)*sy;top=bottom+(double(source->y)+source->height)*sy;
	}
	left=std::clamp(left,0.0,double(width));right=std::clamp(right,0.0,double(width));
	low=std::clamp(low,0.0,double(height));top=std::clamp(top,0.0,double(height));
	if(right<=left||top<=low)return std::nullopt;
	const auto x=NSUInteger(std::floor(left)), y=NSUInteger(std::floor(double(height)-top));
	return MTLScissorRect{x,y,NSUInteger(std::ceil(right))-x,NSUInteger(std::ceil(double(height)-low))-y};
}
} // namespace
namespace {
struct ScreenMipState { bool initialized=false, queued=false; };
struct ScreenMipGeneration {
	std::shared_ptr<const ScreenTexture> owner;
	__strong id<MTLTexture> image=nil;
	std::shared_ptr<ScreenMipState> state;
};
}
struct PreparedScreenMipmaps::Impl {
	std::vector<ScreenMipGeneration> images;
	mutable bool encoded=false;
};
PreparedScreenMipmaps::PreparedScreenMipmaps():impl(std::make_unique<Impl>()){}
PreparedScreenMipmaps::~PreparedScreenMipmaps()=default;
std::uint32_t PreparedScreenMipmaps::TextureCount() const { return std::uint32_t(impl->images.size()); }
void PreparedScreenMipmaps::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	Require(!impl->encoded,"Original image mip plan was encoded twice");
	if (!impl->images.empty()) {
		auto encoder=[command computeCommandEncoder];
		Require(encoder!=nil,"Cannot create original named-image mip encoder");
		encoder.label=@"Original named-image immutable GPU mip chain";
		[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit|MTLStageDispatch beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		for(const auto& image:impl->images) [encoder generateMipmapsForTexture:image.image];
		[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder endEncoding];
	}
	impl->encoded=true;
}
void PreparedScreenMipmaps::DidComplete() const
{
	Require(impl->encoded,"Original named-image mip plan has no encoded GPU producer");
	for(const auto& image:impl->images) image.state->initialized=true;
}
struct PreparedScreenFrame::Impl {
	struct Draw {
		__strong id<MTLRenderPipelineState> pipeline=nil; __strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;MTLViewport viewport{};MTLScissorRect scissor{};
		uint32_t frontReference=0,backReference=0,first=0,count=0,originalCommand=0;
		std::unique_ptr<PreparedPiPDraw> pip;std::shared_ptr<const NativePiPDraw> pipOwner;
		std::unique_ptr<PreparedStartBoxDraw> startBox;
		std::unique_ptr<PreparedWeaponRangeDraw> weaponRange;
		std::unique_ptr<PreparedBuildSquareDraw> buildSquare;
		std::unique_ptr<PreparedSensorPreviewCommand> sensorPreview;
  std::unique_ptr<PreparedSensorLOSDraw> sensorLOS;
  std::unique_ptr<PreparedUnitPrimitive> unitIcons;
		std::shared_ptr<const NativeSensorPreviewDraw> sensorPreviewOwner;
		mutable uint32_t startBoxEncoded=0,startBoxCompleted=0;
		std::vector<PreparedScreenExternalImage> externalImages;
		mutable uint32_t pipEncoded=0,pipCompleted=0;
		ScreenCull cull=ScreenCull::None;
		bool healthBars=false;std::uint32_t healthBarsInstances=0;
		std::unique_ptr<PreparedPanelShaderDraw> panelShader;
		std::shared_ptr<const NativePanelShaderDraw> panelShaderOwner;
	};
	__strong id<MTLBuffer> indices=nil;
	MTLViewport viewport{};
	std::vector<Draw> draws;
	uint32_t uploads=0,sourceCommandCount=0;
	struct TargetPass {
		__strong id<MTLTexture> previous = nil;
		__strong MTL4RenderPassDescriptor* descriptor = nil;
		std::unique_ptr<PreparedScreenFrame> frame;
		std::shared_ptr<bool> initialized;
		bool stored = true;
		bool sourceDependent=false;std::uint32_t offset=0;std::uint64_t sequence=0;mutable bool encoded=false;
		NativePostprocessStage sourceStage=NativePostprocessStage::Screen;std::uint64_t sourceDrawFrame=0;
		std::shared_ptr<bool> encodedVersion;
		std::function<void()> requirePreviousReadable;
		__strong id<MTLTexture> previousDepth=nil;
		std::shared_ptr<bool> depthInitialized,depthEncoded;
		std::function<void()> requirePreviousDepthReadable;
	};
	std::vector<TargetPass> targets;
	std::vector<std::shared_ptr<const ScreenTexture>> producedImages;
	std::vector<std::shared_ptr<PreparedMiniMap>> minimaps;
	std::function<PreparedScreenExternalImage(RuntimeLease&,const std::shared_ptr<const ScreenTexture>&,NativePostprocessConsumer)> resolvePrevious;
};
PreparedScreenFrame::PreparedScreenFrame():impl(std::make_unique<Impl>()){}
PreparedScreenFrame::~PreparedScreenFrame()=default;
PreparedScreenExternalImage PreparedScreenFrame::ResolvePreviousImage(RuntimeLease& lease,
	const std::shared_ptr<const ScreenTexture>& source,NativePostprocessConsumer consumer)const
{
	Require(bool(impl->resolvePrevious),"Prior panel image has no actual ScreenGPU producer resolver");
	return impl->resolvePrevious(lease,source,consumer);
}
uint32_t PreparedScreenFrame::DrawCount()const{uint32_t count=uint32_t(impl->draws.size());for(const auto& target:impl->targets)count+=target.frame->DrawCount();return count;}
uint32_t PreparedScreenFrame::DrawCountFrom(std::uint32_t first)const
{
	Require(first<=impl->sourceCommandCount,"Screen draw count suffix starts outside the actual command stream");
	uint32_t count=0;for(const auto& draw:impl->draws)if(draw.originalCommand>=first)++count;
	for(const auto& target:impl->targets)if(target.encoded)count+=target.frame->DrawCount();return count;
}
uint32_t PreparedScreenFrame::TextureUploads()const{return impl->uploads;}
uint32_t PreparedScreenFrame::PiPDrawsCompleted()const
{
	uint64_t count=0;for(const auto& draw:impl->draws)count+=draw.pipCompleted;
	for(const auto& target:impl->targets)if(target.encoded)count+=target.frame->PiPDrawsCompleted();
	Require(count<=UINT32_MAX,"Completed PiP packet count exceeds bounded frame metrics");return uint32_t(count);
}
uint32_t PreparedScreenFrame::StartBoxDrawsCompleted()const
{
	uint64_t count=0;for(const auto& draw:impl->draws)count+=draw.startBoxCompleted;
	for(const auto& target:impl->targets)if(target.encoded)count+=target.frame->StartBoxDrawsCompleted();
	Require(count<=UINT32_MAX,"Completed original StartBox draw count overflow");return uint32_t(count);
}
void PreparedScreenFrame::EncodeNonTargetDependencies(id<MTL4CommandBuffer> command) const
{
	for(const auto& map:impl->minimaps)map->EncodeDependencies(command);
	for(const auto& draw:impl->draws)if(draw.pip)draw.pip->EncodeDependencies(command);
	for(const auto& draw:impl->draws)if(draw.sensorPreview)draw.sensorPreview->EncodeDependencies(command);
}
void PreparedScreenFrame::EncodeDependencies(id<MTL4CommandBuffer> command) const
{
	EncodeNonTargetDependencies(command);
	for(std::size_t i=0;i<impl->targets.size();++i)if(!impl->targets[i].sourceDependent && impl->targets[i].sourceStage==NativePostprocessStage::Screen)EncodeDeferredTarget(command,i);
}
void PreparedScreenFrame::EncodeIndependentTargetClosure(id<MTL4CommandBuffer> command) const
{
	Require(command!=nil && impl->sourceCommandCount==0,
		"Independent SaveImage closure must not execute a root screen stream");
	for(const auto& target:impl->targets)
		Require(!target.sourceDependent && !target.encoded &&
			(target.sourceStage==NativePostprocessStage::Screen || target.sourceStage==NativePostprocessStage::World),
			"Independent SaveImage closure contains an unproven or already encoded target");
	EncodeNonTargetDependencies(command);
	// Prepare appends targets in OrderScreenRenderTargetPasses order. Keep
	// this order intact: a Screen export can sample/LOAD a World paint from
	// this very submission, including a shared producer or nested RTT.
	// EncodeDeferredTarget retains all exact prior/image readability checks.
	for(std::size_t i=0;i<impl->targets.size();++i)EncodeDeferredTarget(command,i);
}
std::vector<PreparedScreenFrame::TargetInsertion> PreparedScreenFrame::DeferredTargets() const
{
	std::vector<TargetInsertion> result;
	for(std::size_t i=0;i<impl->targets.size();++i)if(impl->targets[i].sourceDependent && impl->targets[i].sourceStage==NativePostprocessStage::Screen)result.push_back({impl->targets[i].offset,impl->targets[i].sequence,i});
	return result;
}
std::vector<PreparedScreenFrame::TargetInsertion> PreparedScreenFrame::WorldTargets() const
{
 std::vector<TargetInsertion> result;
 for(std::size_t i=0;i<impl->targets.size();++i){const auto& target=impl->targets[i];
  if(target.sourceStage!=NativePostprocessStage::Screen)result.push_back({0,target.sequence,i,target.sourceStage,target.sourceDrawFrame});}
 return result;
}
bool PreparedScreenFrame::AllTargetsEncoded() const
{
	return std::all_of(impl->targets.begin(),impl->targets.end(),[](const auto& target){return target.encoded;});
}
void PreparedScreenFrame::EncodeDeferredTarget(id<MTL4CommandBuffer> command,std::size_t index) const
{
	Require(command!=nil && index<impl->targets.size(),"Missing actual native target producer");const auto& target=impl->targets[index];
	Require(!target.encoded,"Native target producer encoded twice");
	target.frame->EncodeDependencies(command);
	if(target.previous!=nil){
		Require(bool(target.requirePreviousReadable),"RTT Load has no exact prior-version readability proof");target.requirePreviousReadable();
		auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot create native UI RTT copy encoder");
		[copy barrierAfterQueueStages:MTLStageVertex|MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:target.previous toTexture:target.descriptor.colorAttachments[0].texture];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	if(target.previousDepth){
		Require(bool(target.requirePreviousDepthReadable),"Coverage depth Load lacks actual prior-version proof");target.requirePreviousDepthReadable();
		auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot encode coverage depth version preservation");
		[copy barrierAfterQueueStages:MTLStageVertex|MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:target.previousDepth toTexture:target.descriptor.depthAttachment.texture];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	auto encoder=[command renderCommandEncoderWithDescriptor:target.descriptor];Require(encoder!=nil,"Cannot create native UI RTT render encoder");
	[encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	try{target.frame->Encode(encoder);}catch(...){[encoder endEncoding];throw;}
	[encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
	target.encoded=true;
	Require(bool(target.encodedVersion),"RTT target lost its exact encoded-version state");*target.encodedVersion=target.stored;
	if(target.depthEncoded)*target.depthEncoded=target.stored;
}
void PreparedScreenFrame::DidComplete() const
{
	for (const auto& map: impl->minimaps) map->DidComplete();
	for (const auto& draw:impl->draws)if(draw.pip){draw.pip->DidComplete();draw.pipCompleted=draw.pipEncoded;}
	for (const auto& draw:impl->draws)if(draw.sensorPreview)draw.sensorPreview->DidComplete();
	for (const auto& draw:impl->draws)if(draw.startBox)draw.startBoxCompleted=draw.startBoxEncoded;
	for (const auto& target: impl->targets) if(target.encoded){target.frame->DidComplete();*target.initialized = target.stored;if(target.depthInitialized)*target.depthInitialized=target.stored;}
}
std::uint32_t PreparedScreenFrame::CommandCount() const { return impl->sourceCommandCount; }
MTLViewport PreparedScreenFrame::Viewport() const { return impl->viewport; }
void PreparedScreenFrame::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	EncodeRange(encoder,0,impl->sourceCommandCount);
}
void PreparedScreenFrame::EncodeRange(id<MTL4RenderCommandEncoder> encoder,std::uint32_t first,std::uint32_t end)const
{
	Require(first<=end && end<=impl->sourceCommandCount,"Postprocess root screen command range is invalid");
	if (impl->draws.empty()) return;
	if (!impl->targets.empty() || !impl->minimaps.empty()) [encoder barrierAfterQueueStages:MTLStageVertex | MTLStageFragment | MTLStageBlit | MTLStageDispatch beforeStages:MTLStageVertex | MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setCullMode:MTLCullModeNone];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setDepthBias:0 slopeScale:0 clamp:0];
	for(const auto& draw:impl->draws) {
		if(draw.originalCommand<first || draw.originalCommand>=end)continue;
  if(draw.unitIcons){
   for(std::size_t i=0;i<draw.unitIcons->DrawCount();++i)draw.unitIcons->EncodeDraw(i,encoder);
   [encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
  }
  if(draw.sensorLOS){draw.sensorLOS->Encode(encoder);continue;}
  if(draw.sensorPreview){
   draw.sensorPreview->Encode(encoder);
   [encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
   [encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];
   [encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
  }
		if(draw.buildSquare){
			draw.buildSquare->Encode(encoder);
			[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
			[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
		}
		if(draw.weaponRange){
			draw.weaponRange->Encode(encoder);
			[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
			[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
		}		if(draw.startBox){
			draw.startBox->Encode(encoder);++draw.startBoxEncoded;
			[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
			[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
		}
		if(draw.pip) {
			draw.pip->Encode(encoder);
			if(!(draw.pipOwner->raster.cullEnabled && draw.pipOwner->raster.cull==NativeWorldCull::FrontAndBack)) {
				Require(draw.pipEncoded<UINT32_MAX,"Encoded PiP packet count exceeds bounded frame metrics");++draw.pipEncoded;
			}
			// Typed PiP programs may change raster state. Each ordinary screen
			// command restores its own original cull state below.
			[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
			[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setDepthClipMode:MTLDepthClipModeClip];
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];continue;
		}
		for(const auto& image:draw.externalImages)image.requireReadable();
		[encoder setCullMode:draw.cull==ScreenCull::Front?MTLCullModeFront:draw.cull==ScreenCull::Back?MTLCullModeBack:MTLCullModeNone];
		[encoder setDepthStencilState:draw.depth];
		[encoder setStencilFrontReferenceValue:draw.frontReference backReferenceValue:draw.backReference];
		[encoder setViewport:draw.viewport];[encoder setScissorRect:draw.scissor];
		if(draw.panelShader){draw.panelShader->Encode(encoder);continue;}
		[encoder setRenderPipelineState:draw.pipeline];
		[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		if(draw.healthBars){[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.count instanceCount:draw.healthBarsInstances];continue;}
		const size_t offset=size_t(draw.first)*sizeof(uint32_t);
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:MTLIndexTypeUInt32 indexBuffer:impl->indices.gpuAddress+offset indexBufferLength:impl->indices.length-offset];
	}
}
struct TerraformCompiledOwner {__strong id<MTLLibrary> library=nil;std::array<__strong id<MTLRenderPipelineState>,3> pipelines{};};
struct DiffuseCompiledOwner {__strong id<MTLLibrary> library=nil;std::array<__strong id<MTLRenderPipelineState>,4> pipelines{};};
struct SplatCompiledOwner {__strong id<MTLLibrary> library=nil;std::array<__strong id<MTLRenderPipelineState>,2> pipelines{};};
struct VisualizerCompiledOwner {__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> pipeline=nil;};
struct ScreenGPU::Impl {
 std::shared_ptr<const VisualizerCompiledOwner> visualizerCompiled,decalCaptureCompiled;
 std::shared_ptr<const SplatCompiledOwner> splatCompiled;
 std::shared_ptr<const DiffuseCompiledOwner> diffuseCompiled;
 std::shared_ptr<const TerraformCompiledOwner> terraformCompiled;
	bool targetPriorityCompiled=false;
	__strong id<MTLLibrary> diagnosticLibrary=nil;
	__strong id<MTLRenderPipelineState> diagnosticProgram=nil;
	~Impl(){if(targetPriorityCompiled)--targetPriorityProgram::compiledConsumers;}
	MiniMapGPU minimaps;
	PiPGPU pip;
	PanelGPU panel;
	StartBoxGPU startBox;
	WeaponRangeGPU weaponRange;
	BuildSquareGPU buildSquare;
	StartBoxImageProvider startBoxImages;
	SensorPreviewProvider sensorPreviews;
 SensorLOSProvider sensorLOS;
 UnitPrimitiveGPU unitIcons;
	SharedImageProvider sharedImages;
	ExternalImageProvider externalImages;
	PiPSourceImageProvider pipSourceImages;
	struct Texture {
		std::weak_ptr<const ScreenTexture> owner;
		__strong id<MTLTexture> image=nil;
		std::shared_ptr<bool> initialized;
		// Original glCreateTexture allocates real storage before the first RTT
		// write. Generation0 may be sampled; its initial pixels are undefined.
		bool allocationOnly=false;
		std::shared_ptr<ScreenMipState> mipState={};
		std::shared_ptr<bool> encodedVersion;
		NativePostprocessStage sourceStage=NativePostprocessStage::Screen;
		std::uint64_t sourceSequence=0;
		std::uint32_t sourceOffset=0;
		bool sourceDependent=false;
	};
	std::unordered_map<const ScreenTexture*,Texture> textures;
	struct HealthBarsPointStorage {std::weak_ptr<const NativeBufferSnapshot> owner;__strong id<MTLBuffer> value=nil;};
	std::unordered_map<const NativeBufferSnapshot*,HealthBarsPointStorage> healthBarsPoints;
	id<MTLBuffer> HealthBarsPoints(RuntimeLease& lease,const NativeBufferSnapshotRef& source){
		std::erase_if(healthBarsPoints,[](const auto& entry){return entry.second.owner.expired();});
		auto& cached=healthBarsPoints[source.get()];
		if(cached.owner.expired()){cached.value=Buffer(lease,source->bytes.data(),source->bytes.size());cached.owner=source;}
		// An unchanged original VBO revision remains the same GPU input.
		lease.AddAllocation(cached.value);return cached.value;
	}

	std::vector<ScreenMipGeneration> pendingMipmaps;
	void QueueMipmaps(const std::shared_ptr<const ScreenTexture>& source,Texture& texture) {
		if (!texture.mipState || texture.mipState->initialized || texture.mipState->queued) return;
		pendingMipmaps.push_back({source,texture.image,texture.mipState});
		texture.mipState->queued=true;
	}
	__strong NSMutableDictionary<NSNumber*,id<MTLSamplerState>>* samplers=[NSMutableDictionary dictionary];
	__strong NSMutableDictionary<NSNumber*,id<MTLRenderPipelineState>>* pipelines=[NSMutableDictionary dictionary];
	__strong id<MTLLibrary> library=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid,depthFormat=MTLPixelFormatInvalid,stencilFormat=MTLPixelFormatInvalid;bool validation=false;
	struct DepthState {ScreenDepthStencilState source;MTLPixelFormat depthFormat,stencilFormat;__strong id<MTLDepthStencilState> value=nil;};
	std::vector<DepthState> depthStates;
	id<MTLDepthStencilState> DepthStencil(RuntimeLease& lease,const ScreenDepthStencilState& source,MTLPixelFormat actualDepth,MTLPixelFormat actualStencil) {
		Require(actualDepth==MTLPixelFormatInvalid || actualDepth==MTLPixelFormatDepth16Unorm || actualDepth==MTLPixelFormatDepth32Float,"Native screen requires an actual supported depth attachment format");
		Require(actualStencil==MTLPixelFormatInvalid || actualStencil==MTLPixelFormatStencil8,"Native screen requires an actual supported stencil attachment format");
		for(const auto& state:depthStates)if(state.source==source && state.depthFormat==actualDepth && state.stencilFormat==actualStencil){lease.Retain(state.value);return state.value;}
		auto descriptor=[MTLDepthStencilDescriptor new];
		// GL tests and writes have no effect when the framebuffer has no buffer.
		descriptor.depthCompareFunction=actualDepth!=MTLPixelFormatInvalid && source.depthTest?Compare(source.depthCompare):MTLCompareFunctionAlways;
		descriptor.depthWriteEnabled=actualDepth!=MTLPixelFormatInvalid && source.depthTest && source.depthWrite;
		if(actualStencil!=MTLPixelFormatInvalid && source.stencilTest)for(bool front:{true,false}) {
			const auto& original=front?source.frontStencil:source.backStencil;auto face=[MTLStencilDescriptor new];
			face.stencilCompareFunction=Compare(original.compare);face.stencilFailureOperation=Operation(original.fail);
			face.depthFailureOperation=Operation(original.depthFail);face.depthStencilPassOperation=Operation(original.pass);
			face.readMask=original.readMask;face.writeMask=original.writeMask;if(front)descriptor.frontFaceStencil=face;else descriptor.backFaceStencil=face;
		}
		auto value=[lease.Device() newDepthStencilStateWithDescriptor:descriptor];Require(value!=nil,"Cannot create original screen depth/stencil state");
		depthStates.push_back({source,actualDepth,actualStencil,value});lease.Retain(value);return value;
	}
	__strong id<MTLTexture> white=nil;
	id<MTLTexture> Image(RuntimeLease& lease,const std::shared_ptr<const ScreenTexture>& source,uint32_t& uploads,
		const std::vector<const ScreenTexture*>& available,std::optional<NativePostprocessConsumer> consumer=std::nullopt,
		std::function<void(PreparedScreenExternalImage)> retainExternal = {}) {
		if (source) ValidateScreenTextureMipGeneration(*source);
		if (source && source->incomplete) {
			Require(!source->width && !source->height && !source->bytesPerRow && source->bytes.empty() && source->mips.empty() && !source->renderTarget && !source->postprocessVersion && !source->sourceTexture,"Incomplete native texture must have no storage");
			// This neutral argument resource is not replacement image storage.
			// The shader preserves the material-specific incomplete contract.
			return Image(lease,nullptr,uploads,available,consumer);
		}
		if(!source) {
			if(white==nil) {uint8_t pixel[4]={255,255,255,255};MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;white=[lease.Device() newTextureWithDescriptor:d];Require(white!=nil,"Cannot allocate UI white texture");[white replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];}
			lease.AddAllocation(white);return white;
		}
		if(source->sourceTexture) {
			Require(!source->renderTarget&&!source->postprocessVersion&&!source->bytesPerRow&&source->bytes.empty()&&source->mips.empty()&&!source->generateMipmaps,
				"External screen image has competing CPU/RTT storage");
			const auto& object=*source->sourceTexture;
			Require((object.backing||object.image)&&source->width==object.allocation.width&&source->height==object.allocation.height&&object.allocation.samples==1,
				"External screen image differs from its frozen original allocation");
			Require(bool(externalImages)&&bool(retainExternal),"External screen image needs its actual provider and encode-time consumer");
			auto resolved=externalImages(lease,source->sourceTexture,consumer);auto image=resolved.texture;
			Require(image!=nil&&resolved.storageOwner&&bool(resolved.requireReadable)&&image.textureType==MTLTextureType2D&&image.sampleCount==1&&
				image.width==source->width&&image.height==source->height&&(image.usage&MTLTextureUsageShaderRead),"External screen provider returned a mismatched resource/owner");
			const bool depth=source->format==ScreenTextureFormat::Depth32Float||source->format==ScreenTextureFormat::Depth24Unorm||source->format==ScreenTextureFormat::Depth16Unorm;
			Require(depth==(image.pixelFormat==MTLPixelFormatDepth32Float||image.pixelFormat==MTLPixelFormatDepth32Float_Stencil8||image.pixelFormat==MTLPixelFormatDepth16Unorm),"External screen depth/color sampled type differs");
			lease.AddAllocation(image);retainExternal(std::move(resolved));return image;
		}
		if(source->postprocessVersion) {
			Require(!source->renderTarget && source->bytes.empty() && !source->bytesPerRow && source->mips.empty() && source->flipV,
				"Shared copied image must retain one exact native version without competing storage");
			const auto& version=*source->postprocessVersion;
			Require(version.image && source->width==version.image->width && source->height==version.image->height && source->generation==version.generation,
				"Shared image snapshot differs from its actual immutable native version");
			Require(bool(sharedImages),"Actual shared copy image provider is not initialized");
			Require(bool(retainExternal),"Shared copied image needs its encode-time readable receipt");
			auto resolved=sharedImages(lease,source->postprocessVersion,consumer);auto image=resolved.texture;
			Require(image!=nil && resolved.storageOwner && bool(resolved.requireReadable),"Shared provider returned no actual retained native image/readable receipt");
			retainExternal(std::move(resolved));lease.AddAllocation(image);return image;
		}
		const auto found=textures.find(source.get());if(found!=textures.end()&&!found->second.owner.expired()){
			Require(!source->renderTarget || found->second.allocationOnly || (found->second.initialized && *found->second.initialized) ||
				std::find(available.begin(),available.end(),source.get())!=available.end(),"Native UI RTT version has no retired or earlier ordered producer");
			const auto& entry=found->second;
			if(source->renderTarget && source->generation){
				if(entry.sourceDependent && entry.sourceStage==NativePostprocessStage::Screen && !(entry.initialized && *entry.initialized))
					Require(consumer && consumer->stage==NativePostprocessStage::Screen &&
						(consumer->screenCommandOffset>entry.sourceOffset ||
						(consumer->screenCommandOffset==entry.sourceOffset && (!consumer->commandSequence || consumer->commandSequence>entry.sourceSequence))),
						"Copied/sampled RTT version precedes its actual screen producer insertion");
				if(entry.sourceStage!=NativePostprocessStage::Screen && !(entry.initialized && *entry.initialized)){
					Require(consumer && (consumer->stage>entry.sourceStage ||
						(consumer->stage==entry.sourceStage && consumer->sequence>entry.sourceSequence)),
						"Actual world sampler precedes its pending PreUnit RTT producer");
				}
				Require(entry.initialized && entry.encodedVersion,"RTT version lost its immutable producer state");
				if(retainExternal)retainExternal({entry.image,source,[completed=entry.initialized,encoded=entry.encodedVersion,owner=source]{
					Require(*completed || *encoded,"RTT image sampled before its exact immutable version encoded");}});
			}
			QueueMipmaps(source,found->second);lease.AddAllocation(found->second.image);return found->second.image;
		}
		if (source->renderTarget && source->generation==0) {
			Require(source->renderTarget->resourceId && source->width && source->height && source->width<=16384 && source->height<=16384 &&
				source->width==source->renderTarget->width && source->height==source->renderTarget->height &&
				AttachmentPixelBytes(source->format)!=0 && source->flipV && source->bytes.empty() &&
				!source->bytesPerRow && source->mips.empty() && size_t(source->width)*source->height*AttachmentPixelBytes(source->format)<=256*1024*1024,
				"Native UI allocation-only generation0 requires an original bounded color target");
			MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ColorTargetFormat(source->format)
				width:source->width height:source->height mipmapped:NO];
			d.storageMode=MTLStorageModePrivate;
			d.usage=MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
			id<MTLTexture> image=[lease.Device() newTextureWithDescriptor:d];
			Require(image!=nil,"Cannot allocate original native UI generation0 storage");
			image.label=String(source->sourceName+" allocation generation0 (undefined initial pixels)");
			// Match glTexImage2D(..., nullptr): no invented CPU payload or clear.
			textures[source.get()]={source,image,{},true};
			lease.AddAllocation(image);return image;
		}
		if (source->renderTarget) {
			const auto describe=[](const ScreenTexture& value) {
				return value.sourceName + "/resource=" + std::to_string(value.renderTarget->resourceId) +
					"/generation=" + std::to_string(value.generation);
			};
			std::string producers; size_t count=0;
			for (const auto& entry: textures) {
				const auto owner=entry.second.owner.lock();
				if (!owner || !owner->renderTarget) continue;
				if (count++<24) producers += " [" + describe(*owner) + (entry.second.allocationOnly ? "/allocation-only]" :
					(entry.second.initialized && *entry.second.initialized ? "/retired]" : "/pending]"));
			}
			throw std::runtime_error("Native UI RTT version has no native producer: requested [" + describe(*source) +
				"], known target count=" + std::to_string(count) + producers +
				"; cached versions cannot be reconstructed from CPU pixels");
		}
		Require(source->width&&source->height&&source->width<=16384&&source->height<=16384,"Native UI image dimensions outside bounds");
		MTLPixelFormat pixelFormat;
		size_t bytesPerUnit = 4;
		bool compressed = false;
		switch (source->format) {
			case ScreenTextureFormat::R8Unorm: pixelFormat = MTLPixelFormatR8Unorm; bytesPerUnit = 1; break;
			case ScreenTextureFormat::RGBA8Unorm: pixelFormat = MTLPixelFormatRGBA8Unorm; break;
			case ScreenTextureFormat::BGRA8Unorm: pixelFormat = MTLPixelFormatBGRA8Unorm; break;
			case ScreenTextureFormat::BC1RGBA: pixelFormat = MTLPixelFormatBC1_RGBA; compressed = true; bytesPerUnit = 8; break;
			case ScreenTextureFormat::BC2RGBA: pixelFormat = MTLPixelFormatBC2_RGBA; compressed = true; bytesPerUnit = 16; break;
			case ScreenTextureFormat::BC3RGBA: pixelFormat = MTLPixelFormatBC3_RGBA; compressed = true; bytesPerUnit = 16; break;
			case ScreenTextureFormat::BC4Unorm: pixelFormat = MTLPixelFormatBC4_RUnorm; compressed = true; bytesPerUnit = 8; break;
			case ScreenTextureFormat::BC5Unorm: pixelFormat = MTLPixelFormatBC5_RGUnorm; compressed = true; bytesPerUnit = 16; break;
			case ScreenTextureFormat::BC7RGBA: pixelFormat = MTLPixelFormatBC7_RGBAUnorm; compressed = true; bytesPerUnit = 16; break;
			case ScreenTextureFormat::BC7RGBASRGB: pixelFormat = MTLPixelFormatBC7_RGBAUnorm_sRGB; compressed = true; bytesPerUnit = 16; break;
			default: throw std::runtime_error("Unsupported native UI image format");
		}
		Require(!compressed || !source->mips.empty(), "Original native UI BC texture requires its owned mip chain");
		const bool explicitMips = !source->mips.empty();
		Require(!explicitMips || (source->bytes.empty() && source->bytesPerRow == 0), "Native UI image has competing legacy and mip payloads");
		uint32_t maximumLevels = 1;
		for (uint32_t side = std::max(source->width, source->height); side > 1; side >>= 1) ++maximumLevels;
		const size_t suppliedLevels = explicitMips ? source->mips.size() : 1;
		Require(!source->generateMipmaps || suppliedLevels==1,"Original embedded image mip levels must not be overwritten");
		const size_t levels = source->generateMipmaps ? maximumLevels : suppliedLevels;
		Require(suppliedLevels <= maximumLevels, "Native UI image has excess mip levels");
		uint32_t mipWidth = source->width, mipHeight = source->height;
		size_t totalBytes = 0;
		for (size_t level = 0; level < suppliedLevels; ++level) {
			const auto* mip = explicitMips ? &source->mips[level] : nullptr;
			Require(!mip || (mip->width == mipWidth && mip->height == mipHeight), "Native UI original mip dimensions do not match the declared chain");
			const size_t rowBytes = mip ? mip->bytesPerRow : source->bytesPerRow;
			const size_t payload = mip ? mip->bytes.size() : source->bytes.size();
			const size_t columns = compressed ? (size_t(mipWidth) + 3) / 4 : mipWidth;
			const size_t rows = compressed ? (size_t(mipHeight) + 3) / 4 : mipHeight;
			if(mip&&mip->allocatedUndefined){
				Require(source->levelDefinitions&&mip->bytes.empty()&&rowBytes==columns*bytesPerUnit,
					"Undefined mip must retain an actual TexImage allocation without fabricated pixels");
				mipWidth=std::max(1u,mipWidth/2);mipHeight=std::max(1u,mipHeight/2);continue;
			}
			Require(rowBytes >= columns * bytesPerUnit && rowBytes <= 128 * 1024 &&
				(!compressed || rowBytes % bytesPerUnit == 0) && rows <= payload / rowBytes,
				"Native UI original mip byte extent invalid");
			Require(payload <= 256 * 1024 * 1024 - totalBytes, "Native UI image mip storage exceeds diagnostic bound");
			totalBytes += payload;
			mipWidth = std::max(1u, mipWidth / 2); mipHeight = std::max(1u, mipHeight / 2);
		}
		// Retain the original compressed snapshot/cache identity. Only upload storage
		// changes so the existing GPU producer can generate color-renderable mips.
		std::optional<WorldTextureMip> decodedBase;
		const bool decodeForMipmaps = compressed && source->generateMipmaps && levels > 1;
		if (decodeForMipmaps) {
			Require(source->width <= lease.Capabilities().maxTextureDimension2D &&
				source->height <= lease.Capabilities().maxTextureDimension2D,
				"Original native UI BC mip transport exceeds actual device dimensions");
			const auto& base = source->mips.front();
			if (!base.allocatedUndefined) {
				const auto format = source->format == ScreenTextureFormat::BC1RGBA ? WorldTextureFormat::BC1RGBA :
					source->format == ScreenTextureFormat::BC2RGBA ? WorldTextureFormat::BC2RGBA :
					source->format == ScreenTextureFormat::BC3RGBA ? WorldTextureFormat::BC3RGBA :
					source->format == ScreenTextureFormat::BC4Unorm ? WorldTextureFormat::BC4Unorm :
					source->format == ScreenTextureFormat::BC5Unorm ? WorldTextureFormat::BC5Unorm :
					source->format == ScreenTextureFormat::BC7RGBA ? WorldTextureFormat::BC7RGBA : WorldTextureFormat::BC7RGBASRGB;
				decodedBase = DecodeWorldBCBase(base.width, base.height, base.bytesPerRow,
					std::span<const uint8_t>(base.bytes), format, lease.Capabilities().maxTextureDimension2D);
			}
			// An original nullptr allocation stays undefined: no decode, clear or upload.
			pixelFormat = source->format == ScreenTextureFormat::BC7RGBASRGB ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm;
		}
		Require(!compressed || decodeForMipmaps || lease.Capabilities().bcTextureCompression,
			"Original native UI BC texture is unsupported on this device");
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat width:source->width height:source->height mipmapped:levels > 1];
		d.mipmapLevelCount = levels; d.storageMode = MTLStorageModeShared; d.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> image = [lease.Device() newTextureWithDescriptor:d];
		Require(image != nil, "Cannot allocate original native UI image"); image.label = String(source->sourceName);
		for (size_t level = 0; level < suppliedLevels; ++level) {
			const auto* mip = explicitMips ? &source->mips[level] : nullptr;
			if(mip&&mip->allocatedUndefined)continue; // Preserve original undefined storage; no clear/upload.
			[image replaceRegion:MTLRegionMake2D(0, 0, mip ? mip->width : source->width, mip ? mip->height : source->height)
				mipmapLevel:level withBytes:decodedBase ? decodedBase->bytes.data() : (mip ? mip->bytes.data() : source->bytes.data())
				bytesPerRow:decodedBase ? decodedBase->bytesPerRow : (mip ? mip->bytesPerRow : source->bytesPerRow)];
		}
		Texture candidate{source,image,{}};
		if (source->generateMipmaps && levels>1) candidate.mipState=std::make_shared<ScreenMipState>();
		auto& cached=textures[source.get()];cached=std::move(candidate);QueueMipmaps(source,cached);
		lease.AddAllocation(image);++uploads;return image;
	}
	id<MTLSamplerState> Sampler(RuntimeLease& lease,const ScreenSampler& value) {
		ValidateScreenSampler(value);const uint32_t key=uint32_t(value.filter)|(uint32_t(value.wrapS)<<2)|(uint32_t(value.wrapT)<<4)|(uint32_t(value.mipFilter)<<6)|(uint32_t(value.magnificationFilter.value_or(value.filter))<<8)|(value.anisotropy<<10)|(uint32_t(value.whiteBorder)<<15);
		id<MTLSamplerState> result=samplers[@(key)];if(result==nil){MTLSamplerDescriptor* d=[MTLSamplerDescriptor new];d.minFilter=value.filter==ScreenFilter::Linear?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;d.magFilter=value.magnificationFilter.value_or(value.filter)==ScreenFilter::Linear?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;d.mipFilter=value.mipFilter==ScreenMipFilter::Linear?MTLSamplerMipFilterLinear:(value.mipFilter==ScreenMipFilter::Nearest?MTLSamplerMipFilterNearest:MTLSamplerMipFilterNotMipmapped);d.sAddressMode=Wrap(value.wrapS,value.filter);d.tAddressMode=Wrap(value.wrapT,value.filter);d.borderColor=value.whiteBorder?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorTransparentBlack;d.maxAnisotropy=value.anisotropy;d.supportArgumentBuffers=YES;result=[lease.Device() newSamplerStateWithDescriptor:d];Require(result!=nil,"Cannot create original UI sampler");samplers[@(key)]=result;}lease.Retain(result);return result;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const ScreenBlendState& blend,const std::array<bool,4>& mask,MTLPixelFormat target,bool validate,double& milliseconds,bool minimap,MTLPixelFormat actualDepth,MTLPixelFormat actualStencil,unsigned sampledDepthMask,bool healthBars,std::uint32_t samples,bool alphaToCoverage,bool visualizer=false,unsigned splatProgram=0,unsigned diffuseProgram=0,unsigned terraformProgram=0) {
		const auto start=Clock::now();NSError* error=nil;
		if(library==nil){MTL4LibraryDescriptor* d=[MTL4LibraryDescriptor new];d.source=String(std::string(SHADER)+HealthBarsShaderSource+MiniMapScreenShaderSource()+NativeSplatPainterShaderSource+NativeDiffusePainterShaderSource+NativeTerraformCaptureShaderSource);d.name=@"Original native UI texture and font coverage";library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile native UI shader: "+Error(error));}
		// Metal4 depth/stencil formats belong to actual pass attachments, not its pipeline descriptor.
		if(format!=target||validation!=validate||depthFormat!=actualDepth||stencilFormat!=actualStencil){[pipelines removeAllObjects];format=target;validation=validate;depthFormat=actualDepth;stencilFormat=actualStencil;}
		Require(splatProgram<=3&&(!splatProgram||(!minimap&&!visualizer&&!healthBars&&!sampledDepthMask)),"Splat Painter pipeline family differs");
		Require(diffuseProgram<=4&&(!diffuseProgram||(!splatProgram&&!minimap&&!visualizer&&!healthBars&&!sampledDepthMask)),"Diffuse Painter pipeline family differs");
		Require(terraformProgram<=3&&(!terraformProgram||(!diffuseProgram&&!splatProgram&&!minimap&&!visualizer&&!healthBars&&!sampledDepthMask)),"Terraform Capture pipeline family differs");
		const uint64_t key=(uint64_t(terraformProgram)<<45)|(uint64_t(diffuseProgram)<<42)|(uint64_t(splatProgram)<<40)|(uint64_t(samples)<<32)|(uint64_t(lease.SampleShadingEnabled(samples))<<39)|(uint64_t(alphaToCoverage)<<38)|BlendKey(blend)|(uint32_t(mask[0])<<23)|(uint32_t(mask[1])<<24)|(uint32_t(mask[2])<<25)|(uint32_t(mask[3])<<26)|(uint32_t(minimap)<<27)|(uint32_t(sampledDepthMask)<<28)|(uint32_t(healthBars)<<30)|(uint64_t(visualizer)<<31);id<MTLRenderPipelineState> result=pipelines[@(key)];
		if(result==nil){MTL4RenderPipelineDescriptor* d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.alphaToCoverageState=alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;MTL4LibraryFunctionDescriptor* vs=[MTL4LibraryFunctionDescriptor new];vs.library=library;vs.name=healthBars?@"HealthBarsVertexMain":@"ScreenVertexMain";d.vertexFunctionDescriptor=vs;MTL4LibraryFunctionDescriptor* fs=[MTL4LibraryFunctionDescriptor new];fs.library=library;fs.name=terraformProgram==1?@"TerraformCaptureReprojectFragment":terraformProgram==2?@"TerraformCaptureSpriteFragment":terraformProgram==3?@"TerraformCaptureModelFragment":diffuseProgram==1?@"DiffuseCompositeFragment":diffuseProgram==2?@"DiffuseStampFragment":diffuseProgram==3?@"DiffuseChannelFragment":diffuseProgram==4?@"DiffuseCopyFragment":splatProgram==1?@"SplatPaintFragment":splatProgram==2?@"SplatCopyFragment":splatProgram==3?@"SplatOverlayFragment":visualizer?@"DeferredVisualizerFragment":healthBars?@"HealthBarsFragmentMain":minimap?@"MiniMapScreenFragment":String(std::string("ScreenFragmentMain")+(sampledDepthMask?std::to_string(sampledDepthMask):""));d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;MTL4PipelineOptions* o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validate);d.options=o;auto* a=d.colorAttachments[0];a.pixelFormat=target;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=Factor(blend.sourceColor);a.destinationRGBBlendFactor=Factor(blend.destinationColor);a.sourceAlphaBlendFactor=Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));result=lease.NewRenderPipeline(d,&error);Require(result!=nil,"Cannot create native UI blend pipeline: "+Error(error));pipelines[@(key)]=result;milliseconds+=Milliseconds(start);}
		if(!terraformProgram&&!diffuseProgram&&!splatProgram&&!visualizer&&!minimap&&!healthBars&&!sampledDepthMask&&!targetPriorityCompiled){diagnosticLibrary=library;diagnosticProgram=result;++targetPriorityProgram::compiledConsumers;targetPriorityCompiled=true;}
		lease.Retain(library);lease.Retain(result);return result;
	}
};
ScreenGPU::ScreenGPU():impl(std::make_unique<Impl>()){AcquireNativeHealthBarsConsumer();}
ScreenGPU::~ScreenGPU(){ReleaseNativeHealthBarsConsumer();}
void ScreenGPU::InitializeDiagnosticPrograms(RuntimeLease& lease,MTLPixelFormat format,bool validation,double& milliseconds,unsigned samples)
{
	(void)impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},format,validation,milliseconds,false,MTLPixelFormatInvalid,MTLPixelFormatInvalid,0,false,samples,false);
	Require(impl->diagnosticLibrary!=nil&&impl->diagnosticProgram!=nil&&impl->targetPriorityCompiled,"Actual screen diagnostic program was not compiled");
 auto capture=std::make_shared<VisualizerCompiledOwner>();capture->library=impl->diagnosticLibrary;capture->pipeline=impl->diagnosticProgram;
 impl->decalCaptureCompiled=capture;NativeDecalCaptureOwner()=capture;
 auto splat=std::make_shared<SplatCompiledOwner>();splat->library=impl->library;
 for(unsigned n=0;n<2;++n){splat->pipelines[n]=impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},MTLPixelFormatRGBA8Unorm,validation,milliseconds,false,MTLPixelFormatInvalid,MTLPixelFormatInvalid,0,false,1,false,false,n+1);Require(splat->pipelines[n]!=nil,"Splat Painter actual RTT pipeline unavailable");}
 impl->splatCompiled=splat;NativeSplatPainterOwner()=splat;
 auto diffuse=std::make_shared<DiffuseCompiledOwner>();diffuse->library=impl->library;
 for(unsigned n=0;n<4;++n){diffuse->pipelines[n]=impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},MTLPixelFormatRGBA8Unorm,validation,milliseconds,false,MTLPixelFormatInvalid,MTLPixelFormatInvalid,0,false,1,false,false,0,n+1);Require(diffuse->pipelines[n]!=nil,"Diffuse Painter actual RTT pipeline unavailable");}
 impl->diffuseCompiled=diffuse;NativeDiffusePainterOwner()=diffuse;
 auto terraform=std::make_shared<TerraformCompiledOwner>();terraform->library=impl->library;
 for(unsigned n=0;n<3;++n){terraform->pipelines[n]=impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},MTLPixelFormatRGBA8Unorm,validation,milliseconds,false,MTLPixelFormatInvalid,MTLPixelFormatInvalid,0,false,1,false,false,0,0,n+1);Require(terraform->pipelines[n]!=nil,"Terraform Capture actual RTT pipeline unavailable");}
 impl->terraformCompiled=terraform;NativeTerraformCaptureOwner()=terraform;
 auto owner=std::make_shared<VisualizerCompiledOwner>();owner->pipeline=impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},format,validation,milliseconds,false,MTLPixelFormatInvalid,MTLPixelFormatInvalid,0,false,samples,false,true);owner->library=impl->library;
 Require(owner->pipeline!=nil&&owner->library!=nil,"Deferred visualizer actual program unavailable");impl->visualizerCompiled=owner;NativeDeferredVisualizerOwner()=owner;
}
void ScreenGPU::ClearResources()
{
	auto next=std::make_unique<Impl>();
	next->sharedImages=std::move(impl->sharedImages);next->externalImages=std::move(impl->externalImages);
	next->startBoxImages=std::move(impl->startBoxImages);next->sensorPreviews=std::move(impl->sensorPreviews);
	next->sensorLOS=std::move(impl->sensorLOS);next->pipSourceImages=std::move(impl->pipSourceImages);
	// Widget/image clearing is not renderer-context destruction. Retain an
	// actual compiled program, not just its availability counter, across it.
	next->diagnosticLibrary=impl->diagnosticLibrary;next->diagnosticProgram=impl->diagnosticProgram;
	next->visualizerCompiled=std::move(impl->visualizerCompiled);
	next->decalCaptureCompiled=std::move(impl->decalCaptureCompiled);
	next->splatCompiled=std::move(impl->splatCompiled);
	next->diffuseCompiled=std::move(impl->diffuseCompiled);
	next->terraformCompiled=std::move(impl->terraformCompiled);
	next->targetPriorityCompiled=impl->targetPriorityCompiled;impl->targetPriorityCompiled=false;
	impl=std::move(next);
}
void ScreenGPU::SetSharedImageProvider(SharedImageProvider provider){impl->sharedImages=std::move(provider);}
void ScreenGPU::SetExternalImageProvider(ExternalImageProvider provider){impl->externalImages=std::move(provider);}
void ScreenGPU::SetPiPSourceImageProvider(PiPSourceImageProvider provider){impl->pipSourceImages=std::move(provider);}
PreparedPiPSourceImage ScreenGPU::ResolveOriginalSourceImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,const ScreenSampler& sampling,bool validation,double& ms){
 Require(bool(impl->pipSourceImages),"Actual typed image provider unavailable");return impl->pipSourceImages(lease,binding,sampling,validation,ms);
}
void ScreenGPU::SetStartBoxImageProvider(StartBoxImageProvider provider){impl->startBoxImages=std::move(provider);}
void ScreenGPU::SetSensorLOSProvider(SensorLOSProvider provider){impl->sensorLOS=std::move(provider);}
void ScreenGPU::SetSensorPreviewProvider(SensorPreviewProvider provider){impl->sensorPreviews=std::move(provider);}
PreparedStartBoxImage ScreenGPU::ResolveStartBoxImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,const PreparedScreenFrame* producers,std::optional<NativePostprocessConsumer> consumer){Require(bool(impl->startBoxImages),"Original StartBox resolver is not attached");return impl->startBoxImages(lease,binding,producers,consumer);}
id<MTLSamplerState> ScreenGPU::PrepareSampler(RuntimeLease& lease,const ScreenSampler& sampler){return impl->Sampler(lease,sampler);}
std::unique_ptr<PreparedScreenMipmaps> ScreenGPU::PrepareMipmaps(RuntimeLease& lease)
{
	auto plan=std::unique_ptr<PreparedScreenMipmaps>(new PreparedScreenMipmaps);
	for(const auto& image:impl->pendingMipmaps) lease.AddAllocation(image.image);
	plan->impl->images=std::move(impl->pendingMipmaps);
	impl->pendingMipmaps.clear();
	for(const auto& image:plan->impl->images) image.state->queued=false;
	return plan;
}

std::unique_ptr<PreparedScreenFrame> ScreenGPU::Prepare(RuntimeLease& lease,const ScreenFrameView& frame,uint32_t width,uint32_t height,
	uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat format,bool validation,double& shaderMs,
	MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,std::uint32_t samples,std::uint32_t firstOriginalCommand)
{
	impl->pip.BeginFrame();
	const auto order = OrderScreenRenderTargetPasses(frame);
	for (auto i=impl->textures.begin(); i!=impl->textures.end();)
		if (i->second.owner.expired()) i=impl->textures.erase(i); else ++i;
	for (size_t index: order) {
		const auto& sourcePass=frame.targetPasses[index];
		const auto& source = sourcePass.destination;
		const auto found = impl->textures.find(source.get());
		if (found != impl->textures.end()) {
			Require(found->second.initialized && !*found->second.initialized,
				"Native UI RTT generation is immutable after successful retirement");
			continue;
		}
		Require(ScreenRenderTargetPixelBytes(source->format)!=0 &&
			size_t(source->width)*source->height*ScreenRenderTargetPixelBytes(source->format) <= 256*1024*1024,
			"Native UI RTT image exceeds bounded attachment allocation");
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ColorTargetFormat(source->format)
			width:source->width height:source->height mipmapped:NO];
		d.storageMode = MTLStorageModePrivate;
		d.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		id<MTLTexture> image = [lease.Device() newTextureWithDescriptor:d];
		Require(image != nil, "Cannot allocate original native UI RTT generation");
		image.label = String(source->sourceName + " generation " + std::to_string(source->generation));
		impl->textures.emplace(source.get(), Impl::Texture{source,image,std::make_shared<bool>(false),false,{},std::make_shared<bool>(false),sourcePass.sourceStage,sourcePass.commandSequence,sourcePass.screenCommandOffset,sourcePass.sourceDependent});
	}
	for(size_t index:order){const auto& pass=frame.targetPasses[index];const auto& source=pass.depthDestination;if(!source)continue;
		auto found=impl->textures.find(source.get());if(found!=impl->textures.end()){
			Require(found->second.initialized && !*found->second.initialized,"Coverage depth generation is immutable after retirement");continue;}
		Require(source->width<=lease.Capabilities().maxTextureDimension2D && source->height<=lease.Capabilities().maxTextureDimension2D,"Coverage depth extent exceeds actual device");
		auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth16Unorm width:source->width height:source->height mipmapped:NO];
		d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
		auto image=[lease.Device() newTextureWithDescriptor:d];Require(image!=nil,"Cannot allocate actual coverage D16 version");
		impl->textures.emplace(source.get(),Impl::Texture{source,image,std::make_shared<bool>(false),false,{},std::make_shared<bool>(false),pass.sourceStage,pass.commandSequence,pass.screenCommandOffset,pass.sourceDependent});
	}
	std::vector<const ScreenTexture*> available;
	std::vector<PreparedScreenFrame::Impl::TargetPass> targets;
	uint32_t uploads = 0;
	for (size_t index: order) {
		const auto& source = frame.targetPasses[index];
		auto& destination = impl->textures.at(source.destination.get());
		lease.AddAllocation(destination.image);
		id<MTLTexture> previous = nil;
		std::function<void()> requirePreviousReadable;
		try {
			if (source.load == ScreenTargetLoad::Load) previous = impl->Image(lease,source.previous,uploads,available,
				NativePostprocessConsumer{source.sourceStage,source.sourceStage==NativePostprocessStage::Screen?0:source.commandSequence,source.screenCommandOffset,source.commandSequence},
				[&](PreparedScreenExternalImage value){requirePreviousReadable=std::move(value.requireReadable);});
		} catch (const std::exception& error) {
			throw std::runtime_error("Native UI target-pass " + std::to_string(index) + " [" + source.destination->sourceName +
				"/generation=" + std::to_string(source.destination->generation) + "] Load dependency: " + error.what());
		}
		Impl::Texture* depth=nullptr;id<MTLTexture> previousDepth=nil;std::function<void()> requirePreviousDepthReadable;
		if(source.depthDestination){depth=&impl->textures.at(source.depthDestination.get());lease.AddAllocation(depth->image);
			if(source.previousDepth)previousDepth=impl->Image(lease,source.previousDepth,uploads,available,
				NativePostprocessConsumer{source.sourceStage,source.commandSequence,0,source.commandSequence},
				[&](PreparedScreenExternalImage image){requirePreviousDepthReadable=std::move(image.requireReadable);});
		}
		const ScreenFrameView targetFrame{source.drawList.get(),source.coordinateWidth,source.coordinateHeight,std::nullopt,{}};
		std::unique_ptr<PreparedScreenFrame> target;
		try {
			target = PrepareList(lease,targetFrame,source.destination->width,source.destination->height,
			source.coordinateWidth,source.coordinateHeight,destination.image.pixelFormat,validation,shaderMs,available,false,
				(source.sourceDependent||source.sourceStage!=NativePostprocessStage::Screen||std::any_of(source.drawList->Commands().begin(),source.drawList->Commands().end(),[](const auto& c){return bool(c.sensorPreview)||bool(c.sensorLOS);}))?
     std::optional(NativePostprocessConsumer{source.sourceStage,source.sourceStage==NativePostprocessStage::Screen?0:source.commandSequence,source.screenCommandOffset,source.commandSequence}):std::nullopt,depth?depth->image.pixelFormat:MTLPixelFormatInvalid,MTLPixelFormatInvalid);
		} catch (const std::exception& error) {
			throw std::runtime_error("Native UI target-pass " + std::to_string(index) + " [" + source.destination->sourceName +
				"/generation=" + std::to_string(source.destination->generation) + "] draws: " + error.what());
		}
		uploads += target->TextureUploads();
		MTL4RenderPassDescriptor* d = [MTL4RenderPassDescriptor new];
		d.colorAttachments[0].texture = destination.image;
		d.colorAttachments[0].loadAction = source.load == ScreenTargetLoad::Load ? MTLLoadActionLoad :
			(source.load == ScreenTargetLoad::Clear ? MTLLoadActionClear : MTLLoadActionDontCare);
		d.colorAttachments[0].storeAction = source.store == ScreenTargetStore::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
		d.colorAttachments[0].clearColor = MTLClearColorMake(source.clearColor[0],source.clearColor[1],source.clearColor[2],source.clearColor[3]);
		if(depth){d.depthAttachment.texture=depth->image;d.depthAttachment.loadAction=source.previousDepth?MTLLoadActionLoad:MTLLoadActionDontCare;d.depthAttachment.storeAction=MTLStoreActionStore;}
		d.renderTargetWidth = source.destination->width; d.renderTargetHeight = source.destination->height;
		targets.push_back({previous,d,std::move(target),destination.initialized,source.store == ScreenTargetStore::Store,source.sourceDependent,source.screenCommandOffset,source.commandSequence,false,source.sourceStage,source.sourceDrawFrame,destination.encodedVersion,std::move(requirePreviousReadable),previousDepth,depth?depth->initialized:nullptr,depth?depth->encodedVersion:nullptr,std::move(requirePreviousDepthReadable)});
		if (source.store == ScreenTargetStore::Store){available.push_back(source.destination.get());if(source.depthDestination)available.push_back(source.depthDestination.get());}
	}
	std::unique_ptr<PreparedScreenFrame> prepared;
	try {
		prepared = PrepareList(lease,frame,width,height,logicalWidth,logicalHeight,format,validation,shaderMs,available,true,std::nullopt,depthFormat,stencilFormat,samples,firstOriginalCommand);
	} catch (const std::exception& error) {
		throw std::runtime_error("Native UI final screen draws: " + std::string(error.what()));
	}
	prepared->impl->uploads += uploads;
	prepared->impl->targets = std::move(targets);
	for(size_t index:order)if(frame.targetPasses[index].store==ScreenTargetStore::Store){
		prepared->impl->producedImages.push_back(frame.targetPasses[index].destination);
		if(frame.targetPasses[index].depthDestination)prepared->impl->producedImages.push_back(frame.targetPasses[index].depthDestination);
	}
	prepared->impl->resolvePrevious=[this,producers=prepared.get()](RuntimeLease& currentLease,
		const std::shared_ptr<const ScreenTexture>& source,NativePostprocessConsumer consumer) {
		PreparedScreenExternalImage retained;
		auto resolved=PrepareImage(currentLease,source,ScreenSampler{},producers,consumer,
			[&](PreparedScreenExternalImage image){retained=std::move(image);});
		Require(retained.texture==resolved.texture && retained.storageOwner && bool(retained.requireReadable),
			"Prior FBO contents lack their exact retained producer/readability proof");
		return retained;
	};
	return prepared;
}

namespace {
std::uint32_t ReadbackPixelBytes(MTLPixelFormat format)
{
 switch(format){
  case MTLPixelFormatR8Unorm:return 1;
  case MTLPixelFormatRG8Unorm:case MTLPixelFormatR16Float:return 2;
  case MTLPixelFormatRGBA8Unorm:case MTLPixelFormatBGRA8Unorm:
  case MTLPixelFormatRGBA8Unorm_sRGB:case MTLPixelFormatBGRA8Unorm_sRGB:
  case MTLPixelFormatRG16Float:case MTLPixelFormatR32Float:
  case MTLPixelFormatRG11B10Float:return 4;
  case MTLPixelFormatRGBA16Float:return 8;
  default:throw std::runtime_error("SaveImage attachment has no implemented original RGBA8 conversion");
 }
}
float ReadbackUnsignedFloat(std::uint32_t value,unsigned mantissaBits)
{
 const auto mantissa=value&((1u<<mantissaBits)-1u),exponent=value>>mantissaBits;
 if(exponent==31)return mantissa?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();
 if(!exponent)return std::ldexp(float(mantissa),-14-int(mantissaBits));
 return std::ldexp(1.0f+float(mantissa)/float(1u<<mantissaBits),int(exponent)-15);
}
float ReadbackHalf(const std::uint8_t* input)
{
 std::uint16_t value;std::memcpy(&value,input,sizeof(value));
 const float magnitude=ReadbackUnsignedFloat(value&0x7fffu,10);
 return value&0x8000u?-magnitude:magnitude;
}
std::uint8_t ReadbackByte(float value)
{
 // Conversion is from stored color components, independent of texture
 // sampling swizzles, blend state or sRGB sampling transfer functions.
 if(!(value>0))return 0;
 if(value>=1)return 255;
 return std::uint8_t(std::lround(value*255.0f));
}
}
struct PreparedScreenReadback::Impl {
 PreparedScreenExternalImage source;
 __strong id<MTLBuffer> buffer=nil;
 ScreenScissor rectangle,clipped;
 std::size_t rowBytes=0;
 std::uint32_t pixelBytes=0;
 bool encoded=false,completed=false;
};
PreparedScreenReadback::PreparedScreenReadback():impl(std::make_shared<Impl>()){}
PreparedScreenReadback::~PreparedScreenReadback()=default;
void PreparedScreenReadback::Encode(id<MTL4CommandBuffer> command)
{
 Require(command && !impl->encoded && !impl->completed,"SaveImage copy must encode once on its actual shared submission");
 impl->source.requireReadable();
 const auto& r=impl->clipped;
 if(!r.width || !r.height){impl->encoded=true;return;}
 auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot encode SaveImage attachment copy");
 [copy barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
 // Native attachment row0 is top-left; glReadPixels x/y addresses bottom-left.
 [copy copyFromTexture:impl->source.texture sourceSlice:0 sourceLevel:0
  sourceOrigin:MTLOriginMake(r.x,impl->source.texture.height-std::size_t(r.y)-r.height,0)
  sourceSize:MTLSizeMake(r.width,r.height,1) toBuffer:impl->buffer destinationOffset:0
  destinationBytesPerRow:impl->rowBytes destinationBytesPerImage:0];
 [copy endEncoding];impl->encoded=true;
}
void PreparedScreenReadback::DidComplete()
{
 Require(impl->encoded && !impl->completed,"SaveImage completion requires its encoded GPU copy and one real retirement");
 impl->completed=true;
}
std::vector<std::uint8_t> PreparedScreenReadback::PixelsRGBA8() const
{
 Require(impl->completed,"SaveImage cannot read CPU bytes before actual GPU retirement");
 const auto& r=impl->rectangle;const auto format=impl->source.texture.pixelFormat;
 std::vector<std::uint8_t> output(std::size_t(r.width)*r.height*4);
 // OpenGL 4.3 core section18.2 defines pixels outside the read attachment
 // as undefined. Keep those output bytes zero; never clamp/duplicate an edge
 // or replace any in-bounds pixel. All defined pixels come from the GPU copy.
 const auto& clipped=impl->clipped;
 if(!clipped.width || !clipped.height)return output;
 const auto dx=std::size_t(std::int64_t(clipped.x)-r.x),dy=std::size_t(std::int64_t(clipped.y)-r.y);
 const auto* bytes=static_cast<const std::uint8_t*>(impl->buffer.contents);
 for(std::size_t y=0;y<clipped.height;++y)for(std::size_t x=0;x<clipped.width;++x){
  const auto* in=bytes+(clipped.height-1-y)*impl->rowBytes+x*impl->pixelBytes;
  auto* out=output.data()+((dy+y)*r.width+dx+x)*4;
  out[0]=out[1]=out[2]=0;out[3]=255;
  switch(format){
   case MTLPixelFormatRGBA8Unorm:case MTLPixelFormatRGBA8Unorm_sRGB:
    std::memcpy(out,in,4);break;
   case MTLPixelFormatBGRA8Unorm:case MTLPixelFormatBGRA8Unorm_sRGB:
    out[0]=in[2];out[1]=in[1];out[2]=in[0];out[3]=in[3];break;
   case MTLPixelFormatR8Unorm:out[0]=in[0];break;
   case MTLPixelFormatRG8Unorm:out[0]=in[0];out[1]=in[1];break;
   case MTLPixelFormatR16Float:out[0]=ReadbackByte(ReadbackHalf(in));break;
   case MTLPixelFormatRG16Float:
    for(unsigned c=0;c<2;++c)out[c]=ReadbackByte(ReadbackHalf(in+2*c));break;
   case MTLPixelFormatRGBA16Float:
    for(unsigned c=0;c<4;++c)out[c]=ReadbackByte(ReadbackHalf(in+2*c));break;
   case MTLPixelFormatR32Float:{float value;std::memcpy(&value,in,4);out[0]=ReadbackByte(value);break;}
   case MTLPixelFormatRG11B10Float:{
    std::uint32_t value;std::memcpy(&value,in,4);
    out[0]=ReadbackByte(ReadbackUnsignedFloat(value&0x7ffu,6));
    out[1]=ReadbackByte(ReadbackUnsignedFloat((value>>11)&0x7ffu,6));
    out[2]=ReadbackByte(ReadbackUnsignedFloat(value>>22,5));break;
   }
   default:throw std::runtime_error("SaveImage retired attachment format changed");
  }
 }
 return output;
}
std::shared_ptr<PreparedScreenReadback> ScreenGPU::PrepareReadback(RuntimeLease& lease,
 PreparedScreenExternalImage image,ScreenScissor rectangle)
{
 Require(image.texture && image.storageOwner && image.requireReadable &&
  image.texture.device==lease.Device() && image.texture.textureType==MTLTextureType2D &&
  image.texture.sampleCount==1 && !image.texture.framebufferOnly,
  "SaveImage requires its actual owned readable single-sample color attachment or resolve");
 Require(rectangle.width && rectangle.height &&
  rectangle.width<=std::numeric_limits<std::size_t>::max()/4 &&
  rectangle.height<=std::numeric_limits<std::size_t>::max()/(std::size_t(rectangle.width)*4),
  "SaveImage output rectangle overflows host RGBA8 storage");
 auto prepared=std::shared_ptr<PreparedScreenReadback>(new PreparedScreenReadback());auto& state=*prepared->impl;
 state.pixelBytes=ReadbackPixelBytes(image.texture.pixelFormat);
 const auto x0=std::clamp(std::int64_t(rectangle.x),std::int64_t(0),std::int64_t(image.texture.width));
 const auto y0=std::clamp(std::int64_t(rectangle.y),std::int64_t(0),std::int64_t(image.texture.height));
 const auto x1=std::clamp(std::int64_t(rectangle.x)+rectangle.width,std::int64_t(0),std::int64_t(image.texture.width));
 const auto y1=std::clamp(std::int64_t(rectangle.y)+rectangle.height,std::int64_t(0),std::int64_t(image.texture.height));
 state.clipped={std::int32_t(x0),std::int32_t(y0),std::uint32_t(x1-x0),std::uint32_t(y1-y0)};
 state.rowBytes=std::size_t(state.clipped.width)*state.pixelBytes;
 if(state.rowBytes && state.clipped.height){
  Require(state.clipped.height<=std::numeric_limits<std::size_t>::max()/state.rowBytes,
   "SaveImage actual copy extent overflows host storage");
  const auto bytes=state.rowBytes*state.clipped.height;
  Require(bytes<=lease.Device().maxBufferLength,"SaveImage exceeds actual device readback buffer capacity");
  state.buffer=[lease.Device() newBufferWithLength:bytes options:MTLResourceStorageModeShared];
  Require(state.buffer!=nil,"Cannot allocate actual SaveImage readback buffer");
  lease.AddAllocation(state.buffer);
 }
 state.rectangle=rectangle;state.source=std::move(image);
 lease.AddAllocation(state.source.texture);
 auto lifetime=[M4ScreenReadbackLifetime new];lifetime->retained=prepared->impl;lease.Retain(lifetime);
 return prepared;
}

id<MTLTexture> ScreenGPU::PrepareAttachmentAllocation(RuntimeLease& lease,const std::shared_ptr<const ScreenTexture>& source)
{
 Require(source && source->renderTarget && !source->generation && (source->format==ScreenTextureFormat::R8Unorm || source->format==ScreenTextureFormat::Depth16Unorm),
  "Coverage FBO allocation requires original R8/D16 allocation identities");
 uint32_t uploads=0;return impl->Image(lease,source,uploads,{});
}
PreparedScreenImage ScreenGPU::PrepareImage(RuntimeLease& lease,const std::shared_ptr<const ScreenTexture>& source,
	const ScreenSampler& sampler,const PreparedScreenFrame* producers,std::optional<NativePostprocessConsumer> consumer,
	std::function<void(PreparedScreenExternalImage)> retainExternal,id<MTLSamplerState> actualSampler)
{
	std::vector<const ScreenTexture*> available;
	if(producers)for(const auto& image:producers->impl->producedImages)available.push_back(image.get());
	uint32_t uploads=0;
	if(actualSampler){Require(actualSampler.device==lease.Device(),"Shared screen image sampler belongs to another actual device");lease.Retain(actualSampler);}
	return {impl->Image(lease,source,uploads,available,consumer,std::move(retainExternal)),actualSampler?actualSampler:impl->Sampler(lease,sampler)};
}
std::unique_ptr<PreparedScreenFrame> ScreenGPU::PrepareColorAttachmentClear(RuntimeLease& lease,ScreenColor color,
	uint32_t width,uint32_t height,std::optional<ScreenScissor> scissor,std::array<bool,4> colorWrite,
	MTLPixelFormat format,bool validation,double& shaderMilliseconds,std::uint32_t samples)
{
	// The existing clear leaf clamps normalized color values. The selected
	// original Sensor clears are zero; reject unsupported float clear values
	// rather than silently clamp them through that normalized-image operation.
	for(float value:color)Require(std::isfinite(value)&&value>=0&&value<=1,"Original attachment clear exceeds the existing clear consumer's color range");
	ScreenDrawList geometry;geometry.ClearColorAttachment(color,width,height,scissor,colorWrite);
	const ScreenFrameView frame{&geometry,width,height,std::nullopt,{}};
	return PrepareList(lease,frame,width,height,width,height,format,validation,shaderMilliseconds,{},false,std::nullopt,MTLPixelFormatInvalid,MTLPixelFormatInvalid,samples);
}

std::unique_ptr<PreparedScreenFrame> ScreenGPU::PreparePresentationMargins(RuntimeLease& lease,
	const ScreenScissor& viewport,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,
	MTLPixelFormat color,bool validation,double& shaderMilliseconds,std::uint32_t samples)
{
	Require(logicalWidth && logicalHeight && viewport.x>=0 && viewport.y>=0 &&
		uint64_t(viewport.x)+viewport.width<=logicalWidth && uint64_t(viewport.y)+viewport.height<=logicalHeight,
		"Presentation safe viewport exceeds its actual logical window");
	const uint32_t left=uint32_t(viewport.x),bottom=uint32_t(viewport.y);
	const uint32_t right=left+viewport.width,top=bottom+viewport.height;
	const std::array<ScreenScissor,4> margins{{
		{0,0,left,logicalHeight},{int32_t(right),0,logicalWidth-right,logicalHeight},
		{int32_t(left),0,viewport.width,bottom},{int32_t(left),int32_t(top),viewport.width,logicalHeight-top}
	}};
	ScreenDrawList geometry;
	for(const auto& margin:margins)if(margin.width && margin.height)
		geometry.ClearColorAttachment({0,0,0,1},logicalWidth,logicalHeight,margin,{true,true,true,true});
	if(geometry.Commands().empty())return {};
	const ScreenFrameView frame{&geometry,logicalWidth,logicalHeight,std::nullopt,{}};
	// PrepareList preserves the existing original screen/RTT cache state.
	return PrepareList(lease,frame,width,height,logicalWidth,logicalHeight,color,validation,shaderMilliseconds,{},false,std::nullopt,MTLPixelFormatInvalid,MTLPixelFormatInvalid,samples);
}

std::unique_ptr<PreparedScreenFrame> ScreenGPU::PrepareList(RuntimeLease& lease,const ScreenFrameView& frame,uint32_t width,uint32_t height,
	uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat format,bool validation,double& shaderMs,
	const std::vector<const ScreenTexture*>& availableTargets,bool rootScreen,std::optional<NativePostprocessConsumer> targetConsumer,
	MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,std::uint32_t samples,std::uint32_t firstOriginalCommand)
{
	Require(frame.drawList&&!frame.drawList->IsRecording()&&frame.coordinateWidth&&frame.coordinateHeight,"Native UI requires completed frame and explicit logical coordinate basis");
	Require(width&&height&&logicalWidth&&logicalHeight,"Native UI requires actual window and target extents");
	Require(samples && [lease.Device() supportsTextureSampleCount:samples],"Native UI requires its actual supported output sample count");
	const auto& list=*frame.drawList;Require(list.Vertices().size()==list.TexCoords().size()&&list.Vertices().size()<=2'000'000&&list.Indices().size()<=12'000'000&&list.Commands().size()<=100'000,"Native UI geometry/UV extent invalid");
	for(const auto& v:list.Vertices()){Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"Native UI requires finite logical pixels and orthographic GL clip depth");for(float c:v.color)Require(std::isfinite(c),"Nonfinite UI color");}
	for(const auto& uv:list.TexCoords())for(float v:uv)Require(std::isfinite(v),"Nonfinite UI UV");for(uint32_t i:list.Indices())Require(i<list.Vertices().size(),"UI index exceeds source vertices");
	for(auto i=impl->textures.begin();i!=impl->textures.end();)if(i->second.owner.expired())i=impl->textures.erase(i);else ++i;
	auto prepared=std::make_unique<PreparedScreenFrame>();prepared->impl->sourceCommandCount=std::uint32_t(list.Commands().size());std::array<double,4> viewport{0,0,double(width),double(height)};bool visibleViewport=true;
	if(frame.logicalViewport){const auto& v=*frame.logicalViewport;Require(v.width<=16384&&v.height<=16384,"UI viewport outside numeric bounds");auto mapped=(v.width && v.height)?MapWorldViewport({v.x,v.y,int32_t(v.width),int32_t(v.height)},logicalWidth,logicalHeight,width,height):std::nullopt;if(mapped)viewport=mapped->viewport;else visibleViewport=false;}
	for (double value: viewport) Require(std::isfinite(value) && std::abs(value) <= 16384, "Mapped UI viewport outside numeric bounds");
	prepared->impl->viewport={viewport[0],viewport[1],viewport[2],viewport[3],0,1};
	Require(firstOriginalCommand<=list.Commands().size() && (rootScreen || !firstOriginalCommand),
		"Screen preparation suffix is outside its original root command stream");
	if(firstOriginalCommand==list.Commands().size())return prepared;
	const bool fixedGeometry=std::any_of(list.Commands().begin()+firstOriginalCommand,list.Commands().end(),[](const auto& command){return !command.pip && command.indexCount!=0;});
	id<MTLBuffer> vertices=nil,uvs=nil;
	if(fixedGeometry) {
		vertices=Buffer(lease,list.Vertices().data(),list.Vertices().size_bytes());
		uvs=Buffer(lease,list.TexCoords().data(),list.TexCoords().size_bytes());
		prepared->impl->indices=Buffer(lease,list.Indices().data(),list.Indices().size_bytes());
	}
	// A PiP-only list has its own original vertex/uniform storage. Never upload
	// unused fixed-function placeholders merely because its marker has no indices.
	std::vector<uint8_t> bytes(fixedGeometry?list.Commands().size()*256:0,0);
	for(size_t i=firstOriginalCommand;i<list.Commands().size();++i){if(list.Commands()[i].pip || list.Commands()[i].startBox || list.Commands()[i].weaponRange || list.Commands()[i].buildSquare || !list.Commands()[i].indexCount)continue;const auto& s=list.Commands()[i].state;Require(s.material<=ScreenMaterial::TerraformCapture&&s.uvMode<=ScreenUVMode::Texel&&s.alphaTest.compare<=ScreenAlphaCompare::Always&&std::isfinite(s.alphaTest.reference),"Unsupported native UI material/alpha state");Require(s.material==ScreenMaterial::Solid||s.material==ScreenMaterial::TargetPriorityStroke||s.material==ScreenMaterial::FrameGrapher||s.material==ScreenMaterial::DeferredVisualizer||s.material==ScreenMaterial::SpectatorHUD||s.material==ScreenMaterial::MiniMapBackground||s.texture!=nullptr,"Missing original UI texture");Uniform u{{1.0f/float(frame.coordinateWidth),1.0f/float(frame.coordinateHeight),1,1},{uint32_t(s.material),uint32_t(s.material==ScreenMaterial::MiniMapBackground?s.minimapBackground&&s.minimapBackground->base&&s.minimapBackground->base->flipV:s.texture&&s.texture->flipV),uint32_t(s.alphaTest.enabled),uint32_t(s.alphaTest.compare)},{s.alphaTest.reference,float(s.sampler.wrapS==ScreenWrap::LegacyClamp),float(s.sampler.wrapT==ScreenWrap::LegacyClamp),float(s.texture&&s.texture->incomplete)}};if(s.material==ScreenMaterial::TargetPriorityStroke||s.material==ScreenMaterial::FrameGrapher){u.dimensions[0]=s.targetPriorityViewGeometry[0];u.dimensions[1]=s.targetPriorityViewGeometry[1];if(s.material==ScreenMaterial::FrameGrapher)u.dimensions1=s.frameGrapherParameters;};if(s.texture&&!s.texture->incomplete&&s.uvMode==ScreenUVMode::Texel){Require(s.texture->width&&s.texture->height,"Empty texel-coordinate atlas");u.dimensions[2]=1.0f/float(s.texture->width);u.dimensions[3]=1.0f/float(s.texture->height);}if(s.secondary.texture && s.material!=ScreenMaterial::FontCoverage && s.material!=ScreenMaterial::MiniMapBackground){const auto& second=s.secondary;ValidateScreenSampler(second.sampler);Require(second.uvMode<=ScreenUVMode::Texel,"Unsupported secondary texture coordinate mode");u.dimensions1={1,1,0,0};u.options1={1,uint32_t(second.texture->flipV),0,0};u.parameters1={0,float(second.sampler.wrapS==ScreenWrap::LegacyClamp),float(second.sampler.wrapT==ScreenWrap::LegacyClamp),float(second.texture->incomplete)};if(second.uvMode==ScreenUVMode::Texel&&!second.texture->incomplete){Require(second.texture->width&&second.texture->height,"Empty secondary texel-coordinate image");u.dimensions1[0]=1.f/second.texture->width;u.dimensions1[1]=1.f/second.texture->height;}}if(s.material==ScreenMaterial::DecalCapture){Require(s.decalCapture!=nullptr,"Decal Capture immutable uniforms missing");ValidateNativeDecalCapture(*s.decalCapture);u.capture=s.decalCapture->uniforms;}std::memcpy(bytes.data()+i*256,&u,sizeof(u));}
	id<MTLBuffer> uniforms=fixedGeometry?Buffer(lease,bytes.data(),bytes.size()):nil;NSError* error=nil;
	// Only earlier ordered targets are available to a typed shader, exactly as
	// for fixed-function draws. This proxy owns no executable producers.
	std::unique_ptr<PreparedScreenFrame> pipProducers;
	if(!availableTargets.empty() && std::any_of(list.Commands().begin()+firstOriginalCommand,list.Commands().end(),[](const auto& command){return bool(command.pip)||bool(command.startBox)||bool(command.weaponRange)||bool(command.buildSquare)||bool(command.sensorPreview)||bool(command.sensorLOS);})) {
		pipProducers=std::make_unique<PreparedScreenFrame>();
		for(const auto* target:availableTargets) {
			const auto found=impl->textures.find(target);Require(found!=impl->textures.end(),"Typed screen image has no allocated ordered target");
			auto owner=found->second.owner.lock();Require(owner!=nullptr,"Typed screen ordered target lost its immutable owner");
			pipProducers->impl->producedImages.push_back(std::move(owner));
		}
	}
	for(size_t i=firstOriginalCommand;i<list.Commands().size();++i) {
		const auto& command=list.Commands()[i];const auto& state=command.state;
		Require(command.indexCount%3==0&&command.firstIndex<=list.Indices().size()&&command.indexCount<=list.Indices().size()-command.firstIndex,"Invalid original UI draw range");
		// Viewport is original per-draw window state; changing it never moves the
		// existing scissor. glClear ignores it and still uses full attachments.
		const std::array<double,4> fullViewport{0,0,double(width),double(height)};
		auto drawViewport=viewport;
		if(state.attachmentClear)drawViewport=fullViewport;
		else if(state.viewport){const auto& v=*state.viewport;Require(v.width<=INT32_MAX && v.height<=INT32_MAX,"Original viewport exceeds GLsizei");if(!v.width || !v.height)continue;auto mapped=MapWorldViewport({v.x,v.y,std::int32_t(v.width),std::int32_t(v.height)},logicalWidth,logicalHeight,width,height);if(!mapped)continue;drawViewport=mapped->viewport;}
		else if(!visibleViewport)continue;
		const bool absoluteScissor=state.scissor && state.scissor->windowCoordinates;
		const bool windowBasis=state.attachmentClear || absoluteScissor;
		auto scissor=Scissor(state.scissor,windowBasis?fullViewport:viewport,windowBasis?logicalWidth:frame.coordinateWidth,
			windowBasis?logicalHeight:frame.coordinateHeight,width,height);
		if(!scissor)continue;
		const auto consumer=rootScreen?std::optional(NativePostprocessConsumer{NativePostprocessStage::Screen,0,std::uint32_t(i)}):targetConsumer;
		if(command.unitIcons){
			Require(rootScreen && command.indexCount==0 && !state.attachmentClear,"Original unit icons require ordered root DrawScreenEffects insertion");
			ValidateNativeUnitIconFrame(*command.unitIcons);
			WorldCamera camera{};camera.viewport={0,0,int32_t(logicalWidth),int32_t(logicalHeight)};
			if(frame.logicalViewport){const auto& v=*frame.logicalViewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
			auto packet=impl->unitIcons.Prepare(lease,command.unitIcons,camera,width,height,logicalWidth,logicalHeight,*this,nullptr,format,depthFormat,stencilFormat,samples,validation,shaderMs,consumer);
			if(packet && packet->DrawCount()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);draw.unitIcons=std::move(packet);prepared->impl->draws.push_back(std::move(draw));}
			continue;
		}
		if(command.healthBars){
			Require(rootScreen && command.indexCount==0 && !state.attachmentClear,
				"Original Healthbars require their ordered root DrawScreenEffects insertion");
			const auto& source=*command.healthBars;
			ValidateNativeHealthBarsDraw(source);
			if(state.cull==ScreenCull::FrontAndBack)continue;
			if(!source.count)continue;
			// Immutable original stream/model/UBO uploads only. Health, animation,
			// camera projection, strip topology and glyph decisions run on GPU.
			auto points=impl->HealthBarsPoints(lease,source.vertices);
			const auto modelRows=ReadNativeHealthBarsModelRows(source);
			auto models=Buffer(lease,modelRows.data(),modelRows.size()*sizeof(NativeHealthBarsModelRow));
			const auto engine=ReadNativeUnitPrimitiveUniforms(source.uniforms);
			HealthBarsUniform u{engine.viewProjection,engine.viewInverse,engine.time,
				{source.iconDistance,source.cameraDistanceMult,source.cameraDistanceMultGlyph,source.skipGlyphsNumbers},
				{source.config.height,source.config.corner,source.config.smallCorner,float(source.config.debugShow)},
				{std::uint32_t(engine.clipDepth==ClipDepthConvention::MinusOneToOne),source.first,0,0},
				{std::uint32_t(source.atlas->flipV),std::uint32_t(source.sampler.wrapS==ScreenWrap::LegacyClamp),std::uint32_t(source.sampler.wrapT==ScreenWrap::LegacyClamp),0}};
			auto parameters=Buffer(lease,&u,sizeof(u));
			std::vector<PreparedScreenExternalImage> external;
			const auto retainExternal=[&](PreparedScreenExternalImage image){external.push_back(std::move(image));};
			auto image=impl->Image(lease,source.atlas,prepared->impl->uploads,availableTargets,consumer,retainExternal);
			auto sampler=impl->Sampler(lease,source.sampler);
			MTL4ArgumentTableDescriptor* d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=3;d.maxTextureBindCount=1;d.maxSamplerStateBindCount=1;d.initializeBindings=YES;
			id<MTL4ArgumentTable> args=[lease.Device() newArgumentTableWithDescriptor:d error:&error];Require(args!=nil,"Cannot allocate original Healthbars arguments: "+Error(error));
			[args setAddress:points.gpuAddress atIndex:0];[args setAddress:models.gpuAddress atIndex:1];[args setAddress:parameters.gpuAddress atIndex:2];
			[args setTexture:image.gpuResourceID atIndex:0];[args setSamplerState:sampler.gpuResourceID atIndex:0];lease.Retain(args);
			PreparedScreenFrame::Impl::Draw draw;
			draw.pipeline=impl->Pipeline(lease,state.blend,state.colorWrite,format,validation,shaderMs,false,depthFormat,stencilFormat,0,true,samples,state.alphaToCoverage);
			// Root screen already loads/stores the actual world depth. Preserve
			// this original DepthTest/DepthMask rather than substituting a HUD layer.
			draw.depth=impl->DepthStencil(lease,state.depthStencil,depthFormat,stencilFormat);
			draw.arguments=args;draw.viewport={drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1};draw.scissor=*scissor;
			draw.frontReference=std::clamp(state.depthStencil.frontStencil.reference,0,255);draw.backReference=std::clamp(state.depthStencil.backStencil.reference,0,255);
			draw.count=78;draw.healthBarsInstances=source.count;draw.originalCommand=std::uint32_t(i);draw.externalImages=std::move(external);draw.cull=state.cull;draw.healthBars=true;
			prepared->impl->draws.push_back(std::move(draw));continue;
		}
		if(command.panelShader) {
			Require(command.indexCount==0 && !state.attachmentClear,"Panel shader competes with fixed geometry or an attachment clear");
			const auto& source=*command.panelShader;ValidateNativePanelShaderDraw(source);
			if(!source.instanceCount || state.cull==ScreenCull::FrontAndBack)continue;
			PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);
			const auto retainExternal=[&](PreparedScreenExternalImage image){draw.externalImages.push_back(std::move(image));};
			std::array<id<MTLTexture>,3> images{};std::array<id<MTLSamplerState>,3> samplers{};
			for(unsigned slot=0;slot<3;++slot) {
				images[slot]=impl->Image(lease,source.images[slot],prepared->impl->uploads,availableTargets,consumer,retainExternal);
				samplers[slot]=impl->Sampler(lease,source.samplers[slot]);
			}
			draw.panelShader=impl->panel.Prepare(lease,source,images,samplers,height,format,validation,shaderMs,samples);
			draw.panelShaderOwner=command.panelShader;
			draw.depth=impl->DepthStencil(lease,state.depthStencil,depthFormat,stencilFormat);
			draw.frontReference=std::uint32_t(std::clamp(state.depthStencil.frontStencil.reference,0,255));
			draw.backReference=std::uint32_t(std::clamp(state.depthStencil.backStencil.reference,0,255));
			draw.viewport={drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1};
			draw.scissor=*scissor;draw.cull=state.cull;prepared->impl->draws.push_back(std::move(draw));
			continue;
		}
  if(command.sensorLOS){
   Require(command.indexCount==0&&!state.attachmentClear&&bool(impl->sensorLOS)&&consumer.has_value(),"LOS fan requires its actual target/root provider");
   ValidateNativeSensorLOSDraw(*command.sensorLOS);Require(command.sensorLOS->program.defines->stencilPass,"LOS outline cannot enter target renderer");
   SensorPreviewTarget target;target.width=width;target.height=height;target.logicalWidth=logicalWidth;target.logicalHeight=logicalHeight;
   target.color=format;target.depth=depthFormat;target.stencil=stencilFormat;target.samples=samples;
   target.cameraViewport={0,0,int32_t(logicalWidth),int32_t(logicalHeight)};
   auto packet=impl->sensorLOS(lease,*command.sensorLOS,target,pipProducers.get(),validation,shaderMs,*consumer);
   Require(packet!=nullptr,"LOS provider returned no actual fan consumer");
   if(!packet->Empty()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=uint32_t(i);draw.sensorLOS=std::move(packet);prepared->impl->draws.push_back(std::move(draw));}
   continue;
  }
  if(command.sensorPreview){
   Require(command.indexCount==0&&!state.attachmentClear&&bool(impl->sensorPreviews)&&consumer.has_value(),
    "Sensor Preview minimap requires its actual target insertion and root-owned GPU provider");
   ValidateNativeSensorPreviewDraw(*command.sensorPreview);
   Require(command.sensorPreview->callin==NativeSensorPreviewCallin::Minimap&&
    command.sensorPreview->program.source->kind==NativeSensorPreviewProgramKind::Minimap,
    "Only the original final Minimap program belongs to this ScreenGPU marker");
   SensorPreviewTarget target;target.width=width;target.height=height;
   target.logicalWidth=logicalWidth;target.logicalHeight=logicalHeight;
   target.color=format;target.depth=depthFormat;target.stencil=stencilFormat;target.samples=samples;
   const auto originalViewport=frame.logicalViewport.value_or(ScreenScissor{0,0,logicalWidth,logicalHeight});
   Require(originalViewport.width<=INT32_MAX&&originalViewport.height<=INT32_MAX,"Sensor Preview target viewport exceeds GLsizei");
   target.cameraViewport={originalViewport.x,originalViewport.y,int32_t(originalViewport.width),int32_t(originalViewport.height)};
   auto packet=impl->sensorPreviews(lease,*command.sensorPreview,target,pipProducers.get(),validation,shaderMs,*consumer);
   Require(packet&&!packet->IsTarget(),"Sensor Preview provider returned no actual final-attachment command");
   if(!packet->Empty()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);
    draw.sensorPreview=std::move(packet);draw.sensorPreviewOwner=command.sensorPreview;prepared->impl->draws.push_back(std::move(draw));}
   continue;
  }
		if(command.buildSquare){
			Require(command.indexCount==0&&!state.attachmentClear,"Original BuildSquare typed marker competes with fixed geometry");
			auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> c){Require(bool(impl->startBoxImages),"BuildSquare height resolver unavailable");return impl->startBoxImages(l,b,pipProducers.get(),c);};
			auto packet=impl->buildSquare.Prepare(lease,*command.buildSquare,resolver,
				MTLViewport{drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1},*scissor,
				format,depthFormat,stencilFormat,samples,validation,shaderMs,consumer);
			if(!packet->Empty()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);draw.buildSquare=std::move(packet);prepared->impl->draws.push_back(std::move(draw));}
			continue;
		}
		if(command.weaponRange){
			Require(command.indexCount==0 && !state.attachmentClear && bool(impl->startBoxImages),"Original weapon range requires its ordered consumer and actual image owner");
			auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> c){return impl->startBoxImages(l,b,pipProducers.get(),c);};
			auto packet=impl->weaponRange.Prepare(lease,*command.weaponRange,resolver,
				MTLViewport{drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1},*scissor,{width,height},
				format,depthFormat,stencilFormat,samples,validation,shaderMs,consumer);
			if(!packet->Empty()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);draw.weaponRange=std::move(packet);prepared->impl->draws.push_back(std::move(draw));}
			continue;
		}		if(command.startBox){
			Require(command.indexCount==0 && !state.attachmentClear && bool(impl->startBoxImages),"Original StartBox requires its ordered consumer and actual image owner");
			auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> c){return impl->startBoxImages(l,b,pipProducers.get(),c);};
			auto packet=impl->startBox.Prepare(lease,*command.startBox,resolver,
				MTLViewport{drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1},*scissor,
				format,depthFormat,stencilFormat,samples,validation,shaderMs,consumer);
			if(!packet->Empty()){PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);draw.startBox=std::move(packet);prepared->impl->draws.push_back(std::move(draw));}
			continue;
		}
		if(command.pip) {
			Require(command.indexCount==0 && !state.attachmentClear,"Typed PiP packet competes with fixed-function geometry or an attachment clear");
			auto packet=impl->pip.Prepare(lease,*command.pip,*this,pipProducers.get(),
				MTLViewport{drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1},*scissor,
				format,depthFormat,stencilFormat,validation,shaderMs,consumer,impl->pipSourceImages,samples,width,height);
			if(!packet->Empty()) {PreparedScreenFrame::Impl::Draw draw;draw.originalCommand=std::uint32_t(i);draw.pip=std::move(packet);draw.pipOwner=command.pip;prepared->impl->draws.push_back(std::move(draw));}
			continue;
		}
		if(!command.indexCount || (!state.attachmentClear && state.cull==ScreenCull::FrontAndBack))continue;
		const bool map=state.material==ScreenMaterial::MiniMapBackground;
  const bool visualizer=state.material==ScreenMaterial::DeferredVisualizer;
  const bool splat=state.material==ScreenMaterial::SplatPainter;
  const bool diffuse=state.material==ScreenMaterial::DiffusePainter;
  const bool terraform=state.material==ScreenMaterial::TerraformCapture;
  if(terraform){Require(state.terraformCapture!=nullptr,"Terraform Capture immutable input missing");ValidateNativeTerraformCapture(*state.terraformCapture);}
  if(diffuse){Require(state.diffusePainter!=nullptr,"Diffuse Painter immutable input missing");ValidateNativeDiffusePainter(*state.diffusePainter);}
  if(splat){Require(state.splatPainter!=nullptr,"Splat Painter immutable input missing");ValidateNativeSplatPainter(*state.splatPainter);}
  if(visualizer){Require(state.deferredVisualizer!=nullptr,"Deferred visualizer captured inputs missing");ValidateNativeDeferredVisualizer(*state.deferredVisualizer);}
		const auto depthImage=[](const auto& image){return image&&!image->incomplete&&(image->format==ScreenTextureFormat::Depth32Float||image->format==ScreenTextureFormat::Depth24Unorm||image->format==ScreenTextureFormat::Depth16Unorm);};
		const unsigned sampledDepthMask=map?0u:unsigned(state.material!=ScreenMaterial::Solid&&depthImage(state.texture))|(unsigned(state.material!=ScreenMaterial::FontCoverage&&depthImage(state.secondary.texture))<<1);
		auto pipeline=impl->Pipeline(lease,state.blend,state.colorWrite,format,validation,shaderMs,map,depthFormat,stencilFormat,sampledDepthMask,false,samples,!state.attachmentClear && state.alphaToCoverage,visualizer,splat?unsigned(state.splatPainter->program)+1:0,diffuse?unsigned(state.diffusePainter->program)+1:0,terraform?unsigned(state.terraformCapture->program)+1:0);
		std::vector<PreparedScreenExternalImage> external;
		const auto retainExternal=[&](PreparedScreenExternalImage image){external.push_back(std::move(image));};
		auto depth=impl->DepthStencil(lease,state.depthStencil,depthFormat,stencilFormat);
		std::shared_ptr<PreparedMiniMap> background;
		if(map) {
			Require(state.minimapBackground!=nullptr,"Missing original minimap background snapshot");
			background=impl->minimaps.Prepare(lease,state.minimapBackground,validation,shaderMs);
			prepared->impl->minimaps.push_back(background);
		}
		auto image=impl->Image(lease,map?state.minimapBackground->base:((state.material==ScreenMaterial::Solid||state.material==ScreenMaterial::TargetPriorityStroke||state.material==ScreenMaterial::FrameGrapher||state.material==ScreenMaterial::SpectatorHUD||visualizer)?nullptr:state.texture),prepared->impl->uploads,availableTargets,consumer,retainExternal);
		auto sampler=impl->Sampler(lease,state.sampler);
		const bool secondary=!map&&state.material!=ScreenMaterial::FontCoverage&&state.secondary.texture!=nullptr;
		auto image1=map?nil:impl->Image(lease,secondary?state.secondary.texture:nullptr,prepared->impl->uploads,availableTargets,consumer,retainExternal);
		auto sampler1=map?nil:impl->Sampler(lease,state.secondary.sampler);
		MTL4ArgumentTableDescriptor* d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=(map||visualizer||splat||diffuse||terraform)?4:3;d.maxTextureBindCount=visualizer?13:diffuse?4:(map?3:2);d.maxSamplerStateBindCount=visualizer?13:diffuse?4:(map?1:2);d.initializeBindings=YES;
		id<MTL4ArgumentTable> args=[lease.Device() newArgumentTableWithDescriptor:d error:&error];Require(args!=nil,"Cannot allocate native UI argument table: "+Error(error));
		[args setAddress:vertices.gpuAddress atIndex:0];[args setAddress:uvs.gpuAddress atIndex:1];[args setAddress:uniforms.gpuAddress+i*256 atIndex:2];
		[args setTexture:image.gpuResourceID atIndex:0];[args setSamplerState:sampler.gpuResourceID atIndex:0];
		if(!map){[args setTexture:image1.gpuResourceID atIndex:1];[args setSamplerState:sampler1.gpuResourceID atIndex:1];}
  if(visualizer){
   std::array<std::array<uint32_t,4>,13> flags{};
   for(unsigned slot=0;slot<13;++slot){const auto& source=state.deferredVisualizer->images[slot];auto texture=impl->Image(lease,source.texture,prepared->impl->uploads,availableTargets,consumer,retainExternal);auto s=impl->Sampler(lease,source.sampler);[args setTexture:texture.gpuResourceID atIndex:slot];[args setSamplerState:s.gpuResourceID atIndex:slot];flags[slot]={uint32_t(source.texture->flipV),uint32_t(source.sampler.wrapS==ScreenWrap::LegacyClamp),uint32_t(source.sampler.wrapT==ScreenWrap::LegacyClamp),0};}
   auto parameters=Buffer(lease,flags.data(),sizeof(flags));[args setAddress:parameters.gpuAddress atIndex:3];
  }
  if(terraform){
   NativeTerraformCaptureParameters values;values.uniforms=state.terraformCapture->uniforms;
   for(unsigned slot=0;slot<(state.terraformCapture->program==NativeTerraformCaptureProgram::Reproject?2u:1u);++slot){const auto& source=state.terraformCapture->images[slot];auto texture=impl->Image(lease,source.texture,prepared->impl->uploads,availableTargets,consumer,retainExternal);auto sampling=impl->Sampler(lease,source.sampler);
    [args setTexture:texture.gpuResourceID atIndex:slot];[args setSamplerState:sampling.gpuResourceID atIndex:slot];values.sampling[slot]={uint32_t(source.texture->flipV),uint32_t(source.sampler.wrapS==ScreenWrap::LegacyClamp),uint32_t(source.sampler.wrapT==ScreenWrap::LegacyClamp),0};}
   auto parameters=Buffer(lease,&values,sizeof(values));[args setAddress:parameters.gpuAddress atIndex:3];
  }
  if(diffuse){
   NativeDiffusePainterParameters values;values.uniforms=state.diffusePainter->uniforms;
   const unsigned count=state.diffusePainter->program==NativeDiffusePainterProgram::Copy?1:4;
   for(unsigned slot=0;slot<count;++slot){const auto& source=state.diffusePainter->images[slot];auto texture=impl->Image(lease,source.texture,prepared->impl->uploads,availableTargets,consumer,retainExternal);auto sampling=impl->Sampler(lease,source.sampler);
    [args setTexture:texture.gpuResourceID atIndex:slot];[args setSamplerState:sampling.gpuResourceID atIndex:slot];values.sampling[slot]={uint32_t(source.texture->flipV),uint32_t(source.sampler.wrapS==ScreenWrap::LegacyClamp),uint32_t(source.sampler.wrapT==ScreenWrap::LegacyClamp),uint32_t(source.texture->incomplete)};}
   auto parameters=Buffer(lease,&values,sizeof(values));[args setAddress:parameters.gpuAddress atIndex:3];
  }
		if(splat){auto parameters=Buffer(lease,&state.splatPainter->uniforms,sizeof(NativeSplatPainterUniforms));[args setAddress:parameters.gpuAddress atIndex:3];}
		if(background) {[args setAddress:background->Uniforms().gpuAddress atIndex:3];[args setTexture:background->Shading().gpuResourceID atIndex:1];[args setTexture:background->Info().gpuResourceID atIndex:2];}
		lease.Retain(args);prepared->impl->draws.push_back({pipeline,depth,args,MTLViewport{drawViewport[0],drawViewport[1],drawViewport[2],drawViewport[3],0,1},*scissor,
			uint32_t(std::clamp(state.depthStencil.frontStencil.reference,0,255)),uint32_t(std::clamp(state.depthStencil.backStencil.reference,0,255)),
			command.firstIndex,command.indexCount,std::uint32_t(i)});
		prepared->impl->draws.back().externalImages=std::move(external);
		prepared->impl->draws.back().cull=state.attachmentClear?ScreenCull::None:state.cull;
	}
	return prepared;
}
} // namespace rendering::metal4
