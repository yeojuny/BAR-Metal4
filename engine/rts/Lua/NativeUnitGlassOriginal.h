/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <string_view>
namespace glassoriginal {
inline constexpr std::string_view vertex=R"GLASSORIG(#version 150 compatibility
#line 100054

uniform vec3 sunPos;
//uniform mat4 viewMat;
uniform mat4 viewInvMat;

out Data {
	vec3 T;
	vec3 B;
	vec3 vertexN;

	vec3 L;

	vec3 viewCameraDir;

	vec2 uv;
};

void main() {
	// view space?
	T = mat3(viewInvMat) * (gl_NormalMatrix * gl_MultiTexCoord5.xyz);
	B = mat3(viewInvMat) * (gl_NormalMatrix * gl_MultiTexCoord6.xyz);
	vertexN = mat3(viewInvMat) * (gl_NormalMatrix * gl_Normal);

	vec4 worldVertPos = viewInvMat * (gl_ModelViewMatrix * gl_Vertex);
	vec4 worldCamPos = viewInvMat * vec4(0.0, 0.0, 0.0, 1.0);

	viewCameraDir = worldCamPos.xyz - worldVertPos.xyz;

	L = sunPos;

	uv = gl_MultiTexCoord0.xy;

	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLASSORIG";
inline constexpr std::string_view fragment=R"GLASSORIG(#version 150 compatibility
#line 200094

uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D normalTex;
uniform samplerCube reflectTex;

uniform vec4 teamColor;
uniform vec3 sunSpecular;

uniform float pbrParams[8];

// Indices of refraction
const float air = 1.0;
const float glass = 1.5;

// Air to glass ratio of the indices of refraction (Eta)
const float eta = air / glass;

// see http://en.wikipedia.org/wiki/Refractive_index Reflectivity
const float R0 = ((air - glass) * (air - glass)) / ((air + glass) * (air + glass));

in Data {
	vec3 T;
	vec3 B;
	vec3 vertexN;

	vec3 L;

	vec3 viewCameraDir;

	vec2 uv;
};

#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)

#define REFL_MULT 1.35

vec3 GetSpecularBlinnPhong(float HdotN, float roughness) {
	float power = 2.0 / max(roughness * 0.25, 0.01);
	float powerNorm = (power + 8.0) / 32.0;
	return sunSpecular * pow(HdotN, power) * powerNorm;
}

vec3 SampleEnvironmentWithRoughness(vec3 samplingVec, float roughness) {
	float maxLodLevel = log2(float(textureSize(reflectTex, 0).x));

	// makes roughness of reflection scale perceptually much more linear
	// Assumes "CubeTexSizeReflection" = 1024
	//maxLodLevel -= 4.0;

	float lodBias = maxLodLevel * roughness;

	return texture(reflectTex, samplingVec, lodBias).rgb;
}

vec3 CustomTM(const vec3 x) {
	return ((x * (pbrParams[0] * x + pbrParams[1])) / (x * (pbrParams[2] * x + pbrParams[3]) + pbrParams[4]));
}

void main(void){
	vec4 tex1Color = texture(tex1, uv);
	vec4 tex2Color = texture(tex2, uv);

	vec3 normal = NORM2SNORM(texture(normalTex, uv).xyz);

	vec3 diffColor = mix(tex1Color.rgb, teamColor.rgb, tex1Color.a);

	vec3 N = normalize(mat3(T, B, vertexN) * normal);
	N = mix(-N, N, float(gl_FrontFacing));

	float metalness = clamp(tex2Color.g, 0.04, 1.0);
	float roughness = clamp(tex2Color.b, 0.04, 1.0);

	float roughness4 = roughness * roughness;
	roughness4 *= roughness4;

	float R0v = mix(R0, 1.0, metalness);

	vec3 V = normalize(viewCameraDir);
	vec3 I = -V;

	vec3 H = normalize(L + V); //half vector

	vec3 Rl = reflect(I, N);

	// getSpecularDominantDirection (Filament)
	Rl = mix(Rl, N, roughness4);

	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float HdotN = clamp(dot(H, N), 0.0, 1.0);

	float fresnel = R0v + (1.0 - R0v) * pow((1.0 - NdotV), 5.0);
	vec3 reflColor = REFL_MULT * SampleEnvironmentWithRoughness(Rl, roughness).rgb;
	reflColor *= fresnel;

	reflColor += pbrParams[6] * GetSpecularBlinnPhong(HdotN, roughness); // pbrParams[6] is sun mult

	gl_FragColor.rgb  = pbrParams[7] * (diffColor + reflColor); // pbrParams[7] is exposure
	gl_FragColor.rgb  = CustomTM(gl_FragColor.rgb);

	gl_FragColor.a = NORM2SNORM(tex2Color.a);
}

)GLASSORIG";
}
