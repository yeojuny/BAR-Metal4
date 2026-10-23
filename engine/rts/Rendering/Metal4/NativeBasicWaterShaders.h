/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
// Original RenderBufferShader<VA_TYPE_T>, used by CBasicWater::Draw. SetupFog
// changes fixed-function state, but this original GLSL shader never reads fog.
inline constexpr const char* NativeBasicWaterShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BasicVertex {packed_float3 position;packed_float2 uv;};
struct BasicUniforms {float4x4 viewProjection;uint4 flags;};
struct BasicVarying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex BasicVarying BasicWaterVertex(uint vid [[vertex_id]],device const BasicVertex* v [[buffer(0)]],constant BasicUniforms& u [[buffer(1)]]) {
	BasicVarying out;out.position=u.viewProjection*float4(float3(v[vid].position),1);
	if(u.flags.x)out.position.z=(out.position.z+out.position.w)*.5f;
	out.uv=float2(v[vid].uv);return out;
}
fragment float4 BasicWaterFragment(BasicVarying v [[stage_in]],constant BasicUniforms& u [[buffer(1)]],texture2d<float> tex [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
	const float2 uv=float2(v.uv.x,u.flags.y?1-v.uv.y:v.uv.y);
	return tex.sample(imageSampler,uv)*float4(.7f,.7f,.7f,.5f);
}
)MSL";
} // namespace rendering::metal4
