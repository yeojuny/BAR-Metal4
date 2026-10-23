/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include "NativeMultisampleLineShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeWeaponRangeShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
#define DEBUG 0
struct RangeUniform {
 float4x4 cameraView,cameraViewProjection,cameraViewInverse,miniMapViewProjection;
 float4 mapSize,fogColor,fogParams,mouseWorldPos,values0,values1,pipVisibleArea,raster;
 uint4 options;float4 viewport;uint4 renderOptions;float4 outputViewport;
 float4 maskParameters,fillColors[4],viewGeometry,sourceViewport;uint4 programOptions;
};
struct RangeInstance {float4 posscale,color1,visibility,projectileParams,additionalParams;uint4 instData;};
struct RangeVertexValue {float4 position,color;};
inline float HeightAtWorldPos(float2 w,constant RangeUniform& u,texture2d<float> height,sampler hs) {
 const float2 size=u.mapSize.xy;const float2 inverseMapSize=1.0f/size;w+=float2(-8.0f)*(w*inverseMapSize)+float2(4.0f);
 float2 uv=clamp(w,float2(8.0f),size-float2(8.0f))*inverseMapSize;
 if(u.options.z)uv.y=1.0f-uv.y;
 return height.sample(hs,uv,level(0.0f)).x;
}
inline bool IsSphereVisibleXY(float4 wP,float wR,constant RangeUniform& u) {
 float3 toCamera=wP.xyz-u.cameraViewInverse[3].xyz;
 float ratio=wR*rsqrt(dot(toCamera,toCamera));float4 clip=u.cameraViewProjection*wP;
 return any(abs(clip.xy)>clip.ww*(1.0f+ratio));
}
float GetRangeFactor(float projectileSpeed) { // returns >0 if weapon can shoot here, <0 if it cannot, 0 if just right
	// on first run, with yDiff = 0, what do we get?
	float speed2d = projectileSpeed * 0.707106;
	float gravity =  120.0 	* (0.001111111);
	return ((speed2d * speed2d) * 2.0 ) / (gravity);
}

float GetRange2DCannon(float yDiff,float projectileSpeed,float rangeFactor,float heightBoostFactor) { // returns >0 if weapon can shoot here, <0 if it cannot, 0 if just right
	// on first run, with yDiff = 0, what do we get?

	//float factor = 0.707106;
	float smoothHeight = 100.0;
	float speed2d = projectileSpeed*0.707106;
	float speed2dSq = speed2d * speed2d;
	float gravity = -1.0*  (120.0 /900);

	if (heightBoostFactor < 0){
		heightBoostFactor = (2.0 - rangeFactor) / sqrt(rangeFactor);
	}

	if (yDiff < -100.0){
		yDiff = yDiff * heightBoostFactor;
	}else {
		if (yDiff < 0.0) {
			yDiff = yDiff * (1.0 + (heightBoostFactor - 1.0 ) * (-1.0 * yDiff) * 0.01);
		}
	}

	float root1 = speed2dSq + 2 * gravity *yDiff;
	if (root1 < 0.0 ){
		return 0.0;
	}else{
		return rangeFactor * ( speed2dSq + speed2d * sqrt( root1 ) ) / (-1.0 * gravity);
	}
}

float2 rotate2D(float2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	return float2(c*v.x-s*v.y,s*v.x+c*v.y);
}

// Ease of use defines for the vertex shader inputs:
#define RANGE 					posscale.w
#define TURRETHEIGHT			posscale.y

#define PROJECTILESPEED 		projectileParams.x
#define ISCYLINDER 				projectileParams.y
#define HEIGHTBOOSTFACTOR 		projectileParams.z
#define HEIGHTMOD 				projectileParams.w

#define GROUPSELECTIONFADESCALE	additionalParams.x
#define WEAPONTYPE				additionalParams.y
#define ISDGUN 					additionalParams.z
#define MAXANGLEDIF 			additionalParams.w

#define FADESTART				visibility.x
#define FADEEND					visibility.y
#define STARTALPHA				visibility.z
#define ENDALPHA				visibility.w

#define UNUSEDALPHA				alphaControl.x
#define OUTOFBOUNDSALPHA		alphaControl.y
#define FADEALPHA				alphaControl.z
#define MOUSEALPHA				alphaControl.w

// SELECTEDNESS has no fragment/live consumer.


#define HEIGHTMAP_SAMPLE_STEPS int(u.programOptions.y)
#define heightAtWorldPos(w) HeightAtWorldPos(w,u,height,hs)
#define posscale row.posscale
#define color1 row.color1
#define visibility row.visibility
#define projectileParams row.projectileParams
#define additionalParams row.additionalParams
#define lineAlphaUniform u.values0.x
#define cannonmode u.values0.y
#define fadeDistOffset u.values0.z
#define inMiniMap u.values0.w
#define selUnitCount u.values1.x
#define drawAlpha u.values1.y
#define drawMode u.values1.z
#define staticUnits u.values1.w
#define selBuilderCount u.maskParameters.x
#define rotationMiniMap int(u.options.x)
#define pipVisibleArea u.pipVisibleArea
#define mapSize u.mapSize
#define cameraView u.cameraView
#define cameraViewProj u.cameraViewProjection
#define cameraViewInv u.cameraViewInverse
#define mmDrawViewProj u.miniMapViewProjection
#define fogColor u.fogColor
#define fogParams u.fogParams
#define mouseWorldPos u.mouseWorldPos
#define MOUSEOVERALPHAMULTIPLIER u.raster.x
#define maskDepthBase u.raster.w
inline RangeVertexValue OriginalRangeVertex(float4 circlepointposition,const device RangeInstance& row,float4 modelDrawPos,
 uint instance,constant RangeUniform& u,texture2d<float> height,sampler hs) {
 float4 gl_Position,v_blendedcolor;

	float4 circleWorldPos = float4(1.0);
	float3 modelWorldPos = float3(0.0);		
	float maxAngleDif = 1;
	float mainDirDegrees = 0; 
	float4 circleprogress = float4(0.0);
	circleprogress.xy = circlepointposition.xy;
	if (staticUnits > 0.5) {
		// we need to add the aim pos of the turret coming in at posscale.y to the actual ground height.
		modelWorldPos = posscale.xyz;
		float modelposgroundheight = heightAtWorldPos(modelWorldPos.xz);
		modelWorldPos.y = modelposgroundheight + posscale.y;
		circleWorldPos.xz = circlepointposition.xy * RANGE +  modelWorldPos.xz;
	}else {
		// Get the center pos of the unit
		modelWorldPos = modelDrawPos.xyz;

		// The turret is a bit higher up than drawPos.y (which is the ground pos)
		modelWorldPos.y += TURRETHEIGHT;

		// Get its heading
		float unitHeading = modelDrawPos.w ;
		
		
		// find angle between unit Heading and circleprogress.xy
		//unitHeading is -pi to +pi, with zero on z+, and increasing towards x+
		//circleheading is -pi to +pi, with zero z-, and increasing towards x+ 
		
		// rotate the circle into unit space, weird that it has to be rotated on other direction
		if (MAXANGLEDIF > 0.0) {
			maxAngleDif = fract(MAXANGLEDIF);// goes from 0.0 to 1.0, where 0.25 would mean a 90 deg cone
			mainDirDegrees = MAXANGLEDIF - maxAngleDif;// Is the offset in degrees. 
		}
		circleprogress.xy = rotate2D(circleprogress.xy, (3.141592 -1.0*unitHeading + mainDirDegrees * 3.141592 / 180.0));
		if (ISDGUN > 0.5) {
			// TODO move this to config instead of here
			circleWorldPos.xz = circleprogress.xy * RANGE * 1.05 + modelWorldPos.xz;
		} else {
			circleWorldPos.xz = circleprogress.xy * RANGE +  modelWorldPos.xz;
		}
	}

	circleprogress.w = circlepointposition.z;
	v_blendedcolor = color1;

	if (staticUnits > 0.5) {
		//gl_Position = cameraViewProj * float4(circleWorldPos.xyz, 1.0); return;
	}

	float4 alphaControl = float4(1.0);

	// get heightmap
	circleWorldPos.y = heightAtWorldPos(circleWorldPos.xz);
	#if (DEBUG == 1)
		v_debug = float4(modelWorldPos.xyzy);
	#endif
	
	
	if (cannonmode > 0.5){

		// BAR only has 3 distinct ballistic projectiles, heightBoostFactor is only a handful from -1 to 2.8 and 6 and 8
		// gravity we can assume to be linear

		float heightDiff = (circleWorldPos.y - modelWorldPos.y) * 0.5;

		float rangeFactor = RANGE /  GetRangeFactor(PROJECTILESPEED); //correct
		if (rangeFactor > 1.0 ) rangeFactor = 1.0;
		if (rangeFactor <= 0.0 ) rangeFactor = 1.0;
		float radius = RANGE;// - heightDiff;
		float adjRadius = GetRange2DCannon(heightDiff * HEIGHTMOD, PROJECTILESPEED, rangeFactor, HEIGHTBOOSTFACTOR);
		float adjustment = radius * 0.5;
		for (int i = 0; i < HEIGHTMAP_SAMPLE_STEPS; i ++){
				if (adjRadius > radius){
					radius = radius + adjustment;
				}else{
					radius = radius - adjustment;
				}
				adjustment = adjustment * 0.5;
				circleWorldPos.xz = circleprogress.xy * radius + modelWorldPos.xz;
				circleWorldPos.y = max(0.0f, heightAtWorldPos(circleWorldPos.xz));
				heightDiff = circleWorldPos.y - modelWorldPos.y;
				adjRadius = GetRange2DCannon(heightDiff * HEIGHTMOD, PROJECTILESPEED, rangeFactor, HEIGHTBOOSTFACTOR);
		}
	}else{
		// IF ITS A SPHERE:
		if (ISCYLINDER < 0.5){ 
			// fixed-point iteration: move the vertex to where the sphere meets the terrain
			float surfaceSphereClampHeight = min(modelWorldPos.y, 0.0);

			for (int i = 0; i< HEIGHTMAP_SAMPLE_STEPS / 2; i++){
				// draw vector from centerpoint to new height point and normalize it to range length
				float3 tonew = circleWorldPos.xyz - modelWorldPos.xyz;
				tonew.y *= HEIGHTMOD;

				tonew = normalize(tonew) * RANGE;
			
				circleWorldPos.xz = modelWorldPos.xz + tonew.xz;
				// if underwater model
				circleWorldPos.y = max(surfaceSphereClampHeight, heightAtWorldPos(circleWorldPos.xz));
			}
		}
	}
	

	// -- MAP OUT OF BOUNDS
	float2 mymin = min(circleWorldPos.xz,mapSize.xy - circleWorldPos.xz);
	float inboundsness = min(mymin.x, mymin.y);
	OUTOFBOUNDSALPHA = 1.0 - clamp(inboundsness*(-0.02),0.0,1.0);
	
	
	// -- Better drawing of underwater stuff
	if (modelWorldPos.y > -20) { // for submerged units, try to keep the ranges above the water for clarity
		modelWorldPos.y = max(1.0f, modelWorldPos.y);
		circleWorldPos.y = max(1.0f, circleWorldPos.y);
	}

	// -- HANDLE MAXANGLEDIFF
	// If the unit can't fire in that direction due to maxanglediff constraints, then put the point back to modelWorldPos
	// Also, dont 
	// convert current circleprogress to relative heading:
	float relheadingradians = abs(((circleprogress.w - 0.5)) * 2);
	if (MAXANGLEDIF != 0.0) {
		if(relheadingradians > maxAngleDif){
			circleWorldPos.xyz = modelWorldPos.xyz;
		}
		OUTOFBOUNDSALPHA = 1.0;
	}
	
	circleWorldPos.y += 4; // lift it from the ground


	//--- DISTANCE FADE ---
	float4 camPos = cameraViewInv[3];

	// Note that this is not the same as the distance from the unit to the camera, but the distance from the circle to the camera
	float distToCam = length(modelWorldPos.xyz - camPos.xyz) ; //dist from cam
	// FadeStart, FadeEnd, StartAlpha, EndAlpha
	float fadeDist = FADEEND - FADESTART;
																							 
	// TODO VALIDATE
	if (ISDGUN > 0.5) {
		FADEALPHA  = clamp((FADEEND + fadeDistOffset + 1000 - distToCam)/(fadeDist), ENDALPHA, STARTALPHA);
	} else {
		FADEALPHA  = clamp((FADEEND + fadeDistOffset -        distToCam)/(fadeDist), ENDALPHA, STARTALPHA);
	}
		// -- IN-SHADER MOUSE-POS BASED HIGHLIGHTING
	float disttomousefromunit = 1.0 - smoothstep(48.0f, 64.0f, length(modelWorldPos.xz - mouseWorldPos.xz));
	// this will be positive if in mouse, negative else
	float highlightme = clamp( (disttomousefromunit ) + 0.0, 0.0, 1.0) * MOUSEOVERALPHAMULTIPLIER;
	// Note that this doesn't really work well with boundary-only stenciling, due to random draw order. 
	MOUSEALPHA = (0.1  + 0.5 * step(0.5,drawMode)) * highlightme;


	if (inMiniMap> 0.5){
		// No extra fade control when on the minimap
		FADEALPHA = 1.0;
	}
	else if (u.programOptions.x==0 && highlightme <= 0.0 && FADESTART < FADEEND && ENDALPHA <= 0.0 && distToCam >= FADEEND + fadeDistOffset) {
		// Fully faded out and not mouse-highlighted: collapse the ring onto the unit so it
		// rasterizes nothing. (The mask pass keeps it, its fill does not follow the fade.)
		FADEALPHA = 0.0;
		circleWorldPos.xz = modelWorldPos.xz;
	}

	// --- YES FOG
	float fogDist = length((cameraView * float4(circleWorldPos.xyz,1.0)).xyz);
	float fogFactor = clamp((fogParams.y - fogDist) * fogParams.w, 0.0f, 1.0f);
	v_blendedcolor.rgb = mix(fogColor.rgb, float3(v_blendedcolor), fogFactor);

	if (inMiniMap < 0.5) {
		gl_Position = cameraViewProj * float4(circleWorldPos.xyz, 1.0);
		//pull 16 elmos forward in Z:
		gl_Position.z = (gl_Position.z) - 128.0 / (gl_Position.w); // send 16 elmos forward in Z
		if (u.programOptions.x==1) {
			// Coverage mask pass: with GL_LESS against a cleared depth buffer the first disc drawn
			// wins per pixel. Instances are drawn in order, so giving later instances a larger depth
			// leaves the depth test a clear margin and lets the hierarchical depth test reject
			// covered tiles outright instead of testing every fragment. maskDepthBase separates
			// classes that share a mask channel.
			// maskDepth stays within [0, 1): valid with zero-to-one clip control (the engine's
			// default) as well as with the classic [-1, 1] clip volume, and below the cleared 1.0.
			float maskDepth = maskDepthBase + float(instance) * (1.0 / 4096.0);
			gl_Position.z = maskDepth * gl_Position.w;
		}
	} else {
		// Check if PIP mode (visible area not default)
		bool isPip = (pipVisibleArea.x != 0.0 || pipVisibleArea.y != 1.0 || pipVisibleArea.z != 0.0 || pipVisibleArea.w != 1.0);
		
		float4 ndcxy;
		if (isPip) {
			// For PIP: calculate screen position based on visible area
			// Convert world position to normalized [0,1] map coords
			float2 normPos = circleWorldPos.xz / mapSize.xy;
			
			// Map from world [0,1] to screen position based on visible area
			float2 screenPos;
			screenPos.x = (normPos.x - pipVisibleArea.x) / (pipVisibleArea.y - pipVisibleArea.x);
			// Flip Y: world Z in [visB, visT] -> screen Y flipped
			screenPos.y = 1.0 - (normPos.y - pipVisibleArea.z) / (pipVisibleArea.w - pipVisibleArea.z);
			
			// Apply rotation
			if (rotationMiniMap == 0) {
				screenPos.y = 1.0 - screenPos.y;
			} else if (rotationMiniMap == 1) {
				screenPos.xy = screenPos.yx;
			} else if (rotationMiniMap == 2) {
				screenPos.x = 1.0 - screenPos.x;
			} else if (rotationMiniMap == 3) {
				screenPos.xy = float2(1.0) - screenPos.yx;
			}
			
			// Convert to NDC [-1,1]
			ndcxy = float4(screenPos * 2.0 - 1.0, 0.0, 1.0);
		} else {
			// Normal minimap mode - use engine matrix
			ndcxy = mmDrawViewProj * float4(circleWorldPos.xyz, 1.0);
			if (rotationMiniMap == 1) {
				ndcxy.xy = float2(-ndcxy.y, ndcxy.x);
			}else if (rotationMiniMap == 2) {
				ndcxy.xy = -ndcxy.xy;
			}else if (rotationMiniMap == 3) {
				ndcxy.xy = float2(ndcxy.y, -ndcxy.x);
			}
		}
		gl_Position = ndcxy;
	}

	//lets blend the alpha here, and save work in FS:
	float outalpha = OUTOFBOUNDSALPHA * (MOUSEALPHA + FADEALPHA *  lineAlphaUniform);
	v_blendedcolor.a *= outalpha ;
	if (ISDGUN > 0.5) {
		v_blendedcolor.a = clamp(v_blendedcolor.a * 3, 0.1, 1.0);
	}
	// Additional unituniform based selectedness metrics:

	// 0 = unit is un selected, 1 = unit is selected, 0.5 =  ally also selected unit, +2 = its mouseovered
	float selectedness = 0.0;
	
	// selectedness is unconsumed; no fabricated static model read.

	float selectedUnitCount = selUnitCount;
	// -- nano is 2
	if(WEAPONTYPE == 2.0) {
		selectedUnitCount = selBuilderCount;
	}
	selectedUnitCount = clamp(selectedUnitCount, 1.0f, 25.0f);

	float innerRingDim = GROUPSELECTIONFADESCALE * 0.1 * selectedUnitCount;
	float finalAlpha = drawAlpha;
	if(drawMode == 2.0) {
		finalAlpha = drawAlpha / (innerRingDim * innerRingDim);
	}
	finalAlpha = clamp(finalAlpha, 0.0, 1.0);
	v_blendedcolor.a *= finalAlpha;

 if(u.options.y)gl_Position.z=(gl_Position.z+gl_Position.w)*0.5f;
 return {gl_Position,v_blendedcolor};
}
#undef heightAtWorldPos
#undef posscale
#undef color1
#undef visibility
#undef projectileParams
#undef additionalParams
#undef lineAlphaUniform
#undef cannonmode
#undef fadeDistOffset
#undef inMiniMap
#undef selUnitCount
#undef drawAlpha
#undef drawMode
#undef staticUnits
#undef selBuilderCount
#undef rotationMiniMap
#undef pipVisibleArea
#undef mapSize
#undef cameraView
#undef cameraViewProj
#undef cameraViewInv
#undef mmDrawViewProj
#undef fogColor
#undef fogParams
#undef mouseWorldPos
#undef MOUSEOVERALPHAMULTIPLIER
#undef maskDepthBase
#undef HEIGHTMAP_SAMPLE_STEPS
struct RangeVarying {
 float4 position [[position]];
 float4 color [[flat]];
 float3 lineA [[flat]],lineB [[flat]];
};
inline bool ClipRangeLine(thread float4& a,thread float4& b) {
 float enter=0.0f,leave=1.0f;const float4 start=a,delta=b-a;
 const float av[6]={a.x+a.w,a.w-a.x,a.y+a.w,a.w-a.y,a.z,a.w-a.z};
 const float bv[6]={b.x+b.w,b.w-b.x,b.y+b.w,b.w-b.y,b.z,b.w-b.z};
 for(uint i=0;i<6;++i){if(av[i]<0.0f&&bv[i]<0.0f)return false;if(av[i]<0.0f)enter=max(enter,av[i]/(av[i]-bv[i]));if(bv[i]<0.0f)leave=min(leave,av[i]/(av[i]-bv[i]));}
 if(enter>leave)return false;a=start+enter*delta;b=start+leave*delta;return a.w>0.0f&&b.w>0.0f;
}
vertex RangeVarying OriginalWeaponRangeFill(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device RangeInstance* rows [[buffer(1)]],
 const device float4* positions [[buffer(2)]],constant RangeUniform& u [[buffer(3)]],
 texture2d<float> height [[texture(0)]],sampler hs [[sampler(0)]]) {
 const uint triangle=vertexIndex/3,corner=vertexIndex%3,index=corner==0?0:triangle+corner;
 const float4 model=u.values1.w>0.5f?float4(0):positions[instance];
 const auto value=OriginalRangeVertex(vertices[index],rows[instance],model,instance,u,height,hs);
 const auto provoking=OriginalRangeVertex(vertices[triangle+2],rows[instance],model,instance,u,height,hs);
 RangeVarying out;out.position=value.position;out.color=provoking.color;
 out.lineA=out.lineB=float3(0);return out;
}
vertex RangeVarying OriginalWeaponRangeLine(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device RangeInstance* rows [[buffer(1)]],
 const device float4* positions [[buffer(2)]],constant RangeUniform& u [[buffer(3)]],
 texture2d<float> height [[texture(0)]],sampler hs [[sampler(0)]]) {
 const uint segment=vertexIndex/6,corner=vertexIndex%6;
 const float4 model=u.values1.w>0.5f?float4(0):positions[instance];
 const auto va=OriginalRangeVertex(vertices[segment],rows[instance],model,instance,u,height,hs);
 const auto vb=OriginalRangeVertex(vertices[(segment+1)%u.options.w],rows[instance],model,instance,u,height,hs);
 float4 a=va.position,b=vb.position;
 RangeVarying out;out.color=vb.color;
 out.lineA=out.lineB=float3(0);out.position=float4(2,2,2,1);
 if(!all(isfinite(a))||!all(isfinite(b))||!ClipRangeLine(a,b))return out;
 const float3 av=a.xyz/a.w,bv=b.xyz/b.w;
 // Negative Metal-window Y differs from the GL window grid by integer target height.
 out.lineA=float3(u.viewport.x+(av.x+1.0f)*0.5f*u.viewport.z,-u.viewport.y-(1.0f-av.y)*0.5f*u.viewport.w,av.z);
 out.lineB=float3(u.viewport.x+(bv.x+1.0f)*0.5f*u.viewport.z,-u.viewport.y-(1.0f-bv.y)*0.5f*u.viewport.w,bv.z);
 const float extent=max(1.0f,floor(u.raster.y+0.5f))*0.5f+2.0f;
 const float2 low=min(out.lineA.xy,out.lineB.xy)-extent,high=max(out.lineA.xy,out.lineB.xy)+extent;
 constexpr uint indices[6]={0,1,2,0,2,3};constexpr float2 corners[4]={float2(0,0),float2(1,0),float2(1,1),float2(0,1)};
 const float2 pixel=mix(low,high,corners[indices[corner]]);
 out.position=float4((pixel.x-u.outputViewport.x)/u.outputViewport.z*2.0f-1.0f,1.0f+2.0f*(pixel.y+u.outputViewport.y)/u.outputViewport.w,0.5f,1.0f);return out;
}
// Fixed-subpixel diamond tests (8-bit precision requires reference pixel acceptance). Integer fractions retain the GL
// symbolic (-epsilon,-epsilon^2) endpoint perturbation without dropping it
// at large map/window coordinates when MSL has no double arithmetic.
struct RangeFraction {long n,d,e1,e2;};
inline int FractionCompare(RangeFraction a,RangeFraction b) {
 const long x=a.n*b.d-b.n*a.d;if(x)return x<0?-1:1;
 const long y=a.e1*b.d-b.e1*a.d;if(y)return y<0?-1:1;
 const long z=a.e2*b.d-b.e2*a.d;return z==0?0:z<0?-1:1;
}
inline RangeFraction Fraction(long n,long d,long e1,long e2) {if(d<0)return {-n,-d,-e1,-e2};return {n,d,e1,e2};}
inline bool RangeDiamond(int2 a,int2 b,int2 pixel) {
 const long2 start=long2(long(a.x)+a.y,long(a.x)-a.y),delta=long2(long(b.x)+b.y,long(b.x)-b.y)-start;
 const long2 center=long2(long(pixel.x)+pixel.y+1,long(pixel.x)-pixel.y)*256;
 const RangeFraction zero={0,1,0,0},one={1,1,0,0};RangeFraction enter=zero,leave=one;
 for(uint axis=0;axis<2;++axis){const long low=center[axis]-128,high=center[axis]+128;
  if(delta[axis]==0){if(start[axis]<=low||start[axis]>high)return false;continue;}
  RangeFraction lo=Fraction(low-start[axis],delta[axis],1,axis==0?1:-1),hi=Fraction(high-start[axis],delta[axis],1,axis==0?1:-1);
  if(FractionCompare(lo,hi)>0){auto swap=lo;lo=hi;hi=swap;}
  if(FractionCompare(lo,enter)>0)enter=lo;if(FractionCompare(hi,leave)<0)leave=hi;
 }
 return FractionCompare(leave,enter)>0&&FractionCompare(leave,zero)>=0&&FractionCompare(enter,one)<=0&&FractionCompare(leave,one)<0;
}
inline long FloorDivide(long n,long d) {return n>=0?n/d:-((-n+d-1)/d);}
inline bool RangeLineCovered(float3 originalA,float3 originalB,float2 fragmentPosition,float width,thread float& depth) {
 const int2 a=int2(floor(originalA.xy*256.0f+0.5f)),b=int2(floor(originalB.xy*256.0f+0.5f));
 const int2 delta=b-a;if(all(delta==0))return false;const bool xMajor=abs(delta.x)>=abs(delta.y);
 const int2 pixel=int2(floor(fragmentPosition));const int major=xMajor?pixel.x:pixel.y;
 const long ma=xMajor?a.x:a.y,mi=xMajor?a.y:a.x,dm=xMajor?delta.x:delta.y,di=xMajor?delta.y:delta.x;
 long numerator=mi*dm+(long(major)*256+128-ma)*di,denominator=dm*256;
 if(denominator<0){denominator=-denominator;numerator=-numerator;}
 const int center=int(FloorDivide(numerator,denominator));bool covered=false;
 for(int minor=center-1;minor<=center+1;++minor)if(RangeDiamond(a,b,xMajor?int2(major,minor):int2(minor,major))){covered=true;break;}
 if(!covered)return false;
 const int count=max(1,int(floor(width+0.5f)));
 // minor+(1-count)/2-epsilon, exactly as NativeScreenLine's GL wide-line
 // replication. Unit-width major coverage is retained for every width.
 const int lower=int(FloorDivide(2*numerator-long(count-1)*denominator-1,2*denominator));
 const int actual=xMajor?pixel.y:pixel.x;if(actual<lower||actual>=lower+count)return false;
 const float2 d=originalB.xy-originalA.xy;
 const float t=xMajor?(fragmentPosition.x-originalA.x)/d.x:(fragmentPosition.y-originalA.y)/d.y;
 depth=originalA.z+(originalB.z-originalA.z)*t;return depth>=0.0f&&depth<=1.0f;
}
inline bool RangeAlpha(float a,float reference,uint packed) {
 if(!(packed&256))return true;
 switch(packed&255){case 0:return false;case 1:return a<reference;case 2:return a==reference;case 3:return a<=reference;case 4:return a>reference;case 5:return a!=reference;case 6:return a>=reference;default:return true;}
}
fragment float4 OriginalWeaponRangeFillFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 if(!RangeAlpha(input.color.a,u.raster.z,u.renderOptions.y))discard_fragment();
 return input.color;
}
struct RangeFragment {float4 color [[color(0)]];float depth [[depth(any)]];};
fragment RangeFragment OriginalWeaponRangeLineFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const float2 p=input.position.xy;float depth=input.position.z;
 if(!RangeLineCovered(input.lineA,input.lineB,float2(p.x,-p.y),u.raster.y,depth))discard_fragment();
 if(!RangeAlpha(input.color.a,u.raster.z,u.renderOptions.y))discard_fragment();
 return {input.color,depth};
}

// Same GL window-to-view texel lookup, expressed in the actual native viewport.
inline int ReadRangeMask(float2 fragmentPosition,constant RangeUniform& u,texture2d<float> mask) {
 const float2 logical=(fragmentPosition-u.viewport.xy)/u.viewport.zw;
 const float2 glFragment=u.sourceViewport.xy+float2(logical.x,1.0f-logical.y)*u.sourceViewport.zw;
 int2 texel=int2(glFragment-u.viewGeometry.zw);
 if(u.programOptions.w)texel.y=int(mask.get_height())-1-texel.y;
 if(any(texel<0)||texel.x>=int(mask.get_width())||texel.y>=int(mask.get_height()))return 0;
 return int(mask.read(uint2(texel),0).r*255.0f+0.5f);
}
inline float4 MaskedRangeColor(float4 color,float2 fragmentPosition,constant RangeUniform& u,texture2d<float> mask) {
 if(u.maskParameters.y>0.5f && (ReadRangeMask(fragmentPosition,u,mask)&int(u.maskParameters.z))!=0)color.a=0.0f;
 return color;
}
fragment float4 OriginalWeaponRangeMaskFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const float4 color=float4(u.maskParameters.w);
 if(!RangeAlpha(color.a,u.raster.z,u.renderOptions.y))discard_fragment();return color;
}
fragment float4 OriginalWeaponRangeMaskedFillFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const auto color=MaskedRangeColor(input.color,input.position.xy,u,mask);
 if(!RangeAlpha(color.a,u.raster.z,u.renderOptions.y))discard_fragment();return color;
}
fragment RangeFragment OriginalWeaponRangeMaskedLineFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const float2 p=input.position.xy;float depth=input.position.z;
 if(!RangeLineCovered(input.lineA,input.lineB,float2(p.x,-p.y),u.raster.y,depth))discard_fragment();
 const auto color=MaskedRangeColor(input.color,p,u,mask);
 if(!RangeAlpha(color.a,u.raster.z,u.renderOptions.y))discard_fragment();return {color,depth};
}
vertex RangeVarying OriginalWeaponRangeComposite(uint vertexIndex [[vertex_id]],const device float4* vertices [[buffer(0)]],constant RangeUniform& u [[buffer(3)]]) {
 RangeVarying out;out.position=float4(vertices[vertexIndex].xy,u.options.y?0.5f:0.0f,1.0f);out.color=float4(0);out.lineA=out.lineB=float3(0);return out;
}
fragment float4 OriginalWeaponRangeCompositeFragment(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const int bits=ReadRangeMask(input.position.xy,u,mask);if(bits==0)discard_fragment();float4 acc=float4(0);
 for(uint i=0;i<4;++i)if((bits&(1<<i))!=0){const auto fill=u.fillColors[i];acc=acc*(1.0f-fill.a)+float4(fill.rgb*fill.a,fill.a);}
 if(!RangeAlpha(acc.a,u.raster.z,u.renderOptions.y))discard_fragment();return acc;
}
)MSL"
RECOIL_NATIVE_MULTISAMPLE_LINE_MSL
RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
// Actual per-sample rectangle coverage; original one-sample entry points above are unchanged.
fragment RangeFragment OriginalWeaponRangeLineFragmentMSAA(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]],
	uint nativeSampleID [[sample_id]]) {
	const float2 nativeSamplePosition=get_sample_position(nativeSampleID);
	RECOIL_SAMPLE_FIX_POSITION(input.position,nativeSamplePosition);
 const float2 p=OriginalMultisampleFragmentPosition(input.position,nativeSamplePosition);float depth=input.position.z;
 if(!OriginalMultisampleLineCovered(input.lineA,input.lineB,float2(p.x,-p.y),u.raster.y,depth))discard_fragment();
 if(!RangeAlpha(input.color.a,u.raster.z,u.renderOptions.y))discard_fragment();
 return {input.color,depth};
}
fragment RangeFragment OriginalWeaponRangeMaskedLineFragmentMSAA(RangeVarying input [[stage_in]],constant RangeUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]],
	uint nativeSampleID [[sample_id]]) {
	const float2 nativeSamplePosition=get_sample_position(nativeSampleID);
	RECOIL_SAMPLE_FIX_POSITION(input.position,nativeSamplePosition);
 const float2 p=OriginalMultisampleFragmentPosition(input.position,nativeSamplePosition);float depth=input.position.z;
 if(!OriginalMultisampleLineCovered(input.lineA,input.lineB,float2(p.x,-p.y),u.raster.y,depth))discard_fragment();
 const auto color=MaskedRangeColor(input.color,p,u,mask);
 if(!RangeAlpha(color.a,u.raster.z,u.renderOptions.y))discard_fragment();return {color,depth};
}
)MSL";
}
