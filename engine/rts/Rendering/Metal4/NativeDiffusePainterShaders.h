/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// OpenAI Codex adaptation of pinned original BAR Diffuse Painter shaders.
namespace rendering::metal4 {
inline constexpr const char* NativeDiffusePainterShaderSource=R"DIFFUSEMSL(
struct DiffusePainterUniforms {
 float4 squareOrigin;
 float4 squareSize;
 float4 mapSize;
 float4 layerColor;
 float4 layerOpacity;
 int4 blendMode;
 int4 useLayerTex;
 float4 tileScale;
 int4 altEnabled;
 float4 altMin;
 float4 altMax;
 float4 altFalloffLo;
 float4 altFalloffHi;
 int4 slopeEnabled;
 float4 slopeMinCos;
 float4 slopeMaxCos;
 float4 slopeFalloffLo;
 float4 slopeFalloffHi;
 int4 handPaintEnabled;
 int4 hydroEnabled;
 float4 hydroStrength;
 float4 hydroFalloffLo;
 float4 hydroFalloffHi;
 int4 thermoEnabled;
 float4 thermoAngle;
 float4 thermoFalloff;
 float4 brushPos;
 float4 brushRadius;
 float4 brushStrength;
 float4 brushCurve;
 int4 brushErase;
 int4 useNormalTex;
 int4 useRoughTex;
 float4 pbrStrength;
 float4 fractalAmount;
 float4 fractalFreq;
 float4 dstUvOffset;
 float4 dstUvScale;
 int4 channelMode;
 float4 glowStrength;
 float4 specIntensity;
};
struct DiffusePainterParameters {DiffusePainterUniforms uniforms;uint4 sampling[4];};
float4 DiffuseSample(texture2d<float> image,sampler imageSampler,uint4 flags,float2 uv){
 // Original incomplete/default texture lookup produces (0,0,0,1).
 // This flag comes from the real bound object's descriptor, never a load retry.
 if(flags.w)return float4(0,0,0,1);
 if(flags.x)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(flags.y)uv.x=clamp(uv.x,0.f,1.f);if(flags.z)uv.y=clamp(uv.y,0.f,1.f);
 return image.sample(imageSampler,uv,gradient2d(dx,dy));
}
	float4 DiffuseHash4(float2 p) {
		p = float2(dot(p, float2(127.1f, 311.7f)),
		         dot(p, float2(269.5f, 183.3f)));
		return fract(sin(float4(p.x, p.y, p.x + p.y, p.x - p.y)) * 43758.5453f);
	}
	float3 DiffuseNoTile(texture2d<float> tex,sampler texSampler,uint4 texFlags, float2 uv) {
		float2 iuv = floor(uv);
		float2 fuv = fract(uv);
		float4 ofa = DiffuseHash4(iuv + float2(0.0f, 0.0f));
		float4 ofb = DiffuseHash4(iuv + float2(1.0f, 0.0f));
		float4 ofc = DiffuseHash4(iuv + float2(0.0f, 1.0f));
		float4 ofd = DiffuseHash4(iuv + float2(1.0f, 1.0f));
		// random per-cell sign flip (mirror) on uv coords for variety
		ofa.zw = sign(ofa.zw - 0.5f);
		ofb.zw = sign(ofb.zw - 0.5f);
		ofc.zw = sign(ofc.zw - 0.5f);
		ofd.zw = sign(ofd.zw - 0.5f);
		float2 uva = uv * ofa.zw + ofa.xy;
		float2 uvb = uv * ofb.zw + ofb.xy;
		float2 uvc = uv * ofc.zw + ofc.xy;
		float2 uvd = uv * ofd.zw + ofd.xy;
		// Narrow blend band (0.45..0.55) keeps each cell sharp; only a
		// thin seam shows the crossfade. Wider bands ghost the texture.
		float2 b = smoothstep(0.45f, 0.55f, fuv);
		float3 sa = DiffuseSample(tex,texSampler,texFlags, uva).rgb;
		float3 sb = DiffuseSample(tex,texSampler,texFlags, uvb).rgb;
		float3 sc = DiffuseSample(tex,texSampler,texFlags, uvc).rgb;
		float3 sd = DiffuseSample(tex,texSampler,texFlags, uvd).rgb;
		return mix(mix(sa, sb, b.x), mix(sc, sd, b.x), b.y);
	}
	// Direction-preserving variant for normal maps: random per-cell OFFSETS
	// only, no mirroring — flipping a tangent-space normal map's UVs without
	// negating its XY inverts the lighting in the mirrored cells.
	float3 DiffuseNoTileOffset(texture2d<float> tex,sampler texSampler,uint4 texFlags, float2 uv) {
		float2 iuv = floor(uv);
		float2 fuv = fract(uv);
		float2 oa = DiffuseHash4(iuv + float2(0.0f, 0.0f)).xy;
		float2 ob = DiffuseHash4(iuv + float2(1.0f, 0.0f)).xy;
		float2 oc = DiffuseHash4(iuv + float2(0.0f, 1.0f)).xy;
		float2 od = DiffuseHash4(iuv + float2(1.0f, 1.0f)).xy;
		float2 b = smoothstep(0.45f, 0.55f, fuv);
		float3 sa = DiffuseSample(tex,texSampler,texFlags, uv + oa).rgb;
		float3 sb = DiffuseSample(tex,texSampler,texFlags, uv + ob).rgb;
		float3 sc = DiffuseSample(tex,texSampler,texFlags, uv + oc).rgb;
		float3 sd = DiffuseSample(tex,texSampler,texFlags, uv + od).rgb;
		return mix(mix(sa, sb, b.x), mix(sc, sd, b.x), b.y);
	}
	// Altitude mask
	// Slope mask
	// Hand paint
	// Hydro erosion (valley/channel affinity via heightmap concavity)
	// Thermo erosion (talus / repose-angle zone)

	float DiffuseComposite_smoothBand(float v, float lo, float hi, float fLo, float fHi) {
		float a = (fLo > 0.001f) ? smoothstep(lo - fLo, lo, v) : step(lo, v);
		float b = (fHi > 0.001f) ? (1.0f - smoothstep(hi, hi + fHi, v)) : step(v, hi);
		return clamp(a * b, 0.0f, 1.0f);
	}

	// Photoshop-style blend: base = existing layer, blend = incoming colour.
	float3 DiffuseComposite_blendColors(int mode, float3 base, float3 blend) {
		if (mode == 0) return blend;                                              // Normal
		if (mode == 1) return base * blend;                                       // Multiply
		if (mode == 2) return base + blend - base * blend;                        // Screen
		if (mode == 3) return mix(2.0f*base*blend,                                 // Overlay
		                          1.0f - 2.0f*(1.0f-base)*(1.0f-blend),
		                          step(float3(0.5f), base));
		if (mode == 4) return mix(                                                // Soft Light
		                    2.0f*base*blend + base*base*(1.0f-2.0f*blend),
		                    sqrt(clamp(base,0.0001f,1.0f))*(2.0f*blend-1.0f) + 2.0f*base*(1.0f-blend),
		                    step(float3(0.5f), blend));
		if (mode == 5) return clamp(base / max(1.0f-blend, float3(0.001f)), 0.0f, 1.4f); // Color Dodge
		if (mode == 6) return mix(2.0f*base*blend,                                 // Hard Light
		                          1.0f - 2.0f*(1.0f-base)*(1.0f-blend),
		                          step(float3(0.5f), blend));
		if (mode == 7) return abs(base - blend);                                  // Difference
		return blend;
	}

	float4 DiffuseComposite(constant DiffusePainterParameters& params,texture2d<float> srcTex,sampler srcTexSampler,texture2d<float> maskTex,sampler maskTexSampler,texture2d<float> heightMap,sampler heightMapSampler,texture2d<float> layerTex,sampler layerTexSampler,float2 inputUV) {
 float4 fragmentColor;
		float2 localUV = inputUV;
		float2 worldXZ = params.uniforms.squareOrigin.xy + localUV * params.uniforms.squareSize.xy;
		float4 src = DiffuseSample(srcTex,srcTexSampler,params.sampling[0], localUV);

		// Hand-paint: colour already baked at stroke time; just blend with mode.
		if (params.uniforms.handPaintEnabled.x == 1) {
			float4 paint = DiffuseSample(maskTex,maskTexSampler,params.sampling[1], localUV);
			float pa = clamp(paint.a * params.uniforms.layerOpacity.x, 0.0f, 1.0f);
			// paint.rgb is premultiplied (stored as color * alpha by the stamp shader).
			// De-premultiply to recover the actual deposited colour before blending,
			// otherwise the deposit contribution is paint.a² instead of paint.a.
			float3 paintColor = (paint.a > 0.001f) ? paint.rgb / paint.a : float3(0.0f);
			float3 bl = DiffuseComposite_blendColors(params.uniforms.blendMode.x, src.rgb, paintColor);
			fragmentColor = float4(mix(src.rgb, bl, pa), 1.0f);
			return fragmentColor;
		}

		float m = 1.0f;

		// Shared heightmap samples (one tap for alt, four more for gradients).
		float2 hmTexel = 1.0f / float2(heightMap.get_width(),heightMap.get_height());
		float2 uvCtr   = worldXZ / params.uniforms.mapSize.xy;
		float2 cellSz  = params.uniforms.mapSize.xy * hmTexel;

		if (params.uniforms.altEnabled.x == 1 || params.uniforms.slopeEnabled.x == 1 || params.uniforms.hydroEnabled.x == 1 || params.uniforms.thermoEnabled.x == 1) {
			float hC = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], uvCtr).x;
			float hL = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], uvCtr + float2(-hmTexel.x, 0.0f)).x;
			float hR = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], uvCtr + float2( hmTexel.x, 0.0f)).x;
			float hD = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], uvCtr + float2(0.0f, -hmTexel.y)).x;
			float hU = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], uvCtr + float2(0.0f,  hmTexel.y)).x;

			if (params.uniforms.altEnabled.x == 1) {
				m *= DiffuseComposite_smoothBand(hC, params.uniforms.altMin.x, params.uniforms.altMax.x, params.uniforms.altFalloffLo.x, params.uniforms.altFalloffHi.x);
			}

			if (params.uniforms.slopeEnabled.x == 1) {
				float3 nrm = normalize(float3(hL - hR, 2.0f * cellSz.x, hD - hU));
				m *= DiffuseComposite_smoothBand(nrm.y, params.uniforms.slopeMinCos.x, params.uniforms.slopeMaxCos.x, params.uniforms.slopeFalloffLo.x, params.uniforms.slopeFalloffHi.x);
			}

			if (params.uniforms.hydroEnabled.x == 1) {
				// Positive Laplacian = concave = valley/channel = high flow.
				float lap  = hL + hR + hD + hU - 4.0f * hC;
				float flow = clamp(lap * params.uniforms.hydroStrength.x, 0.0f, 1.0f);
				float hfHi = max(params.uniforms.hydroFalloffHi.x, params.uniforms.hydroFalloffLo.x + 0.001f);
				m *= smoothstep(params.uniforms.hydroFalloffLo.x, hfHi, flow);
			}

			if (params.uniforms.thermoEnabled.x == 1) {
				// Surface normal → slope angle in degrees.
				float3 nrmT = normalize(float3(hL - hR, 2.0f * cellSz.x, hD - hU));
				float slopeAngleDeg = acos(clamp(nrmT.y, 0.0f, 1.0f)) * (180.0f / 3.14159265f);
				float tFall = max(params.uniforms.thermoFalloff.x, 0.5f);
				float tLo   = max(0.0f, params.uniforms.thermoAngle.x - tFall);
				float tHi   = params.uniforms.thermoAngle.x + tFall;
				float tMask = DiffuseComposite_smoothBand(slopeAngleDeg, tLo, tHi, tFall, tFall);
				// Weight by steepness of uphill source terrain (6-texel radius).
				float2 grad2d = float2(hR - hL, hU - hD);
				float gLen  = length(grad2d);
				float steep = 0.0f;
				if (gLen > 0.0001f) {
					float2 upDir = (grad2d / gLen) * hmTexel * 6.0f;
					float2 upUV  = clamp(uvCtr + upDir, float2(0.001f), float2(0.999f));
					float hUp  = DiffuseSample(heightMap,heightMapSampler,params.sampling[2], upUV).x;
					float rise = hUp - hC;
					float run  = length((grad2d / gLen) * cellSz * 6.0f);
					float upAng = atan(rise / max(run, 0.001f)) * (180.0f / 3.14159265f);
					steep = clamp(upAng / 45.0f, 0.0f, 1.0f);
				}
				m *= tMask * steep;
			}
		}

		float a = clamp(m * params.uniforms.layerOpacity.x, 0.0f, 1.0f);
		float3 layerRGB = params.uniforms.layerColor.xyz;
		if (params.uniforms.useLayerTex.x == 1) {
			float2 tileUV = worldXZ / max(params.uniforms.tileScale.x, 1.0f);
			layerRGB = DiffuseNoTile(layerTex,layerTexSampler,params.sampling[3], tileUV) * params.uniforms.layerColor.xyz;
		}
		float3 blended = DiffuseComposite_blendColors(params.uniforms.blendMode.x, src.rgb, layerRGB);
		float3 outRGB  = mix(src.rgb, blended, a);
		fragmentColor = float4(outRGB, 1.0f);
	 return fragmentColor;
}

fragment float4 DiffuseCompositeFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant DiffusePainterParameters& params [[buffer(3)]],
texture2d<float> srcTex [[texture(0)]],sampler srcTexSampler [[sampler(0)]],
texture2d<float> maskTex [[texture(1)]],sampler maskTexSampler [[sampler(1)]],
texture2d<float> heightMap [[texture(2)]],sampler heightMapSampler [[sampler(2)]],
texture2d<float> layerTex [[texture(3)]],sampler layerTexSampler [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=DiffuseComposite(params,srcTex,srcTexSampler,maskTex,maskTexSampler,heightMap,heightMapSampler,layerTex,layerTexSampler,in.uv);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}

	float DiffuseStamp_luma(float3 c) { return dot(c, float3(0.299f, 0.587f, 0.114f)); }

	// Value-noise fBm for organic brush-edge warping
	float DiffuseStamp_hfh(float2 p) { return fract(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f); }
	float DiffuseStamp_hfn(float2 p) {
		float2 i = floor(p), f = fract(p);
		float2 u = f * f * (3.0f - 2.0f * f);
		return mix(mix(DiffuseStamp_hfh(i), DiffuseStamp_hfh(i+float2(1.0f,0.0f)), u.x),
		           mix(DiffuseStamp_hfh(i+float2(0.0f,1.0f)), DiffuseStamp_hfh(i+float2(1.0f,1.0f)), u.x), u.y) * 2.0f - 1.0f;
	}
	float DiffuseStamp_hffbm(float2 p) {
		float v = 0.0f;
		v += 0.500f * DiffuseStamp_hfn(p); p *= 2.13f;
		v += 0.225f * DiffuseStamp_hfn(p); p *= 2.13f;
		v += 0.101f * DiffuseStamp_hfn(p); p *= 2.13f;
		v += 0.045f * DiffuseStamp_hfn(p);
		return v;
	}

	float4 DiffuseStamp(constant DiffusePainterParameters& params,texture2d<float> srcMask,sampler srcMaskSampler,texture2d<float> layerTex,sampler layerTexSampler,texture2d<float> normalTex,sampler normalTexSampler,texture2d<float> roughTex,sampler roughTexSampler,float2 inputUV) {
 float4 fragmentColor;
		float2 uv    = inputUV;
		float2 world = params.uniforms.squareOrigin.xy + uv * params.uniforms.squareSize.xy;

		// Domain-warp world position to produce fractal / organic brush edges.
		if (params.uniforms.fractalAmount.x > 0.001f) {
			float wx = DiffuseStamp_hffbm(world * params.uniforms.fractalFreq.x + float2(31.4f, 57.2f));
			float wy = DiffuseStamp_hffbm(world * params.uniforms.fractalFreq.x + float2(89.7f, 23.1f));
			world += float2(wx, wy) * params.uniforms.brushRadius.x * params.uniforms.fractalAmount.x;
		}

		float2  d = world - params.uniforms.brushPos.xy;
		float r = length(d) / params.uniforms.brushRadius.x;
		float4 current = DiffuseSample(srcMask,srcMaskSampler,params.sampling[0], uv);
		if (r >= 1.0f) { fragmentColor = current; return fragmentColor; }
		float fall   = 1.0f - pow(r, params.uniforms.brushCurve.x);
		float amount = clamp(params.uniforms.brushStrength.x * fall, 0.0f, 1.0f);
		if (params.uniforms.brushErase.x == 1) {
			float newA = max(0.0f, current.a - amount);
			fragmentColor = float4(current.rgb, newA);
			return fragmentColor;
		}
		float3 deposit = params.uniforms.layerColor.xyz;
		float2 tileUV  = world / max(params.uniforms.tileScale.x, 1.0f);
		if (params.uniforms.useLayerTex.x == 1) {
			deposit = DiffuseNoTile(layerTex,layerTexSampler,params.sampling[1], tileUV) * params.uniforms.layerColor.xyz;
		}
		// Faux PBR shading bake. Engine API only takes diffuse, so we bake
		// directional light + crevice AO + bump-from-luminance + specular
		// highlight into RGB.
		if (params.uniforms.pbrStrength.x > 0.001f) {
			// Build a normal: prefer real normal map, else derive from diffuse
			// luminance gradient at multiple scales (free pseudo-bump).
			float3 N;
			if (params.uniforms.useNormalTex.x == 1) {
				float3 nm = DiffuseNoTileOffset(normalTex,normalTexSampler,params.sampling[2], tileUV) * 2.0f - 1.0f;
				N = normalize(float3(nm.x, nm.y, max(nm.z, 0.05f)));
			} else if (params.uniforms.useLayerTex.x == 1) {
				// Gradient must use PLAIN texture() (not sampleNoTile) — the
				// no-tile sampler randomizes per-cell so neighbor taps would
				// cross random offsets and produce pure noise.
				float tx = 1.0f / max(params.uniforms.tileScale.x, 1.0f);
				float cL = DiffuseStamp_luma(DiffuseSample(layerTex,layerTexSampler,params.sampling[1], tileUV - float2(tx, 0.0f)).rgb);
				float cR = DiffuseStamp_luma(DiffuseSample(layerTex,layerTexSampler,params.sampling[1], tileUV + float2(tx, 0.0f)).rgb);
				float cD = DiffuseStamp_luma(DiffuseSample(layerTex,layerTexSampler,params.sampling[1], tileUV - float2(0.0f, tx)).rgb);
				float cU = DiffuseStamp_luma(DiffuseSample(layerTex,layerTexSampler,params.sampling[1], tileUV + float2(0.0f, tx)).rgb);
				float gx = (cR - cL) * 5.0f;
				float gy = (cU - cD) * 5.0f;
				N = normalize(float3(-gx, -gy, 1.0f));
			} else {
				N = float3(0.0f, 0.0f, 1.0f);
			}
			float rough = 0.55f;
			if (params.uniforms.useRoughTex.x == 1) {
				rough = clamp(DiffuseNoTile(roughTex,roughTexSampler,params.sampling[3], tileUV).r, 0.0f, 1.0f);
			} else if (params.uniforms.useLayerTex.x == 1) {
				// Approximate roughness from diffuse luma: darker -> rougher.
				rough = clamp(1.0f - DiffuseStamp_luma(deposit), 0.25f, 0.85f);
			}
			float3 L = normalize(float3(0.35f, 0.45f, 0.82f));
			float3 V = float3(0.0f, 0.0f, 1.0f);          // top-down view
			float3 H = normalize(L + V);
			float lambert = clamp(dot(N, L), 0.0f, 1.0f);
			float ao = mix(0.55f, 1.0f, N.z);         // gentle crevice darkening
			float shade = (0.45f + 0.75f * lambert) * ao;
			// Specular highlight: Blinn-Phong, gated by (1 - rough).
			float specExp = mix(12.0f, 48.0f, 1.0f - rough);
			float specTerm = pow(clamp(dot(N, H), 0.0f, 1.0f), specExp);
			float specStrength = mix(0.02f, 0.18f, 1.0f - rough);
			// Mild contrast pump.
			float lum = DiffuseStamp_luma(deposit);
			float contrast = mix(0.88f, 1.12f, smoothstep(0.0f, 1.0f, lum));
			shade *= contrast;
			shade = mix(1.0f, shade, params.uniforms.pbrStrength.x);
			deposit *= clamp(shade, 0.0f, 1.6f);
			deposit += float3(specTerm * specStrength * params.uniforms.pbrStrength.x);
			deposit = clamp(deposit, 0.0f, 1.4f);
		}
		float newA  = current.a + (1.0f - current.a) * amount;
		float3 newRGB = mix(current.rgb, deposit, amount);
		fragmentColor = float4(newRGB, clamp(newA, 0.0f, 1.0f));
	 return fragmentColor;
}

fragment float4 DiffuseStampFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant DiffusePainterParameters& params [[buffer(3)]],
texture2d<float> srcMask [[texture(0)]],sampler srcMaskSampler [[sampler(0)]],
texture2d<float> layerTex [[texture(1)]],sampler layerTexSampler [[sampler(1)]],
texture2d<float> normalTex [[texture(2)]],sampler normalTexSampler [[sampler(2)]],
texture2d<float> roughTex [[texture(3)]],sampler roughTexSampler [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=DiffuseStamp(params,srcMask,srcMaskSampler,layerTex,layerTexSampler,normalTex,normalTexSampler,roughTex,roughTexSampler,in.uv);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}

	// Same value-noise fBm as the diffuse stamp, so channel strokes warp
	// identically and stay registered with the painted diffuse.
	float DiffuseChannel_hfh(float2 p) { return fract(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f); }
	float DiffuseChannel_hfn(float2 p) {
		float2 i = floor(p), f = fract(p);
		float2 u = f * f * (3.0f - 2.0f * f);
		return mix(mix(DiffuseChannel_hfh(i), DiffuseChannel_hfh(i+float2(1.0f,0.0f)), u.x),
		           mix(DiffuseChannel_hfh(i+float2(0.0f,1.0f)), DiffuseChannel_hfh(i+float2(1.0f,1.0f)), u.x), u.y) * 2.0f - 1.0f;
	}
	float DiffuseChannel_hffbm(float2 p) {
		float v = 0.0f;
		v += 0.500f * DiffuseChannel_hfn(p); p *= 2.13f;
		v += 0.225f * DiffuseChannel_hfn(p); p *= 2.13f;
		v += 0.101f * DiffuseChannel_hfn(p); p *= 2.13f;
		v += 0.045f * DiffuseChannel_hfn(p);
		return v;
	}

	float4 DiffuseChannel(constant DiffusePainterParameters& params,texture2d<float> dstTex,sampler dstTexSampler,texture2d<float> layerTex,sampler layerTexSampler,texture2d<float> normalTex,sampler normalTexSampler,texture2d<float> roughTex,sampler roughTexSampler,float2 inputUV) {
 float4 fragmentColor;
		float2 uv    = inputUV;
		float2 world = uv * params.uniforms.mapSize.xy;
		if (params.uniforms.fractalAmount.x > 0.001f) {
			float wx = DiffuseChannel_hffbm(world * params.uniforms.fractalFreq.x + float2(31.4f, 57.2f));
			float wy = DiffuseChannel_hffbm(world * params.uniforms.fractalFreq.x + float2(89.7f, 23.1f));
			world += float2(wx, wy) * params.uniforms.brushRadius.x * params.uniforms.fractalAmount.x;
		}
		float4 dst = DiffuseSample(dstTex,dstTexSampler,params.sampling[0], (uv - params.uniforms.dstUvOffset.xy) * params.uniforms.dstUvScale.xy);
		float2  d = world - params.uniforms.brushPos.xy;
		float r = length(d) / params.uniforms.brushRadius.x;
		if (r >= 1.0f) { fragmentColor = dst; return fragmentColor; }
		float fall   = 1.0f - pow(r, params.uniforms.brushCurve.x);
		float amount = clamp(params.uniforms.brushStrength.x * fall, 0.0f, 1.0f);
		float2 tileUV  = world / max(params.uniforms.tileScale.x, 1.0f);

		if (params.uniforms.brushErase.x == 1) {
			if (params.uniforms.channelMode.x == 1) {
				// Spec alpha is exponent data, not coverage — fade the color.
				fragmentColor = float4(dst.rgb * (1.0f - amount), dst.a);
			} else if (params.uniforms.channelMode.x == 2) {
				// Emission rgb is added unconditionally by the engine (alpha only
				// masks the base diffuse), so the glow color must fade too or
				// erasing makes the area BRIGHTER.
				fragmentColor = float4(dst.rgb * (1.0f - amount), max(0.0f, dst.a - amount));
			} else {
				fragmentColor = float4(dst.rgb, max(0.0f, dst.a - amount));
			}
			return fragmentColor;
		}

		if (params.uniforms.channelMode.x == 0) {
			// No-mirror sampler: the mirroring no-tile variant flips UVs without
			// negating the normal's XY, which inverts the lighting per macro-cell.
			float3 nm = (params.uniforms.useNormalTex.x == 1) ? DiffuseNoTileOffset(normalTex,normalTexSampler,params.sampling[2], tileUV) : float3(0.5f, 0.5f, 1.0f);
			float newA = dst.a + (1.0f - dst.a) * amount;
			fragmentColor = float4(mix(dst.rgb, nm, amount), newA);
		} else if (params.uniforms.channelMode.x == 1) {
			float3 albedo = params.uniforms.layerColor.xyz;
			if (params.uniforms.useLayerTex.x == 1) albedo = DiffuseNoTile(layerTex,layerTexSampler,params.sampling[1], tileUV) * params.uniforms.layerColor.xyz;
			float rough = 0.55f;
			if (params.uniforms.useRoughTex.x == 1) rough = clamp(DiffuseNoTile(roughTex,roughTexSampler,params.sampling[3], tileUV).r, 0.0f, 1.0f);
			// Exponent below ~16/255 reads as diffuse light (guide p.10) — floor it.
			float expA    = clamp(1.0f - rough, 0.07f, 1.0f);
			float3  specRGB = albedo * params.uniforms.specIntensity.x * expA; // premultiplied by exponent
			fragmentColor = float4(mix(dst.rgb, specRGB, amount), mix(dst.a, expA, amount));
		} else {
			float3 glowRGB = params.uniforms.layerColor.xyz * params.uniforms.glowStrength.x;
			fragmentColor = float4(mix(dst.rgb, glowRGB, amount), mix(dst.a, params.uniforms.glowStrength.x, amount));
		}
	 return fragmentColor;
}

fragment float4 DiffuseChannelFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant DiffusePainterParameters& params [[buffer(3)]],
texture2d<float> dstTex [[texture(0)]],sampler dstTexSampler [[sampler(0)]],
texture2d<float> layerTex [[texture(1)]],sampler layerTexSampler [[sampler(1)]],
texture2d<float> normalTex [[texture(2)]],sampler normalTexSampler [[sampler(2)]],
texture2d<float> roughTex [[texture(3)]],sampler roughTexSampler [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=DiffuseChannel(params,dstTex,dstTexSampler,layerTex,layerTexSampler,normalTex,normalTexSampler,roughTex,roughTexSampler,in.uv);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
	float4 DiffuseCopy(constant DiffusePainterParameters& params,texture2d<float> tex0,sampler tex0Sampler,float2 inputUV) {
 float4 fragmentColor; fragmentColor = DiffuseSample(tex0,tex0Sampler,params.sampling[0], inputUV);  return fragmentColor;
}

fragment float4 DiffuseCopyFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],constant DiffusePainterParameters& params [[buffer(3)]],
texture2d<float> tex0 [[texture(0)]],sampler tex0Sampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=DiffuseCopy(params,tex0,tex0Sampler,in.uv);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
)DIFFUSEMSL";
} // namespace rendering::metal4
