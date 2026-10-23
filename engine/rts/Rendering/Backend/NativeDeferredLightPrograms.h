/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::deferredLightProgram {
inline constexpr std::uint64_t VertexHash=0xe07e88866eef8b03ULL;
inline constexpr std::string_view VertexSHA256="4ce560577f6c5e0b1eeb3e609450a9f72e7d3b202bfa8ff46660dc365ff66d04";
inline constexpr std::uint64_t FragmentHash=0x2d5772f7644927beULL;
inline constexpr std::string_view FragmentSHA256="59bd7a81ed1d3654e1f7e619ac62445399cbbd78af3b61fcbd26309655e8e3c7";
inline constexpr std::uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr std::string_view ShaderHelperSHA256="61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23";
inline constexpr std::uint64_t WidgetHash=0x523ef943690b743eULL;
inline constexpr std::string_view WidgetSHA256="2b7c0034a215fdb15e477835ed5ac282cca64f8cbca713d801495aa61e7404db";
inline constexpr char VertexTemplate[]=R"DLORIG(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  
#line 5000


layout (location = 0) in vec4 position; // xyz and etc garbage
//layout locations 1 and 2 contain primitive specific garbage and should not be used


layout (location = 3) in vec4 worldposrad;  // Centerpos
layout (location = 4) in vec4 worldposrad2; // velocity for points, beam end for beams, dir and theta for cones
layout (location = 5) in vec4 lightcolor;
layout (location = 6) in vec4 modelfactor_specular_scattering_lensflare; //
layout (location = 7) in vec4 otherparams; // spawnframe, lifetime, sustain, selfshadowing
layout (location = 8) in vec4 color2; //
layout (location = 9) in uint pieceIndex; // for piece type lights
layout (location = 10) in uvec4 instData; // matoffset, uniformoffset, teamIndex, drawFlags {id = 5, name = 'instData', size = 4, type = GL.UNSIGNED_INT},

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__

#if USEQUATERNIONS == 0
	layout(std140, binding = 0) readonly buffer MatrixBuffer {
		mat4 mat[];
	};
#else
	//__QUATERNIONDEFS__
#endif


struct SUniformsBuffer {
    uint composite; //     u8 drawFlag; u8 unused1; u16 id;


    uint unused2;
    uint unused3;
    uint unused4;


    float maxHealth;
    float health;
    float unused5;
    float unused6;


    vec4 drawPos;
    vec4 speed;
    vec4[4] userDefined; //can't use float[16] because float in arrays occupies 4 * float space
};


layout(std140, binding=1) readonly buffer UniformsBuffer {
    SUniformsBuffer uni[];
};
#define UNITID (uni[instData.y].composite >> 16)

#line 10000


uniform float pointbeamcone = 0;// = 0; // 0 = point, 1 = beam, 2 = cone

// this uniform needs some extra. If it is 1, then the primitives should point in the -Z direction, and be moved and rotated with the unit itself
// If the unit is not being drawn, it must be switched off
// One should still be able to specify an offset for this.
// our overdraw additional offsets become a problem possibly for this


uniform float attachedtounitID = 0;


uniform float windX = 0;
uniform float windZ = 0;


uniform float radiusMultiplier = 1.0;
uniform float intensityMultiplier = 1.0;


out DataVS {
    flat vec4 v_worldPosRad;
    flat vec4 v_worldPosRad2;
    flat vec4 v_lightcolor;
    flat vec4 v_modelfactor_specular_scattering_lensflare;
    vec4 v_otherparams;
    vec4 v_position;
    vec4 v_noiseoffset;
    noperspective vec2 v_screenUV;
    flat float v_sourceVisibility; // how much of a point or cone light's source is unoccluded, drives its lens flare
};


uniform sampler2D mapDepths;
uniform sampler2D modelDepths;


#define SNORM2NORM(value) (value * 0.5 + 0.5)

// Soft visibility of a world position against the scene depth, in [0, 1]. A bilinear 2x2 depth comparison with a
// smooth band in view depth, so it changes continuously with sub-pixel camera motion instead of flipping.
float softVisibilityAtWorldPos(vec3 worldPos){
    vec4 clipPos = cameraViewProj * vec4(worldPos, 1.0);
    float viewDepth = -(cameraView * vec4(worldPos, 1.0)).z; // not clipPos.w, which is always 1 for an orthographic projection
    if (viewDepth <= 0.0) return 0.0; // behind the camera
    vec2 uv = clamp(SNORM2NORM(clipPos.xy / clipPos.w), 0.0, 1.0);
    vec2 subTexel = fract(uv * vec2(textureSize(mapDepths, 0)) - 0.5);
    vec4 sceneDepths = min(textureGather(mapDepths, uv, 0), textureGather(modelDepths, uv, 0));
    #if (DEPTH_CLIP01 == 0)
        sceneDepths = sceneDepths * 2.0 - 1.0;
    #endif
    // to view depth: for perspective and orthographic projections the inverse's z and w rows do not depend on x and y
    sceneDepths = abs((cameraProjInv[2][2] * sceneDepths + cameraProjInv[3][2]) / (cameraProjInv[2][3] * sceneDepths + cameraProjInv[3][3]));
    float band = max(1.0, viewDepth * 0.003); // elmos, widening with distance as depth precision drops
    vec4 visible = smoothstep(vec4(-band), vec4(band), sceneDepths - viewDepth + band);
    // textureGather order is (0,1) (1,1) (1,0) (0,0)
    return mix(mix(visible.w, visible.z, subTexel.x), mix(visible.x, visible.y, subTexel.x), subTexel.y);
}


void main()
{
	float time = timeInfo.x + timeInfo.w;
	
	float lightRadius = worldposrad.w * radiusMultiplier;
	v_worldPosRad = worldposrad ;
	v_worldPosRad.w = lightRadius;
	
    vec4 v_color2 = color2;
	
	mat4 placeInWorldMatrix = mat4(1.0); // this is unity for non-unitID tied stuff
	#if USEQUATERNIONS == 1 
		Transform tx;
	#endif
	
	// Ok so here comes the fun part, where we if we have a unitID then fun things happen
	// v_worldposrad contains the incoming piece-level offset
	// v_worldPosRad should be after changing to unit-space
	// we have to transform BOTH the center of the light to piece-space
	// and the vertices of the light volume to piece-space
	// we need to go from light-space to world-space 
	vec3 lightCenterPosition =  v_worldPosRad.xyz;
	v_lightcolor = lightcolor;
    float selfIllumMod = 1.0;
    
	if (attachedtounitID > 0){
		#if USEQUATERNIONS == 0 
			mat4 worldMatrix = mat[instData.x];
			placeInWorldMatrix = worldMatrix;
			if (pieceIndex > 0u) {
				mat4 pieceMatrix = mat[instData.x + pieceIndex];
				placeInWorldMatrix = placeInWorldMatrix * pieceMatrix;
			} 
		#else
			tx = GetModelWorldTransform(instData.x);
			if (pieceIndex > 0u){
                // Note the pieceIndex is Lua so, 1-based, so we need to subtract 1 to get the correct index
				Transform ty = GetPieceModelTransform(instData.x, pieceIndex - 1);
				tx = ApplyTransform(tx, ty);
			}
			placeInWorldMatrix = TransformToMatrix(tx);
		#endif
		//uint drawFlags = (instData.z & 0x0000100u);// >> 8 ; // hopefully this works
		//if (drawFlags == 0u)  placeInWorldMatrix = mat4(0.0); // disable if drawflag is set to 0
		// disable if drawflag is set to 0, note that we are exploiting the fact that these should be drawn even if unit is transparent, or if unit only has its shadows drawn. 
		// This is good because the tolerance for distant shadows is much greater
		if ((uni[instData.y].composite & 0x00001fu) == 0u )  placeInWorldMatrix = mat4(0.0); 


        uint teamIndex = (instData.z & 0x000000FFu); //leftmost ubyte is teamIndex
        vec4 teamCol = teamColor[teamIndex];
        if (any(lessThan(lightcolor.rgb, vec3(-0.01)))) 
            v_lightcolor.rgb = teamCol.rgb;
        
        // WARNING: TEAMCOLOR AS COLOR2 REQUIRES AT LEAST -100 
        if (any(lessThan(v_color2.rgb, vec3(-99.00)))) 
            v_color2.rgb = teamCol.rgb;

               
        selfIllumMod = max(-0.2, sin(time * 2.0/30.0 + float(UNITID) * 0.1)) + 0.2;
    }
    // Note that elapsedframes contains the timeOffset as well
    float elapsedframes = time - otherparams.x;
    float lifetime = otherparams.y;
    float sustain = otherparams.z;
    if (lifetime > 0 ){ //lifetime alpha control
        if (sustain >1 ){ // sustain is positive, keep it up for sustain frames, then ramp it down
            v_lightcolor.a = clamp( v_lightcolor.a * ( lifetime - elapsedframes)/(lifetime - sustain ) , 0.0, v_lightcolor.a);
           
        }else{ // sustain is <1, use exp falloff
            v_lightcolor.a = clamp( v_lightcolor.a * exp( -sustain * (elapsedframes) * 100 ) , 0.0, v_lightcolor.a);
           
        }
    }
   
    v_worldPosRad2 = worldposrad2;


    v_modelfactor_specular_scattering_lensflare = modelfactor_specular_scattering_lensflare;
    v_sourceVisibility = 1.0;
    v_otherparams = otherparams;
   
    vec4 worldPos = vec4(1.0);
    #line 11000
    if (pointbeamcone < 0.5){ // point
        //scale it and place it into the world
        //Make it a tiny bit bigger *(1.1) cause the blocky sphere is smaller than the actual radius
        // the -1 is for inverting it so we always see the back faces (great for occlusion testing!) (this should be exploited later on!
       
        // this is centered around the target positional offset, and scaled locally
        vec3 lightVertexPosition = lightCenterPosition + -1 * position.xyz * lightRadius * 1.1;

		// Add animations
		if (worldposrad2.w == 0.0) { // Why was this 1.0 and not 0.0?
			// This allows deferred projectile lights to 'track' with frameOffset fractional times.
			lightCenterPosition += timeInfo.w * worldposrad2.xyz;
			lightVertexPosition += timeInfo.w * worldposrad2.xyz;
		} else {
			vec3 lightWorldMovement = vec3(0.0);


			// Negative thetas mean orbit, positive means linear movement
			if (worldposrad2.w < 0.0){
			// Note: worldposrad2.w is an excellent place to add orbit-style world-placement light animations
			    float xphaseoffset =  0.0; // half pi 1.570796326 
				if (worldposrad2.x == 0.0) xphaseoffset = 0.0; // dont offset Z phase no motion is expected on it
				lightWorldMovement = sin(elapsedframes * 0.2094395 * worldposrad2.xyz + vec3(xphaseoffset, 0.0, 0.0 )) * (-1 * worldposrad2.w);

			}else{
			// for positive numbers, we can do linear movement, with acceleration in w
			// its pretty nasty, but hey, it works
				lightWorldMovement = worldposrad2.xyz * ( elapsedframes  +  elapsedframes * elapsedframes * (worldposrad2.w - 1.0) );
			}
			lightCenterPosition += lightWorldMovement;
			lightVertexPosition += lightWorldMovement;
		}  
       
        // transform the vertices to world-space
        lightVertexPosition = (placeInWorldMatrix * vec4(lightVertexPosition, 1.0)).xyz;
       
        // transform the center to world-space
        lightCenterPosition = (placeInWorldMatrix * vec4(lightCenterPosition, 1.0)).xyz;
       
       
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        if  (attachedtounitID > 0.5) { // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
           
        }else{ // World-space point lights
            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = clamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
           
 
        }

		v_worldPosRad.xyz = lightCenterPosition;
		if (modelfactor_specular_scattering_lensflare.w > 0.0) v_sourceVisibility = softVisibilityAtWorldPos(lightCenterPosition);
		v_position = vec4( lightVertexPosition, 1.0);
		//v_position = vec4(ApplyTransform(tx, lightVertexPosition),1.0); // Doesn't work
	}
    #line 12000
    else if (pointbeamcone < 1.5){ // beam
        // we will transform along this vector, where Y shall be the upvector
        // our null vector is +X
        vec3 centertoend = lightCenterPosition - worldposrad2.xyz;
        float halfbeamlength = length(centertoend);
        // Scale the box to correct size (along beam is Y dir)
        worldPos.xyz = position.xyz * vec3( lightRadius ,  step(position.y, 0) * halfbeamlength + lightRadius, lightRadius );
        //else{
            worldPos.xyz = position.xyz * vec3( lightRadius ,  step(position.y, 0) * halfbeamlength + lightRadius, lightRadius );
            //worldPos.xyz = position.xyz * vec3( lightRadius , halfbeamlength + lightRadius, lightRadius );
            //worldPos.xyz += vec3(50);
            //}
       
        // TODO rotate this box
        vec3 oldfw = vec3(0,1,0); // The old forward direction is -y
        vec3 newfw = normalize(centertoend); // the new forward direction shall be the normal that we want
        vec3 newright = normalize(cross(newfw, oldfw)); // the new right direction shall be the vector perpendicular to old and new forward
        vec3 newup = normalize(cross(newright, newfw)); // the new up direction shall be the vector perpendicular to new right and new forward
        // TODO: handle the two edge cases where newfw == (oldfw or -1*oldfw)
        mat3 rotmat = mat3( // assemble the rotation matrix
                newup,
                newfw,
                newright
            );
        worldPos.xyz = rotmat * worldPos.xyz;
       
        // so we now have our rotated box, we need to place it not at the center, but where the piece matrix tells us to
        // or where the lightcenterpos tells us to
       
               
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        // if the cone is not attached to the unit, exploit that direction allows us to smoothen anim
        if (attachedtounitID < 0.5){ // World Space Light


            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = clamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
        }else{  // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
        }


        // Place the box in the world
        worldPos.xyz += lightCenterPosition;

        v_worldPosRad2.xyz = (placeInWorldMatrix * vec4(v_worldPosRad2.xyz, 1.0)).xyz;;
        v_worldPosRad.xyz = (placeInWorldMatrix * vec4(lightCenterPosition.xyz, 1.0)).xyz;
        v_worldPosRad.xyz += (v_worldPosRad2.xyz - v_worldPosRad.xyz) * 0.5;
        v_position.xyz = (placeInWorldMatrix * vec4(worldPos.xyz, 1.0)).xyz;
    }
    #line 13000
    else if (pointbeamcone > 1.5){ // cone
        // input cone that has pointy end up, (y = 1), with radius =1, flat on Y=0 plane
        // make it so that cone tip is at 0 and the opening points to -y
        worldPos.xyz = position.xyz;
        worldPos.x *= -1.0; // flip the cone inside out
        worldPos.y = (worldPos.y*1.1 - 1.) * -1;
        //worldPos.y *= 1;
   
        worldPos.xz *= tan(worldposrad2.w); // Scale the flat of the cone by the half-angle of its opening
        worldPos.xyz *= lightRadius * 1.05; // scale it all by the height of the cone, and a bit of extra
       
        // Now our cone is opening forward towards  -y, but we want it to point into the worldposrad2.xyz
        vec3 oldfw = vec3(0, -1,0); // The old forward direction is -y
        vec3 newfw = normalize(worldposrad2.xyz); // the new forward direction shall be the normal that we want
        vec3 newright = normalize(cross(newfw, oldfw)); // the new right direction shall be the vector perpendicular to old and new forward
        vec3 newup = normalize(cross(newright, newfw)); // the new up direction shall be the vector perpendicular to new right and new forward
        // TODO: handle the two edge cases where newfw == (oldfw or -1*oldfw)
        mat3 rotmat = mat3( // assemble the rotation matrix
                newup,
                newfw,
                newright
            );
           
       
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        // if the cone is not attached to the unit, exploit that direction allows us to smoothen anim
        if (attachedtounitID < 0.5){ // World Space Light
            lightCenterPosition += worldposrad2.xyz * timeInfo.w;


            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = clamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
        }else{  // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
        }
       
        // rotate the cone, and place it into local space
        worldPos.xyz = rotmat * worldPos.xyz + lightCenterPosition;

        // move the cone into piece or world space:
        worldPos.xyz = (placeInWorldMatrix * vec4(worldPos.xyz, 1.0)).xyz;
       
        // set the center pos of the light:
        v_worldPosRad.xyz = (placeInWorldMatrix * vec4(lightCenterPosition.xyz, 1.0)).xyz;;

        v_worldPosRad2.w = cos(worldposrad2.w); // pass through the cosine to avoid this calc later on
        v_worldPosRad2.xyz = normalize(worldposrad2.xyz); // normalize this here for sanity
       
        // Clear out the translation from the cone direction, and turn the cone according to the piece matrix
        v_worldPosRad2.xyz = mat3(placeInWorldMatrix) * v_worldPosRad2.xyz;

        // Lens flare visibility. The apex usually sits on or inside the emitting piece, so also probe the first
        // stretch of the beam and take the most visible point. Seen from behind, the beam probes would show the
        // flare through the unit, so they fade out as the camera moves behind the source, and are skipped there.
        if (modelfactor_specular_scattering_lensflare.w > 0.0) {
            float sourcevisible = softVisibilityAtWorldPos(v_worldPosRad.xyz);
            vec3 conedir = normalize(v_worldPosRad2.xyz);
            float beamweight = smoothstep(-0.2, 0.2, dot(conedir, normalize(cameraViewInv[3].xyz - v_worldPosRad.xyz)));
            if (sourcevisible < 1.0 && beamweight > 0.0) {
                vec3 probestep = conedir * min(lightRadius * 0.25, 16.0) * 0.5;
                float beamvisible = max(softVisibilityAtWorldPos(v_worldPosRad.xyz + probestep), softVisibilityAtWorldPos(v_worldPosRad.xyz + probestep * 2.0));
                sourcevisible = max(sourcevisible, beamvisible * beamweight);
            }
            v_sourceVisibility = sourcevisible;
        }
       
        v_position =  worldPos;
    }
    #line 14000
   
    //  vec4 windInfo; // windx, windy, windz, windStrength
    v_noiseoffset = vec4(windX, 0, windZ,0) * (-0.0156);
    v_noiseoffset.a = float(gl_InstanceID); // InstanceID is only avail in vertex shader, and we use this as a unique offset for noise sampling. 
    //v_noiseoffset = vec4(0.0);
    //v_noiseoffset.y = windX + windZ;
   
    gl_Position = cameraViewProj * v_position;
    v_screenUV = SNORM2NORM(gl_Position.xy / gl_Position.w);
   
    // pass everything on to fragment shader:

}


)DLORIG";
inline constexpr char FragmentTemplate[]=R"DLORIG(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__

#line 30000


in DataVS {
	flat vec4 v_worldPosRad;
	flat vec4 v_worldPosRad2;
	flat vec4 v_lightcolor;
	flat vec4 v_modelfactor_specular_scattering_lensflare;
	vec4 v_otherparams;
	vec4 v_position;
	vec4 v_noiseoffset;
	noperspective vec2 v_screenUV;
	flat float v_sourceVisibility;
};

uniform sampler2D mapDepths;
uniform sampler2D modelDepths;
uniform sampler2D mapNormals;
uniform sampler2D modelNormals;
uniform sampler2D mapExtra;
uniform sampler2D modelExtra;
uniform sampler2D mapDiffuse;
uniform sampler2D modelDiffuse;
uniform sampler3D noise3DCube;
uniform sampler2D blueNoise;

uniform float pointbeamcone = 0;
uniform float nightFactor = 1.0;
// = 0; // 0 = point, 1 = beam, 2 = cone
uniform float radiusMultiplier = 1.0;
uniform float intensityMultiplier = 1.0;
uniform int screenSpaceShadows = 0;

out vec4 fragColor;


// Given a beam between beamStart and beamEnd, returns the pos on the beam closest to desired Point
vec3 closestbeam(vec3 point, vec3 beamStart, vec3 beamEnd){
	vec3 beamvec = beamEnd -beamStart ;
	vec3 pointbeam = point - beamStart;
	float c1 = dot(pointbeam, beamvec);
	float c2 = dot(beamvec, beamvec);
	if (c1 <=0 ) return beamStart;
	if (c2 <=c1) return beamEnd;
	return beamStart + (c1/c2) * beamvec;
}
//https://iquilezles.org/articles/distfunctions/

// Given a ray origin and a direction, returns the closes point on the ray in xyz and the distance in w
vec4 closestlightlp_distance (vec3 ro, vec3 rd, vec3 P){
	float t0 = dot(rd, P - ro) / dot(rd, rd);
	vec3 intersectPoint = ro + t0 * rd;
	return vec4(intersectPoint, length(P - intersectPoint));
}

// Lens flare effect
float LensFlareDistanceSqrt(vec3 pointpos, vec3 lightpos, float radius){
	float sqrtsum = dot(sqrt(abs(pointpos - lightpos)), vec3(1.0));
	return (sqrtsum * sqrtsum) / radius;
}

float distToCapsuleSqr( vec3 p, vec3 a, vec3 b, float r )
{
  vec3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  pa =  pa - ba*h;
  return dot(pa, pa) ;
}

// marginally faster, about the cost as a single octave of perlin
vec4 ray_to_capsule_distance_squared(vec3 rayOrigin, vec3 rayDirection, vec3 cap1, vec3 cap2){ // point1, dir1, beamstart, beamend
	// returns the squared distance of the ray and the line segment in w
	// returns the closest point on beam in xyz
	float rd_dot_rd_inv = 1.0 / dot(rayDirection, rayDirection);
	
	float t1 = dot(rayDirection, cap1 - rayOrigin) * rd_dot_rd_inv;
	vec3 intersectPoint1 = rayOrigin + t1 * rayDirection;
	
	float t2 = dot(rayDirection, cap2 - rayOrigin) * rd_dot_rd_inv;
	vec3 intersectPoint2 = rayOrigin + t2 * rayDirection;
	
	vec3 cap2tocap1 = cap2 - cap1;

	vec3 interSectToC2 = cap2 - intersectPoint2;
	vec3 interSectToC1 = cap1 - intersectPoint1;	

	float angle1 = dot(cap2tocap1, interSectToC1);
	float angle2 = dot(cap2tocap1, interSectToC2);
	
	vec3 connectornormal = cross(rayDirection, cap2tocap1); // n is the normal of the line connecting them
	float dd = dot(connectornormal, rayOrigin - cap1); // this is the angle between the connector normal and 
	float sqdistline = dd*dd / dot(connectornormal, connectornormal); // sqdistline; 

	float distcap2 = dot(interSectToC2, interSectToC2) ;
	float distcap1 = dot(interSectToC1, interSectToC1) ;  //sqdistends
	
	vec3 closestpointbeam = cap1 + cap2tocap1 * sqrt(abs(distcap1 - sqdistline)/dot(cap2tocap1,cap2tocap1));
	vec4 finalposanddist = mix(vec4(cap1, distcap1) ,
		vec4(cap2, distcap2) ,
		step(distcap2, distcap1));
		
	if (angle1 < 0 && angle2 > 0){ // this means that our ray is hitting between the caps
		finalposanddist.w = sqdistline;
		finalposanddist.xyz = closestpointbeam;
	}
	finalposanddist.w = sqrt(finalposanddist.w);
	return finalposanddist;
}
//http://www.realtimerendering.com/intersections.html

//https://iquilezles.org/articles/intersectors/
// capsule defined by extremes pa and pb, and radious ra
// Note that only ONE of the two spherical caps is checked for intersections,
// which is a nice optimization
float capIntersect( in vec3 ro, in vec3 rd, in vec3 pa, in vec3 pb, in float ra )
{
    vec3  ba = pb - pa;
    vec3  oa = ro - pa;
    float baba = dot(ba,ba);
    float bard = dot(ba,rd);
    float baoa = dot(ba,oa);
    float rdoa = dot(rd,oa);
    float oaoa = dot(oa,oa);
    float a = baba      - bard*bard;
    float b = baba*rdoa - baoa*bard;
    float c = baba*oaoa - baoa*baoa - ra*ra*baba;
    float h = b*b - a*c;
    if( h >= 0.0 )
    {
        float t = (-b-sqrt(h))/a;
        float y = baoa + t*bard;
        // body
        if( y>0.0 && y<baba ) return t;
        // caps
        vec3 oc = (y <= 0.0) ? oa : ro - pb;
        b = dot(rd,oc);
        c = dot(oc,oc) - ra*ra;
        h = b*b - c;
        if( h>0.0 ) return -b - sqrt(h);
    }
    return -1.0;
}

// capsule defined by extremes pa and pb, and radious ra
// Note that only ONE of the two spherical caps is checked for intersections,
// which is a nice optimization


//https://www.symbolab.com/solver/definite-integral-calculator/%5Cint_%7B0%7D%5E%7B1%7D%5Cleft(1-%5Cfrac%7B%5Cleft(A_%7B2%7D%2BB_%7B2%7D%5Ccdot%20x%5Cright)%7D%7Bh%5E%7B2%7D%7D%20%5Cright)%5E%7B%20%7D%5Ccdot%5Cleft(1-%5Cfrac%7B%5Csqrt%7B%5Cleft(A_%7B1%7D%2BB_%7B1%7D%5Ccdot%20x%5Cright)%5E%7B2%7D%7D%7D%7B%5Cfrac%7B%5Cleft(A_%7B2%7D%2BB_%7B2%7D%5Ccdot%20x%5Cright)%5Ccdot%20r%7D%7Bh%7D%7D%5Cright)%20dx?or=input

float dot2(vec3 a){ return dot(a,a);}

// Defines the falloff function of scattered light one gets more distant from the light source
float scatterfalloff(float disttolight, float radius){
	float x = clamp(1.0 - disttolight/radius, 0.0, 1.0);
	return (-0.5*x*x+x)/(0.5*x*x-x+1);
}

// Integratates the scattering from closes to eye pos to most distant pos
float integratescatterocclusion(float depthratio){
	float x = clamp(depthratio, 0.0, 1.0);
	return (x*x)/ (2*x*x -2*x +1.0);
}

// cone defined by extremes pa and pb, and radious ra and rb
// Only one square root and one division is employed in the worst case. dot2(v) is dot(v,v)

// Intersects a cone with a ray, with an optional occluding fragment distance
// Returns in xy the close and far distance cone intersection distances
// Note that the close one will be negative if the if the ray origin is inside the cone
// in z returns Rayleigh scattered light amount 
// in w returns Mie scattered light amount

#line 30400
vec4 halfconeIntersectScatter( in vec3  rayOrigin, in vec3 rayDirection, in vec3 coneTip, in vec3 coneDirection, in float coneHeight, in float bottomRadius, in float fragmentdistance)
{
	vec3  coneFlatEnd = coneTip + coneDirection * coneHeight;
	vec3  TipToEnd = coneFlatEnd - coneTip;
	vec3  TipToEye = rayOrigin - coneTip;
	vec3  EndToEye = rayOrigin - coneFlatEnd;
	float ConeHeightSqr = dot(TipToEnd, TipToEnd); 
	float EyeDotEnd = dot(TipToEye, TipToEnd);
	float RayDotEnd = dot(rayDirection, TipToEnd);
	float RayDotEye = dot(rayDirection, TipToEye);
	float TipToEyeSqr = dot(TipToEye, TipToEye);
	float TipDotEnd = dot(EndToEye, TipToEnd); 
	vec4 coneflatdistances = vec4(-1.0);

	// Bottom Cap collision, note that we still need to check this if we want the exit point too!
	// a ray can only collide with the end once!
	float distToFlat = - TipDotEnd / RayDotEnd;                     // NOT delayed division
	if( dot2(EndToEye+rayDirection*distToFlat)<=(bottomRadius*bottomRadius) )
		coneflatdistances.zw = vec2(distToFlat);
		// vec3 normal = TipToEnd*inversesqrt(ConeHeightSqr)
	
	// Test Cone Body intersections
	float CapeLengthSqr = ConeHeightSqr + bottomRadius * bottomRadius;
	float k2 = ConeHeightSqr * ConeHeightSqr - RayDotEnd * RayDotEnd * CapeLengthSqr;
	float k1 = ConeHeightSqr * ConeHeightSqr * RayDotEye - EyeDotEnd * RayDotEnd * CapeLengthSqr ;
	float k0 = ConeHeightSqr*ConeHeightSqr*TipToEyeSqr - EyeDotEnd * EyeDotEnd * CapeLengthSqr ;
	float h = k1*k1 - k2*k0;
	if( h<0.0 ) return vec4(-1.0, -1.0, 0.0, 0.0); //no intersection as we are _above_ the cone tip
	float sqrth = sqrt(max(0,h));
	float CloseDistToCone = (-k1-sqrth)/k2;
	float FarDistToCone = (-k1+sqrth)/k2;
	float CloseY = EyeDotEnd + CloseDistToCone * RayDotEnd;
	float FarY = EyeDotEnd + FarDistToCone * RayDotEnd;
	
	// if we are within the cone from the front
	if( CloseY >=0.0 && CloseY <= ConeHeightSqr ) coneflatdistances.x = CloseDistToCone;
	else {
		coneflatdistances.x = max(coneflatdistances.x,coneflatdistances.z);//no intersection, as we are below the bottom of the cone
	}
	// if we are within the cone from the back
	if( FarY >=0.0 && FarY <= ConeHeightSqr ) coneflatdistances.y = FarDistToCone; //no intersection, as we are below the bottom of the cone
	else {
		coneflatdistances.y = max(coneflatdistances.y,coneflatdistances.z);
	}
	 // vec3 normal =normalize(ConeHeightSqr*(ConeHeightSqr*(TipToEye+CloseDistToCone*rayDirection))-TipToEnd*CapeLengthSqr*y)
	// Bail early if we are not on the cone
	if (abs(coneflatdistances.y - coneflatdistances.x) < 0.001) return vec4(-1, -1, 0, 0);
	 
	// try to get some sort of falloff factor
	// calc angle between entry and exit
	vec3 EntryPoint = rayDirection * coneflatdistances.x + rayOrigin;
	vec3 ExitPoint =  rayDirection * coneflatdistances.y + rayOrigin;
	
	vec3 stepVec = rayDirection * ( coneflatdistances.y  - coneflatdistances.x) / RAYMARCHSTEPS;
	float stepVecSqr = dot(stepVec, stepVec);
	
	float cosTheta = sqrt(ConeHeightSqr / CapeLengthSqr);
	float oneminuscosThetainv = 1.0 / (1.0 - cosTheta);
	float ConeHeightSqrInv = 1.0 / ConeHeightSqr;
	float rayleighScatterSum = 0;
	float miescattersum = 0;

	for (int i = 1; i < RAYMARCHSTEPS; i++){
		// step the ray forward 
		vec3 marchPos = stepVec * i + EntryPoint;

		// Sample 3D noise if needed (noise should be 0.5 centered)
		#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
			float noise = textureLod(noise3DCube, fract(marchPos * 0.005 + 0 / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0	).r * 2.0 ;
		#else
			float noise = 1.0;
		#endif
		
		// Calculate the distatten (which is squared)
		float distatten = clamp((1.0 - dot(marchPos - coneTip, marchPos - coneTip) * ConeHeightSqrInv), 0.0, 1.0);
		
		vec3 tiptomarchnorm = normalize(marchPos - coneTip);
		float falloffatten = clamp(1.0 - (1.0-dot(coneDirection, tiptomarchnorm))* oneminuscosThetainv, 0.0,1.0);
		
		float rayleighhere = distatten*falloffatten * noise;
		rayleighScatterSum += rayleighhere;
		// as mie mainly scatters forward, we will need to dot raydirection with lightdirection
		float miefactor = clamp(dot(-tiptomarchnorm, rayDirection) * 10.0 -9 , 0.0,1.0);
		//rayleighScatterSum += miefactor * falloffatten;
		miescattersum += rayleighhere * miefactor ;
	}
	
	// Simplest occlusion calculation just integrates scatter in between fragment distance and eye distance
	// cone close distance is negative if rayorigin is inside it
	float tofrag = integratescatterocclusion((fragmentdistance - coneflatdistances.x ) / (coneflatdistances.y - coneflatdistances.x));
	
	float fromeye = integratescatterocclusion( ( -coneflatdistances.x ) / (coneflatdistances.y - coneflatdistances.x));
	
	
	rayleighScatterSum = (tofrag - fromeye) * rayleighScatterSum / (RAYMARCHSTEPS - 1);
	miescattersum = (tofrag - fromeye) * miescattersum / (RAYMARCHSTEPS - 1);

	return vec4(coneflatdistances.x, coneflatdistances.y, rayleighScatterSum, miescattersum);
	
}

// https://gist.github.com/wwwtyro/beecc31d65d1004f5a9d
vec2 raySphereIntersect(vec3 r0, vec3 rd, vec3 s0, float sr) {
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersection with sphere,
    //   or -1.0 if no intersection.
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
	float disc = b * b - 4.0 * a* c;
    if (disc < 0.0) {
        return vec2(-1.0, -1.0);
    }else{
		disc = sqrt(disc);
		return vec2(-b - disc, -b + disc) / (2.0 * a);
	}
}
// This is the fast approx scattering using a mierayleighratio, where 1.0 = Rayleigh, ~0.1 = Mie
// TODO: handle the case where viewpos is inside the volume!
float SlowSphereRayMarchedScattering(vec3 campos, vec3 viewdirection, vec3 lightposition, float lightradius, float fragmentdistance, float lightdistance, float mierayleighratio){
	vec2 closeandfardistance = raySphereIntersect(campos, -viewdirection,  lightposition, lightradius * mierayleighratio);
	
	float noise = 1.0;
	#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
		vec3 EntryPoint = (-viewdirection) * closeandfardistance.x + campos;
		//vec3 ExitPoint =  (viewdirection) * closeandfardistance.y + campos;
		vec3 stepVec =  (-viewdirection) * ( closeandfardistance.y  -  closeandfardistance.x) / RAYMARCHSTEPS;
		for (int i = 1; i < RAYMARCHSTEPS; i++){
			vec3 marchPos = stepVec * i + EntryPoint;
			noise += textureLod(noise3DCube, fract(marchPos * 0.005 + i / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0).r * 2.0 ;
		}
		noise = noise / (RAYMARCHSTEPS -1);
	#endif
	
	float depthratio = clamp(0.5 * (fragmentdistance - closeandfardistance.x) / (lightradius * mierayleighratio), 0.0, 1.0);
	float tofrag = integratescatterocclusion(depthratio);
	float fromeye = integratescatterocclusion( ( -closeandfardistance.x ) / (closeandfardistance.y - closeandfardistance.x));
	return scatterfalloff(lightdistance, lightradius * mierayleighratio) * (tofrag - fromeye) * noise;
}

float FastApproximateScattering(vec3 campos, vec3 viewdirection, vec3 lightposition, float lightradius, float fragmentdistance, float lightdistance, float mierayleighratio){
	vec2 closeandfardistance = raySphereIntersect(campos, -viewdirection,  lightposition, lightradius * mierayleighratio);
	float depthratio = clamp(0.5 * (fragmentdistance - closeandfardistance.x) / (lightradius * mierayleighratio), 0.0, 1.0);
	float tofrag = integratescatterocclusion(depthratio);
	float fromeye = integratescatterocclusion( ( -closeandfardistance.x ) / (closeandfardistance.y - closeandfardistance.x));
	return scatterfalloff(lightdistance, lightradius * mierayleighratio) * (tofrag - fromeye);
}


// Works fine, but 16 FMA

// Works fine, but 16 FMA
vec3 WorldToScreen(vec3 worldCoords){ // returns screen UV and depth position
	vec4 screenPosition = cameraViewProj * vec4(worldCoords,1.0);
	return screenPosition.xyz / screenPosition.w;
}

// Interesting function, takes two depths in screen space, and given 
// nxp2 = 2 * nearPlane * farplane 
// fpn = nearPlane * farPlane
// fmn = nearPlane - farPlane
// return the delta in elmos between the two depths
float depthToDeltaElmos(float depthCloser, float depthFarther, float nxp2, float fpn, float fmn){
	return 36.0 * ( nxp2 / (fpn - (depthCloser* 2.0 - 1.0) * fmn) - nxp2 / (fpn - (depthFarther * 2.0 - 1.0) * fmn));
}

// Additional notes and reading:
//https://andrew-pham.blog/2019/10/03/volumetric-lighting/ ???

#ifdef SCREENSPACESHADOWS
	// See Shader Amortization using Pixel Quad Message Passing
	vec2 quadVector = vec2(0); // REQUIRED, contains the [-1,1] mappings
	// one-hot encoding of thread ID
	vec4 threadMask = vec4(0); // contains the thread ID in one-hot
	#define selfWeightFactor 0.07
	vec4 selfWeights = vec4(0.25) + vec4(selfWeightFactor, selfWeightFactor/ -3.0, selfWeightFactor/ -3.0, selfWeightFactor/-3.0);

	vec4 quadGetThreadMask(vec2 qv){ 
		vec4 threadMask =  step(vec4(qv.xy,0,0),vec4( 0,0,qv.xy));
		return threadMask.xzxz * threadMask.yyww;
	}

	vec4 get_quad_vector_naive(vec4 output_pixel_num_wrt_uvxy)
	{
		//  Requires:   Two measures of the current fragment's output pixel number
		//              in the range ([0, IN.output_size.x), [0, IN.output_size.y)):
		//              1.) output_pixel_num_wrt_uvxy.xy increase with uv coords.
		//              2.) output_pixel_num_wrt_uvxy.zw increase with screen xy.
		//  Returns:    Two measures of the fragment's position in its 2x2 quad:
		//              1.) The .xy components are its 2x2 placement with respect to
		//                  uv direction (the origin (0, 0) is at the top-left):
		//                  top-left     = (-1.0, -1.0) top-right    = ( 1.0, -1.0)
		//                  bottom-left  = (-1.0,  1.0) bottom-right = ( 1.0,  1.0)
		//                  You need this to arrange/weight shared texture samples.
		//              2.) The .zw components are its 2x2 placement with respect to
		//                  screen xy direction (IN.position); the origin varies.
		//                  quad_gather needs this measure to work correctly.
		//              Note: quad_vector.zw = quad_vector.xy * float2(
		//                      ddx(output_pixel_num_wrt_uvxy.x),
		//                      ddy(output_pixel_num_wrt_uvxy.y));
		//  Caveats:    This function assumes the GPU driver always starts 2x2 pixel
		//              quads at even pixel numbers.  This assumption can be wrong
		//              for odd output resolutions (nondeterministically so).
		vec4 pixel_odd = fract(output_pixel_num_wrt_uvxy * 0.5) * 2.0;
		vec4 quad_vector = pixel_odd * 2.0 - vec4(1.0);
		return quad_vector;
	}

	vec4 get_quad_vector(vec4 output_pixel_num_wrt_uvxy)
	{
		//  Requires:   Same as get_quad_vector_naive() (see that first).
		//  Returns:    Same as get_quad_vector_naive() (see that first), but it's
		//              correct even if the 2x2 pixel quad starts at an odd pixel,
		//              which can occur at odd resolutions.
		vec4 quad_vector_guess =
			get_quad_vector_naive(output_pixel_num_wrt_uvxy);
		//  If quad_vector_guess.zw doesn't increase with screen xy, we know
		//  the 2x2 pixel quad starts at an odd pixel:
		vec2 odd_start_mirror = 0.5 * vec2(dFdx(quad_vector_guess.z),
													dFdy(quad_vector_guess.w));
		return quad_vector_guess * odd_start_mirror.xyxy;
	}


	vec4 quadGather(float inputthis){
		float inputadjx = inputthis - dFdx(inputthis) * quadVector.x;
		float inputadjy = inputthis - dFdy(inputthis) * quadVector.y;
		float inputdiag = inputadjx - dFdy(inputadjx) * quadVector.y;
		return vec4(inputthis, inputadjx, inputadjy, inputdiag);
	}


#endif

#line 31000
void main(void)
{
	
	float mapdepth = textureLod(mapDepths, v_screenUV, 0.0).x;
	float modeldepth = textureLod(modelDepths, v_screenUV, 0.0).x;
	float worlddepth = min(mapdepth, modeldepth);
	vec4 normals = vec4(0, 1, 0, 0); // points up by default
	vec4 extratex = vec4(0);
	vec4 targetcolor = vec4(0); // target color of the surface
	float ismodel = 0;
	
	// Only query the textures if the backface of the volume is further than the world fragment
	if (gl_FragCoord.z > worlddepth) {
		if (modeldepth < mapdepth) { // We are processing a model fragment
			ismodel = 1;
			normals =  textureLod(modelNormals, v_screenUV, 0.0) * 2.0 - 1.0;
			extratex = textureLod(modelExtra, v_screenUV, 0.0);
			targetcolor = textureLod(modelDiffuse, v_screenUV, 0.0);
			
		}else{
			normals =  textureLod(mapNormals, v_screenUV, 0.0) * 2.0 - 1.0;
			extratex = textureLod(mapExtra, v_screenUV, 0.0);
			targetcolor = textureLod(mapDiffuse, v_screenUV, 0.0) * nightFactor;
		}
	}
	normals.xyz = normalize(normals.xyz);
	
	
	vec4 fragWorldPos =  vec4( vec3(v_screenUV.xy * 2.0 - 1.0, worlddepth),  1.0);
	fragWorldPos = cameraViewProjInv * fragWorldPos;
	fragWorldPos.xyz = fragWorldPos.xyz / fragWorldPos.w; // YAAAY this works!
	
	vec3 camPos = cameraViewInv[3].xyz;
	float fragDistance = length(camPos - fragWorldPos.xyz);
	vec3 viewDirection = (camPos - fragWorldPos.xyz) / fragDistance; // vector pointing in the direction of the eye ray
	
	float lightRadius = v_worldPosRad.w;
	float lightRadiusInv = 1.0 / lightRadius;
	vec3 lightDirection = vec3(0); // the normalized vector from the light source to the fragment
	vec3 lightToWorld = vec3(0); // The vector pointing from light source to fragment world pos
	vec3 lightPosition = vec3(0); // This is the position of the light illuminating the fragment
	vec3 lightEmitPosition = vec3(0); // == lightPosition, except for beams, where the lightEmitPosition for a ray is different than for a world fragment
	
	float raytolightdistance = 0;
	vec4 closestpoint_dist = vec4(0); // the point that is closest to the light source (xyz) and the distance to it (w)
	
	// Lighting components we wish to collect along the way:
	float attenuation = 0; // Just the distance from the light source (multiplied with falloff for cones
	float falloff = 1; // only cone light have this
	float lensFlare = 0;
	
	float scatteringRayleigh = 0; // the integration of the light reflected into the eye from the eye ray through the volume
	
	float scatteringMie = 0; // the integration of the light reflected into the eye from the eye ray through a size-reduced volume
	float diffuse = 0; // The amount of diffuse reflection from the world-hitting fragment
	float specular = 0; // The amount of specular reflection from the world-hitting fragment
	
	float shadowedness = 0; // how much the light source is screen-space shadowed

	#line 32000
	if (pointbeamcone < 0.5){ //point
		lightPosition = v_worldPosRad.xyz;
		lightEmitPosition = lightPosition;
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		lightDirection = normalize(lightToWorld);
		
		attenuation = clamp( 1.0 - length (lightToWorld) * lightRadiusInv, 0,1);
		
		// example of exponential attenuation instead of clamped,
		//Use a smoother distance-falloff formula such as an exponential or inverse-square:
		//float dist = length(lightToWorld);
		//attenuation = exp(-dist * 0.5);
		
		closestpoint_dist = closestlightlp_distance(camPos, viewDirection, lightPosition);
		
		// Both scattering components
		scatteringRayleigh = SlowSphereRayMarchedScattering(camPos, viewDirection, lightPosition, lightRadius, fragDistance, closestpoint_dist.w, 1.0 );
		
		scatteringMie = FastApproximateScattering(camPos, viewDirection, lightPosition, lightRadius, fragDistance, closestpoint_dist.w, MIERAYLEIGHRATIO);
		
		lensFlare = v_sourceVisibility * clamp( LensFlareDistanceSqrt(closestpoint_dist.xyz, lightPosition,lightRadius) *(-10/(0.01+v_modelfactor_specular_scattering_lensflare.w)) + 1.0, 0, 1);

	#line 33000
	}else if (pointbeamcone > 1.5){ // cone
		lightPosition = v_worldPosRad.xyz;
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		lightDirection = normalize(lightToWorld);
		vec3 coneDirection = v_worldPosRad2.xyz;
		float lightCosTheta = v_worldPosRad2.w; // The cos of the half angle of the spot cone light
		float lightSinTheta = sqrt(max(0.0, 1.0 - lightCosTheta * lightCosTheta)); // The sin of the half angle of the spot cone light
		
		float lightandworldangle = dot(lightDirection, coneDirection);
		//falloff = smoothstep(lightCosTheta, 1.0, lightandworldangle) ; // this is softer, but attenuates too much
		falloff = clamp(1.0 - (1.0-lightandworldangle)/ (1.0 - lightCosTheta), 0.0,1.0);

		attenuation = clamp( 1.0 - length (lightToWorld) / lightRadius, 0,1) * falloff;
		
		lightEmitPosition = lightPosition;
		closestpoint_dist = closestlightlp_distance(camPos, viewDirection, lightPosition);
		
		vec4 rayconedist = ray_to_capsule_distance_squared(camPos, viewDirection, lightPosition + coneDirection * lightRadius,lightPosition); // this now contains the point on ConeDirection 
		
		float localradius = max(0, lightSinTheta * length(rayconedist.xyz - lightPosition.xyz));
		vec4 iCone = halfconeIntersectScatter(camPos , -viewDirection, lightPosition, coneDirection , lightRadius,  lightRadius * lightSinTheta, fragDistance);
		
		scatteringRayleigh = iCone.z * 0.33;
		scatteringMie = iCone.w;

		lensFlare = v_sourceVisibility * clamp( LensFlareDistanceSqrt(closestpoint_dist.xyz, lightPosition,lightRadius) *(-15 / (0.01 +v_modelfactor_specular_scattering_lensflare.w)) +1, 0, 1);
		
		float lightandcameraangle = dot(coneDirection, viewDirection) + 1.0;
		//lensFlare *= smoothstep(lightCosTheta, 1.0, lightandcameraangle);
		lensFlare *= lightandcameraangle;
		//lensFlare = fract(localradius *0.02);
		
	#line 34000
	}else if (pointbeamcone < 1.5){ // beam 
		vec3 beamhalflength = v_worldPosRad2.xyz - v_worldPosRad.xyz; 
		vec3 beamstart = v_worldPosRad.xyz - beamhalflength;
		vec3 beamend = v_worldPosRad.xyz + beamhalflength;
		lightPosition = closestbeam(fragWorldPos.xyz, beamstart, beamend);
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		
		lightDirection = normalize(lightToWorld);
		attenuation = clamp( 1.0 - length (lightToWorld) *lightRadiusInv, 0,1);
		
		
		closestpoint_dist = ray_to_capsule_distance_squared(camPos, viewDirection, beamstart, beamend);
		lightEmitPosition = closestpoint_dist.xyz;
		
		
		lensFlare = 0.0;

		// where the view ray enters and leaves the capsule
		float closedist =  capIntersect( camPos, -viewDirection, beamstart, beamend, lightRadius);
		float fardist = - capIntersect( camPos, viewDirection, beamstart, beamend, lightRadius);
		
		
		vec3 EntryPoint = (-viewDirection) * closedist + camPos;
		
		vec3 stepVec = - viewDirection * ( fardist  -  closedist) / RAYMARCHSTEPS;
		
		float rayleighScatterSum = 0;
		float miescattersum = 0;

		for (int i = 1; i < RAYMARCHSTEPS; i++){
			// step the ray forward 
			vec3 marchPos = stepVec * i + EntryPoint;

			// Sample 3D noise if needed (noise should be 0.5 centered)
			#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
				float noise = textureLod(noise3DCube, fract(marchPos * 0.005 + i / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0).r * 2.0 ;
			#else
				float noise = 1.0;
			#endif
			
			float relativeclosenesstobeam = clamp(distToCapsuleSqr(marchPos, beamstart, beamend, lightRadius) * lightRadiusInv * lightRadiusInv, 0.0, 1.0) ;
			
			rayleighScatterSum +=  (1.0 - relativeclosenesstobeam) * noise;
			
			miescattersum += (max(0, 0.1 - relativeclosenesstobeam) * noise  * 10.0);
		}
		
		// Simplest occlusion calculation just integrates scatter in between fragment distance and eye distance
		// cone close distance is negative if rayorigin is inside it
		float tofrag = integratescatterocclusion((fragDistance - closedist ) / (fardist - closedist));
		
		float fromeye = integratescatterocclusion( ( -closedist ) / (fardist - closedist));
		
		rayleighScatterSum = (tofrag - fromeye) * rayleighScatterSum / (RAYMARCHSTEPS - 1);
		miescattersum = (tofrag - fromeye) * miescattersum / (RAYMARCHSTEPS - 1);

		scatteringMie = (miescattersum *miescattersum);
		scatteringRayleigh = rayleighScatterSum;

	}
	#line 35000
	
	diffuse = clamp(dot(-lightDirection, normals.xyz), 0.0, 1.0);
	
	
	// Blinn-Phong specular: half-vector instead of reflect(). Produces rounder
	// highlights that elongate naturally at grazing angles (more wet-metal/PBR look)
	// instead of Phong's teardrop-then-pop. Exponent is ~4x Phong to match the
	// equivalent highlight tightness.
	vec3 halfVector = normalize(-lightDirection + viewDirection);
	specular = max(0.0, dot(halfVector, normals.xyz));
	specular = v_modelfactor_specular_scattering_lensflare.y * pow(specular, 32.0 * ( 1.0 + ismodel * v_modelfactor_specular_scattering_lensflare.x) ) * (1.0 + ismodel * v_modelfactor_specular_scattering_lensflare.x);
	// Core-weighted falloff:
	//  - attCore: quick drop right outside the center (reduces "blob" look)
	//  - attTailEase: smooth/eased approach to zero near the max radius
	// Blend keeps the soft radius edge while making the center visibly hotter.
	float attLinear = attenuation;
	float attCore = pow(attLinear, 2.5);
	float attTailEase = attLinear * attLinear * (3.0 - 2.0 * attLinear);
	attenuation = mix(attCore, attTailEase, 0.17);
	attenuation = clamp(attenuation * (1.0 + 0.22 * attLinear * attLinear), 0.0, 1.0);

	//Give each light a unique blue noise sampling offset 
	vec2 blueNoiseUV = (gl_FragCoord.xy + float(v_noiseoffset.a)*7.0)/64.0;
	vec4 blueNoiseSample = textureLod(blueNoise, blueNoiseUV, 0.0);

	// Do screen-space sampling for shadowing because you are a silly boy:
	float unoccluded = 1.0;
	#ifdef SCREENSPACESHADOWS
		// But only for point and cone lights:
		// also assume that only models will shadow
		if ((v_otherparams.w) > 0.01 && (screenSpaceShadows > 0)) {
			const int sampleCount = screenSpaceShadows;
			// Initialize the quad vectors, so we can do Pixel Quad Message Passing
			quadVector = get_quad_vector(vec4(v_screenUV.xy, floor(gl_FragCoord.xy))).zw;
			threadMask = quadGetThreadMask(quadVector);

			// Split up a step evenly between the 4 threads in the quad
			vec4 threadOffset = vec4(0.125, 0.375, 0.625, 0.875);
			//vec4 threadOffset = vec4(0.5);

			// Generate a small, random offset to jiggle the samples a little bit, this provides a bit of noise to the shadowing
			//float randomOffset = rand(gl_FragCoord.xy * 0.013971639) * (- 0.25);
			float randomOffset = blueNoiseSample.a * (-0.25);
			// Collect occludedness in this variable
			float occludedness = 0.0;
			// Skip the march where attenuation is already 0, as unoccluded only scales blendedlights, which gets multiplied by attenuation anyway. Gated per quad, as the quadGather below needs all four lanes
			if (any(greaterThan(quadGather(attenuation), vec4(0.0)))) {
			
				// Steps in screen space
							
				// Set up the raytracing variables
				vec3 rayStartScreen = WorldToScreen(lightEmitPosition.xyz);
				vec3 rayEndScreen = WorldToScreen(fragWorldPos.xyz);
				vec3 rayStepScreen = (rayEndScreen - rayStartScreen) / (sampleCount);
				float rayStepWorldLength = length(lightEmitPosition.xyz - fragWorldPos.xyz) / sampleCount;
				float threadRandom = dot(threadMask, threadOffset) + randomOffset;
				
				// Get the near and far planes from the viewProjection matrix:
				float nearPlane = cameraProj[2][3] / (cameraProj[2][2] - 1.0);
				float farPlane = cameraProj[2][3] / (cameraProj[2][2] + 1.0);
				float nearplane_x_farplane_2 = 2.0 * nearPlane * farPlane;
				float farplane_minus_nearplane = farPlane - nearPlane;
				float farplane_plus_nearplane = farPlane + nearPlane;

				// float LinearDepthGL(float z_s, float nearPlane, float farPlane){
				//    float z_ndc   = z_s * 2.0 - 1.0;                // back-project to NDC
    			// return (2.0 * nearPlane * farPlane) /  (farPlane + nearPlane - z_ndc * (farPlane - nearPlane));}
				// same units as nearPlane & f
				// Use the code below to verify the behavior of the depth buffer, and the linearity of the depth buffer
				/*
					float lightY = lightEmitPosition.y;
					float groundY = fragWorldPos.y;
					//printf(lightY);
					//printf(groundY);
					float dheight = lightY - groundY;
					//printf(dheight);
					//printf(nearPlane);
					//printf(farPlane);

					// For Proper debugging, we need to actually verify the Z distance between the light origin and the depth of the fragment at the cursor. 
					float lightDistanceToCamera = 36.0 * nearplane_x_farplane_2 / (farplane_plus_nearplane - (rayStartScreen.z * 2.0 - 1.0) * farplane_minus_nearplane);

					//printf(lightDistanceToCamera);

					float fragDistance = 36.0 *  nearplane_x_farplane_2 / (farplane_plus_nearplane - (rayEndScreen.z * 2.0 - 1.0) * farplane_minus_nearplane);

					//printf(fragDistance);

					float elmoDistance = (lightDistanceToCamera - fragDistance); // Yup, 36.0 is the magic number that still provides decent linearity along the range. 
					//printf(elmoDistance);

					// Ok, elmodistance seems to be correct, now we need to simplify the above calculation
					float nxp2 = 2.0 * nearPlane * farPlane;
					float fmn  = farPlane - nearPlane;
					float fpn  = farPlane + nearPlane;

					float testDistance = depthToDeltaElmos(rayStartScreen.z, rayEndScreen.z, nearplane_x_farplane_2, farplane_plus_nearplane, farplane_minus_nearplane);
					//printf(testDistance);	
				*/

				for (int i = 0; i < sampleCount; i++){ 
					float stepSize =  (float(i) + threadRandom );

					// Calculate the current ray position in screenspace only!

					vec3 rayScreenPos = rayStartScreen + rayStepScreen * stepSize; 
					// Convert the NDC to UV space, and clamping is not needed because enabling it brings bad artifacts
					rayScreenPos.xy = rayScreenPos.xy * 0.5 + 0.5;
					
					float rayScreenDepthSample = textureLod(modelDepths, rayScreenPos.xy, 0.0).x;

					// Assume that any sample outside of the edges of the screen will not occlude
					if (any(lessThan(rayScreenPos.xy, vec2(0.001))) || any(greaterThan(rayScreenPos.xy, vec2(1.0-0.001)))) rayScreenDepthSample = 1.0;
					
					// Since modeldepth is zero where there is no model, we need to convert this to 1.0
					if (rayScreenDepthSample < 0.01) rayScreenDepthSample = 1.0;

					// we need to soften this, based on the squared world-space distance between the ray point and the sampled depth:
					
					float sampleDistance = depthToDeltaElmos(rayScreenPos.z, rayScreenDepthSample, nearplane_x_farplane_2, farplane_plus_nearplane, farplane_minus_nearplane);

					// Occlude down to 48 elmos in depth. 
					#if 1
						//if (sampleDistance > 0.0 && sampleDistance < 48.0) {
						//	occludedness += 1.0; // This is a fully occluded sample
						//}
						// We need a function centerered around 24, where its 1, and then goes down with rayStepWorldLength.  
						#define OCCLUSIONWIDTH 32.0
						#define OCCLUSIONSLOPE 0.3
						occludedness += clamp( OCCLUSIONSLOPE *( OCCLUSIONWIDTH - abs(sampleDistance  + 1.0/(OCCLUSIONSLOPE) -  OCCLUSIONWIDTH)), 0.0, 1.0  );


					#else
						float sampleOcclusionStrength = 0.0;
											// Assume that a ray that hits exactly occludes 0.5
						if (sampleDistance > 0 ) {
							// If the sample actually occludes, then softly occlude it based on the distance from the light
							if (sampleDistance > 48.0) // Very deep occluders dont occlude
								sampleOcclusionStrength = 1.0 -  clamp((sampleDistance - 48.0) / (rayStepWorldLength * 0.05), 0.0, 1.0);
							else
								sampleOcclusionStrength = 0.5 + clamp(sampleDistance / (rayStepWorldLength * 0.05), 0.0, 1.0) * 0.5;

						}else{
							// if the sample does not occlude, but is close to doing so, then softly occlude it based on the distance from the light	
							sampleOcclusionStrength = 0.5 - clamp( -1 * sampleDistance / (rayStepWorldLength * 0.2), 0.0, 1.0) * 0.5;
						}
				
						occludedness += sampleOcclusionStrength * 1.0;

					#endif
					
				}
			}

			float prob = 0.56;

			//vec4 weights = vec4(prob*prob, prob*(1.0-prob), (1.0-prob)*prob, (1.0-prob)*(1.0-prob));
			vec4 weights = vec4(prob, 1.0 - prob, 1.0 - prob, prob) * 0.5;
			vec4 gatheredunoccluded = quadGather(occludedness);
			float dotproduct = dot(gatheredunoccluded, vec4(4* weights)); // This will multiply by 4, effectively
			//dotproduct = occludedness;
			unoccluded = 1.0 - smoothstep(0.0, 0.25 * v_otherparams.w * float(screenSpaceShadows) , dotproduct);
			//unoccluded = 1.0;
			//printf(unoccluded);
		}	

		
	#endif
	fragColor.rgb = vec3(
			(diffuse) * attenuation + lensFlare,
			//relativedistancetolight * relativedistancetolight * selfglowfalloff * sourceVisible + 
			scatteringMie + 
			(specular) * attenuation,
			 scatteringRayleigh
			);
			
	
	// light mixdown:
	targetcolor.rgb = max(vec3(0.2), targetcolor.rgb); // we shouldn't let the targetcolor be fully black, or else we will have a bad time blending onto it.
	
	float mintarg = 0.4;
	float targetbrightness =dot(targetcolor.rgb, vec3(0.375,0.5,0.125));
	
	targetcolor *= (1.0 -  step(mintarg,targetbrightness) * (targetbrightness -mintarg));
	// if brightness is > 0.5, start reducing it?
	
	
	// Sum up the additives of Rayleigh, Mie and LensFlare, colorize and alpha control them
	vec3 additivelights = ((scatteringRayleigh + scatteringMie) * v_modelfactor_specular_scattering_lensflare.z + lensFlare) * v_lightcolor.rgb * v_lightcolor.w * 0.4  ;

	// Sum up diffuse+specular and colorize the light
	vec3 blendedlights = (v_lightcolor.rgb * v_lightcolor.w) * (diffuse + specular) * unoccluded;

	// Modulate color with target color of the surface. 
	blendedlights = mix(blendedlights, blendedlights * targetcolor.rgb * 2.0, SURFACECOLORMODULATION);
	#if (VOIDWATER == 1) 
		if (fragWorldPos.y < 0) 
			blendedlights.rgb = vec3(0.0);
	#endif
	// Calculate attenuation and blend more lights onto models
	blendedlights *= attenuation * 2.0 * (1.0 + 2.0 * ismodel * v_modelfactor_specular_scattering_lensflare.x);
	
	// Estimate how 'lit' our target fragment is going to be, once we additively blend all light factors and the target surface color
	vec3 outlight_unclamped = targetcolor.rgb + blendedlights + additivelights;
	
	// The amount of light that will 'bleed', or overflow from that pixel is the sum of each channel that is over 1.0 according to our estimate
	float bleed = dot(max(vec3(0.0), outlight_unclamped - vec3(1.0)), vec3(1.0));
	
	// Square the bleed because why not
	bleed = bleed * bleed;
	
	// bleeding makes the other channels brighter when we 'overflow' with lighting
	fragColor.rgb = (blendedlights*0.9  + additivelights*0.5) + vec3(bleed)* BLEEDFACTOR; 
	
	fragColor.rgb *= intensityMultiplier;

	// Add a half a bit of blue noise to the color to prevent banding, especially with contrast adaptive sharpening:
	// Which should kinda be color dependent anyway, so lets find the max of all the color, and use that to ensure we get half a bit on the output:
	//v_lightcolor.rgb
	fragColor.rgb -=  (blueNoiseSample.rgb - 0.5) * (1.0 / 384.0);
	//fragColor.rgb *= v_lightcolor.a;
	//fragColor.rgb = vec3(bleed);
	//fragColor.rgb = vec3(targetcolor.rgb + blendedlights + additivelights);
	//fragColor.rgb = outlight_unclamped;
	//fragColor.rgb = vec3(scatteringRayleigh);
	//fragColor.rgb = vec3(dot(fragColor.rgb, vec3(1.0, 0.5, 0.5)));
	//fragColor.rgb += 0.5;
	//fragColor.rgb += clamp(0.25* vec3(pow(1.0-rcdsqr / (v_worldPosRad.w * v_worldPosRad.w),16)), 0.0, 1.0);
	//fragColor.rgb = vec3(rcdsqr / (v_worldPosRad.w * v_worldPosRad.w));
	//fragColor.rgb = (fract(lightEmitPosition*0.02));
	fragColor.a = 1.0;
	//fragColor.rgb = vec3(targetcolor);
	//fragColor.rgb = vec3(attenuation);
	//fragColor.rgb = fract(v_lightcenter_gradient_height.www*0.1);
	
}
)DLORIG";
inline constexpr char EngineDefinitions[]=R"DLORIG(layout(std140, binding = 0) uniform UniformMatrixBuffer {
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

)DLORIG";
inline constexpr char QuaternionDefinitions[]=R"DLORIG(// Quaternion math functions
struct Transform {
	vec4 quat;
	vec4 trSc;
};

layout(std140, binding = 0) readonly buffer TransformBuffer {
	Transform transforms[];
};

uint GetUnpackedValue(uint packedValue, uint byteNum) {
	return (packedValue >> (8u * byteNum)) & 0xFFu;
}

vec4 MultiplyQuat(vec4 a, vec4 b)
{
    return vec4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz));
}

vec3 RotateByQuaternion(vec4 q, vec3 v) {
	return 2.0 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2.0 * q.w * cross(q.xyz, v);
}

vec4 RotateByQuaternion(vec4 q, vec4 v) {
	return vec4(RotateByQuaternion(q, v.xyz), v.w);
}

vec4 InvertNormalizedQuaternion(vec4 q) {
	return vec4(-q.x, -q.y, -q.z, q.w);
}

vec3 ApplyTransform(Transform tra, vec3 v) {
	return RotateByQuaternion(tra.quat, v * tra.trSc.w) + tra.trSc.xyz;
}

vec4 ApplyTransform(Transform tra, vec4 v) {
	return vec4(RotateByQuaternion(tra.quat, v.xyz * tra.trSc.w) + tra.trSc.xyz * v.w, v.w);
}

Transform ApplyTransform(Transform parentTra, Transform childTra) {
	return Transform(
		MultiplyQuat(parentTra.quat, childTra.quat),
		vec4(
			parentTra.trSc.xyz + RotateByQuaternion(parentTra.quat, parentTra.trSc.w * childTra.trSc.xyz),
			parentTra.trSc.w * childTra.trSc.w
		)
	);
}

Transform InvertTransformAffine(Transform tra) {
	vec4 invR = InvertNormalizedQuaternion(tra.quat);
	float invS = 1.0 / tra.trSc.w;
	return Transform(
		invR,
		vec4(
			RotateByQuaternion(invR, -tra.trSc.xyz * invS),
			invS
		)
	);
}

mat4 TransformToMatrix(Transform tra) {
	float qxx = tra.quat.x * tra.quat.x;
	float qyy = tra.quat.y * tra.quat.y;
	float qzz = tra.quat.z * tra.quat.z;
	float qxz = tra.quat.x * tra.quat.z;
	float qxy = tra.quat.x * tra.quat.y;
	float qyz = tra.quat.y * tra.quat.z;
	float qrx = tra.quat.w * tra.quat.x;
	float qry = tra.quat.w * tra.quat.y;
	float qrz = tra.quat.w * tra.quat.z;

	mat3 rot = mat3(
		vec3(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qrz)      , 2.0 * (qxz - qry)      ),
		vec3(2.0 * (qxy - qrz)      , 1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qrx)      ),
		vec3(2.0 * (qxz + qry)      , 2.0 * (qyz - qrx)      , 1.0 - 2.0 * (qxx + qyy))
	);

	rot *= tra.trSc.w;

	return mat4(
		vec4(rot[0]      , 0.0),
		vec4(rot[1]      , 0.0),
		vec4(rot[2]      , 0.0),
		vec4(tra.trSc.xyz, 1.0)
	);
}
vec4 SLerp(vec4 qa, vec4 qb, float t) {
	// Calculate angle between them.
	float cosHalfTheta = dot(qa, qb);

	// Every rotation can be represented by two quaternions: (++++) or (----)
	// avoid taking the longer way: choose one representation
	float s = sign(cosHalfTheta);
	qb *= s;
	cosHalfTheta *= s;
	// now cosHalfTheta is >= 0.0

	// if qa and qb (or -qb originally) represent ~ the same rotation
	if (cosHalfTheta >= (1.0 - 0.005))
		return normalize(mix(qa, qb, t));

	// Interpolation of orthogonal rotations (i.e. cosHalfTheta ~ 0)
	// does not require special handling, however this usually represents
	// "physically impossible" 180 degree turns with infinite speed so perhaps
	// it can be handled in the following (currently disabled) special way
	#if 0
	if (cosHalfTheta <= 0.005)
		return mix(qa, qb, step(0.5, t));
	#endif

	float halfTheta = acos(cosHalfTheta);

	// both should be divided by sinHalfTheta (calculation skipped),
	// but it makes no sense to do it due to follow up normalization
	float ratioA = sin((1.0 - t) * halfTheta);
	float ratioB = sin((      t) * halfTheta);

	return qa * ratioA + qb * ratioB; // already normalized
}

vec4 Lerp(vec4 qa, vec4 qb, float t) {
	// Ensure shortest path
	if (dot(qa, qb) < 0.0)
		qb = -qb;
	return normalize(mix(qa, qb, t)); // GLSL's mix() = (1 - t) * qa + t * qb
}

Transform SLerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = clamp(a, 0.0, 1.0);
	return Transform(
		SLerp(t0.quat, t1.quat, a),
		mix(t0.trSc, t1.trSc, a)
	);
}


Transform Lerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = clamp(a, 0.0, 1.0);
	return Transform(
		Lerp(t0.quat, t1.quat, a),
		mix(t0.trSc, t1.trSc, a)
	);
}



// This helper function gets the transform that gets you the model-space to world-space transform
Transform GetModelWorldTransform(uint baseIndex)
{
	return Lerp(
		transforms[baseIndex + 0u],
		transforms[baseIndex + 1u],
		timeInfo.w
	);
}

// This helper function gets the transform that gets you the piece-space to model-space transform
Transform GetPieceModelTransform(uint baseIndex, uint pieceID)
{
	return Lerp(
		transforms[baseIndex + 2u * (1u + pieceID) + 0u],
		transforms[baseIndex + 2u * (1u + pieceID) + 1u],
		timeInfo.w
	);
}

// This helper function gets the transform that gets you the piece-space to world-space transform
Transform GetPieceWorldTransform(uint baseIndex, uint pieceID)
{
	Transform pieceToMModelTX =  GetPieceModelTransform(baseIndex, pieceID);

	Transform modelToWorldTX = GetModelWorldTransform(baseIndex);

	return ApplyTransform(modelToWorldTX, pieceToMModelTX);
}

Transform GetStaticPieceModelTransform(uint baseIndex, uint pieceID)
{
	return transforms[baseIndex + 1u * (pieceID) + 0u];
}

)DLORIG";
inline constexpr std::array<std::string_view,9> DefineNames={"MIERAYLEIGHRATIO","RAYMARCHSTEPS","USE3DNOISE","SURFACECOLORMODULATION","BLEEDFACTOR","VOIDWATER","SCREENSPACESHADOWS","DEPTH_CLIP01","USEQUATERNIONS"};
inline constexpr std::array<std::string_view,10> SamplerNames={"mapDepths","modelDepths","mapNormals","modelNormals","mapExtra","modelExtra","mapDiffuse","modelDiffuse","noise3DCube","blueNoise"};
struct UniformMetadataEntry {std::string_view name;unsigned glType;bool integer;float sourceDefault;};
inline constexpr std::array<UniformMetadataEntry,18> UniformMetadata={{
{"pointbeamcone",0x1406,false,0},
{"attachedtounitID",0x1406,false,0},
{"windX",0x1406,false,0},
{"windZ",0x1406,false,0},
{"radiusMultiplier",0x1406,false,1},
{"intensityMultiplier",0x1406,false,1},
{"nightFactor",0x1406,false,1},
{"screenSpaceShadows",0x1404,true,0},
{"mapDepths",0x8b5e,true,0},
{"modelDepths",0x8b5e,true,0},
{"mapNormals",0x8b5e,true,0},
{"modelNormals",0x8b5e,true,0},
{"mapExtra",0x8b5e,true,0},
{"modelExtra",0x8b5e,true,0},
{"mapDiffuse",0x8b5e,true,0},
{"modelDiffuse",0x8b5e,true,0},
{"noise3DCube",0x8b5f,true,0},
{"blueNoise",0x8b5e,true,0},
}};
}
