/* Exact selected30770 source literals; original Lua files are PD. */
#pragma once
#include <string_view>
namespace rendering::pbrsource {
inline constexpr std::string_view BRDFVS=R"SOURCE(	#version 150 compatibility

	void main() {
		gl_Position = gl_Vertex;
	}
)SOURCE";
inline constexpr std::string_view BRDFFS=R"SOURCE(	#version 150 compatibility

	const uint NUM_SAMPLES = 1024u;
	const float PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;

	uniform vec2 texSize;

	//https://www.shadertoy.com/view/4djSRW
	// Hash without Sine
	#define HASHSCALE1 443.8975

	//----------------------------------------------------------------------------------------
	//  1 out, 2 in...
	float hash12(vec2 p) {
		vec3 p3  = fract(vec3(p.xyx) * HASHSCALE1);
		p3 += dot(p3, p3.yzx + 19.19);
		return fract((p3.x + p3.y) * p3.z);
	}

	vec2 hammersley2d(uint i, uint N) {
		// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
		uint bits = (i << 16u) | (i >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		float rdi = float(bits) * 2.3283064365386963e-10;
		return vec2(float(i) /float(N), rdi);
	}

	#define RESTRICT_BRANCHING

	// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
	vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal) {
		// Maps a 2D point to a hemisphere with spread based on roughness
		float roughness2 = roughness * roughness;
		float phi = 2.0 * PI * Xi.x;
		float cosTheta2 = (1.0 - Xi.y) / (1.0 + (roughness2 * roughness2 - 1.0) * Xi.y);
		float cosTheta = sqrt(cosTheta2);
		float sinTheta = sqrt(1.0 - cosTheta2);
		vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

		// Tangent space
		#ifdef RESTRICT_BRANCHING
			vec3 up = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3( abs(normal.z) < 0.999));
		#else
			vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
		#endif
		vec3 tangentX = normalize(cross(up, normal));
		vec3 tangentY = normalize(cross(normal, tangentX));

		// Convert to world Space
		return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
	}


	#define G_OPTION ###G_OPTION###

	// Geometric Shadowing function
	float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
		#if (G_OPTION == 1)
			float k = (roughness * roughness) / 2.0;
		#elif (G_OPTION == 2)
			float k = (roughness + 1.0);
			//k = k * k;
		#elif (G_OPTION == 3)
			float k = (roughness + 1.0);
			k = k * k / 8.0;
		#elif (G_OPTION == 4)
			float k = (0.5 + 0.5 * roughness);
			k = k * k;
		#endif
		float GL = dotNL / (dotNL * (1.0 - k) + k);
		float GV = dotNV / (dotNV * (1.0 - k) + k);
		return GL * GV;
	}

	vec2 BRDF(float NoV, float roughness) {
		// Normal always points along z-axis for the 2D lookup
		const vec3 N = vec3(0.0, 0.0, 1.0);
		vec3 V = vec3(sqrt(1.0 - NoV * NoV), 0.0, NoV); //normalized

		vec2 LUT = vec2(0.0);
		for(uint i = 0u; i < NUM_SAMPLES; i++) {
			vec2 Xi = hammersley2d(i, NUM_SAMPLES);
			vec3 H = importanceSample_GGX(Xi, roughness, N);
			vec3 L = 2.0 * dot(V, H) * H - V;

			float dotNL = max(dot(N, L), 0.0);
			float dotVH = max(dot(V, H), 0.0);
			float dotNV = clamp(dot(N, V), 0.0000001, 1.0);
			float dotNH = clamp(dot(H, N), 0.0000001, 1.0);

			#ifdef RESTRICT_BRANCHING
				float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
				float G_Vis = (G * dotVH) / (dotNH * dotNV);
				float Fc = pow(1.0 - dotVH, 5.0);
				vec2 lutAdd = mix( vec2(0.0), vec2((1.0 - Fc) * G_Vis, Fc * G_Vis), vec2(float(dotNL > 0.0)) );
				LUT += lutAdd;
			#else
				if (dotNL > 0.0) {
					float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
					float G_Vis = (G * dotVH) / (dotNH * dotNV);
					float Fc = pow(1.0 - dotVH, 5.0);
					LUT += vec2((1.0 - Fc) * G_Vis, Fc * G_Vis);
				}
			#endif
		}
		return LUT / vec2( float(NUM_SAMPLES) );
	}

	void main() {
		//NdotV, roughness
		vec2 inUV = gl_FragCoord.xy / texSize;
		gl_FragColor = vec4(BRDF(inUV.x, inUV.y), 0.0, 1.0);
	}
)SOURCE";
inline constexpr std::string_view EnvironmentVS=R"SOURCE(	#version 150 compatibility
	#line 10004

	void main() {
		gl_Position = gl_Vertex;
	}
)SOURCE";
inline constexpr std::string_view EnvironmentFS=R"SOURCE(	#version 150 compatibility
	#line 20014

	#define ENV_SMPL_NUM ###ENV_SMPL_NUM###

	const float PI = 3.1415926535897932384626433832795;
	const float goldenAngle = 2.3999632297286533222315555066336; //PI * (3.0 - sqrt(5.0));

	uniform samplerCube reflectTex;

	out vec3 outData;

	#define NORM2SNORM(value) (value * 2.0 - 1.0)
	#define SNORM2NORM(value) (value * 0.5 + 0.5)

	vec3 SpherePoints_GoldenAngle(float i, float numSamples) {
		float theta = i * goldenAngle;
		float z = (1.0 - 1.0 / numSamples) * (1.0 - 2.0 * i / (numSamples - 1.0));
		float radius = sqrt(1.0 - z * z);
		return vec3(radius * vec2(cos(theta), sin(theta)), z);
	}

	const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

	#define SRGBtoLINEAR(c) ( c )

	#define FAKE_ENV_HDR 0.4
	#define FAKE_ENV_THR 0.55

	vec3 sampleReflectionMap(vec3 sp, float lodBias){
		vec3 color = SRGBtoLINEAR(textureLod(reflectTex, sp, lodBias).rgb);
		#if defined (FAKE_ENV_HDR)
			color *= 1.0 + FAKE_ENV_HDR * smoothstep(FAKE_ENV_THR, 1.0, dot(LUMA, color)); //HDR for poors
		#endif
		return color;
	}

	// Constants, see here: http://en.wikipedia.org/wiki/Table_of_spherical_harmonics
	#define k01 0.2820947918 // sqrt(  1/PI)/2
	#define k02 0.4886025119 // sqrt(  3/PI)/2
	#define k03 1.0925484306 // sqrt( 15/PI)/2
	#define k04 0.3153915652 // sqrt(  5/PI)/4
	#define k05 0.5462742153 // sqrt( 15/PI)/4
	#define k06 0.5900435860 // sqrt( 70/PI)/8
	#define k07 2.8906114210 // sqrt(105/PI)/2
	#define k08 0.4570214810 // sqrt( 42/PI)/8
	#define k09 0.3731763300 // sqrt(  7/PI)/4
	#define k10 1.4453057110 // sqrt(105/PI)/4

	// Y_l_m(s), where l is the band and m the range in [-l..l]
	float SphericalHarmonic( in int l, in int m, in vec3 n )
	{
		//----------------------------------------------------------
		if( l==0 )          return   k01;

		//----------------------------------------------------------
		if( l==1 && m==-1 ) return  -k02*n.y;
		if( l==1 && m== 0 ) return   k02*n.z;
		if( l==1 && m== 1 ) return  -k02*n.x;

		//----------------------------------------------------------
		if( l==2 && m==-2 ) return   k03*n.x*n.y;
		if( l==2 && m==-1 ) return  -k03*n.y*n.z;
		if( l==2 && m== 0 ) return   k04*(3.0*n.z*n.z-1.0);
		if( l==2 && m== 1 ) return  -k03*n.x*n.z;
		if( l==2 && m== 2 ) return   k05*(n.x*n.x-n.y*n.y);
		//----------------------------------------------------------

		return 0.0;
	}

	float shEvaluate(vec3 n, ivec2 idx) {
		     if (idx.x == 0 && idx.y == 0) return  SphericalHarmonic(0,  0, n);
		else if (idx.x == 0 && idx.y == 1) return -SphericalHarmonic(1, -1, n);
		else if (idx.x == 0 && idx.y == 2) return  SphericalHarmonic(1,  0, n);
		else if (idx.x == 1 && idx.y == 0) return -SphericalHarmonic(1,  1, n);
		else if (idx.x == 1 && idx.y == 1) return  SphericalHarmonic(2, -2, n);
		else if (idx.x == 1 && idx.y == 2) return -SphericalHarmonic(2, -1, n);
		else if (idx.x == 2 && idx.y == 0) return  SphericalHarmonic(2,  0, n);
		else if (idx.x == 2 && idx.y == 1) return -SphericalHarmonic(2,  1, n);
		else if (idx.x == 2 && idx.y == 2) return  SphericalHarmonic(2,  2, n);
		else return 0.0;
	}

	const mat3 convCoeffMat = mat3(
		1.0/1.0, 2.0/3.0, 2.0/3.0,
		2.0/3.0, 1.0/4.0, 1.0/4.0,
		1.0/4.0, 1.0/4.0, 1.0/4.0
	);

	vec3 shDiffuseConvolution(vec3 shRGB, ivec2 idx) {
		return shRGB * convCoeffMat[idx.x][idx.y];
	}

	vec3 CubeMapToSH2(ivec2 idx) {
		// Initialise sh to 0
		vec3 shRGB = vec3(0.0);

		vec2 ts = vec2(textureSize(reflectTex, 0));
		float maxMipMap = log2(max(ts.x, ts.y));

		float lodBias = maxMipMap - 6.0;

		for (int i=0; i < ENV_SMPL_NUM; ++i) {
			vec3 sp = SpherePoints_GoldenAngle(float(i), float(ENV_SMPL_NUM));
			vec3 color = sampleReflectionMap(sp, lodBias);

			float sh = shEvaluate(sp, idx);

			shRGB += sh * color;
		}

		// integrating over a sphere so each sample has a weight of 4*PI/samplecount (uniform solid angle, for each sample)
		float shFactor = 4.0 * PI / float(ENV_SMPL_NUM);
		shRGB *= shFactor;

		return shRGB;
	}


	void main() {
		ivec2 idx = ivec2(floor(gl_FragCoord.xy));

		vec3 shRGB = CubeMapToSH2(idx);

		#if 0
			shRGB = shDiffuseConvolution(shRGB, idx);
		#endif

		outData = shRGB;
	}
)SOURCE";
}
