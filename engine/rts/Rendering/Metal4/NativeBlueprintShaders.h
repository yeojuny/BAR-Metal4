/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

namespace rendering::metal4 {

// Source-paired exact30770 api_blueprint and gfx_DrawUnitShape_GL4 equations.
// This leaf is not general GLSL support. Consumer availability remains false
// until model capture, ordered stencil clear and the two-pixel line contract
// are connected and validated by the real world pass.
inline constexpr const char* NativeBlueprintShaderSource = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;

struct BlueprintTransform { float4 quaternion; float4 translationScale; };
struct BlueprintModelVertexData { packed_float3 position; packed_float2 uv; uint boneIDs; };
struct BlueprintModelUniform {
	float4x4 viewProjection;
	float4 worldPositionRotation;
	float4 parameters;
	float4 teamColor;
	float4 cameraPositionIconDistance;
	float4 timeInfo;
	uint4 options; // depth conversion, texture0 V flip, texture1 V flip, alpha test flags
	float4 alphaState;
 float4 pitchRoll,tint;
};
struct BlueprintModelOut {
	float4 position [[position]];
	float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 parameters [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 teamColor [[RECOIL_SAMPLE_PERSPECTIVE]];
	float3 worldPosition [[RECOIL_SAMPLE_PERSPECTIVE]];
};
bool BlueprintAlpha(float a, float r, uint comparison) {
	switch (comparison) {case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;case 7:return true;}return false;
}
float3 BlueprintRotateQuaternion(float4 q, float3 v)
{
	return 2.f * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2.f * q.w * cross(q.xyz, v);
}
vertex BlueprintModelOut BlueprintModelVertex(uint vertexID [[vertex_id]],
	device const BlueprintModelVertexData* vertices [[buffer(0)]],
	device const BlueprintTransform* transforms [[buffer(1)]],
	constant BlueprintModelUniform& u [[buffer(2)]])
{
	const auto inputVertex = vertices[vertexID];
	// Exact source GetStaticPieceModelTransform is used even by UnitGL4 when
	// USEQUATERNIONS=1. Do not silently replace it with another animation path.
	const auto transform = transforms[inputVertex.boneIDs & 0x000000ffu];
	float3 localModelPosition = BlueprintRotateQuaternion(transform.quaternion,
		float3(inputVertex.position) * transform.translationScale.w) + transform.translationScale.xyz;
	const float sine = sin(u.worldPositionRotation.w), cosine = cos(u.worldPositionRotation.w);
	const float3x3 rotation(cosine, 0.f, -sine, 0.f, 1.f, 0.f, sine, 0.f, cosine);
	const float3 worldPosition = rotation * localModelPosition + u.worldPositionRotation.xyz;
	BlueprintModelOut out;
	out.position = u.viewProjection * float4(worldPosition, 1.f);
	if (u.options.x) out.position.z = (out.position.z + out.position.w) * .5f;
	out.teamColor = float4(u.teamColor.rgb, u.parameters.x);
	const float3 modelBaseToCamera = u.cameraPositionIconDistance.xyz - u.worldPositionRotation.xyz;
	if (dot(modelBaseToCamera, modelBaseToCamera) > u.cameraPositionIconDistance.w * u.cameraPositionIconDistance.w) {
		if (u.parameters.y <= .5f) out.teamColor.a = 0.f;
	}
	out.parameters = u.parameters;
	out.uv = float2(inputVertex.uv);
	out.worldPosition = worldPosition;
	return out;
}
fragment float4 BlueprintModelFragment(BlueprintModelOut in [[stage_in]],
	constant BlueprintModelUniform& u [[buffer(2)]],
	texture2d<float> texture0 [[texture(0)]], sampler sampler0 [[sampler(0)]],
	texture2d<float> texture1 [[texture(1)]], sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	float2 uv0 = in.uv, uv1 = in.uv;
	if (u.options.y) uv0.y = 1.f - uv0.y;
	if (u.options.z) uv1.y = 1.f - uv1.y;
	float4 modelColor = texture0.sample(sampler0, uv0);
	const float4 extraColor = texture1.sample(sampler1, uv1);
	modelColor += modelColor * extraColor.r;
	modelColor.a *= extraColor.a;
	modelColor.rgb = mix(modelColor.rgb, in.teamColor.rgb, modelColor.a);
	modelColor.a *= in.teamColor.a;
	modelColor.rgb = mix(modelColor.rgb, in.teamColor.rgb, in.parameters.z);
	if (in.parameters.w > 0.f) {
		modelColor.rgb = mix(modelColor.rgb, float3(1.f),
			in.parameters.w * fract(in.worldPosition.y * .03f + (u.timeInfo.x + u.timeInfo.w) * .05f));
	}
	if ((u.options.w & 256u) && !BlueprintAlpha(in.teamColor.a,u.alphaState.x,u.options.w & 255u)) discard_fragment();
	return float4(modelColor.rgb, in.teamColor.a);
}

// Original FeatureShape: static piece base has no leading unit world matrix.
struct BlueprintFeatureOut {
 float4 position [[position]];
 float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 parameters [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 teamColor [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 tint [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex BlueprintFeatureOut BlueprintFeatureVertex(uint vertexID [[vertex_id]],
 device const BlueprintModelVertexData* vertices [[buffer(0)]],
 device const BlueprintTransform* transforms [[buffer(1)]],
 constant BlueprintModelUniform& u [[buffer(2)]]) {
 const auto v=vertices[vertexID];const auto t=transforms[v.boneIDs&255u];
 float3 local=BlueprintRotateQuaternion(t.quaternion,float3(v.position)*t.translationScale.w)+t.translationScale.xyz;
 local*=u.pitchRoll.z>0.f?u.pitchRoll.z:1.f; // Includes original piece offsets.
 // Column-vector Rx(-pitch)*Ry(-yaw)*Rz(-roll), so apply Z first.
 const float sz=sin(-u.pitchRoll.y),cz=cos(-u.pitchRoll.y);
 local=float3(cz*local.x-sz*local.y,sz*local.x+cz*local.y,local.z);
 const float sy=sin(-u.worldPositionRotation.w),cy=cos(-u.worldPositionRotation.w);
 local=float3(cy*local.x+sy*local.z,local.y,-sy*local.x+cy*local.z);
 const float sx=sin(-u.pitchRoll.x),cx=cos(-u.pitchRoll.x);
 local=float3(local.x,cx*local.y-sx*local.z,sx*local.y+cx*local.z);
 BlueprintFeatureOut out;out.position=u.viewProjection*float4(local+u.worldPositionRotation.xyz,1.f);
 if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;
 out.uv=float2(v.uv);out.parameters=u.parameters;out.teamColor=u.teamColor;out.tint=u.tint;return out;
}
fragment float4 BlueprintFeatureFragment(BlueprintFeatureOut in [[stage_in]],
 constant BlueprintModelUniform& u [[buffer(2)]],
 texture2d<float> texture0 [[texture(0)]],sampler sampler0 [[sampler(0)]],
 texture2d<float> texture1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 uv0=in.uv,uv1=in.uv;if(u.options.y)uv0.y=1.f-uv0.y;if(u.options.z)uv1.y=1.f-uv1.y;
 const float4 color=texture0.sample(sampler0,uv0),extra=texture1.sample(sampler1,uv1);
 if(extra.a<.5f)discard_fragment();
 float3 albedo=mix(color.rgb,in.teamColor.rgb,color.a);albedo+=albedo*extra.r;
 albedo=mix(albedo,in.tint.rgb,in.parameters.y);albedo=mix(albedo,float3(1.f),in.parameters.z);
 if((u.options.w&256u)&&!BlueprintAlpha(in.parameters.x,u.alphaState.x,u.options.w&255u))discard_fragment();
 return float4(albedo,in.parameters.x);
}

struct BlueprintOutlineInstance { packed_float3 position; packed_float2 dimensions; packed_float4 color; };
struct BlueprintOutlineUniform { float4x4 viewProjection; float4 mapWorldSize; uint4 options; };
struct BlueprintOutlineOut { float4 position [[position]]; float4 color [[RECOIL_SAMPLE_PERSPECTIVE]]; };
float2 BlueprintHeightUV(float2 worldPosition, float2 mapWorldSize)
{
	const float2 inverse = float2(1.f) / mapWorldSize;
	worldPosition += float2(-8.f) * (worldPosition * inverse) + float2(4.f);
	return clamp(worldPosition, float2(8.f), mapWorldSize - float2(8.f)) * inverse;
}
// The original line-strip vertex program. The connected consumer must apply
// its existing clipped two-pixel line rasterizer; Metal's one-pixel line
// primitive is not an acceptable replacement for gl.LineWidth(2).
vertex BlueprintOutlineOut BlueprintOutlineVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]],
	device const packed_float2* vertices [[buffer(0)]], device const BlueprintOutlineInstance* instances [[buffer(1)]],
	constant BlueprintOutlineUniform& u [[buffer(2)]], texture2d<float> heightmap [[texture(0)]], sampler heightSampler [[sampler(0)]])
{
	const auto instance = instances[instanceID]; const float2 localPosition = float2(vertices[vertexID]);
	const float3 world = float3(instance.position); const float2 dimensions = float2(instance.dimensions);
	const float2 uv = BlueprintHeightUV(world.xz, u.mapWorldSize.xy);
	const float3 result(world.x + localPosition.x * dimensions.x, heightmap.sample(heightSampler, uv, level(0.f)).x,
		world.z + localPosition.y * dimensions.y);
	BlueprintOutlineOut out; out.position = u.viewProjection * float4(result, 1.f);
	if (u.options.x) out.position.z = (out.position.z + out.position.w) * .5f;
	out.color = float4(instance.color); return out;
}
fragment float4 BlueprintOutlineFragment(BlueprintOutlineOut in [[stage_in]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID)); return in.color; }

// Common original line coverage keeps clipped homogeneous coordinates and
// expands the source two-pixel strip with the established aliased GL rasterizer.
struct BlueprintRasterVertexData { packed_float4 clip, color, uv; };
struct BlueprintRasterUniform { uint4 options; float4 alphaState; };
vertex BlueprintOutlineOut BlueprintRasterVertex(uint vertexID [[vertex_id]], device const BlueprintRasterVertexData* vertices [[buffer(0)]]) {
	const auto v=vertices[vertexID];BlueprintOutlineOut out;out.position=float4(v.clip);
	out.position.z=(out.position.z+out.position.w)*.5f;out.color=float4(v.color);return out;
}
fragment float4 BlueprintRasterFragment(BlueprintOutlineOut in [[stage_in]],constant BlueprintRasterUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	if(u.options.x && !BlueprintAlpha(in.color.a,u.alphaState.x,u.options.y))discard_fragment();return in.color;
}

// Ordered stencil clear uses full framebuffer viewport and only the source
// scissor/writeMask. The host disables color/depth writes and uses ALWAYS /
// REPLACE with the source clear value. No active material can discard pixels.
vertex float4 BlueprintStencilClearVertex(uint vertexID [[vertex_id]])
{
	constexpr float2 positions[3] = {float2(-1.f,-1.f),float2(3.f,-1.f),float2(-1.f,3.f)};
	return float4(positions[vertexID],0.f,1.f);
}
fragment float4 BlueprintStencilClearFragment( RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) { return float4(0.f); }
)MSL";

} // namespace rendering::metal4
