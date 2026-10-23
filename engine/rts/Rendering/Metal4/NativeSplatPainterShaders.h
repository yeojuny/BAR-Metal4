/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// OpenAI Codex adaptation of the pinned original BAR Splat Painter shaders.
namespace rendering::metal4 {
inline constexpr const char* NativeSplatPainterShaderSource=R"SPLATMSL(
struct SplatPainterUniforms {
 float4 brushPos;
 float4 brushRadius;
 float4 brushStrength;
 int4 brushChannel;
 int4 brushErase;
 float4 texSize;
 float4 mapSize;
 float4 brushCurve;
 int4 brushShape;
 float4 brushRotation;
 float4 fractalAmount;
 float4 fractalFreq;
 int4 sfEnabled;
 int4 sfAvoidWater;
 int4 sfAvoidCliffs;
 float4 sfSlopeMax;
 int4 sfPreferSlopes;
 float4 sfSlopeMin;
 int4 sfAltMinEnable;
 float4 sfAltMin;
 int4 sfAltMaxEnable;
 float4 sfAltMax;
 int4 infAltOn;
 float4 infAlt;
 int4 infSlopeOn;
 float4 infSlope;
};
float4 SplatOriginalSample(texture2d<float> image,sampler sample,float2 uv,uint flip,float wrapS,float wrapT){
 if(flip)uv.y=1.f-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);
 if(wrapS)uv.x=clamp(uv.x,0.f,1.f);if(wrapT)uv.y=clamp(uv.y,0.f,1.f);
 return image.sample(sample,uv,gradient2d(dx,dy));
}

	// Smart-filter uniforms
	// INFLUENCE (soft bands scaling the stroke; x = min, y = max, z = feather
	// below min, w = feather above max; slope in degrees)

	// ------ smart-filter helpers ------
	float Splat_sampleHeight(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 uv) {
		return SplatOriginalSample(heightMap,heightSampler,uv,screen.options1.y,screen.parameters1.y,screen.parameters1.z).x;
	}

	float Splat_smoothBand(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float v, float lo, float hi, float fLo, float fHi) {
		float a = (fLo > 0.001f) ? smoothstep(lo - fLo, lo, v) : step(lo, v);
		float b = (fHi > 0.001f) ? (1.0f - smoothstep(hi, hi + fHi, v)) : step(v, hi);
		return clamp(a * b, 0.0f, 1.0f);
	}

	float Splat_influenceAt(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 uv) {
		if (brushUniform.infAltOn.x == 0 && brushUniform.infSlopeOn.x == 0) return 1.0f;
		float w = 1.0f;
		if (brushUniform.infAltOn.x == 1) {
			w *= Splat_smoothBand(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv), brushUniform.infAlt.xyzw.x, brushUniform.infAlt.xyzw.y, brushUniform.infAlt.xyzw.z, brushUniform.infAlt.xyzw.w);
		}
		if (brushUniform.infSlopeOn.x == 1) {
			float2 hmTexel = 1.0f / float2(heightMap.get_width(),heightMap.get_height());
			float hL = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(-hmTexel.x, 0.0f));
			float hR = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2( hmTexel.x, 0.0f));
			float hD = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(0.0f, -hmTexel.y));
			float hU = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(0.0f,  hmTexel.y));
			float2 cellSize = brushUniform.mapSize.xy * hmTexel;
			float3 n = normalize(float3(hL - hR, 2.0f * cellSize.x, hD - hU));
			float slopeDeg = acos(clamp(n.y, 0.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);
			w *= Splat_smoothBand(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,slopeDeg, brushUniform.infSlope.xyzw.x, brushUniform.infSlope.xyzw.y, brushUniform.infSlope.xyzw.z, brushUniform.infSlope.xyzw.w);
		}
		return w;
	}

	bool Splat_passesSmartFilter(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 uv) {
		if (brushUniform.sfEnabled.x == 0) return true;

		float h = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv);

		// Water
		if (brushUniform.sfAvoidWater.x == 1 && h < 0.0f) return false;

		// Altitude caps
		if (brushUniform.sfAltMinEnable.x == 1 && h < brushUniform.sfAltMin.x) return false;
		if (brushUniform.sfAltMaxEnable.x == 1 && h > brushUniform.sfAltMax.x) return false;

		// Slope (compute normal from heightmap finite-differences)
		if (brushUniform.sfAvoidCliffs.x == 1 || brushUniform.sfPreferSlopes.x == 1) {
			// One heightmap texel in UV space
			float2 hmTexel = 1.0f / float2(heightMap.get_width(),heightMap.get_height());
			float hL = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(-hmTexel.x, 0.0f));
			float hR = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2( hmTexel.x, 0.0f));
			float hD = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(0.0f, -hmTexel.y));
			float hU = Splat_sampleHeight(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv + float2(0.0f,  hmTexel.y));

			// World distance between adjacent heightmap samples
			float2 cellSize = brushUniform.mapSize.xy * hmTexel;
			float3 normal = normalize(float3(hL - hR, 2.0f * cellSize.x, hD - hU));
			float ny = normal.y;

			if (brushUniform.sfAvoidCliffs.x == 1) {
				float cosMax = cos(brushUniform.sfSlopeMax.x * 3.14159265f / 180.0f);
				if (ny < cosMax) return false;
			}
			if (brushUniform.sfPreferSlopes.x == 1) {
				float cosMin = cos(brushUniform.sfSlopeMin.x * 3.14159265f / 180.0f);
				if (ny > cosMin) return false;
			}
		}

		return true;
	}

	// ------ fBm domain-warp for organic brush edges ------
	float Splat_hfh(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 p) { return fract(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f); }
	float Splat_hfn(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 p) {
		float2 i = floor(p), f = fract(p);
		float2 u = f * f * (3.0f - 2.0f * f);
		return mix(mix(Splat_hfh(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,i), Splat_hfh(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,i+float2(1.0f,0.0f)), u.x),
		           mix(Splat_hfh(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,i+float2(0.0f,1.0f)), Splat_hfh(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,i+float2(1.0f,1.0f)), u.x), u.y) * 2.0f - 1.0f;
	}
	float Splat_hffbm(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 p) {
		float v = 0.0f;
		v += 0.500f * Splat_hfn(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,p); p *= 2.13f;
		v += 0.225f * Splat_hfn(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,p); p *= 2.13f;
		v += 0.101f * Splat_hfn(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,p); p *= 2.13f;
		v += 0.045f * Splat_hfn(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,p);
		return v;
	}

	float4 SplatPaint(constant SplatPainterUniforms& brushUniform,constant ScreenUniform& screen,texture2d<float> tex0,sampler colorSampler,texture2d<float> heightMap,sampler heightSampler,float2 inputUV) {
		float2 uv = inputUV;
		float4 current = SplatOriginalSample(tex0,colorSampler,uv,screen.options.y,screen.parameters.y,screen.parameters.z);

		// Convert UV to world position
		float2 worldPos = uv * brushUniform.mapSize.xy;

		// Domain-warp the sample position for fractal / organic brush edges.
		// Only shifts the falloff shape; the smart filter still tests the real pixel.
		if (brushUniform.fractalAmount.x > 0.001f) {
			float wx = Splat_hffbm(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,worldPos * brushUniform.fractalFreq.x + float2(31.4f, 57.2f));
			float wy = Splat_hffbm(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,worldPos * brushUniform.fractalFreq.x + float2(89.7f, 23.1f));
			worldPos += float2(wx, wy) * brushUniform.brushRadius.x * brushUniform.fractalAmount.x;
		}

		// Vector from brush center to this pixel
		float2 delta = worldPos - brushUniform.brushPos.xy;

		// Apply rotation
		float cr = cos(-brushUniform.brushRotation.x);
		float sr = sin(-brushUniform.brushRotation.x);
		float2 rotDelta = float2(
			delta.x * cr - delta.y * sr,
			delta.x * sr + delta.y * cr
		);

		// Compute distance based on shape
		float dist = 0.0f;
		if (brushUniform.brushShape.x == 0) {
			// Circle
			dist = length(rotDelta) / brushUniform.brushRadius.x;
		} else if (brushUniform.brushShape.x == 1) {
			// Square
			dist = max(abs(rotDelta.x), abs(rotDelta.y)) / brushUniform.brushRadius.x;
		} else if (brushUniform.brushShape.x == 2) {
			// Triangle (equilateral, apex at -Z = north, matches outline)
			float2 p = rotDelta / brushUniform.brushRadius.x;
			// Negate p.y so apex points toward -Z (north) matching the visual outline
			float py = -p.y - 0.333333f;
			// Half-plane distances for equilateral triangle
			float d1 = -py - 0.5f;                              // south edge
			float d2 =  0.866025f * p.x + 0.5f * py - 0.5f;      // lower-right edge
			float d3 = -0.866025f * p.x + 0.5f * py - 0.5f;      // lower-left edge
			dist = max(max(d1, d2), d3) + 1.0f;
		} else if (brushUniform.brushShape.x == 3) {
			// Hexagon
			float2 a = abs(rotDelta) / brushUniform.brushRadius.x;
			dist = max(a.x * 0.866025f + a.y * 0.5f, a.y);
		} else if (brushUniform.brushShape.x == 4) {
			// Octagon
			float2 a = abs(rotDelta) / brushUniform.brushRadius.x;
			float oct = 0.4142136f * (a.x + a.y);
			dist = max(max(a.x, a.y), oct);
		}

		if (dist >= 1.0f) {
			return current;
		}

		// Smart filter: reject pixels that fail terrain checks
		if (!Splat_passesSmartFilter(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv)) {
			return current;
		}

		// Falloff
		float falloff = 1.0f - pow(dist, brushUniform.brushCurve.x);
		float amount = brushUniform.brushStrength.x * falloff;
		// INFLUENCE scales paint only; erase always lands at full strength
		if (brushUniform.brushErase.x == 0) amount *= Splat_influenceAt(brushUniform,screen,tex0,colorSampler,heightMap,heightSampler,uv);

		float4 result = current;
		if (brushUniform.brushErase.x == 1) {
			// Erase: reduce selected channel
			result[brushUniform.brushChannel.x] = max(0.0f, result[brushUniform.brushChannel.x] - amount);
		} else {
			// Paint: increase selected channel, optionally reduce others
			result[brushUniform.brushChannel.x] = min(1.0f, result[brushUniform.brushChannel.x] + amount);
			// Normalize so channels sum to ~1 (keeps weights balanced)
			float total = result.r + result.g + result.b + result.a;
			if (total > 1.0f) {
				float excess = total - 1.0f;
				// Reduce other channels proportionally
				for (int i = 0; i < 4; i++) {
					if (i != brushUniform.brushChannel.x) {
						float share = result[i] / (total - result[brushUniform.brushChannel.x]);
						result[i] = max(0.0f, result[i] - excess * share);
					}
				}
			}
		}

		return result;
	}

fragment float4 SplatPaintFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],
 constant SplatPainterUniforms& brushUniform [[buffer(3)]],texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]],
 texture2d<float> height [[texture(1)]],sampler heightSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=SplatPaint(brushUniform,screen,image,imageSampler,height,heightSampler,in.uv);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
fragment float4 SplatCopyFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],
 texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 out=SplatOriginalSample(image,imageSampler,in.uv,screen.options.y,screen.parameters.y,screen.parameters.z);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
fragment float4 SplatOverlayFragment(ScreenVarying in [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],
 texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const float4 sp=SplatOriginalSample(image,imageSampler,in.uv,screen.options.y,screen.parameters.y,screen.parameters.z);
 const float3 c=sp.r*float3(1.f,.15f,.15f)+sp.g*float3(.15f,1.f,.15f)+sp.b*float3(.2f,.45f,1.f)+sp.a*float3(1.f,.85f,.1f);
 const float4 out=float4(c,clamp(sp.r+sp.g+sp.b+sp.a,0.f,1.f)*.55f);
 if(screen.options.z&&!AlphaPass(out.a,screen.parameters.x,screen.options.w))discard_fragment();return out;
}
)SPLATMSL";
} // namespace rendering::metal4
