/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::ssao {
inline constexpr char SHADER[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"SSAO(
#include <metal_stdlib>
using namespace metal;
struct SSAOUniform {
 float4x4 cameraView,cameraProj,cameraProjInv,invProjMatrix;
 float4 samplingKernel[64];
 float4 weights[12],offsets[12];
 float4 directionStrength,alphaConfig;
 uint4 textureFlip;
 uint4 imageSwizzle[8],depthMode[2];
 float4 framebuffer; // logical/native scale.xy, native attachment height.z
};
struct SSAOVarying {float4 position [[position]];float4 vs_position_texcoords [[RECOIL_SAMPLE_PERSPECTIVE]];};
#if NORMAL_COLOR
using NormalTexture=texture2d<float>;
#else
using NormalTexture=depth2d<float>;
#endif
#if MODEL_COLOR
using ModelTexture=texture2d<float>;
#else
using ModelTexture=depth2d<float>;
#endif
#if MAP_COLOR
using MapTexture=texture2d<float>;
#else
using MapTexture=depth2d<float>;
#endif
float2 ImageUV(float2 uv,uint unit,constant SSAOUniform& u) {if(u.textureFlip[unit/4]&(1u<<(unit%4)))uv.y=1-uv.y;return uv;}
bool AlphaPass(float value,constant SSAOUniform&u){if(u.alphaConfig.x==0)return true;float reference=u.alphaConfig.y;switch(uint(u.alphaConfig.z)){case 0:return false;case 1:return value<reference;case 2:return value==reference;case 3:return value<=reference;case 4:return value>reference;case 5:return value!=reference;case 6:return value>=reference;default:return true;}}
float4 LogicalRGBA(float4 v,uint unit,constant SSAOUniform&u){return v;}
float4 LogicalRGBA(float d,uint unit,constant SSAOUniform&u){uint mode=u.depthMode[unit/4][unit%4];return mode==0x1906u?float4(1,1,1,d):mode==0x8049u?float4(d):float4(d,d,d,1);}
float SourceChannel(float4 v,uint channel){switch(channel){case 0:return 0;case 1:return 1;case 0x1903:return v.r;case 0x1904:return v.g;case 0x1905:return v.b;case 0x1906:return v.a;default:return 0;}}
float4 SourceSwizzle(float4 v,uint unit,constant SSAOUniform&u){uint4 s=u.imageSwizzle[unit];return float4(SourceChannel(v,s.x),SourceChannel(v,s.y),SourceChannel(v,s.z),SourceChannel(v,s.w));}
template<class T> float4 ReadColor(T t,sampler s,float2 uv,uint unit,constant SSAOUniform&u){return SourceSwizzle(LogicalRGBA(t.sample(s,ImageUV(uv,unit,u)),unit,u),unit,u);}
template<class T> float4 ReadColorLOD0(T t,sampler s,float2 uv,uint unit,constant SSAOUniform&u){return SourceSwizzle(LogicalRGBA(t.sample(s,ImageUV(uv,unit,u),level(0.0)),unit,u),unit,u);}
template<class T> float ReadDepthValue(T t,sampler s,float2 uv,uint unit,constant SSAOUniform&u){return ReadColorLOD0(t,s,uv,unit,u).r;}
float2 FragmentCoord(float4 position,constant SSAOUniform&u){return float2(position.x,u.framebuffer.z-position.y)*u.framebuffer.xy;}
template<class T> float ReadDepthTexel(T t,int2 pixel,uint unit,constant SSAOUniform&u){
 if(u.textureFlip[unit/4]&(1u<<(unit%4)))pixel.y=int(t.get_height())-1-pixel.y;
 return SourceSwizzle(LogicalRGBA(t.read(uint2(pixel),0),unit,u),unit,u).r;
}
float GbufferDepth(float4 position,ModelTexture modelDepthTex,MapTexture mapDepthTex,constant SSAOUniform&u){
 int2 pixel=int2(FragmentCoord(position,u));
 return min(ReadDepthTexel(modelDepthTex,pixel,1,u),ReadDepthTexel(mapDepthTex,pixel,4,u))-1e-5;
}
#ifdef TILE_CULL
float4 GatherStencil(texture2d<float> tex,sampler state,float2 uv,constant SSAOUniform&u){
 uv=ImageUV(uv,7,u);
 // textureGather(...,0) follows the actual original red-channel swizzle.
 switch(u.imageSwizzle[7].x){
  case 0:return float4(0);case 1:return float4(1);
  case 0x1903:return tex.gather(state,uv,int2(0),component::x);
  case 0x1904:return tex.gather(state,uv,int2(0),component::y);
  case 0x1905:return tex.gather(state,uv,int2(0),component::z);
  case 0x1906:return tex.gather(state,uv,int2(0),component::w);
  default:return float4(0);
 }
}
#endif
vertex SSAOVarying SSAOVertex(uint id [[vertex_id]],constant float4* vertices [[buffer(1)]]
#ifdef TILE_CULL
 ,uint instance [[instance_id]],constant float2* tiles [[buffer(2)]],constant SSAOUniform& u [[buffer(0)]]
 ,texture2d<float> unitStencilTex [[texture(7)]],sampler unitStencilTexSampler [[sampler(7)]]
#endif
){
 SSAOVarying v;float4 p=vertices[id];
#ifdef TILE_CULL
 const int gathers=TILE_TEXELS/2+2;
 float2 tile=tiles[instance];
 float2 texelSize=1.0/float2(unitStencilTex.get_width(),unitStencilTex.get_height());
 float2 first=tile*float(TILE_TEXELS)-1.0;
 float coverage=0.0;
 for(int y=0;y<gathers;++y)for(int x=0;x<gathers;++x){
  float4 texels=GatherStencil(unitStencilTex,unitStencilTexSampler,(first+2.0*float2(x,y))*texelSize,u);
  coverage=max(coverage,max(max(texels.x,texels.y),max(texels.z,texels.w)));
 }
 float2 ndc=(tile+p.xy)*(2.0*float(TILE_PX)/float2(VSX,VSY))-1.0;
 v.vs_position_texcoords=float4(ndc,ndc*0.5+0.5);
 v.position=coverage>=0.1?float4(ndc,0.0,1.0):float4(2.0,2.0,2.0,1.0);
#else
 v.position=float4(p.xy,0.0,1.0);v.vs_position_texcoords=p;
#endif
#if DEPTH_CLIP01 != 1
 v.position.z=(v.position.z+v.position.w)*0.5;
#endif
 return v;
}

#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)
float IGN(float2 p) {
	return fract(52.9829189 * fract(dot(p, float2(0.06711056, 0.00583715))));
}


float4 GetModelNormalCameraSpace(float2 uv,NormalTexture modelNormalTex,sampler modelNormalTexSampler,constant SSAOUniform& u){
	float4 modelNormal = ReadColorLOD0(modelNormalTex, modelNormalTexSampler, uv, 0, u);
	modelNormal.a = step(0.1, dot(modelNormal,modelNormal));
	modelNormal.xyz = normalize(NORM2SNORM(modelNormal.xyz));
	modelNormal.xyz = float3(u.cameraView * float4(modelNormal.xyz, 0.0));
	return modelNormal;
}


float4 GetViewPos(float2 texCoord,ModelTexture modelDepthTex,sampler modelDepthTexSampler,MapTexture mapDepthTex,sampler mapDepthTexSampler,constant SSAOUniform& u) {
	float mapDepth = ReadDepthValue(mapDepthTex, mapDepthTexSampler, texCoord, 4, u);
	float modelDepth =  ReadDepthValue(modelDepthTex, modelDepthTexSampler, texCoord, 1, u);

	float modelOccludesMap = float(modelDepth < mapDepth);
	float depth = min(mapDepth, modelDepth);

	float4 projPosition = float4(0.0, 0.0, 0.0, 1.0);

	//texture space [0;1] to NDC space [-1;1]
	#if (DEPTH_CLIP01 == 1)
		//don't transform depth as it's in the same [0;1] space
		projPosition.xyz = float3(NORM2SNORM(texCoord), depth);
	#else
		projPosition.xyz = NORM2SNORM(float3(texCoord, depth));
	#endif

	float4 viewPosition = u.cameraProjInv * projPosition;
	viewPosition /= viewPosition.w;
	viewPosition.w = modelOccludesMap;
	return viewPosition;
}


float GetViewPosZ(float2 texCoord,ModelTexture modelDepthTex,sampler modelDepthTexSampler,MapTexture mapDepthTex,sampler mapDepthTexSampler,constant SSAOUniform&u){
 float depth=min(ReadDepthValue(mapDepthTex,mapDepthTexSampler,texCoord,4,u),ReadDepthValue(modelDepthTex,modelDepthTexSampler,texCoord,1,u));
#if DEPTH_CLIP01 == 1
 float4 projPosition=float4(NORM2SNORM(texCoord),depth,1.0);
#else
 float4 projPosition=float4(NORM2SNORM(float3(texCoord,depth)),1.0);
#endif
 float z=dot(float4(u.cameraProjInv[0].z,u.cameraProjInv[1].z,u.cameraProjInv[2].z,u.cameraProjInv[3].z),projPosition);
 float w=dot(float4(u.cameraProjInv[0].w,u.cameraProjInv[1].w,u.cameraProjInv[2].w,u.cameraProjInv[3].w),projPosition);
 return z/w;
}

float4 GetFusedViewPos(float2 texCoord, float sampledDepth,constant SSAOUniform& u) {
	float4 projPosition = float4(0.0, 0.0, 0.0, 1.0);

	//texture space [0;1] to NDC space [-1;1]
	#if (DEPTH_CLIP01 == 1)
		//don't transform depth as it's in the same [0;1] space
		projPosition.xyz = float3(NORM2SNORM(texCoord), sampledDepth);
	#else
		projPosition.xyz = NORM2SNORM(float3(texCoord, sampledDepth));
	#endif

	float4 viewPosition = u.invProjMatrix * projPosition;
	viewPosition /= viewPosition.w;

	return viewPosition;
}


fragment float4 SSAOFuse(SSAOVarying in [[stage_in]],constant SSAOUniform& u [[buffer(0)]]
#if 1
,ModelTexture modelDepthTex [[texture(1)]],sampler modelDepthTexSampler [[sampler(1)]]
#endif

#if 1
,MapTexture mapDepthTex [[texture(4)]],sampler mapDepthTexSampler [[sampler(4)]]
#endif

#if USE_STENCIL == 1
,texture2d<float> unitStencilTex [[texture(7)]],sampler unitStencilTexSampler [[sampler(7)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

	
	float2 uv = FragmentCoord(in.position,u) * float2(1.0/VSX, 1.0/VSY);
	//float2 uv = gl_TexCoord[0].xy * float2(2,-2) + float2(0,2.0);
	//fragColor = float4(uv.xy, 0.0, 1.0); if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
	#if USE_STENCIL == 1 
		if (ReadColorLOD0(unitStencilTex, unitStencilTexSampler, uv, 7, u).r < 0.1) {
			fragColor = float4(0,0,0,0) ; 
			if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
		}
	#endif

	float modelDepth = ReadDepthValue(modelDepthTex, modelDepthTexSampler, uv, 1, u);
	float mapDepth = ReadDepthValue(mapDepthTex, mapDepthTexSampler, uv, 4, u);

	float modelOccludesMap = float(modelDepth < mapDepth);
	float depth = mix(mapDepth, modelDepth, modelOccludesMap);

	float4 viewPosition = GetFusedViewPos(uv, depth,u);

	if (modelOccludesMap < 0.5) viewPosition.z *= -1.0;
	fragColor.xyz = viewPosition.xyz;


if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
fragment float4 SSAOSample(SSAOVarying in [[stage_in]],constant SSAOUniform& u [[buffer(0)]]
#if 1
,NormalTexture modelNormalTex [[texture(0)]],sampler modelNormalTexSampler [[sampler(0)]]
#endif

#if NOFUSE == 1
,ModelTexture modelDepthTex [[texture(1)]],sampler modelDepthTexSampler [[sampler(1)]]
#endif

#if NOFUSE == 1
,MapTexture mapDepthTex [[texture(4)]],sampler mapDepthTexSampler [[sampler(4)]]
#endif

#if NOFUSE == 0
,texture2d<float> viewPosTex [[texture(5)]],sampler viewPosTexSampler [[sampler(5)]]
#endif

#if USE_STENCIL == 1
,texture2d<float> unitStencilTex [[texture(7)]],sampler unitStencilTexSampler [[sampler(7)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;


	float2 texelSize = 0.5 / float2(VSX,VSY);
	float2 uv = FragmentCoord(in.position,u) * DOWNSAMPLE / float2(VSX,VSY);

	#if USE_STENCIL == 1 
		if (ReadColorLOD0(unitStencilTex, unitStencilTexSampler, uv, 7, u).r < 0.1) { discard_fragment(); }
	#endif

	#if NOFUSE == 1 
		float4 viewNormalSample = GetModelNormalCameraSpace(uv,modelNormalTex,modelNormalTexSampler,u);
		float validFragment =viewNormalSample.a;
		float3 viewNormal = viewNormalSample.xyz * validFragment;

		float4 viewPosition = GetViewPos(uv,modelDepthTex,modelDepthTexSampler,mapDepthTex,mapDepthTexSampler,u) ;
		validFragment *= viewPosition.w;

	#else
		float4 viewPosition = float4( ReadColorLOD0(viewPosTex, viewPosTexSampler, uv, 5, u).xyz, 1.0 );
		float4 viewNormalSample = GetModelNormalCameraSpace(uv,modelNormalTex,modelNormalTexSampler,u);
		float3 viewNormal = viewNormalSample.xyz;
		float validFragment = viewNormalSample.a * step(viewPosition.z,0.0);
	#endif

	//fragColor = float4(fract(viewPosition.xyz * 0.02) , 1.0); if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
	//--------------------------- DEBUG---------------

	float4 collectedNormal = float4(viewNormal, 1.0);
	float2 collectedDistance = float2(abs(viewPosition.z), 1.0);
	float fragDistFactor = smoothstep( SSAO_FADE_DIST_0 * 1.0, SSAO_FADE_DIST_1 * 1.0, -viewPosition.z );
	//gl_FragColor = float4(fract(viewPosition.xyz * 0.1), 1.0); if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;

	//fragColor = float4(SNORM2NORM(normalize(collectedNormal.xy)), (abs(viewPosition.z) / (SSAO_FADE_DIST_0)), 1.0); if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
	
	// Indicate that we are sampling a ground position, so pass zero vector as normal in RG
	// Also pass distance in B channel, and no occlusion (1.0) in alpha
	fragColor = float4(float2(0.5), (abs(viewPosition.z) / (SSAO_FADE_DIST_0)), 1.0);

	if ( dot(viewNormal, viewNormal) > 0.1 && fragDistFactor > 0.0 && (validFragment > 0.5) ) {
		// Calculate the rotation matrix for the kernel.
		// IGN gives a structured 2D rotation phase per pixel (much better blur convergence than white-noise hash).
		// We construct an in-plane random direction and combine with a stable z lift to avoid degeneracy.
		float ignPhase = IGN(FragmentCoord(in.position,u)) * 6.28318530718;
		float3 randomVector = normalize(float3(cos(ignPhase), sin(ignPhase), 0.5));

		// Using Gram-Schmidt process to get an orthogonal vector to the normal vector.
		// The resulting tangent is on the same plane as the random and normal vector.
		// see http://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
		// Note: No division by <u,u> needed, as this is for normal vectors 1.
		float3 viewTangent = normalize(randomVector - dot(randomVector, viewNormal) * viewNormal);

		float3 viewBitangent = cross(viewNormal, viewTangent);

		// Final matrix to reorient the kernel depending on the normal and the random vector.
		// TBN matrix. Transforms from tangent space to view space
		float3x3 kernelMatrix = float3x3(viewTangent, viewBitangent, viewNormal);

		// Go through the kernel samples and create occlusion factor.
		float occlusion = 0.0; // higher numbers mean more occlusion

		for (int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
			// silly resample:
			#if ((OFFSET == 1) && (DOWNSAMPLE> 1 ))
				if ( (i % (SSAO_KERNEL_SIZE/4)) == (SSAO_KERNEL_SIZE/4 - 1) ){
					float2 newUV = uv;
					if (i == SSAO_KERNEL_SIZE/4 - 1) newUV.x += texelSize.x;
					
					if (i == 2* SSAO_KERNEL_SIZE/4 - 1) newUV.y += texelSize.y;
					
					if (i == 3 * SSAO_KERNEL_SIZE/4 - 1) newUV.xy += texelSize.xy;

					viewNormalSample = GetModelNormalCameraSpace(newUV,modelNormalTex,modelNormalTexSampler,u);
					if (dot(viewNormalSample.xyz, viewNormalSample.xyz) > 0.1  && viewNormalSample.a > 0.5){
						viewNormal = viewNormalSample.xyz;
						#if NOFUSE == 1 
							viewPosition = GetViewPos(uv,modelDepthTex,modelDepthTexSampler,mapDepthTex,mapDepthTexSampler,u) ;
						#else
							viewPosition = float4( ReadColorLOD0(viewPosTex, viewPosTexSampler, newUV, 5, u).xyz, 1.0 );
						#endif
						viewTangent = normalize(randomVector - dot(randomVector, viewNormal) * viewNormal);
						viewBitangent = cross(viewNormal, viewTangent);
						kernelMatrix = float3x3(viewTangent, viewBitangent, viewNormal);
					}
					collectedNormal+= float4(viewNormal, 1.0);
					collectedDistance += float2(abs(viewPosition.z), 1.0);
				}
			#endif

			// Reorient sample vector in view space ...
			float3 viewSampleVector = kernelMatrix * u.samplingKernel[i].xyz;

			// ... and calculate sample point.
			float4 viewTestPosition = viewPosition + SSAO_RADIUS * float4(viewSampleVector, 0.0);

			// projection
			float4 ndcTestPosition = u.cameraProj * viewTestPosition;
			// perspecitive division
			ndcTestPosition /= ndcTestPosition.w;

			// [-1;1] to [0;1]
			float2 texSampingPoint = SNORM2NORM(ndcTestPosition.xy);

			// Get sample viewPos from the viewPosTex texture
			float viewPositionSampledZ;
			#if NOFUSE == 1 
				viewPositionSampledZ = GetViewPosZ(texSampingPoint,modelDepthTex,modelDepthTexSampler,mapDepthTex,mapDepthTexSampler,u);
			#else
				viewPositionSampledZ = -1 * abs(ReadColorLOD0(viewPosTex, viewPosTexSampler, texSampingPoint, 5, u).z);
			#endif
			// Delta is how much deeper our ray is compared to sample, in elmos
			// negative numbers mean ray is in front of sample, no occulsion
			// positive means ray is behind sample, thus occlusion
			float delta = viewPositionSampledZ - viewTestPosition.z;

			float occlusionCondition = float(delta >= SSAO_MIN); 
			// longer rays should occlude less 
			float myraylen = 1;//1.3 - dot(viewSampleVector, viewSampleVector); // 1-0;

			// smaller delta hits should occlude less
			float occlDelta = smoothstep(0,1,delta);

			// hits further thay the rays length shouldnt occlude either

			float toofar = 1.0 - smoothstep (SSAO_RADIUS * 0.75, SSAO_RADIUS * 1.25, delta);

			occlusionCondition *= myraylen * occlDelta *toofar;
			// old method:
			//float occlusionCondition = float(delta >= SSAO_MIN) * (1.0 - smoothstep(SSAO_RADIUS * 0.75, SSAO_RADIUS * 1.0, delta) );

			occlusion += occlusionCondition;
		}

		occlusion *= fragDistFactor ; //more distance fragments are less occluded

		// TODO: Shadow Density!

		occlusion = occlusion / float(SSAO_KERNEL_SIZE);// normalize by sample count

		float fullylit = clamp(1.0 - occlusion, 0.0, 1.0);

		fullylit = pow(fullylit, SSAO_OCCLUSION_POWER);

		// Jimenez multi-bounce AO approximation (Activision GDC 2016, "Practical Realtime Strategies for
		// Accurate Indirect Occlusion"). Pre-baked for an average albedo of 0.95 since this widget outputs
		// scalar AO that gets multiplied into deferred lighting downstream. The curve mostly affects the
		// dark end: it lifts already-dark crevices back up to account for indirect light bouncing back into
		// them, so AO no longer over-darkens regions that physical light would partially fill in.
		// Coefficients: a = 2.0404*albedo - 0.3324; b = -4.7951*albedo + 0.6417; c = 2.7552*albedo + 0.6903.
		fullylit = max(fullylit, ((fullylit * 1.6060 - 3.9136) * fullylit + 3.3077) * fullylit);
		fullylit = clamp(fullylit, 0.0, 1.0);

		#if DEBUG_SSAO == 1
			fragColor = float4(float3( fullylit ), 1.0 );
			if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
		#endif
		// Finally, we pass the normalized XY coords of the normals packed into RG
		// We also pass the distance of the fragment up to far distance in B channel
		// And pass the occlusion level in alpha
		collectedNormal.xyz /= collectedNormal.w;
		collectedDistance.x /= collectedDistance.y;
		fragColor = float4(float3(SNORM2NORM(normalize(collectedNormal.xy)), (abs(collectedDistance.x )/ (SSAO_FADE_DIST_0)) ), fullylit);
	}

if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
struct SSAOCompositeResult {float4 color [[color(0)]];float depth [[depth(any)]];};
#if BLUR_COMPOSITE == 1
fragment SSAOCompositeResult SSAOBlur(
#else
fragment float4 SSAOBlur(
#endif
SSAOVarying in [[stage_in]],constant SSAOUniform& u [[buffer(0)]]
#if 1
,texture2d<float> tex [[texture(0)]],sampler texSampler [[sampler(0)]]
#endif

#if BLUR_COMPOSITE == 1
,ModelTexture modelDepthTex [[texture(1)]]
,MapTexture mapDepthTex [[texture(4)]]
#endif

#if USE_STENCIL == 1
,texture2d<float> unitStencilTex [[texture(7)]],sampler unitStencilTexSampler [[sampler(7)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

	//float2 uv = FragmentCoord(in.position,u) / viewPortSize;

	float2 uv = FragmentCoord(in.position,u) / float2(HSX,HSY);


	#if USE_STENCIL == 1 
		if (ReadColorLOD0(unitStencilTex, unitStencilTexSampler, uv, 7, u).r < 0.1) {
			#if (BLUR_COMPOSITE == 1) && (DEBUG_BLUR == 1)
            fragColor=float4(0.0,0.0,0.0,1.0);
            if(!AlphaPass(fragColor.a,u))discard_fragment();
            return {fragColor,GbufferDepth(in.position,modelDepthTex,mapDepthTex,u)};
            #else
            discard_fragment();
            #endif
		}
	#endif

		// Allrighty, this is a completely novel implementation now, that will attempt to use a bilateral weighting!
		// tex contains normals, alpha contains occlusion
		// We will weight individual samples by dot-ing them.
		//new bilateral filter implementation

		// TODO: ground has null vector normals, use that
		// implement outlier detection too 
		// MORE GROUND EFFECT! USE UNIFORM KERNEL FOR GROUND!

		float4 texSample = ReadColorLOD0(tex, texSampler, uv , 0, u);
		float3 myNormal = NORM2SNORM(texSample.rgb);
		myNormal.z = texSample.z;

		float imground =step(dot(myNormal.xy, myNormal.xy), 0.1);
		
		float weightSum = u.weights[0].x;
		if (imground > 0.5) {
			//weightSum = 1.0/(2 * BLUR_HALF_KERNEL_SIZE-1);
		}
		float howLit = texSample.a * weightSum;
		float unWeighted = howLit;
		float myDistance = myNormal.z; // 1 at full distance, 0 at camera
		float2 texelOffset = u.directionStrength.xy / float2(HSX,HSY);

		// possibly better L1 cache locality via non expanding stepping?
		for (int i = 1; i < (BLUR_HALF_KERNEL_SIZE  ); ++i) { // i goes from 1 to BLUR_HALF_KERNEL_SIZE-1
			float2 uvOff = u.offsets[i].x * texelOffset ;
			float2 uvP = uv + uvOff;
			float2 uvN = uv - uvOff;

			float weightP = u.weights[i].x;
			float weightN = u.weights[i].x;
			if (imground > 0.5) {
				//weightP = 1.0/(2 * BLUR_HALF_KERNEL_SIZE-1);
				//weightN = 1.0/(2* BLUR_HALF_KERNEL_SIZE-1);
				//uvP = uv + 2*float(i) *  u.directionStrength.xy / float2(HSX,HSY);
				//uvN = uv - 2*float(i) *  u.directionStrength.xy / float2(HSX,HSY);
			}


			float4 leftSample = ReadColorLOD0(tex, texSampler, uvP , 0, u);
			float zclosel = step(abs(myDistance - leftSample.z), ZTHRESHOLD);
			float dotclosel = smoothstep(MINCOSANGLE, 1.0, dot(myNormal.xy, NORM2SNORM(leftSample.xy)));
			dotclosel = max(dotclosel, imground);
			float leftWeight = weightP * dotclosel * zclosel;
			weightSum += leftWeight;
			howLit += leftWeight * leftSample.a;
			unWeighted += weightP * leftSample.a;

			float4 rightSample = ReadColorLOD0(tex, texSampler, uvN , 0, u);
			float zcloser = step(abs(myDistance - rightSample.z), ZTHRESHOLD);
			float dotcloser = smoothstep(MINCOSANGLE, 1.0, dot(myNormal.xy, NORM2SNORM(rightSample.xy)));
			dotcloser = max(dotcloser, imground);
			float rightWeight = weightN * dotcloser * zcloser;
			weightSum += rightWeight;
			howLit += rightWeight * rightSample.a;
			unWeighted += weightP * rightSample.a;
		}

		howLit = howLit / weightSum;		
		if (weightSum < (u.weights[0].x + MINSELFWEIGHT) ){
			howLit = mix(howLit,unWeighted, OUTLIERCORRECTIONFACTOR);

		};
		//if (imground > 0.5) {howLit = unWeighted;}
		fragColor = float4(texSample.rgb, howLit);

		// FINAL MIXDOWN, vert pass last
		if (u.directionStrength.xy.y > 0.5) { 
			if (imground > 0.5) howLit = howLit * howLit;
			howLit = pow(howLit, BLUR_POWER);
			howLit = sqrt(howLit * howLit + BLUR_CLAMP);
			#if DEBUG_BLUR == 1 
				fragColor.rgba = float4(howLit);
			#else
				fragColor.rgba = float4(howLit);
				#if (BRIGHTEN != 0) 
					if (imground < 0.5) {  // is model fragment
						float distRatio = SSAO_FADE_DIST_1/(1.0 * SSAO_FADE_DIST_0);
						float distFactor = 1.0 - smoothstep(0, distRatio, myDistance - distRatio ); // 0 at far distance, 1 at near distance

						float brightenFactor = (BRIGHTEN/255.0) * howLit * distFactor;
						fragColor.rgb = float3(brightenFactor);
					}else{
						fragColor.rgb = float3(0);

					}
				#endif
				fragColor.rgb *= u.directionStrength.z;
				fragColor.a = 1.0 - ((1.0 - fragColor.a) * u.directionStrength.z );
			#endif

		};

#if BLUR_COMPOSITE == 1
 #if DEBUG_BLUR == 0
  float4 stored=round(clamp(fragColor,0.0,1.0)*255.0);
  #if BRIGHTEN != 0
   if(all(stored==float4(0.0,0.0,0.0,255.0)))discard_fragment();
  #else
   if(stored.a==255.0)discard_fragment();
  #endif
  fragColor=stored/255.0;
 #endif
 if(!AlphaPass(fragColor.a,u))discard_fragment();
 return {fragColor,GbufferDepth(in.position,modelDepthTex,mapDepthTex,u)};
#else
 if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
#endif
}
fragment SSAOCompositeResult SSAOComposite(SSAOVarying in [[stage_in]],constant SSAOUniform& u [[buffer(0)]]
#if 1
,texture2d<float> tex [[texture(0)]],sampler texSampler [[sampler(0)]]
#endif

#if 1
,ModelTexture modelDepthTex [[texture(1)]],sampler modelDepthTexSampler [[sampler(1)]]
#endif

#if 1
,MapTexture mapDepthTex [[texture(4)]],sampler mapDepthTexSampler [[sampler(4)]]
#endif

#if (DOWNSAMPLE > 1) && (NOFUSE == 0)
,texture2d<float> viewPosTex [[texture(5)]],sampler viewPosTexSampler [[sampler(5)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

	float2 uv = FragmentCoord(in.position,u) * (1.0-float2(TEXPADDINGX,TEXPADDINGY)/float2(VSX,VSY))/float2(VSX,VSY);

#if (DOWNSAMPLE > 1) && (NOFUSE == 0)
	// ---- Joint bilateral upsample path (half-res -> full-res) ----
	float refZ = ReadColorLOD0(viewPosTex, viewPosTexSampler, uv, 5, u).z;

	float2 halfTexel = 0.5 / float2(HSX, HSY);
	const float2 OFFS[4] = {
		float2(-1.0, -1.0),
		float2( 1.0, -1.0),
		float2(-1.0,  1.0),
		float2( 1.0,  1.0)
	};

	float2 hres = float2(HSX, HSY);
	float2 frac_ = fract(uv * hres - 0.5);
	float bw[4];
	bw[0] = (1.0 - frac_.x) * (1.0 - frac_.y);
	bw[1] =        frac_.x  * (1.0 - frac_.y);
	bw[2] = (1.0 - frac_.x) *        frac_.y;
	bw[3] =        frac_.x  *        frac_.y;

	float zScale = 1.0 / max(1.0, abs(refZ) * 0.05);

	float4 acc = float4(0.0);
	float wSum = 0.0;
	for (int i = 0; i < 4; i++) {
		float2 sUV = uv + OFFS[i] * halfTexel;
		float sZ = ReadColorLOD0(viewPosTex, viewPosTexSampler, sUV, 5, u).z;
		float w = bw[i] * exp(-abs(sZ - refZ) * zScale);
		acc  += w * ReadColorLOD0(tex, texSampler, sUV, 0, u);
		wSum += w;
	}
	fragColor = (wSum < 1e-4) ? ReadColorLOD0(tex, texSampler, uv, 0, u) : (acc / wSum);
#else
	// ---- Single tap (full-res, or no fuse texture available) ----
	fragColor = ReadColorLOD0(tex, texSampler, uv, 0, u);
#endif

#if (DEBUG_SSAO == 0) && (DEBUG_BLUR == 0)
 #if BRIGHTEN != 0
  if(all(fragColor==float4(0.0,0.0,0.0,1.0)))discard_fragment();
 #else
  if(fragColor.a==1.0)discard_fragment();
 #endif
#endif

	// Depth-rejection mask via gl_FragDepth + LEQUAL test.
	// Take the closest of model/map gbuffer depths (the surface SSAO was
	// computed against) and write it as our fragment depth. With LEQUAL,
	// any grass/foliage/particle drawn in front (smaller FB depth) will
	// cause the test to fail and the SSAO contribution to be discarded.
	int2 pixel=int2(FragmentCoord(in.position,u));
	float dM = ReadDepthTexel(modelDepthTex,pixel,1,u);
	float dG = ReadDepthTexel(mapDepthTex,pixel,4,u);
	float gbufDepth = min(dM, dG);

	// Tiny bias compensates for any FP imprecision between the gbuffer
	// pass and the live framebuffer depth attachment. Smaller is safer
	// against false-positives (i.e. SSAO leaking through grass), larger
	// is safer against false-negatives (i.e. SSAO not appearing on the
	// original surface). 1e-5 is well below grass thickness in NDC.
	float fragmentDepth = gbufDepth - 1e-5;

if(!AlphaPass(fragColor.a,u))discard_fragment();return {fragColor, fragmentDepth};
}
fragment float4 SSAOTexRect(SSAOVarying in [[stage_in]],constant SSAOUniform& u [[buffer(0)]],texture2d<float> tex [[texture(0)]],sampler texSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));float4 c=ReadColor(tex,texSampler,in.vs_position_texcoords.zw,0,u);if(!AlphaPass(c.a,u))discard_fragment();return c;}
)SSAO";
}
