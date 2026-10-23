/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::missileThruster {
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
struct MissileThrusterUniform {float4x4 cameraViewProjection,cameraViewInverse;float4 timeInfo;float defines[12];uint4 options;};
struct MissileThrusterInstance {float4 posAndSize,dirAndLength,color1,color2,glowData;};
struct FlameVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 flameColor [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct CrossSectionVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float3 flameColor [[RECOIL_SAMPLE_PERSPECTIVE]];float headOnFactor [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct GlowVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex FlameVarying MissileThrusterFlameVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]){
 FlameVarying out{};float4 position_xy_uv=vertices[vertexId];const auto instance=instances[instanceId];float4 posAndSize=instance.posAndSize,dirAndLength=instance.dirAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData;

	float3 worldPos = posAndSize.xyz;
	float flameWidth = posAndSize.w;
	float3 dir = dirAndLength.xyz;
	float flameLength = dirAndLength.w;

	// Build orientation basis from direction vector
	// The flame quad is stretched along 'dir', with width perpendicular
	float3 forward = normalize(dir);

	// Fixed world-derived perpendicular axis (does not rotate with camera).
	// The cross pass uses the other perpendicular — together they form a
	// stable cross shape visible from all angles.
	float3 right = cross(forward, float3(0.0, 1.0, 0.0));
	float rightLen = length(right);
	if (rightLen < 0.001) {
		right = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
	} else {
		right = right / rightLen;
	}

	// The flame quad extends from the projectile position along the direction.
	// position_xy_uv.y: 0..1 (from makeRectVBO UV), but position_xy_uv.y vertex: -1..1
	// We want the flame to start at the projectile and extend in the flame direction.
	// flameLength sign determines direction: negative = behind projectile, positive = forward
	// Map vertex y from -1..1 to 0..1 (flame starts at projectile, extends away)
	float yNorm = position_xy_uv.y * 0.5 + 0.5;  // 0 at projectile, 1 at tip/tail

	// SizeGrowth: flame widens from base to tail (matching engine sizegrowth behavior)
	float sizeGrowth = color2.a;
	float widthScale = 1.0 + sizeGrowth * yNorm;

	// Per-instance animation phase from world position
	float phase = worldPos.x * 1.0 + worldPos.z * 1.3;

	// Width shimmer: subtle oscillation simulating re-spawned flame overlap
	float shimmer = 1.0 + u.defines[0] * sin(u.timeInfo.z * u.defines[1] + phase) * (u.defines[2] + (1.0 - u.defines[2]) * yNorm);

	float width = flameWidth * widthScale * shimmer;

	float3 vertexWorld = worldPos
		+ right * position_xy_uv.x * width
		+ forward * yNorm * flameLength;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);

	// Swap UV: texture u (256px) = flame length axis, v (128px) = flame width axis
	out.texCoords = float2(position_xy_uv.w, position_xy_uv.z);

	// Interpolate color along the flame length
	float t = yNorm;  // 0 = at projectile (base), 1 = end of flame (tip)

	float3 tipColor = color1.rgb;
	float3 endColor = color2.rgb;
	float alpha = color1.a;

	// Color gradient: tip -> end color, then fade
	float3 col = mix(tipColor, endColor, smoothstep(0.0, u.defines[6], t));

	// Alpha: brightness pulse + fade at tail
	float breathe = u.defines[3] + u.defines[4] * sin(u.timeInfo.z * u.defines[5] + phase * 3.1);
	alpha *= breathe * (1.0 - smoothstep(u.defines[7], u.defines[8], t));

	out.flameColor = float4(col, alpha);

 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5;return out;}
fragment float4 MissileThrusterFlameFragment(FlameVarying in [[stage_in]],texture2d<float> flameTex [[texture(0)]],sampler filter [[sampler(0)]],constant MissileThrusterUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));float4 fragColor;

	float4 texSample = flameTex.sample(filter,in.texCoords);

	// Texture is near-grayscale; per-channel multiply tints it with flame color
	float3 color = texSample.rgb * in.flameColor.rgb * in.flameColor.a;

	// Brightness boost: engine overlaps ~2 additive flames (ttl=2), we draw 1
	color *= u.defines[9];

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.002) discard_fragment();

	fragColor = float4(color, 0.0);

return fragColor;}
vertex FlameVarying MissileThrusterCrossFlameVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]){
 FlameVarying out{};float4 position_xy_uv=vertices[vertexId];const auto instance=instances[instanceId];float4 posAndSize=instance.posAndSize,dirAndLength=instance.dirAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData;

	float3 worldPos = posAndSize.xyz;
	float flameWidth = posAndSize.w;
	float3 dir = dirAndLength.xyz;
	float flameLength = dirAndLength.w;

	float3 forward = normalize(dir);

	// Second perpendicular axis: cross(axis1, forward) where axis1 = cross(forward, worldUp).
	// Together with the main pass (which uses axis1) this forms a stable cross.
	float3 axis1 = cross(forward, float3(0.0, 1.0, 0.0));
	float axis1Len = length(axis1);
	if (axis1Len < 0.001) {
		axis1 = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
	} else {
		axis1 = axis1 / axis1Len;
	}
	float3 right = cross(axis1, forward);

	float yNorm = position_xy_uv.y * 0.5 + 0.5;

	float sizeGrowth = color2.a;
	float widthScale = 1.0 + sizeGrowth * yNorm;

	float phase = worldPos.x * 1.0 + worldPos.z * 1.3;
	float shimmer = 1.0 + u.defines[0] * sin(u.timeInfo.z * u.defines[1] + phase) * (u.defines[2] + (1.0 - u.defines[2]) * yNorm);

	float width = flameWidth * widthScale * shimmer;

	float3 vertexWorld = worldPos
		+ right * position_xy_uv.x * width
		+ forward * yNorm * flameLength;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);

	out.texCoords = float2(position_xy_uv.w, position_xy_uv.z);

	float t = yNorm;
	float3 tipColor = color1.rgb;
	float3 endColor = color2.rgb;
	float alpha = color1.a;

	float3 col = mix(tipColor, endColor, smoothstep(0.0, u.defines[6], t));

	float breathe = u.defines[3] + u.defines[4] * sin(u.timeInfo.z * u.defines[5] + phase * 3.1);
	alpha *= breathe * (1.0 - smoothstep(u.defines[7], u.defines[8], t));

	out.flameColor = float4(col, alpha);

 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5;return out;}
vertex CrossSectionVarying MissileThrusterCrossSectionVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]){
 CrossSectionVarying out{};float4 position_xy_uv=vertices[vertexId];const auto instance=instances[instanceId];float4 posAndSize=instance.posAndSize,dirAndLength=instance.dirAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData;

	float3 worldPos = posAndSize.xyz;
	float flameWidth = posAndSize.w;
	float3 dir = dirAndLength.xyz;

	float3 forward = normalize(dir);

	// How head-on is the camera view? (1 = looking along velocity, 0 = side view)
	float3 camPos = u.cameraViewInverse[3].xyz;
	float3 toCamera = normalize(camPos - worldPos);
	float headOn = abs(dot(forward, toCamera));

	// Only visible when looking along velocity; cull from side view
	if (headOn < 0.3) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// Camera-facing billboard, circular
	float3 camRight = u.cameraViewInverse[0].xyz;
	float3 camUp    = u.cameraViewInverse[1].xyz;

	float crossSize = flameWidth * u.defines[11];

	float3 vertexWorld = worldPos
		+ camRight * position_xy_uv.x * crossSize
		+ camUp    * position_xy_uv.y * crossSize;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);

	out.texCoords = position_xy_uv.zw;
	out.flameColor = color1.rgb;
	out.headOnFactor = smoothstep(0.3, 0.7, headOn);

 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5;return out;}
fragment float4 MissileThrusterCrossSectionFragment(CrossSectionVarying in [[stage_in]],texture2d<float> crossSectionTex [[texture(0)]],sampler filter [[sampler(0)]],constant MissileThrusterUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));float4 fragColor;

	float4 texSample = crossSectionTex.sample(filter,in.texCoords);
	float3 color = in.flameColor * texSample.rgb * in.headOnFactor * u.defines[10];

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.002) discard_fragment();

	fragColor = float4(color, 0.0);

return fragColor;}
vertex GlowVarying MissileThrusterGlowVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]){
 GlowVarying out{};float4 position_xy_uv=vertices[vertexId];const auto instance=instances[instanceId];float4 posAndSize=instance.posAndSize,dirAndLength=instance.dirAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData;

	float3 worldPos = posAndSize.xyz;
	float glowSize = glowData.x;

	// Skip instances with no glow (degenerate quad off-screen)
	if (glowSize <= 0.0) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// Offset glow center 1/3 along the flame length (toward the tail)
	float3 dir = dirAndLength.xyz;
	float flameLength = dirAndLength.w;
	float3 glowCenter = worldPos + dir * flameLength * 0.4;

	// Billboard: camera-facing quad
	float3 camRight = u.cameraViewInverse[0].xyz;
	float3 camUp    = u.cameraViewInverse[1].xyz;

	float3 vertexWorld = glowCenter
		+ camRight * position_xy_uv.x * glowSize
		+ camUp    * position_xy_uv.y * glowSize;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);
	out.texCoords = position_xy_uv.zw;
	out.color = float4(glowData.yzw, 1.0);

 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5;return out;}
fragment float4 MissileThrusterGlowFragment(GlowVarying in [[stage_in]],texture2d<float> glowTex [[texture(0)]],sampler filter [[sampler(0)]],constant MissileThrusterUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));float4 fragColor;

	float4 texSample = glowTex.sample(filter,in.texCoords);
	float3 col = in.color.rgb * texSample.rgb;
	float a = texSample.a * in.color.a;

	if (a < 0.002) discard_fragment();

	fragColor = float4(col * a, a);

return fragColor;}

// Pinned31657: exact common thrusterMotion plus source12-vertex crossed flame.
// Instance IDs and float-bit time hashing preserve original per-draw flicker.
struct MissileThrusterInstance31657 {float4 posAndSize,velAndLength,color1,color2,glowData,shapeData;};
static_assert(sizeof(MissileThrusterInstance31657)==96);
bool MissileThrusterMotion31657(MissileThrusterInstance31657 instance,constant MissileThrusterUniform& u,uint instanceId,
 thread float3& worldPos,thread float3& dir,thread float& flameWidth,thread float& flameLength) {

	float3 vel = instance.velAndLength.xyz;
	float speedSq = dot(vel, vel);
	if (speedSq <= 0.0001) {
		return false;
	}
	dir = vel * rsqrt(speedSq);
	worldPos = instance.posAndSize.xyz + vel * (u.timeInfo.w - 1.0) - dir * instance.shapeData.z;

	// Random length/width flicker: new for every instance and draw frame, frozen while paused
	uint h = as_type<uint>(u.timeInfo.z) ^ (uint(instanceId) * 0x9E3779B9u);
	h = (h ^ (h >> 16u)) * 0x7FEB352Du;
	h = (h ^ (h >> 15u)) * 0x846CA68Bu;
	h ^= h >> 16u;
	float rand = float(h >> 8u) * (1.0 / 16777216.0);
	flameWidth = instance.posAndSize.w * (1.0 + rand * instance.shapeData.y);
	flameLength = instance.velAndLength.w * (1.0 + rand * instance.shapeData.x);
	return true;
}
vertex FlameVarying MissileThrusterFlame31657Vertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance31657* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]) {
 FlameVarying out{};const float4 quadVertex=vertices[vertexId];const auto instance=instances[instanceId];
 const float4 posAndSize=instance.posAndSize,velAndLength=instance.velAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData,shapeData=instance.shapeData;

	float3 worldPos, forward;
	float flameWidth, flameLength;
	if (!MissileThrusterMotion31657(instance,u,instanceId,worldPos, forward, flameWidth, flameLength)) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// Fixed world-derived perpendicular axis (does not rotate with camera).
	// The second plane uses the other perpendicular — together they form a
	// stable cross shape visible from all angles.
	float3 axis1 = cross(forward, float3(0.0, 1.0, 0.0));
	float axis1Len = length(axis1);
	if (axis1Len < 0.001) {
		axis1 = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
	} else {
		axis1 = axis1 / axis1Len;
	}
	float3 right = (quadVertex.z > 0.5) ? cross(axis1, forward) : axis1;

	// The flame quad extends from the projectile position along the direction.
	// flameLength sign determines direction: negative = behind projectile, positive = forward
	// Map vertex y from -1..1 to 0..1 (flame starts at projectile, extends away)
	float yNorm = quadVertex.y * 0.5 + 0.5;  // 0 at projectile, 1 at tip/tail

	// SizeGrowth: flame widens from base to tail (matching engine sizegrowth behavior)
	float sizeGrowth = color2.a;
	float widthScale = 1.0 + sizeGrowth * yNorm;

	// Per-instance animation phase from world position
	float phase = worldPos.x * 1.0 + worldPos.z * 1.3;

	// Width shimmer: subtle oscillation simulating re-spawned flame overlap
	float shimmer = 1.0 + u.defines[0] * sin(u.timeInfo.z * u.defines[1] + phase) * (u.defines[2] + (1.0 - u.defines[2]) * yNorm);

	float width = flameWidth * widthScale * shimmer;

	float3 vertexWorld = worldPos
		+ right * quadVertex.x * width
		+ forward * yNorm * flameLength;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);

	// Swap UV: texture u (256px) = flame length axis, v (128px) = flame width axis
	out.texCoords = float2(yNorm, quadVertex.x * 0.5 + 0.5);

	// Interpolate color along the flame length
	float t = yNorm;  // 0 = at projectile (base), 1 = end of flame (tip)

	float3 tipColor = color1.rgb;
	float3 endColor = color2.rgb;
	float alpha = color1.a;

	// Color gradient: tip -> end color, then fade
	float3 col = mix(tipColor, endColor, smoothstep(0.0, u.defines[6], t));

	// Alpha: brightness pulse + fade at tail
	float breathe = u.defines[3] + u.defines[4] * sin(u.timeInfo.z * u.defines[5] + phase * 3.1);
	alpha *= breathe * (1.0 - smoothstep(u.defines[7], u.defines[8], t));

	out.flameColor = float4(col, alpha);
 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5f;return out;
}
vertex CrossSectionVarying MissileThrusterCrossSection31657Vertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance31657* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]) {
 CrossSectionVarying out{};const float4 quadVertex=vertices[vertexId];const auto instance=instances[instanceId];
 const float4 posAndSize=instance.posAndSize,velAndLength=instance.velAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData,shapeData=instance.shapeData;

	float3 worldPos, forward;
	float flameWidth, flameLength;
	if (!MissileThrusterMotion31657(instance,u,instanceId,worldPos, forward, flameWidth, flameLength)) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// How head-on is the camera view? (1 = looking along velocity, 0 = side view)
	float3 camPos = u.cameraViewInverse[3].xyz;
	float3 toCamera = normalize(camPos - worldPos);
	float headOn = abs(dot(forward, toCamera));

	// Only visible when looking along velocity; cull from side view
	if (headOn < 0.3) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// Camera-facing billboard, circular
	float3 camRight = u.cameraViewInverse[0].xyz;
	float3 camUp    = u.cameraViewInverse[1].xyz;

	float crossSize = flameWidth * u.defines[11];

	float3 vertexWorld = worldPos
		+ camRight * quadVertex.x * crossSize
		+ camUp    * quadVertex.y * crossSize;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);

	out.texCoords = quadVertex.xy * 0.5 + 0.5;
	out.flameColor = color1.rgb;
	out.headOnFactor = smoothstep(0.3, 0.7, headOn);
 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5f;return out;
}
vertex GlowVarying MissileThrusterGlow31657Vertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device MissileThrusterInstance31657* instances [[buffer(1)]],constant MissileThrusterUniform& u [[buffer(2)]]) {
 GlowVarying out{};const float4 quadVertex=vertices[vertexId];const auto instance=instances[instanceId];
 const float4 posAndSize=instance.posAndSize,velAndLength=instance.velAndLength,color1=instance.color1,color2=instance.color2,glowData=instance.glowData,shapeData=instance.shapeData;

	float glowSize = glowData.x;
	float3 worldPos, dir;
	float flameWidth, flameLength;

	// Skip instances with no glow (degenerate quad off-screen)
	if (glowSize <= 0.0 || !MissileThrusterMotion31657(instance,u,instanceId,worldPos, dir, flameWidth, flameLength)) {
		out.position = float4(2.0, 2.0, 2.0, 1.0);
		return out;
	}

	// Offset glow center 1/3 along the flame length (toward the tail)
	float3 glowCenter = worldPos + dir * flameLength * 0.4;

	// Billboard: camera-facing quad
	float3 camRight = u.cameraViewInverse[0].xyz;
	float3 camUp    = u.cameraViewInverse[1].xyz;

	float3 vertexWorld = glowCenter
		+ camRight * quadVertex.x * glowSize
		+ camUp    * quadVertex.y * glowSize;

	out.position = u.cameraViewProjection * float4(vertexWorld, 1.0);
	out.texCoords = quadVertex.xy * 0.5 + 0.5;
	out.color = float4(glowData.yzw, 1.0);
 if(u.options.x)out.position.z=(out.position.z+out.position.w)*0.5f;return out;
}
)MSL";
}
