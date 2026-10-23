/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// OpenAI Codex adaptation of original reprojection, portrait and model shading.
namespace rendering::metal4 {
inline constexpr const char* NativeTerraformCaptureShaderSource=R"TFCAPTUREMSL(
struct TerraformCaptureUniforms {
 float4 worldOrigin;
 float4 worldExtent;
 float4 mapSize;
 float4 clampWater;
 float4 teamCol;
 float4 sizePx;
 float4 borderPx;
 float4 radiusPx;
 float4 inset;
 float4x4 viewProj;
};
struct TerraformCaptureParameters {TerraformCaptureUniforms uniforms;uint4 sampling[2];};
float4 TerraformCaptureSample(texture2d<float> image,sampler imageSampler,uint4 flags,float2 uv){
 if(flags.x)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(flags.y)uv.x=clamp(uv.x,0.f,1.f);if(flags.z)uv.y=clamp(uv.y,0.f,1.f);
 return image.sample(imageSampler,uv,gradient2d(dx,dy));
}

	float4 TerraformCaptureReproject(constant TerraformCaptureParameters& params,texture2d<float> screenTex,sampler screenTexSampler,texture2d<float> heightMap,sampler heightMapSampler,float2 inputUV,float3 inputNormal) {
 float4 fragmentColor;
		float2 uv  = inputUV;
		float2 wxz = params.uniforms.worldOrigin.xy + uv * params.uniforms.worldExtent.xy;

		float2 hUV = clamp(wxz / params.uniforms.mapSize.xy, float2(0.0f), float2(1.0f));
		float wy = TerraformCaptureSample(heightMap,heightMapSampler,params.sampling[1], hUV).x;
		// Submerged ground projects to the seabed while the screen shows the
		// water plane above it; clamping to y=0 samples the surface instead of
		// dragging a displaced seabed pixel up into the photo.
		if (params.uniforms.clampWater.x > 0.5f) wy = max(wy, 0.0f);

		float4 clip = params.uniforms.viewProj * float4(wxz.x, wy, wxz.y, 1.0f);
		if (clip.w <= 0.0f) { fragmentColor = float4(0.0f); return fragmentColor; }

		float2 sUV = (clip.xy / clip.w) * 0.5f + 0.5f;
		if (sUV.x < 0.0f || sUV.x > 1.0f || sUV.y < 0.0f || sUV.y > 1.0f) { fragmentColor = float4(0.0f); return fragmentColor; }

		fragmentColor = float4(TerraformCaptureSample(screenTex,screenTexSampler,params.sampling[0], sUV).rgb, 1.0f);
	 return fragmentColor;
}

fragment float4 TerraformCaptureReprojectFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant TerraformCaptureParameters& params [[buffer(3)]],
texture2d<float> screenTex [[texture(0)]],sampler screenTexSampler [[sampler(0)]],
texture2d<float> heightMap [[texture(1)]],sampler heightMapSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=TerraformCaptureReproject(params,screenTex,screenTexSampler,heightMap,heightMapSampler,in.uv,in.color.xyz);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}

	float TerraformCaptureRoundedBox(float2 p, float2 b, float r) {
		return length(max(abs(p) - b + r, 0.0f)) - r;
	}

	float4 TerraformCaptureSprite(constant TerraformCaptureParameters& params,texture2d<float> pic,sampler picSampler,float2 inputUV,float3 inputNormal) {
 float4 fragmentColor;
		float2 uv = inputUV;
		float2 p  = (uv * 2.0f - 1.0f) * (params.uniforms.sizePx.x * 0.5f);
		float2 b  = float2(params.uniforms.sizePx.x * 0.5f - 1.0f);

		float dOuter = TerraformCaptureRoundedBox(p, b, params.uniforms.radiusPx.x);
		float aOuter = 1.0f - smoothstep(-1.0f, 1.0f, dOuter);
		if (aOuter <= 0.0f) { fragmentColor = float4(0.0f); return fragmentColor; }

		float dInner = TerraformCaptureRoundedBox(p, b - float2(params.uniforms.borderPx.x), max(params.uniforms.radiusPx.x - params.uniforms.borderPx.x, 1.0f));
		float aInner = 1.0f - smoothstep(-1.0f, 1.0f, dInner);

		float2 picUV = mix(float2(params.uniforms.inset.x), float2(1.0f - params.uniforms.inset.x), uv);
		float3 rgb   = mix(params.uniforms.teamCol.xyz, TerraformCaptureSample(pic,picSampler,params.sampling[0], picUV).rgb, aInner);
		fragmentColor = float4(rgb, aOuter);
	 return fragmentColor;
}

fragment float4 TerraformCaptureSpriteFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant TerraformCaptureParameters& params [[buffer(3)]],
texture2d<float> pic [[texture(0)]],sampler picSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=TerraformCaptureSprite(params,pic,picSampler,in.uv,in.color.xyz);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}

	float4 TerraformCaptureModel(constant TerraformCaptureParameters& params,texture2d<float> tex0,sampler tex0Sampler,float2 inputUV,float3 inputNormal) {
 float4 fragmentColor;
		float4 t = TerraformCaptureSample(tex0,tex0Sampler,params.sampling[0], inputUV);
		float3 albedo = mix(t.rgb, params.uniforms.teamCol.xyz, t.a);
		// Fixed key light from above-front: there is no sun uniform on this path,
		// and a plan view wants readable shape over matching in-game lighting.
		float ndl = clamp(dot(normalize(inputNormal), normalize(float3(0.35f, 1.0f, 0.25f))), 0.0f, 1.0f);
		fragmentColor = float4(albedo * (0.55f + 0.45f * ndl), 1.0f);
	 return fragmentColor;
}

fragment float4 TerraformCaptureModelFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant TerraformCaptureParameters& params [[buffer(3)]],
texture2d<float> tex0 [[texture(0)]],sampler tex0Sampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=TerraformCaptureModel(params,tex0,tex0Sampler,in.uv,in.color.xyz);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
)TFCAPTUREMSL";
}
