/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <string_view>
namespace rendering::blockoutsource {
inline constexpr std::string_view Vertex=R"BLOCKOUT(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
#line 10000
//__DEFINES__
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 T;
layout (location = 3) in vec3 B;
layout (location = 4) in vec4 uv;
layout (location = 5) in uvec2 bonesInfo;
#define pieceIndex (bonesInfo.x & 0x000000FFu)
layout (location = 6) in vec4 worldposyaw;
layout (location = 7) in vec4 params;
layout (location = 8) in uvec4 instData;
//__ENGINEUNIFORMBUFFERDEFS__
#if USEQUATERNIONS == 0
	layout(std140, binding=0) buffer MatrixBuffer {
		mat4 mat[];
	};
#else
	//__QUATERNIONDEFS__
#endif

out vec2 v_uv;
out vec3 v_normal;
out vec3 v_tangent;
out vec3 v_bitangent;

void main() {
	uint baseIndex = instData.x;
	#if USEQUATERNIONS == 0
		mat4 pieceMatrix = mat[baseIndex + pieceIndex];
		vec4 localModelPos = pieceMatrix * vec4(pos, 1.0);
		mat3 pieceRot = mat3(pieceMatrix);
	#else
		Transform tx = GetStaticPieceModelTransform(baseIndex, pieceIndex);
		vec4 localModelPos = ApplyTransform(tx, vec4(pos, 1.0));
		mat3 pieceRot = mat3(1.0);
	#endif
	localModelPos.xyz *= params.x;

	// same yaw convention as gfx_DrawFeatureShape_GL4 (left-handed engine transforms)
	mat3 rot = rotation3dY(-worldposyaw.w) * pieceRot;
	vec3 worldPos = rot * localModelPos.xyz + worldposyaw.xyz;

	v_uv = uv.xy;
	v_normal = rot * normal;
	v_tangent = rot * T;
	v_bitangent = rot * B;
	gl_Position = cameraViewProj * vec4(worldPos, 1.0);
}
)BLOCKOUT";
inline constexpr std::string_view Fragment=R"BLOCKOUT(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
#line 20000
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D normalTex;
uniform float hasNormalTex;
//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__
in vec2 v_uv;
in vec3 v_normal;
in vec3 v_tangent;
in vec3 v_bitangent;
out vec4 fragColor;
#line 25000
void main() {
	vec4 color = texture(tex1, v_uv);
	vec4 other = texture(tex2, v_uv);
	if (other.a < 0.5) discard; // CUS cut-out convention

	vec3 N = normalize(v_normal);
	if (hasNormalTex > 0.5) {
		vec3 tn = texture(normalTex, v_uv).xyz * 2.0 - 1.0;
		N = normalize(normalize(v_tangent) * tn.x + normalize(v_bitangent) * tn.y + N * tn.z);
	}
	float ndl = max(dot(N, normalize(sunDir.xyz)), 0.0);
	vec3 lit = color.rgb * (sunAmbientModel.rgb + sunDiffuseModel.rgb * ndl);
	lit += color.rgb * other.r; // emissive, as CUS: strength times albedo
	fragColor = vec4(lit, 1.0);
}
)BLOCKOUT";
}
