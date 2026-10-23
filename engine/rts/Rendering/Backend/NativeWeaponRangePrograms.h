/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <string_view>
#include <cstdint>
namespace rendering::weaponRangeProgram {
inline constexpr std::string_view Vertex=R"original(
#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// This shader is (c) Beherith (mysterme@gmail.com), released under the MIT license
#line 10000

//__DEFINES__

#ifndef MASKPASS
	#define MASKPASS 0 // 1: coverage mask pass, see gui_attackrange_gl4.lua
#endif

layout (location = 0) in vec4 circlepointposition; // x,y in range [-1,1], progress in range [0,1]
layout (location = 1) in vec4 posscale; // abs pos for static units, offset for dynamic units, scale is actual range, Y is turretheight
layout (location = 2) in vec4 color1; // Base color for the circle
layout (location = 3) in vec4 visibility; // FadeStart, FadeEnd, StartAlpha, EndAlpha
layout (location = 4) in vec4 projectileParams; // projectileSpeed, iscylinder, heightBoostFactor , heightMod
layout (location = 5) in vec4 additionalParams; // groupselectionfadescale, weaponType, ISDGUN, MAXANGLEDIF
layout (location = 6) in uvec4 instData;

uniform float lineAlphaUniform = 1.0;
uniform float cannonmode = 0.0;
uniform float fadeDistOffset = 0.0;
uniform float inMiniMap = 0.0;
uniform int rotationMiniMap = 0;
uniform vec4 pipVisibleArea = vec4(0.0, 1.0, 0.0, 1.0); // left, right, bottom, top in normalized [0,1] coords for PIP minimap


uniform float selUnitCount = 1.0;
uniform float selBuilderCount = 1.0;
uniform float drawAlpha = 1.0;
uniform float drawMode = 0.0;
uniform float staticUnits = 0.0; // 1 if static units, 0 if dynamic units
#if (MASKPASS == 1)
	uniform float maskDepthBase = 0.1; // depth band of the class being drawn into the coverage mask
#endif

uniform sampler2D heightmapTex;

// Ease-of-use defines for the vertex shader outputs
#define V_CIRCLEPROGRESS v_params.x
#define V_GROUPSELECTIONFADESCALE v_params.y
#define V_WEAPONTYPE v_params.z

out DataVS {
	flat vec4 v_blendedcolor;	
	#if (DEBUG == 1)
		vec4 v_debug;
	#endif
};

//__ENGINEUNIFORMBUFFERDEFS__


struct SUniformsBuffer {
	uint composite; //     u8 drawFlag; u8 unused1; u16 id;

	uint unused2;
	uint unused3;
	uint unused4;

	float maxHealth;
	float health;
	float unused5;
	float unused6;

	vec4 drawPos; // Note that this is map height at unit.xz
	vec4 speed;
	vec4[4] userDefined; //can't use float[16] because float in arrays occupies 4 * float space
};

layout(std140, binding=1) readonly buffer UniformsBuffer {
	SUniformsBuffer uni[];
};

#define UNITUNIFORMS uni[instData.y]
#define UNITID (uni[instData.y].composite >> 16)


#line 11000

vec2 inverseMapSize = 1.0 / mapSize.xy;

float heightAtWorldPos(vec2 w){
	// Some texel magic to make the heightmap tex perfectly align:
	const vec2 heightmaptexel = vec2(8.0, 8.0);
	w +=  vec2(-8.0, -8.0) * (w * inverseMapSize) + vec2(4.0, 4.0) ;

	vec2 uvhm = clamp(w, heightmaptexel, mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;

	return textureLod(heightmapTex, uvhm, 0.0).x;
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

vec2 rotate2D(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, s, -s, c);
	return m * v;
}

//float heightMod  default: 0.2 (0.8 for #Cannon, 1.0 for #BeamLaser and #LightningCannon)
//Changes the spherical weapon range into an ellipsoid. Values above 1.0 mean the weapon cannot target as high as it can far, values below 1.0 mean it can target higher than it can far. For example 0.5 would allow the weapon to target twice as high as far.

//float heightBoostFactor default: -1.0
//Controls the boost given to range by high terrain. Values > 1.0 result in increased range, 0.0 means the cannon has fixed range regardless of height difference to target. Any value < 0.0 (i.e. the default value) result in an automatically calculated value based on range and theoretical maximum range.

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

#define SELECTEDNESS uni[instData.y].userDefined[1].z

// Heightmap fetches per vertex while fitting the ring to the terrain. Cannons do a binary
// search of this many steps (precision range / 2^steps), spheres half as many fixed-point
// iterations. The widgets set it (rangeHeightmapSampleSteps), 16 is the historical value.
#ifndef HEIGHTMAP_SAMPLE_STEPS
	#define HEIGHTMAP_SAMPLE_STEPS 16
#endif


void main() {
	vec4 circleWorldPos = vec4(1.0);
	vec3 modelWorldPos = vec3(0.0);		
	float maxAngleDif = 1;
	float mainDirDegrees = 0; 
	vec4 circleprogress = vec4(0.0);
	circleprogress.xy = circlepointposition.xy;
	if (staticUnits > 0.5) {
		// we need to add the aim pos of the turret coming in at posscale.y to the actual ground height.
		modelWorldPos = posscale.xyz;
		float modelposgroundheight = heightAtWorldPos(modelWorldPos.xz);
		modelWorldPos.y = modelposgroundheight + posscale.y;
		circleWorldPos.xz = circlepointposition.xy * RANGE +  modelWorldPos.xz;
	}else {
		// Get the center pos of the unit
		modelWorldPos = uni[instData.y].drawPos.xyz;

		// The turret is a bit higher up than drawPos.y (which is the ground pos)
		modelWorldPos.y += TURRETHEIGHT;

		// Get its heading
		float unitHeading = uni[instData.y].drawPos.w ;
		
		
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
		//gl_Position = cameraViewProj * vec4(circleWorldPos.xyz, 1.0); return;
	}

	vec4 alphaControl = vec4(1.0);

	// get heightmap
	circleWorldPos.y = heightAtWorldPos(circleWorldPos.xz);
	#if (DEBUG == 1)
		v_debug = vec4(modelWorldPos.xyzy);
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
				circleWorldPos.y = max(0, heightAtWorldPos(circleWorldPos.xz));
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
				vec3 tonew = circleWorldPos.xyz - modelWorldPos.xyz;
				tonew.y *= HEIGHTMOD;

				tonew = normalize(tonew) * RANGE;
			
				circleWorldPos.xz = modelWorldPos.xz + tonew.xz;
				// if underwater model
				circleWorldPos.y = max(surfaceSphereClampHeight, heightAtWorldPos(circleWorldPos.xz));
			}
		}
	}
	

	// -- MAP OUT OF BOUNDS
	vec2 mymin = min(circleWorldPos.xz,mapSize.xy - circleWorldPos.xz);
	float inboundsness = min(mymin.x, mymin.y);
	OUTOFBOUNDSALPHA = 1.0 - clamp(inboundsness*(-0.02),0.0,1.0);
	
	
	// -- Better drawing of underwater stuff
	if (modelWorldPos.y > -20) { // for submerged units, try to keep the ranges above the water for clarity
		modelWorldPos.y = max(1, modelWorldPos.y);
		circleWorldPos.y = max(1, circleWorldPos.y);
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
	vec4 camPos = cameraViewInv[3];

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
	float disttomousefromunit = 1.0 - smoothstep(48, 64, length(modelWorldPos.xz - mouseWorldPos.xz));
	// this will be positive if in mouse, negative else
	float highlightme = clamp( (disttomousefromunit ) + 0.0, 0.0, 1.0) * MOUSEOVERALPHAMULTIPLIER;
	// Note that this doesn't really work well with boundary-only stenciling, due to random draw order. 
	MOUSEALPHA = (0.1  + 0.5 * step(0.5,drawMode)) * highlightme;


	if (inMiniMap> 0.5){
		// No extra fade control when on the minimap
		FADEALPHA = 1.0;
	}
	#if (MASKPASS == 0)
	else if (highlightme <= 0.0 && FADESTART < FADEEND && ENDALPHA <= 0.0 && distToCam >= FADEEND + fadeDistOffset) {
		// Fully faded out and not mouse-highlighted: collapse the ring onto the unit so it
		// rasterizes nothing. (The mask pass keeps it, its fill does not follow the fade.)
		FADEALPHA = 0.0;
		circleWorldPos.xz = modelWorldPos.xz;
	}
	#endif

	// --- YES FOG
	float fogDist = length((cameraView * vec4(circleWorldPos.xyz,1.0)).xyz);
	float fogFactor = clamp((fogParams.y - fogDist) * fogParams.w, 0, 1);
	v_blendedcolor.rgb = mix(fogColor.rgb, vec3(v_blendedcolor), fogFactor);

	if (inMiniMap < 0.5) {
		gl_Position = cameraViewProj * vec4(circleWorldPos.xyz, 1.0);
		//pull 16 elmos forward in Z:
		gl_Position.z = (gl_Position.z) - 128.0 / (gl_Position.w); // send 16 elmos forward in Z
		#if (MASKPASS == 1)
			// Coverage mask pass: with GL_LESS against a cleared depth buffer the first disc drawn
			// wins per pixel. Instances are drawn in order, so giving later instances a larger depth
			// leaves the depth test a clear margin and lets the hierarchical depth test reject
			// covered tiles outright instead of testing every fragment. maskDepthBase separates
			// classes that share a mask channel.
			// maskDepth stays within [0, 1): valid with zero-to-one clip control (the engine's
			// default) as well as with the classic [-1, 1] clip volume, and below the cleared 1.0.
			float maskDepth = maskDepthBase + float(gl_InstanceID) * (1.0 / 4096.0);
			gl_Position.z = maskDepth * gl_Position.w;
		#endif
	} else {
		// Check if PIP mode (visible area not default)
		bool isPip = (pipVisibleArea.x != 0.0 || pipVisibleArea.y != 1.0 || pipVisibleArea.z != 0.0 || pipVisibleArea.w != 1.0);
		
		vec4 ndcxy;
		if (isPip) {
			// For PIP: calculate screen position based on visible area
			// Convert world position to normalized [0,1] map coords
			vec2 normPos = circleWorldPos.xz / mapSize.xy;
			
			// Map from world [0,1] to screen position based on visible area
			vec2 screenPos;
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
				screenPos.xy = vec2(1.0) - screenPos.yx;
			}
			
			// Convert to NDC [-1,1]
			ndcxy = vec4(screenPos * 2.0 - 1.0, 0.0, 1.0);
		} else {
			// Normal minimap mode - use engine matrix
			ndcxy = mmDrawViewProj * vec4(circleWorldPos.xyz, 1.0);
			if (rotationMiniMap == 1) {
				ndcxy.xy = vec2(-ndcxy.y, ndcxy.x);
			}else if (rotationMiniMap == 2) {
				ndcxy.xy = -ndcxy.xy;
			}else if (rotationMiniMap == 3) {
				ndcxy.xy = vec2(ndcxy.y, -ndcxy.x);
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
	
	if (staticUnits > 0.5) {
		selectedness = UNITUNIFORMS.userDefined[1].z;
	}

	float selectedUnitCount = selUnitCount;
	// -- nano is 2
	if(WEAPONTYPE == 2.0) {
		selectedUnitCount = selBuilderCount;
	}
	selectedUnitCount = clamp(selectedUnitCount, 1, 25);

	float innerRingDim = GROUPSELECTIONFADESCALE * 0.1 * selectedUnitCount;
	float finalAlpha = drawAlpha;
	if(drawMode == 2.0) {
		finalAlpha = drawAlpha / (innerRingDim * innerRingDim);
	}
	finalAlpha = clamp(finalAlpha, 0.0, 1.0);
	v_blendedcolor.a *= finalAlpha;
}
)original";
inline constexpr std::string_view Fragment=R"original(#version 330

#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// This shader is (c) Beherith (mysterme@gmail.com), released under the MIT license

//__DEFINES__

#ifndef MASKPASS
	#define MASKPASS 0 // 1: coverage mask pass, see gui_attackrange_gl4.lua
#endif

#line 20000

uniform float selUnitCount = 1.0;
uniform float selBuilderCount = 1.0;
uniform float drawAlpha = 1.0;
uniform float drawMode = 0.0;

// Range coverage mask, see gui_attackrange_gl4.lua: bit (1 << class) of the 8-bit red
// channel is set where a filled range disc of that class covers the pixel.
#if (MASKPASS == 1)
	uniform float maskWriteValue = 0.0; // class bit / 255, additively blended into the mask
#else
	uniform sampler2D maskTex;
	uniform float maskClip = 0.0; // 1.0: hide fragments inside the mask (outer rings)
	uniform float maskChannelBit = 1.0; // class bit to test
#endif

//__ENGINEUNIFORMBUFFERDEFS__

in DataVS {
	flat vec4 v_blendedcolor;
	#if (DEBUG == 1)
		vec4 v_debug;
	#endif
};

out vec4 fragColor;

void main() {
#if (MASKPASS == 1)
	fragColor = vec4(maskWriteValue);
#else
	fragColor = v_blendedcolor;
	if (maskClip > 0.5) {
		// The mask covers the world viewport, gl_FragCoord is relative to the window.
		ivec2 texel = ivec2(gl_FragCoord.xy - viewGeometry.zw);
		int bits = int(texelFetch(maskTex, texel, 0).r * 255.0 + 0.5);
		if ((bits & int(maskChannelBit)) != 0) {
			fragColor.a = 0.0; // inside the merged area of this class, not part of its outline
		}
	}
	// For testing:
	#if (DEBUG == 1)
		if (fract(gl_FragCoord.x * 0.125) < 0.4) {
			#if (STATICUNITS == 0)
				fragColor.rgba *= 0.0;
			#endif
		}else{
			#if(STATICUNITS == 1)
				fragColor.rgba *= 0.0;
			#endif
		}

	#endif
#endif
}
)original";
inline constexpr std::string_view CompositeVertex=R"original(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// Screen-space pass that paints the merged fill of the attack range rings from the
// coverage mask built by gui_attackrange_gl4.lua, with one screen-sized quad.

//__DEFINES__

layout (location = 0) in vec4 position; // .xy is [-1, +1], .zw is [0, 1]

#line 10000

void main()
{
	gl_Position = vec4(position.xy, 0.0, 1.0);
}
)original";
inline constexpr std::string_view CompositeFragment=R"original(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// Screen-space pass that paints the merged fill of the attack range rings from the
// coverage mask built by gui_attackrange_gl4.lua.
// The mask's 8-bit red channel holds one bit per range class (1 = ground/cannon,
// 2 = nano, 4 = AA, 8 = separate cannon); a set bit means a filled range disc of
// that class covers the pixel.

//__DEFINES__

#line 20000

uniform sampler2D maskTex;
// .rgb = fill colour, .a = fill alpha of the class stored in mask bit 1 << i
uniform vec4 fillColor0 = vec4(0.0);
uniform vec4 fillColor1 = vec4(0.0);
uniform vec4 fillColor2 = vec4(0.0);
uniform vec4 fillColor3 = vec4(0.0);

//__ENGINEUNIFORMBUFFERDEFS__

out vec4 fragColor;

// Premultiplied "over": later classes are painted on top of earlier ones, like the
// per-class fill passes did.
void blendOver(inout vec4 acc, vec4 fill)
{
	acc = acc * (1.0 - fill.a) + vec4(fill.rgb * fill.a, fill.a);
}

void main()
{
	// The mask covers the world viewport, gl_FragCoord is relative to the window.
	ivec2 texel = ivec2(gl_FragCoord.xy) - ivec2(viewGeometry.zw);
	int bits = int(texelFetch(maskTex, texel, 0).r * 255.0 + 0.5);
	if (bits == 0) {
		discard;
	}
	vec4 acc = vec4(0.0);
	if ((bits & 1) != 0) blendOver(acc, fillColor0);
	if ((bits & 2) != 0) blendOver(acc, fillColor1);
	if ((bits & 4) != 0) blendOver(acc, fillColor2);
	if ((bits & 8) != 0) blendOver(acc, fillColor3);
	// premultiplied output, blended with (GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
	fragColor = acc;
}
)original";
inline constexpr std::string_view EngineBlock=R"original(layout(std140, binding = 0) uniform UniformMatrixBuffer {
	mat4 screenView;
	mat4 screenProj;
	mat4 screenViewProj;

	mat4 cameraView;
	mat4 cameraProj;
	mat4 cameraViewProj;
	mat4 cameraBillboardView;

	mat4 cameraViewInv;
	mat4 cameraProjInv;
	mat4 cameraViewProjInv;

	mat4 shadowView;
	mat4 shadowProj;
	mat4 shadowViewProj;

	mat4 reflectionView;
	mat4 reflectionProj;
	mat4 reflectionViewProj;

	mat4 orthoProj01;

	// transforms for [0] := Draw, [1] := DrawInMiniMap, [2] := Lua DrawInMiniMap
	mat4 mmDrawView; //world to MM
	mat4 mmDrawProj; //world to MM
	mat4 mmDrawViewProj; //world to MM

	mat4 mmDrawIMMView; //heightmap to MM
	mat4 mmDrawIMMProj; //heightmap to MM
	mat4 mmDrawIMMViewProj; //heightmap to MM

	mat4 mmDrawDimView; //mm dims
	mat4 mmDrawDimProj; //mm dims
	mat4 mmDrawDimViewProj; //mm dims
};

layout(std140, binding = 1) uniform UniformParamsBuffer {
	vec3 rndVec3; //new every draw frame.
	uint renderCaps; //various render booleans

	vec4 timeInfo; //gameFrame, drawSeconds, interpolated(unsynced)GameSeconds(synced), frameTimeOffset
	vec4 viewGeometry; //vsx, vsy, vpx, vpy
	vec4 mapSize; //xz, xzPO2
	vec4 mapHeight; //height minCur, maxCur, minInit, maxInit

	vec4 fogColor; //fog color
	vec4 fogParams; //fog {start, end, 0.0, scale}

	vec4 sunDir; // (sky != nullptr) ? sky->GetLight()->GetLightDir() : float4(/*map default*/ 0.0f, 0.447214f, 0.894427f, 1.0f);

	vec4 sunAmbientModel;
	vec4 sunAmbientMap;
	vec4 sunDiffuseModel;
	vec4 sunDiffuseMap;
	vec4 sunSpecularModel; // float4{ sunLighting->modelSpecularColor.xyz, sunLighting->specularExponent };
	vec4 sunSpecularMap; //  float4{ sunLighting->groundSpecularColor.xyz, sunLighting->specularExponent };

	vec4 shadowDensity; //  float4{ sunLighting->groundShadowDensity, sunLighting->modelShadowDensity, 0.0, 0.0 };

	vec4 windInfo; // windx, windy, windz, windStrength
	vec2 mouseScreenPos; //x, y. Screen space.
	uint mouseStatus; // bits 0th to 32th: LMB, MMB, RMB, offscreen, mmbScroll, locked
	uint mouseUnused;
	vec4 mouseWorldPos; //x,y,z; w=0 -- offmap. Ignores water, doesn't ignore units/features under the mouse cursor

	vec4 teamColor[255]; //all team colors
};

// glsl rotate convenience funcs: https://github.com/dmnsgn/glsl-rotate

mat3 rotation3dX(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return mat3(
		1.0, 0.0, 0.0,
		0.0, c, s,
		0.0, -s, c
	);
}

mat3 rotation3dY(float a) {
	float s = sin(a);
	float c = cos(a);

  return mat3(
    c, 0.0, -s,
    0.0, 1.0, 0.0,
    s, 0.0, c);
}

mat3 rotation3dZ(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return mat3(
		c, s, 0.0,
		-s, c, 0.0,
		0.0, 0.0, 1.0
	);
}

mat4 scaleMat(vec3 s) {
	return mat4(
		s.x, 0.0, 0.0, 0.0,
		0.0, s.y, 0.0, 0.0,
		0.0, 0.0, s.z, 0.0,
		0.0, 0.0, 0.0, 1.0
	);
}

mat4 translationMat(vec3 t) {
	return mat4(
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		t.x, t.y, t.z, 1.0
	);
}

mat4 mat4mix(mat4 a, mat4 b, float alpha) {
	return (a * (1.0 - alpha) + b * alpha);
}

// Additional helper functions useful in Spring

vec2 heightmapUVatWorldPos(vec2 worldpos){
	vec2 inverseMapSize = vec2(1.0) / mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	vec2 heightmaptexel = vec2(8.0, 8.0);
	worldpos +=  vec2(-8.0, -8.0) * (worldpos * inverseMapSize) + vec2(4.0, 4.0) ;
	vec2 uvhm = clamp(worldpos, heightmaptexel, mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;
	return uvhm;
}

// This does 'mirror' style tiling of UVs like the way the map edge extension works
vec2 heightmapUVatWorldPosMirrored(vec2 worldpos) {
	vec2 inverseMapSize = vec2(1.0) / mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	vec2 heightmaptexel = vec2(8.0, 8.0);
	worldpos +=  vec2(-8.0, -8.0) * (worldpos * inverseMapSize) + vec2(4.0, 4.0) ;
	vec2 uvhm = worldpos * inverseMapSize;

	return abs(fract(uvhm * 0.5 + 0.5) - 0.5) * 2.0;
}

//  SphereInViewSignedDistance
//  Signed distance from a world-space sphere to the X-Y NDC box.
//  Z (near/far) is ignored. Very reliable, reasonably optimized.
//  Returns:
//      < 0  – sphere at least partially inside the X-Y clip box
//      = 0  – sphere exactly touches at least one edge
//      > 0  – sphere is more distance from the edge of frustum by that many NDC units
//
float SphereInViewSignedDistance(vec3 centerWS,  float radiusWS)
{
    // 1.  centre → clip space
    vec4 clipC = cameraViewProj * vec4(centerWS, 1.0);
    if (clipC.w <= 0.0)           // behind the eye? treat as inside
        return -1.0;

    // 2.  NDC centre (one perspective divide, reused later)
    vec2  ndc   = clipC.xy / clipC.w;        // = clipC.xy / clipC.w

    // 3.  Project world-space +X displacement
    //     M * (p + (r,0,0,0)) = (M*p) + r*M*Xcol
    vec4 deltaClip = radiusWS * cameraViewProj[0];   // first column of matrix
    vec4 clipX     = clipC + deltaClip;

    // 4.  Radius in NDC (second perspective divide only once)
    float ndcRadius = length((clipX.xy / clipX.w) - ndc);

    // 5.  Four signed distances, keep the worst (Chebyshev / max)
    float dLeft   = -(ndc.x + 1.0) - ndcRadius;
    float dRight  =  (ndc.x - 1.0) - ndcRadius;
    float dBottom = -(ndc.y + 1.0) - ndcRadius;
    float dTop    =  (ndc.y - 1.0) - ndcRadius;

    return max(max(dLeft, dRight), max(dBottom, dTop));
}



// Note that this function does not check the Z or depth of the clip space, but in regular springrts top-down views, this isn't needed either.
// the radius to cameradist ratio is a good proxy for visibility in the XY plane
bool isSphereVisibleXY(vec4 wP, float wR){ //worldPos, worldRadius
	vec3 ToCamera = wP.xyz - cameraViewInv[3].xyz; // vector from worldpos to camera
	float isqrtDistRatio = wR * inversesqrt(dot(ToCamera, ToCamera)); // calculate the relative screen-space size of it
	vec4 cWPpos = cameraViewProj * wP; // transform the worldpos into clip space
	vec2 clipVec = cWPpos.ww * (1.0 + isqrtDistRatio); // normalize the clip tolerance
	return any(greaterThan(abs(cWPpos.xy), clipVec)); // check if the clip space coords lie outside of the tolerance relaxed [-1.0, 1.0] space
}

/*
vec3 hsv2rgb(vec3 c){
	vec4 K=vec4(1.,2./3.,1./3.,3.);
	return c.z*mix(K.xxx,saturate(abs(fract(c.x+K.xyz)*6.-K.w)-K.x),c.y);
}

vec3 rgb2hsv(vec3 c){
	vec4 K=vec4(0.,-1./3.,2./3.,-1.);
	vec4 p=mix(vec4(c.bg ,K.wz),vec4(c.gb,K.xy ),step(c.b,c.g));
	vec4 q=mix(vec4(p.xyw,c.r ),vec4(c.r ,p.yzx),step(p.x,c.r));
	float d=q.x-min(q.w,q.y);
	float e=1e-10;
	return vec3(abs(q.z+(q.w-q.y)/(6.*d+e)),d/(q.x+e),q.x);
}
*/

)original";
inline constexpr uint64_t AttackHash=0xcc2589589b7b53c7ULL;
inline constexpr uint64_t DefenseHash=0xb7db6eaca48f2ff8ULL;
inline constexpr uint64_t InstanceHelperHash=0x8cef105d28038367ULL;
inline constexpr uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr uint64_t CoverageHelperHash=0x9b690a9192511c54ULL;
}
