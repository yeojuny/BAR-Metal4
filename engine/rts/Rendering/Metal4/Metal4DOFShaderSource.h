/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Complete fixed31657 dof.vs/dof.fs transport. Original focus executes in the
// filter vertex; all eight original fragment passes retain their source equations.
namespace rendering::metal4 {
inline constexpr const char* NATIVE_DOF_SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"DOFMSL(
#include <metal_stdlib>
using namespace metal;
struct DOFUniforms {float4x4 projection;float4 eye,resolutionDistance,mouseFocus,autoParams;int4 settings;float4 target;};
struct DOFResult {float4 c0,c1,c2,c3;};
struct DOFVarying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];float focusDepth [[flat]];float aperture [[flat]];};
struct DOFQuad {float4 clip,uv;};
float2 DOFFlip(float2 uv){return float2(uv.x,1-uv.y);}
float4 DOFSample(texture2d<float> image,sampler filter,float2 uv){return image.sample(filter,DOFFlip(uv),level(0.0f));}
// Original depth is NEAREST/REPEAT. Select its GL texel before reversing
// native storage Y, including exact integer wrap/tie coordinates.
float4 DOFSample(depth2d<float> image,sampler filter,float2 uv){
 int2 extent=int2(image.get_width(),image.get_height());int2 texel=int2(floor(uv*float2(extent)));
 texel=(texel%extent+extent)%extent;texel.y=extent.y-1-texel.y;
 float d=image.read(uint2(texel));return float4(d,d,d,1);
}
float4 DOFTexelFetch(texture2d<float> image,int2 texel){texel.y=int(image.get_height())-1-texel.y;return image.read(uint2(texel),0);}
float2 DOFGLFragCoord(constant DOFUniforms& uniforms,float4 position){return float2(position.x,uniforms.target.x-position.y);}
#define textureLod(TEX,UV,LOD) DOFSample(TEX,TEX##Sampler,UV)
#define textureSize(TEX,LOD) int2(TEX.get_width(LOD),TEX.get_height(LOD))
#define texelFetch(TEX,COORD,LOD) DOFTexelFetch(TEX,COORD)
#define projectionMat uniforms.projection
#define resolution uniforms.resolutionDistance.xy
#define mouseDepthCoord uniforms.mouseFocus.xy
#define manualFocusDepth uniforms.mouseFocus.z
#define fStop uniforms.mouseFocus.w
#define autofocusFudgeFactor uniforms.autoParams.x
#define autofocusPower uniforms.autoParams.y
#define autofocusFocalLength uniforms.autoParams.z
#define autofocus uniforms.settings.x
#define mousefocus uniforms.settings.y
#define FILTER_SIZE_PASS 0
#define INITIAL_BLUR_PASS 1
#define FINAL_BLUR_PASS 2
#define INITIAL_NEAR_BLUR_PASS 3
#define FINAL_NEAR_BLUR_PASS 4
#define COMPOSITION_PASS 5
#define TILE_FLAG_PASS 6
#define TILE_MASK_PASS 7
#define BLUR_START_DIST 10000
#define TILE_SIZE 16
#define NORMALIZE_FILTER(fullRangeFilter) (fullRangeFilter * 0.25) + 0.5
#define UNNORMALIZE_FILTER(normedFilter) ((2.0 * normedFilter) - 1.0) * 2.0
// Circular DOF by Kleber Garcia "Kecho" - 2017
// Publication & Filter generator: https://github.com/kecho/CircularDofFilterGenerator

/** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/
//Main blur pass parameters
constant int KERNEL_RADIUS = 5;
constant int KERNEL_COUNT = 11;
constant float4 Kernel0BracketsRealXY_ImZW = float4(-0.056556,0.920040,-0.035849,0.611305);
constant float2 Kernel0Weights_RealX_ImY = float2(0.411259,-0.548794);
constant float4 Kernel0_RealX_ImY_RealZ_ImW[] = {
	float4(/*XY: Non Bracketed*/0.022302,-0.035849,/*Bracketed WZ:*/0.085711,0.000000),
	float4(/*XY: Non Bracketed*/-0.056556,-0.013273,/*Bracketed WZ:*/0.000000,0.036931),
	float4(/*XY: Non Bracketed*/-0.023847,0.070538,/*Bracketed WZ:*/0.035552,0.174032),
	float4(/*XY: Non Bracketed*/0.059140,0.066382,/*Bracketed WZ:*/0.125751,0.167233),
	float4(/*XY: Non Bracketed*/0.096696,0.020687,/*Bracketed WZ:*/0.166571,0.092483),
	float4(/*XY: Non Bracketed*/0.102454,0.000000,/*Bracketed WZ:*/0.172829,0.058643),
	float4(/*XY: Non Bracketed*/0.096696,0.020687,/*Bracketed WZ:*/0.166571,0.092483),
	float4(/*XY: Non Bracketed*/0.059140,0.066382,/*Bracketed WZ:*/0.125751,0.167233),
	float4(/*XY: Non Bracketed*/-0.023847,0.070538,/*Bracketed WZ:*/0.035552,0.174032),
	float4(/*XY: Non Bracketed*/-0.056556,-0.013273,/*Bracketed WZ:*/0.000000,0.036931),
	float4(/*XY: Non Bracketed*/0.022302,-0.035849,/*Bracketed WZ:*/0.085711,0.000000)
};
constant float4 Kernel1BracketsRealXY_ImZW = float4(0.000181,0.552380,0.000000,0.180493);
constant float2 Kernel1Weights_RealX_ImY = float2(0.513282,4.561110);
constant float4 Kernel1_RealX_ImY_RealZ_ImW[] = {
	float4(/*XY: Non Bracketed*/0.000181,0.014423,/*Bracketed WZ:*/0.000000,0.079908),
	float4(/*XY: Non Bracketed*/0.015852,0.024540,/*Bracketed WZ:*/0.028370,0.135962),
	float4(/*XY: Non Bracketed*/0.042831,0.026910,/*Bracketed WZ:*/0.077211,0.149093),
	float4(/*XY: Non Bracketed*/0.072553,0.018473,/*Bracketed WZ:*/0.131019,0.102347),
	float4(/*XY: Non Bracketed*/0.094542,0.005900,/*Bracketed WZ:*/0.170826,0.032690),
	float4(/*XY: Non Bracketed*/0.102454,0.000000,/*Bracketed WZ:*/0.185149,0.000000),
	float4(/*XY: Non Bracketed*/0.094542,0.005900,/*Bracketed WZ:*/0.170826,0.032690),
	float4(/*XY: Non Bracketed*/0.072553,0.018473,/*Bracketed WZ:*/0.131019,0.102347),
	float4(/*XY: Non Bracketed*/0.042831,0.026910,/*Bracketed WZ:*/0.077211,0.149093),
	float4(/*XY: Non Bracketed*/0.015852,0.024540,/*Bracketed WZ:*/0.028370,0.135962),
	float4(/*XY: Non Bracketed*/0.000181,0.014423,/*Bracketed WZ:*/0.000000,0.079908)
};

constant float baseStepValMag = 1.0/540.0;

constant float colorPower = 1.9;

constant float inFocusThreshold = 0.4 / float(KERNEL_RADIUS);
constant float focusMixDepthRange = (float(KERNEL_RADIUS) * 2.0);
constant float maxFilterRadius = 1.2; //keep between 0 and 2. Any higher than 2 will require modifying the normalization maths
								   //(currently does (radius/4)+0.5 to get [-2..2] to [0..1])

// Per-texel flags for skipping blur tiles, with margin for the 8-bit radius in finalBlurTex and the 16-bit one in the
// base texture: the composition only mixes in the far blur where |radius| > inFocusThreshold, and the near blur
// only gets alpha from radii < -inFocusThreshold
constant float tileFarRadius = 0.06;
constant float tileNearRadius = 0.07;

#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)

float2 multComplex(float2 p, float2 q)
{
	return float2(p.x*q.x-p.y*q.y, p.x*q.y+p.y*q.x);
}

float4 get2CompFilters(int x)
{
	float2 c0 = Kernel0_RealX_ImY_RealZ_ImW[x].xy;
	float2 c1 = Kernel1_RealX_ImY_RealZ_ImW[x].xy;
	return float4(c0.x, c0.y, c1.x, c1.y);
}

constant float2 autofocusTestCoordOffsets[] = {
	float2(-0.71, -0.71),
	float2(-0.71, 0.76),
	float2(0.71, 0.76),
	float2(0.71, -0.71),
	float2(-1.0, 0.0),
	float2(0.0, 1.1),
	float2(0.0, -1.0),
	float2(1.0, 0.0)
};

template<class T> float DOFLinearizeDepth(constant DOFUniforms& uniforms,T blurTex0,sampler blurTex0Sampler,float2 uv){
	float depthNDC = textureLod(blurTex0, uv, 0.0).r;
	if (uniforms.autoParams.w == 0)
		depthNDC = NORM2SNORM(depthNDC);
	
		// no need to do anything depthNDC is already in [0;1] range
	

	//return ((abs(((1.0 + depthNDC) * (1.0 + n22))/(2.0 * (depthNDC + n22)))
	//	* (distanceLimits.y - distanceLimits.x)) + distanceLimits.x) / BLUR_START_DIST;
	return -(projectionMat[3][2] / (projectionMat[2][2] + depthNDC)) / BLUR_START_DIST;
}

float DOFGetFilterRadius(texture2d<float> origTex,sampler origTexSampler,float2 uv)
{
	return UNNORMALIZE_FILTER(textureLod(origTex, uv, 0.0).a);
}

float DOFGetEdgeNearFilterRadius(texture2d<float> origTex,sampler origTexSampler,float2 uv,float2 stepVal)
{
	float2 maxCoordsOffset = stepVal * maxFilterRadius * KERNEL_RADIUS;
	float2 maxCoordsOffsetPerp = float2(stepVal.y, -stepVal.x) * maxFilterRadius * KERNEL_RADIUS;
	float edgeRadius =
	min(min(DOFGetFilterRadius(origTex,origTexSampler,uv + maxCoordsOffset), DOFGetFilterRadius(origTex,origTexSampler,uv - maxCoordsOffset)),
	min(DOFGetFilterRadius(origTex,origTexSampler,uv + maxCoordsOffsetPerp), DOFGetFilterRadius(origTex,origTexSampler,uv - maxCoordsOffsetPerp)));
	float halfEdgeRadius =
	min(min(DOFGetFilterRadius(origTex,origTexSampler,uv + maxCoordsOffset / 2.0), DOFGetFilterRadius(origTex,origTexSampler,uv - maxCoordsOffset / 2.0)),
	min(DOFGetFilterRadius(origTex,origTexSampler,uv + maxCoordsOffsetPerp / 2.0), DOFGetFilterRadius(origTex,origTexSampler,uv - maxCoordsOffsetPerp / 2.0)));
	return min(edgeRadius, halfEdgeRadius);
}

float2 DOFGetFilterCoords(texture2d<float> origTex,sampler origTexSampler,int i,float2 uv,float2 stepVal,float filterRadius,thread float& targetFilterRadius)
{
	float filterDistance = float(i)*abs(filterRadius);
	float2 coords = uv + stepVal*filterDistance;
	targetFilterRadius = DOFGetFilterRadius(origTex,origTexSampler,coords);

	//Taking the filter radius for the first candidate sampled pixel if it's less than the base filter radius
	//makes sure that we both don't blur in-focus objects into out-of-focus regions behind them, and
	//also blur in the out-of-focus objects nearer to the camera than the in-focus region.
	//This works because it's basically checking if the first candidate sampled pixel's blur radius is large
	//enough to hit the pixel we are gathering into now, that means its circle of confusion is big enough to reach
	//that starting pixel.
	if (targetFilterRadius - filterRadius < -0.02 / float(KERNEL_RADIUS))
	{
		filterDistance = (float(i))*abs(targetFilterRadius);
		coords = uv + stepVal*filterDistance;
	}
	return coords;
}

//Used to find the mix value to blend between the full-size screen texture and the
//downscaled out-of-focus textures.
float FocusThresholdMixFactor(float filterRadius, float threshold)
{
	return clamp((filterRadius - threshold) * focusMixDepthRange, 0.0, 1.0);
}

float ApertureSizeToKeepFocusFor(float targetInFocusDepth, float focusDepth)
{
	return (targetInFocusDepth * inFocusThreshold) / abs(targetInFocusDepth - focusDepth);
}

float2 DOFComputeFocus(constant DOFUniforms& uniforms,depth2d<float> blurTex0,sampler blurTex0Sampler){
		float aspectRatio = resolution.y / resolution.x;
		float focusDepth = manualFocusDepth;
		float aperture = 1.0/fStop;

		float2 centerUV = float2(0.5,0.5);
		if (mousefocus == 1)
		{
			centerUV = mouseDepthCoord;
			focusDepth = DOFLinearizeDepth(uniforms,blurTex0,blurTex0Sampler,mouseDepthCoord);
		}

		if (autofocus == 1)
		{
			//The numbers in the autofocus computation that look like magic numbers are,
			//found by experimentation to work well enough in practice, but not sacred.
			float centerDepth = DOFLinearizeDepth(uniforms,blurTex0,blurTex0Sampler,centerUV);
			focusDepth = centerDepth;
			float testFocusDepth = focusDepth;

			//Find the depths to use as a safety bound for the in-focus region
			float minTestDepth = focusDepth;
			float maxTestDepth = focusDepth;
			float testDepth = 0.0;
			int autofocusTestCoordCount = 8;
			for (int i = 0; i < autofocusTestCoordCount; ++i)
			{
				testDepth = DOFLinearizeDepth(uniforms,blurTex0,blurTex0Sampler,centerUV +
					(float2(autofocusTestCoordOffsets[i].x * aspectRatio,
						autofocusTestCoordOffsets[i].y) * clamp(focusDepth * 3.3, 0.1, 0.225)));
				//We use averages here instead of just directly min/max testing testDepth in order to have smoother focus transitions
				//across big changes to focus depth, such as the camera scrolling over a cliff or being zoomed in on a unit.
				minTestDepth = min(minTestDepth, (3.0 * minTestDepth + 2.0 * testDepth) / 5.0);
				maxTestDepth = max(maxTestDepth, (3.0 * maxTestDepth + 2.0 * testDepth) / 5.0);
			}

			//pull focus back a bit to bias slightly towards air units and against distant terrain
			float focusDepthAirFactor = clamp(0.92 + (focusDepth * 12.0), 0.92, 1.2);
			testFocusDepth /= max(focusDepthAirFactor, 1.0);
			focusDepth /= max(focusDepthAirFactor, 1.0);

			//The min depth bound is scaled more strongly to reduce air unit blurring when zoomed moderately out
			minTestDepth = min(minTestDepth / focusDepthAirFactor, focusDepth);
			maxTestDepth =
				max(focusDepthAirFactor > 1.0 ?
					(maxTestDepth + 2.5 * maxTestDepth * focusDepthAirFactor) / 3.5 :
					maxTestDepth * focusDepthAirFactor, focusDepth);

			float minFStop = 1.0;
			float curveDepth = autofocusPower;
			float baseAperture = autofocusFocalLength/max(testFocusDepth * exp(curveDepth * testFocusDepth), minFStop * autofocusFocalLength);

			float apertureBoundsFudgeFactor = 1.0 / autofocusFudgeFactor; //Used to control bounds depths without having to change inFocusThreshold
			float maxDepthAperture = ApertureSizeToKeepFocusFor(maxTestDepth, focusDepth) * apertureBoundsFudgeFactor;
			float minDepthAperture = ApertureSizeToKeepFocusFor(minTestDepth, focusDepth) * apertureBoundsFudgeFactor;

			aperture = min(baseAperture, min(maxDepthAperture, minDepthAperture));
		}

		return float2(focusDepth,aperture);

}
DOFVarying DOFQuadVertex(uint i,constant DOFQuad& q){
 constexpr uint index[6]={0,1,2,0,2,3};constexpr float2 corner[4]={float2(0,0),float2(1,0),float2(1,1),float2(0,1)};
 float2 p=corner[index[i]];DOFVarying o;o.position=float4(mix(q.clip.xy,q.clip.zw,p),0,1);o.uv=mix(q.uv.xy,q.uv.zw,p);o.focusDepth=0;o.aperture=0;return o;
}
vertex DOFVarying DOFVertex(uint i [[vertex_id]],constant DOFQuad& q [[buffer(0)]]){return DOFQuadVertex(i,q);}
vertex DOFVarying DOFFilterVertex(uint i [[vertex_id]],constant DOFQuad& q [[buffer(0)]],constant DOFUniforms& uniforms [[buffer(1)]],depth2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]]){
 auto o=DOFQuadVertex(i,q);const float2 focus=DOFComputeFocus(uniforms,blurTex0,blurTex0Sampler);o.focusDepth=focus.x;o.aperture=focus.y;return o;
}
#define TILE_NEEDED(CHANNEL) (DOFTexelFetch(tileMask,int2(DOFGLFragCoord(uniforms,fragmentPosition))/TILE_SIZE).CHANNEL > 0.5)
template<int PASS_VALUE,int QUALITY_VALUE,class BlurTexture>
DOFResult DOFEvaluate(constant DOFUniforms& uniforms,float2 uv,float4 fragmentPosition,float focusDepthV,float apertureV,
 texture2d<float> origTex,sampler origTexSampler,BlurTexture blurTex0,sampler blurTex0Sampler,
 texture2d<float> blurTex1,sampler blurTex1Sampler,texture2d<float> blurTex2,sampler blurTex2Sampler,
 texture2d<float> blurTex3,sampler blurTex3Sampler,texture2d<float> tileMask,sampler tileMaskSampler){
 DOFResult result;

	
	float aspectRatio = resolution.y / resolution.x;
	float2 stepVal = float2(baseStepValMag * aspectRatio, baseStepValMag);

	if (PASS_VALUE == FILTER_SIZE_PASS)
	{
		float depth = DOFLinearizeDepth(uniforms,blurTex0,blurTex0Sampler,uv);
		float filterRadius = clamp(((depth - focusDepthV) * apertureV)/depth, -maxFilterRadius, maxFilterRadius);

		float4 colors = textureLod(origTex, uv, 0.0);
		//Add extra brightness to brighter colours in blurrier spots to maintain a consistent exposure.
		//In real life there would be a wide enough range of light levels to make bright blurry regions not
		//lose brightness when blurring, but we need to fudge that here.
		float lum = dot(colors.rgb,float3(0.2126,0.7152,0.0722))*(min(0.2 + 0.65 * abs(filterRadius), 1.5));
		colors = colors *(1.0 + 0.2*lum*lum*lum);
		//Raise colours to a power to increase the sharpness of the blur discs
		colors = float4(pow(colors.r, colorPower), pow(colors.g, colorPower), pow(colors.b, colorPower), colors.a);

		result.c0 = float4(colors.rgb, NORMALIZE_FILTER(filterRadius));
		result.c1 = float4(abs(filterRadius) > tileFarRadius ? 1.0 : 0.0, filterRadius < -tileNearRadius ? 1.0 : 0.0, 0.0, 0.0);
	}
	else if (PASS_VALUE == INITIAL_BLUR_PASS)
	{
		if (!TILE_NEEDED(r))
			discard_fragment();

		float4 valR = float4(0,0,0,0);
		float4 valG = float4(0,0,0,0);
		float4 valB = float4(0,0,0,0);
		float4 centerTexel = textureLod(origTex, uv, 0.0);
		float filterRadius = UNNORMALIZE_FILTER(centerTexel.a);
		float targetFilterRadius = 0.0;
		for (int i=-KERNEL_RADIUS; i <=KERNEL_RADIUS; ++i)
		{
			float4 imageTexelRGB = centerTexel;
			if (i != 0)
			{
				float2 coords = DOFGetFilterCoords(origTex,origTexSampler,i, uv, float2(0.0, stepVal.y), filterRadius, targetFilterRadius);
				imageTexelRGB = textureLod(origTex, coords, 0.0);
			}

			float4 c0_c1 = get2CompFilters(i+KERNEL_RADIUS);
			valR.xy += imageTexelRGB.r * c0_c1.xy;
			valR.zw += imageTexelRGB.r * c0_c1.zw;
			valG.xy += imageTexelRGB.g * c0_c1.xy;
			valG.zw += imageTexelRGB.g * c0_c1.zw;
			valB.xy += imageTexelRGB.b * c0_c1.xy;
			valB.zw += imageTexelRGB.b * c0_c1.zw;
		}
		result.c0 = valR;
		result.c1 = valG;
		result.c2 = valB;
	}
	else if (PASS_VALUE == FINAL_BLUR_PASS)
	{
		float filterRadius = DOFGetFilterRadius(origTex,origTexSampler,uv);
		float normalizedRadius = NORMALIZE_FILTER(filterRadius);
		// the composition reads the radius everywhere, the colour only where the tile mask says it can be mixed in
		if (!TILE_NEEDED(g))
		{
			result.c0 = float4(0.0, 0.0, 0.0, normalizedRadius);
			return result;
		}

		float4 valR = float4(0,0,0,0);
		float4 valG = float4(0,0,0,0);
		float4 valB = float4(0,0,0,0);
		float targetFilterRadius = 0.0;
		for (int i=-KERNEL_RADIUS; i <=KERNEL_RADIUS; ++i)
		{
			float2 coords = uv;
			if (i != 0)
			{
				coords = DOFGetFilterCoords(origTex,origTexSampler,i, uv, float2(stepVal.x, 0.0), filterRadius, targetFilterRadius);
			}
			float4 imageTexelR = textureLod(blurTex0, coords, 0.0);
			float4 imageTexelG = textureLod(blurTex1, coords, 0.0);
			float4 imageTexelB = textureLod(blurTex2, coords, 0.0);

			float4 c0_c1 = get2CompFilters(i+KERNEL_RADIUS);

			valR.xy += multComplex(imageTexelR.xy,c0_c1.xy);
			valR.zw += multComplex(imageTexelR.zw,c0_c1.zw);

			valG.xy += multComplex(imageTexelG.xy,c0_c1.xy);
			valG.zw += multComplex(imageTexelG.zw,c0_c1.zw);

			valB.xy += multComplex(imageTexelB.xy,c0_c1.xy);
			valB.zw += multComplex(imageTexelB.zw,c0_c1.zw);
		}

		float redChannel	 = dot(valR.xy,Kernel0Weights_RealX_ImY)+dot(valR.zw,Kernel1Weights_RealX_ImY);
		float greenChannel = dot(valG.xy,Kernel0Weights_RealX_ImY)+dot(valG.zw,Kernel1Weights_RealX_ImY);
		float blueChannel	= dot(valB.xy,Kernel0Weights_RealX_ImY)+dot(valB.zw,Kernel1Weights_RealX_ImY);

		result.c0 = float4(float3(pow(redChannel, 1.0/colorPower),pow(greenChannel, 1.0/colorPower),
			pow(blueChannel, 1.0/colorPower)), normalizedRadius);
	}
	else if (PASS_VALUE == INITIAL_NEAR_BLUR_PASS)
	{
		if (!TILE_NEEDED(b))
			discard_fragment();

		float4 valR = float4(0,0,0,0);
		float4 valG = float4(0,0,0,0);
		float4 valB = float4(0,0,0,0);
		float4 valA = float4(0,0,0,0);
		//Start by finding the maximum possible relevant blur radius, since we're blurring things
		//that will end up in front of more in-focus objects.
		float4 centerTexel = textureLod(origTex, uv, 0.0);
		float baseFilterRadius = UNNORMALIZE_FILTER(centerTexel.a);
		float filterRadius = min(baseFilterRadius, DOFGetEdgeNearFilterRadius(origTex,origTexSampler,uv, stepVal));
		filterRadius = min(filterRadius, DOFGetEdgeNearFilterRadius(origTex,origTexSampler,uv, float2(stepVal.y, 0.0)));
		float targetFilterRadius = 0.0;
		for (int i=-KERNEL_RADIUS; i <=KERNEL_RADIUS; ++i)
		{
			float4 c0_c1 = get2CompFilters(i+KERNEL_RADIUS);
			float4 imageTexelRGB = centerTexel;
			// the centre tap's alpha is min(.., |base - base| / 0.05) = 0
			if (i != 0)
			{
				float2 coords = DOFGetFilterCoords(origTex,origTexSampler,i, uv, float2(0.0, stepVal.y), filterRadius, targetFilterRadius);
				imageTexelRGB = textureLod(origTex, coords, 0.0);
				float alpha = FocusThresholdMixFactor(-targetFilterRadius, inFocusThreshold);
				alpha = min(alpha, clamp(abs(targetFilterRadius - baseFilterRadius) / 0.05, 0.0, 1.0));
				valA.xy += alpha * c0_c1.xy;
				valA.zw += alpha * c0_c1.zw;
			}

			valR.xy += imageTexelRGB.r * c0_c1.xy;
			valR.zw += imageTexelRGB.r * c0_c1.zw;
			valG.xy += imageTexelRGB.g * c0_c1.xy;
			valG.zw += imageTexelRGB.g * c0_c1.zw;
			valB.xy += imageTexelRGB.b * c0_c1.xy;
			valB.zw += imageTexelRGB.b * c0_c1.zw;
		}

		result.c0 = valR;
		result.c1 = valG;
		result.c2 = valB;
		result.c3 = valA;
	}
	else if (PASS_VALUE == FINAL_NEAR_BLUR_PASS)
	{
		// no foreground in reach: alpha is exactly 0, and no mixed-in pixel samples this texel's colour
		if (!TILE_NEEDED(a))
		{
			result.c0 = float4(0.0);
			return result;
		}

		float4 valR = float4(0,0,0,0);
		float4 valG = float4(0,0,0,0);
		float4 valB = float4(0,0,0,0);
		float4 valA = float4(0,0,0,0);
		float baseFilterRadius = DOFGetFilterRadius(origTex,origTexSampler,uv);
		float filterRadius = min(baseFilterRadius, DOFGetEdgeNearFilterRadius(origTex,origTexSampler,uv, stepVal));
		filterRadius = min(filterRadius, DOFGetEdgeNearFilterRadius(origTex,origTexSampler,uv, float2(stepVal.x, 0.0)));
		float targetFilterRadius = 0.0;
		for (int i=-KERNEL_RADIUS; i <=KERNEL_RADIUS; ++i)
		{
			float4 c0_c1 = get2CompFilters(i+KERNEL_RADIUS);
			float2 coords = uv;
			// the centre tap's alpha is min(.., |base - base| / 0.05) = 0
			if (i != 0)
			{
				coords = DOFGetFilterCoords(origTex,origTexSampler,i, uv, float2(stepVal.x, 0.0), filterRadius, targetFilterRadius);

				//imageTexelA has the alpha from the initial pass, but we need to also get it for the
				//final pass separately since alpha represents the edge of different filter radii, and
				//that's not something a single pass of a 2-pass blur will fully pick up.
				float finalPassAlpha = FocusThresholdMixFactor(-targetFilterRadius, inFocusThreshold);
				finalPassAlpha = min(finalPassAlpha, clamp(abs(targetFilterRadius - baseFilterRadius) / 0.05, 0.0, 1.0));
				valA.xy += finalPassAlpha * c0_c1.xy;
				valA.zw += finalPassAlpha * c0_c1.zw;
			}

			float4 imageTexelR = textureLod(blurTex0, coords, 0.0);
			float4 imageTexelG = textureLod(blurTex1, coords, 0.0);
			float4 imageTexelB = textureLod(blurTex2, coords, 0.0);
			float4 imageTexelA = textureLod(blurTex3, coords, 0.0);

			valR.xy += multComplex(imageTexelR.xy,c0_c1.xy);
			valR.zw += multComplex(imageTexelR.zw,c0_c1.zw);

			valG.xy += multComplex(imageTexelG.xy,c0_c1.xy);
			valG.zw += multComplex(imageTexelG.zw,c0_c1.zw);

			valB.xy += multComplex(imageTexelB.xy,c0_c1.xy);
			valB.zw += multComplex(imageTexelB.zw,c0_c1.zw);

			valA.xy += multComplex(imageTexelA.xy,c0_c1.xy);
			valA.zw += multComplex(imageTexelA.zw,c0_c1.zw);
		}

		float redChannel	 = dot(valR.xy,Kernel0Weights_RealX_ImY)+dot(valR.zw,Kernel1Weights_RealX_ImY);
		float greenChannel = dot(valG.xy,Kernel0Weights_RealX_ImY)+dot(valG.zw,Kernel1Weights_RealX_ImY);
		float blueChannel	= dot(valB.xy,Kernel0Weights_RealX_ImY)+dot(valB.zw,Kernel1Weights_RealX_ImY);
		float alphaChannel	= dot(valA.xy,Kernel0Weights_RealX_ImY)+dot(valA.zw,Kernel1Weights_RealX_ImY);

		result.c0 = float4(pow(redChannel, 1.0/colorPower),pow(greenChannel, 1.0/colorPower),
			pow(blueChannel, 1.0/colorPower), clamp(alphaChannel, 0.0, 1.0));
	}
	else if (PASS_VALUE == COMPOSITION_PASS)
	{
		float4 blurTexAtUV = textureLod(blurTex0, uv, 0.0);
		float filterRadius = UNNORMALIZE_FILTER(blurTexAtUV.a);
		float mixFactor = FocusThresholdMixFactor(abs(filterRadius), inFocusThreshold);
		float4 nearBlurTexAtUV=float4(0);float alpha=0;
		if (QUALITY_VALUE == 1) {
			nearBlurTexAtUV = textureLod(blurTex1, uv, 0.0);
			alpha = clamp(nearBlurTexAtUV.a * 1.5, 0.0, 1.0);
			// in focus: the result would be the screen copy, so leave the screen (and its MSAA samples) untouched
			if (mixFactor == 0.0 && alpha == 0.0)
				discard_fragment();
		} else {
			if (mixFactor == 0.0)
				discard_fragment();
		}

		float4 fragColor = mix(textureLod(origTex, uv, 0.0), blurTexAtUV, mixFactor);
		if (QUALITY_VALUE == 1) {
			fragColor.rgb = mix(fragColor.rgb, nearBlurTexAtUV.rgb, alpha);
		}

		result.c0 = fragColor;
	}
	else if (PASS_VALUE == TILE_FLAG_PASS)
	{
		// one fragment per tile of the filter pass's per-texel flags (origTex): a bilinear fetch between 2x2 texels
		// is above 0 if any of them is set
		float2 texelSize = 1.0 / float2(textureSize(origTex, 0));
		float2 firstCorner = (float2(int2(DOFGLFragCoord(uniforms,fragmentPosition)) * TILE_SIZE) + 1.0) * texelSize;
		float2 flags = float2(0.0);
		for (int y = 0; y < TILE_SIZE; y += 2)
		{
			for (int x = 0; x < TILE_SIZE; x += 2)
			{
				flags = max(flags, textureLod(origTex, firstCorner + float2(x, y) * texelSize, 0.0).rg);
			}
		}
		result.c0 = float4(flags.r > 0.0 ? 1.0 : 0.0, flags.g > 0.0 ? 1.0 : 0.0, 0.0, 0.0);
	}
	else if (PASS_VALUE == TILE_MASK_PASS)
	{
		// grow the tile flags (origTex) by how far each pass reaches: a tap moves up to KERNEL_RADIUS * maxFilterRadius
		// steps, plus bilinear neighbours and the final passes' slightly oversized quad; the composition's bilinear
		// upsampling adds a texel (reachNear) or a tile (far)
		float maxTapTexels = float(KERNEL_RADIUS) * maxFilterRadius * baseStepValMag * resolution.y;
		int reachH = int(ceil((maxTapTexels + 2.0) / float(TILE_SIZE)));
		int reachNear = int(ceil((maxTapTexels + 3.0) / float(TILE_SIZE)));
		int rangeX = max(reachH + 1, reachH + reachNear);
		int rangeY = max(1, reachNear);

		int2 tiles = textureSize(origTex, 0);
		int2 tile = int2(DOFGLFragCoord(uniforms,fragmentPosition));
		bool4 needed = bool4(false); // farV, farH, nearV, nearH
		for (int dy = -rangeY; dy <= rangeY; ++dy)
		{
			for (int dx = -rangeX; dx <= rangeX; ++dx)
			{
				int2 t = tile + int2(dx, dy);
				if (any((t < int2(0))) || any((t >= tiles)))
					continue;

				float2 flags = texelFetch(origTex, t, 0).rg;
				int ax = abs(dx);
				int ay = abs(dy);
				if (flags.r > 0.5)
				{
					needed.x = needed.x || (ax <= reachH + 1 && ay <= 1);
					needed.y = needed.y || (ax <= 1 && ay <= 1);
				}
				if (flags.g > 0.5)
				{
					needed.z = needed.z || (ax <= reachH + reachNear && ay <= reachNear);
					needed.w = needed.w || (ax <= reachNear && ay <= reachNear);
				}
			}
		}
		result.c0 = float4(needed);
	}
	

 return result;
}
struct DOFMRT2 {float4 c0 [[color(0)]];float4 c1 [[color(1)]];};
fragment DOFMRT2 DOFFragmentPass0HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
depth2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<0,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0,r.c1};}
struct DOFMRT3 {float4 c0 [[color(0)]];float4 c1 [[color(1)]];float4 c2 [[color(2)]];};
fragment DOFMRT3 DOFFragmentPass1HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<1,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0,r.c1,r.c2};}
struct DOFMRT1 {float4 c0 [[color(0)]];};
fragment DOFMRT1 DOFFragmentPass2HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<2,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
struct DOFMRT4 {float4 c0 [[color(0)]];float4 c1 [[color(1)]];float4 c2 [[color(2)]];float4 c3 [[color(3)]];};
fragment DOFMRT4 DOFFragmentPass3HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<3,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0,r.c1,r.c2,r.c3};}
fragment DOFMRT1 DOFFragmentPass4HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<4,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
fragment DOFMRT1 DOFFragmentPass5LQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<5,0>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
fragment DOFMRT1 DOFFragmentPass5HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<5,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
fragment DOFMRT1 DOFFragmentPass6HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<6,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
fragment DOFMRT1 DOFFragmentPass7HQ(DOFVarying input [[stage_in]],constant DOFUniforms& uniforms [[buffer(1)]],
texture2d<float> origTex [[texture(0)]],sampler origTexSampler [[sampler(0)]],
texture2d<float> blurTex0 [[texture(1)]],sampler blurTex0Sampler [[sampler(1)]],
texture2d<float> blurTex1 [[texture(2)]],sampler blurTex1Sampler [[sampler(2)]],
texture2d<float> blurTex2 [[texture(3)]],sampler blurTex2Sampler [[sampler(3)]],
texture2d<float> blurTex3 [[texture(4)]],sampler blurTex3Sampler [[sampler(4)]],
texture2d<float> tileMask [[texture(5)]],sampler tileMaskSampler [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 auto r=DOFEvaluate<7,1>(uniforms,input.uv,input.position,input.focusDepth,input.aperture,origTex,origTexSampler,blurTex0,blurTex0Sampler,blurTex1,blurTex1Sampler,blurTex2,blurTex2Sampler,blurTex3,blurTex3Sampler,tileMask,tileMaskSampler);return {r.c0};}
#undef textureLod
#undef textureSize
#undef texelFetch
#undef projectionMat
#undef resolution
#undef mouseDepthCoord
#undef manualFocusDepth
#undef fStop
#undef autofocusFudgeFactor
#undef autofocusPower
#undef autofocusFocalLength
#undef autofocus
#undef mousefocus
#undef TILE_NEEDED
)DOFMSL";
}
