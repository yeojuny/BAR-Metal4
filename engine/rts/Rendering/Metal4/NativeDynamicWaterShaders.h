/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Generated ONLY from the fifteen pinned CDynWater ARB sources. See tools/translate_pinned_shaders.py.
namespace rendering::metal4 {
inline constexpr const char* NativeDynamicWaterShaders = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct DynamicVertex {packed_float3 position;packed_float2 uv;packed_float3 normal;};
struct DynamicUniforms {float4x4 mvp,shadow;float4 ve[19],fe[19];float4 fogColor,fogParameters,source;uint4 flags;uint4 channels[8];};
struct DynamicVarying {float4 p [[position]];float4 tc0 [[RECOIL_SAMPLE_PERSPECTIVE]],tc1 [[RECOIL_SAMPLE_PERSPECTIVE]],tc2 [[RECOIL_SAMPLE_PERSPECTIVE]],tc3 [[RECOIL_SAMPLE_PERSPECTIVE]],tc4 [[RECOIL_SAMPLE_PERSPECTIVE]],tc5 [[RECOIL_SAMPLE_PERSPECTIVE]],tc6 [[RECOIL_SAMPLE_PERSPECTIVE]],tc7 [[RECOIL_SAMPLE_PERSPECTIVE]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float fog [[RECOIL_SAMPLE_PERSPECTIVE]];};
float4 Row(float4x4 m,uint r){return float4(m[0][r],m[1][r],m[2][r],m[3][r]);}
float2 Flip(float2 uv,uint mask,uint slot){if(mask&(1u<<slot))uv.y=1.0f-uv.y;return uv;}
float OriginalChannel(float4 value,uint channel){return channel<4?value[channel]:(channel==5?1.0f:0.0f);}
float4 OriginalChannels(float4 value,uint4 channels){return float4(OriginalChannel(value,channels.x),OriginalChannel(value,channels.y),OriginalChannel(value,channels.z),OriginalChannel(value,channels.w));}
float4 OriginalSample(texture2d<float> image,sampler sampling,float2 uv,constant DynamicUniforms& u,uint slot){
 if(u.flags.w&(1u<<slot))return float4(0,0,0,1);
 return OriginalChannels(image.sample(sampling,uv),u.channels[slot]);
}
float4 OriginalSampleLevel(texture2d<float> image,sampler sampling,float2 uv,constant DynamicUniforms& u,uint slot){
 if(u.flags.w&(1u<<slot))return float4(0,0,0,1);
 return OriginalChannels(image.sample(sampling,uv,level(0.0f)),u.channels[slot]);
}
fragment float4 dwAddSplashFragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 temp;
	float4 temp2;
	float4 temp3;
	temp = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX temp, fragment.texcoord[0], texture[0], 2D
	color = ((temp * in.tc1)); // MUL result.color, temp, fragment.texcoord[1]
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	color.rgb=mix(u.flags.y?clamp(u.fogColor.rgb,0.0f,1.0f):u.fogColor.rgb,color.rgb,clamp((u.fogParameters.y-in.fog)*u.fogParameters.w,0.0f,1.0f));
	return color;
}

vertex DynamicVarying dwAddSplashVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	float4 pos;
	pos = (((float4(float3(v.position),1.0f)).xzyw * u.ve[10] + u.ve[11])); // MAD pos, ipos.xzyw, program.env[10], program.env[11]
	out.tc1 = (float4(float3(v.normal),1.0f)); // MOV result.texcoord[1], vertex.normal
	out.p.x = (float4(dot((pos).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((pos).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((pos).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((pos).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	out.tc0 = (float4(float2(v.uv),0.0f,1.0f)); // MOV result.texcoord[0], vertex.texcoord[0]
	out.tc1.w = (float4((float4(float3(v.normal),1.0f)).z)).w; // MOV result.texcoord[1].w, vertex.normal.z
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 dwDetailNormalFragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 temp;
	float4 temp2;
	temp = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX temp, fragment.texcoord[0], texture[0], 2D
	temp2 = ((temp * u.fe[0])); // MUL temp2, temp, program.env[0]
	temp = (OriginalSample(t1,s1,Flip((in.tc1).xy,u.flags.x,1),u,1)); // TEX temp, fragment.texcoord[1], texture[1], 2D
	temp2 = ((temp * u.fe[1] + temp2)); // MAD temp2, temp, program.env[1],temp2
	temp = (OriginalSample(t2,s2,Flip((in.tc2).xy,u.flags.x,2),u,2)); // TEX temp, fragment.texcoord[2], texture[2], 2D
	temp2 = ((temp * u.fe[2] + temp2)); // MAD temp2, temp, program.env[2],temp2
	temp = (OriginalSample(t3,s3,Flip((in.tc3).xy,u.flags.x,3),u,3)); // TEX temp, fragment.texcoord[3], texture[3], 2D
	temp2 = ((temp * u.fe[3] + temp2)); // MAD temp2, temp, program.env[3],temp2
	temp = (OriginalSample(t4,s4,Flip((in.tc4).xy,u.flags.x,4),u,4)); // TEX temp, fragment.texcoord[4], texture[4], 2D
	temp2 = ((temp * u.fe[4] + temp2)); // MAD temp2, temp, program.env[4],temp2
	temp = (OriginalSample(t5,s5,Flip((in.tc5).xy,u.flags.x,5),u,5)); // TEX temp, fragment.texcoord[5], texture[5], 2D
	temp2 = ((temp * u.fe[5] + temp2)); // MAD temp2, temp, program.env[5],temp2
	temp = (OriginalSample(t6,s6,Flip((in.tc6).xy,u.flags.x,6),u,6)); // TEX temp, fragment.texcoord[6], texture[6], 2D
	temp2 = ((temp * u.fe[6] + temp2)); // MAD temp2, temp, program.env[6],temp2
	temp = (OriginalSample(t7,s7,Flip((in.tc7).xy,u.flags.x,7),u,7)); // TEX temp, fragment.texcoord[7], texture[7], 2D
	temp2 = ((temp * u.fe[7] + temp2)); // MAD temp2, temp, program.env[7],temp2
	color = (temp2); // MOV result.color, temp2
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	color.rgb=mix(u.flags.y?clamp(u.fogColor.rgb,0.0f,1.0f):u.fogColor.rgb,color.rgb,clamp((u.fogParameters.y-in.fog)*u.fogParameters.w,0.0f,1.0f));
	return color;
}

vertex DynamicVarying dwDetailNormalVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	out.p.x = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[10]).xyz))); // DP3 temp, vertex.texcoord, program.env[10]
	out.tc0 = ((float4((u.ve[10]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[0], program.env[10].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[11]).xyz))); // DP3 temp, vertex.texcoord, program.env[11]
	out.tc1 = ((float4((u.ve[11]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[1], program.env[11].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[12]).xyz))); // DP3 temp, vertex.texcoord, program.env[12]
	out.tc2 = ((float4((u.ve[12]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[2], program.env[12].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[13]).xyz))); // DP3 temp, vertex.texcoord, program.env[13]
	out.tc3 = ((float4((u.ve[13]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[3], program.env[13].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[14]).xyz))); // DP3 temp, vertex.texcoord, program.env[14]
	out.tc4 = ((float4((u.ve[14]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[4], program.env[14].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[15]).xyz))); // DP3 temp, vertex.texcoord, program.env[15]
	out.tc5 = ((float4((u.ve[15]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[5], program.env[15].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[16]).xyz))); // DP3 temp, vertex.texcoord, program.env[16]
	out.tc6 = ((float4((u.ve[16]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[6], program.env[16].w, program.env[9].x, temp
	temp = (float4(dot((float4(float2(v.uv),0.0f,1.0f)).xyz,(u.ve[17]).xyz))); // DP3 temp, vertex.texcoord, program.env[17]
	out.tc7 = ((float4((u.ve[17]).w) * float4((u.ve[9]).x) + temp)); // MAD result.texcoord[7], program.env[17].w, program.env[9].x, temp
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 waterDynFragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],depth2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 temp;
	float4 camdir;
	float4 reflectdir;
	float4 waterNormal;
	float4 angle;
	float4 reflectColor;
	float4 refractdir;
	float4 refractColor;
	float4 clearcolor;
	float4 specularvalue;
	float4 groundHeight;
	float4 refractMul;
	float4 foamColor;
	float4 lightColor;
	float4 foamIntensity;
	float4 detailNormal;
	float4 shadow;
	float4 camdist;
	float4 waterNormal2;
	float4 angle2;
	float4 reflectdir2;
	shadow = (OriginalChannels(float4(t7.sample_compare(s7,Flip((in.tc1).xy,u.flags.x,7),u.flags.z ? clamp((in.tc1).z,0.0f,1.0f) : (in.tc1).z)),u.channels[7])); // TEX shadow, fragment.texcoord[1], texture[7], SHADOW2D
	camdir = ((in.tc0 - u.fe[1])); // SUB camdir, fragment.texcoord, program.env[1]
	temp.x = (float4(dot((camdir).xyz,(camdir).xyz))).x; // DP3 temp.x, camdir, camdir
	temp.x = (float4(rsqrt(abs((float4((temp).x)).x)))).x; // RSQ temp.x, temp.x
	camdir = ((camdir * float4((temp).x))); // MUL camdir, camdir, temp.x
	camdist.x = (float4(1.0f/(float4((temp).x)).x)).x; // RCP camdist.x, temp.x
	waterNormal = (OriginalSample(t0,s0,Flip((in.tc2).xy,u.flags.x,0),u,0)); // TEX waterNormal, fragment.texcoord[2], texture[0], 2D
	foamIntensity.x = (clamp((float4((waterNormal).w) * float4(5.0f) + float4(-1.0f)),0.0f,1.0f)).x; // MAD_SAT foamIntensity.x, waterNormal.w, 5, -1
	groundHeight = (OriginalSample(t4,s4,Flip((in.tc4).xy,u.flags.x,4),u,4)); // TEX groundHeight, fragment.texcoord[4], texture[4], 2D
	groundHeight.x = ((float4((groundHeight).w) * float4(0.4f) + float4(-0.4f))).x; // MAD groundHeight.x, groundHeight.a, 0.4, -0.4
	temp = (OriginalSample(t5,s5,Flip((in.tc3).xy,u.flags.x,5),u,5)); // TEX temp, fragment.texcoord[3], texture[5], 2D
	foamIntensity.x = ((float4((foamIntensity).x) * float4((temp).x))).x; // MUL foamIntensity.x , foamIntensity.x, temp.x
	detailNormal = (OriginalSample(t6,s6,Flip((in.tc3).xy,u.flags.x,6),u,6)); // TEX detailNormal, fragment.texcoord[3], texture[6], 2D
	waterNormal = (((detailNormal).xzyw * float4(1.0f,0.0f,1.0f,0.0f) + waterNormal)); // MAD waterNormal, detailNormal.xzyw, {1,0,1,0}, waterNormal
	temp.x = (float4(dot((waterNormal).xyz,(waterNormal).xyz))).x; // DP3 temp.x, waterNormal, waterNormal
	temp.x = (float4(rsqrt(abs((float4((temp).x)).x)))).x; // RSQ temp.x, temp.x
	waterNormal = ((waterNormal * float4((temp).x))); // MUL waterNormal, waterNormal, temp.x
	angle.x = (float4(dot((camdir).xyz,(waterNormal).xyz))).x; // DP3 angle.x, camdir, waterNormal
	temp.x = ((float4((angle).x) * float4(-2.0f))).x; // MUL temp.x, angle.x, -2
	reflectdir = ((waterNormal * float4((temp).x) + camdir)); // MAD reflectdir, waterNormal, temp.x, camdir
	specularvalue.x = (clamp(float4(dot((reflectdir).xyz,(u.fe[9]).xyz)),0.0f,1.0f)).x; // DP3_SAT specularvalue.x, reflectdir, program.env[9]
	temp = (float4(pow(abs((float4((specularvalue).x)).x),(float4((float4(16.0f,0.0f,0.0f,0.0f)).x)).x))); // POW temp, specularvalue.x, {16,0,0,0}.x
	specularvalue = (float4(pow(abs((float4((specularvalue).x)).x),(float4((float4(4096.0f,0.0f,0.0f,0.0f)).x)).x))); // POW specularvalue, specularvalue.x, {4096,0,0,0}.x
	specularvalue = ((specularvalue * float4(10.0f))); // MUL specularvalue, specularvalue, 10
	specularvalue = ((specularvalue * float4((shadow).x))); // MUL specularvalue, specularvalue, shadow.x
	shadow.x = ((float4(0.3f) * float4(1.0f) + (float4(1.0f)-float4(0.3f)) * float4((shadow).x))).x; // LRP shadow.x, 0.3, 1, shadow.x
	temp = ((temp * float4((shadow).x))); // MUL temp, temp, shadow.x
	specularvalue = ((temp * float4(0.5f) + specularvalue)); // MAD specularvalue, temp, 0.5, specularvalue
	waterNormal2 = ((float4((camdist).x) * float4(0.0f,0.003f,0.0f,0.0f) + waterNormal)); // MAD waterNormal2, camdist.x, {0,0.003,0,0}, waterNormal
	temp.x = (float4(dot((waterNormal2).xyz,(waterNormal2).xyz))).x; // DP3 temp.x, waterNormal2, waterNormal2
	temp.x = (float4(rsqrt(abs((float4((temp).x)).x)))).x; // RSQ temp.x, temp.x
	waterNormal2 = ((waterNormal2 * float4((temp).x))); // MUL waterNormal2, waterNormal2, temp.x
	angle2.x = (float4(dot((camdir).xyz,(waterNormal2).xyz))).x; // DP3 angle2.x, camdir, waterNormal2
	temp.x = ((float4((angle2).x) * float4(-2.0f))).x; // MUL temp.x, angle2.x, -2
	reflectdir2 = ((waterNormal2 * float4((temp).x) + camdir)); // MAD reflectdir2, waterNormal2, temp.x, camdir
	reflectdir2.y = (max(float4((reflectdir2).y),float4(0.0f))).y; // MAX reflectdir2.y, reflectdir2.y, 0
	temp.x = (float4(dot((reflectdir2).xyz,(u.fe[5]).xyz))).x; // DP3 temp.x, reflectdir2, program.env[5]
	temp.x = (float4(1.0f/(float4((temp).x)).x)).x; // RCP temp.x, temp.x
	reflectdir2 = ((reflectdir2 * float4((temp).x))); // MUL reflectdir2, reflectdir2, temp.x
	temp.x = (float4(dot((reflectdir2).xyz,(u.fe[2]).xyz))).x; // DP3 temp.x, reflectdir2, program.env[2]
	temp.y = (float4(dot((reflectdir2).xyz,(u.fe[3]).xyz))).y; // DP3 temp.y, reflectdir2, program.env[3]
	temp = ((temp * u.fe[4] + float4(0.5f,0.5f,0.0f,0.0f))); // MAD temp, temp, program.env[4], {0.5, 0.5, 0, 0}
	reflectColor = (OriginalSample(t1,s1,Flip((temp).xy,u.flags.x,1),u,1)); // TEX reflectColor, temp, texture[1], 2D
	reflectColor = ((specularvalue * u.fe[10] + reflectColor)); // MAD reflectColor, specularvalue, program.env[10], reflectColor
	refractMul = ((float4((groundHeight).x) * float4(1.0f,0.0f,1.0f,0.0f))); // MUL refractMul, groundHeight.x, {1,0,1,0}
	refractdir = ((waterNormal2 * refractMul + camdir)); // MAD refractdir,waterNormal2, refractMul,camdir
	temp.x = (float4(dot((refractdir).xyz,(u.fe[15]).xyz))).x; // DP3 temp.x, refractdir, program.env[15]
	temp.x = (float4(1.0f/(float4((temp).x)).x)).x; // RCP temp.x, temp.x
	refractdir = ((refractdir * float4((temp).x))); // MUL refractdir, refractdir, temp.x
	temp.x = (float4(dot((refractdir).xyz,(u.fe[12]).xyz))).x; // DP3 temp.x, refractdir, program.env[12]
	temp.y = (float4(dot((refractdir).xyz,(u.fe[13]).xyz))).y; // DP3 temp.y, refractdir, program.env[13]
	temp = ((temp * u.fe[14] + float4(0.5f,0.5f,0.0f,0.0f))); // MAD temp, temp, program.env[14], {0.5, 0.5, 0, 0}
	refractColor = (OriginalSample(t3,s3,Flip((temp).xy,u.flags.x,3),u,3)); // TEX refractColor, temp, texture[3], 2D
	temp.x = ((float4(1.0f) + float4((angle).x))).x; // ADD temp.x, 1, angle.x
	temp.y = ((float4((temp).x) * float4((temp).x))).y; // MUL temp.y, temp.x, temp.x
	temp.y = ((float4((temp).y) * float4((temp).y))).y; // MUL temp.y, temp.y, temp.y
	temp.x = ((float4((temp).x) * float4((temp).y))).x; // MUL temp.x, temp.x, temp.y
	temp.x = ((float4((temp).x) * float4((u.fe[6]).y) + float4((u.fe[6]).x))).x; // MAD temp.x, temp.x, program.env[6].y, program.env[6].x
	clearcolor = ((float4((temp).x) * reflectColor + (float4(1.0f)-float4((temp).x)) * refractColor)); // LRP clearcolor, temp.x, reflectColor, refractColor
	temp.x = (clamp(float4(dot((waterNormal).xyz,(u.fe[9]).xyz)),0.0f,1.0f)).x; // DP3_SAT temp.x, waterNormal, program.env[9]
	temp.x = ((float4((temp).x) * float4((shadow).x))).x; // MUL temp.x, temp.x, shadow.x
	lightColor = ((float4((temp).x) * u.fe[10] + u.fe[11])); // MAD lightColor, temp.x, program.env[10], program.env[11]
	temp = ((lightColor * u.fe[8])); // MUL temp, lightColor, program.env[8]
	temp = ((float4((u.fe[7]).x) * temp + (float4(1.0f)-float4((u.fe[7]).x)) * clearcolor)); // LRP temp, program.env[7].x, temp, clearcolor
	color = ((float4((foamIntensity).x) * lightColor + (float4(1.0f)-float4((foamIntensity).x)) * temp)); // LRP result.color, foamIntensity.x, lightColor, temp
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	color.rgb=mix(u.flags.y?clamp(u.fogColor.rgb,0.0f,1.0f):u.fogColor.rgb,color.rgb,clamp((u.fogParameters.y-in.fog)*u.fogParameters.w,0.0f,1.0f));
	return color;
}

vertex DynamicVarying waterDynVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	float4 temp2;
	float4 pos;
	temp = (((float4(float3(v.position),1.0f)).xzyw * u.ve[10] + u.ve[11])); // MAD temp, vertex.position.xzyw, program.env[10], program.env[11]
	temp2 = (OriginalSampleLevel(t2,s2,Flip((temp).xy,u.flags.x,2),u,2)); // TEX temp2, temp, texture[2], 2D
	out.tc2 = (((float4(float3(v.position),1.0f)).xzyw * u.ve[12] + u.ve[13])); // MAD result.texcoord[2], vertex.position.xzyw, program.env[12], program.env[13]
	out.tc3 = (((float4(float3(v.position),1.0f)).xzyw * float4(0.004f,0.004f,0.0f,0.0f))); // MUL result.texcoord[3], vertex.position.xzyw, {0.004,0.004,0,0}
	out.tc4 = (((float4(float3(v.position),1.0f)).xzyw * u.ve[14])); // MUL result.texcoord[4], vertex.position.xzyw, program.env[14]
	pos = (float4(float3(v.position),1.0f)); // MOV pos, vertex.position
	out.color = (float4(1.0f,1.0f,1.0f,0.5f)); // MOV result.color, vertex.color
	pos.y = ((float4((pos).y) + float4((temp2).x))).y; // ADD pos.y, pos.y, temp2.x
	out.p.x = (float4(dot((pos).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((pos).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((pos).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((pos).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	fogResult.x = (float4(dot((pos).xyzw,(Row(u.mvp,2)).xyzw))).x; // DP4 result.fogcoord.x, pos, mat[2]
	temp.x = (float4(dot((pos).xyzw,(Row(u.shadow,0)).xyzw))).x; // DP4 temp.x, pos, mat3[0]
	temp.y = (float4(dot((pos).xyzw,(Row(u.shadow,1)).xyzw))).y; // DP4 temp.y, pos, mat3[1]
	temp2 = (abs(temp)); // ABS temp2,temp
	temp2 = ((temp2 + u.ve[17])); // ADD temp2,temp2,program.env[17]
	temp2.x = (float4(rsqrt(abs((float4((temp2).x)).x)))).x; // RSQ temp2.x, temp2.x
	temp2.y = (float4(rsqrt(abs((float4((temp2).y)).x)))).y; // RSQ temp2.y, temp2.y
	temp2 = ((temp2 + u.ve[18])); // ADD temp2,temp2,program.env[18]
	out.tc1 = ((temp * temp2 + u.ve[16])); // MAD result.texcoord[1], temp, temp2,program.env[16]
	out.tc1.z = (float4(dot((pos).xyzw,(Row(u.shadow,2)).xyzw))).z; // DP4 result.texcoord[1].z, pos, mat3[2]
	out.tc1.w = (float4(dot((pos).xyzw,(Row(u.shadow,3)).xyzw))).w; // DP4 result.texcoord[1].w, pos, mat3[3]
	out.tc0 = (pos); // MOV result.texcoord, pos
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

vertex DynamicVarying waterDynNTVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	float4 temp2;
	float4 pos;
	out.tc2 = (((float4(float3(v.position),1.0f)).xzyw * u.ve[12] + u.ve[13])); // MAD result.texcoord[2], vertex.position.xzyw, program.env[12], program.env[13]
	out.tc3 = (((float4(float3(v.position),1.0f)).xzyw * float4(0.017f,0.017f,0.0f,0.0f))); // MUL result.texcoord[3], vertex.position.xzyw, {0.017,0.017,0,0}
	out.tc4 = (((float4(float3(v.position),1.0f)).xzyw * u.ve[14])); // MUL result.texcoord[4], vertex.position.xzyw, program.env[14]
	pos = (float4(float3(v.position),1.0f)); // MOV pos, vertex.position
	out.color = (float4(1.0f,1.0f,1.0f,0.5f)); // MOV result.color, vertex.color
	out.p.x = (float4(dot((pos).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((pos).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((pos).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((pos).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	fogResult.x = (float4(dot((pos).xyzw,(Row(u.mvp,2)).xyzw))).x; // DP4 result.fogcoord.x, pos, mat[2]
	temp.x = (float4(dot((pos).xyzw,(Row(u.shadow,0)).xyzw))).x; // DP4 temp.x, pos, mat3[0]
	temp.y = (float4(dot((pos).xyzw,(Row(u.shadow,1)).xyzw))).y; // DP4 temp.y, pos, mat3[1]
	temp2 = (abs(temp)); // ABS temp2,temp
	temp2 = ((temp2 + u.ve[17])); // ADD temp2,temp2,program.env[17]
	temp2.x = (float4(rsqrt(abs((float4((temp2).x)).x)))).x; // RSQ temp2.x, temp2.x
	temp2.y = (float4(rsqrt(abs((float4((temp2).y)).x)))).y; // RSQ temp2.y, temp2.y
	temp2 = ((temp2 + u.ve[18])); // ADD temp2,temp2,program.env[18]
	out.tc1 = ((temp * temp2 + u.ve[16])); // MAD result.texcoord[1], temp, temp2,program.env[16]
	out.tc1.z = (float4(dot((pos).xyzw,(Row(u.shadow,2)).xyzw))).z; // DP4 result.texcoord[1].z, pos, mat3[2]
	out.tc1.w = (float4(dot((pos).xyzw,(Row(u.shadow,3)).xyzw))).w; // DP4 result.texcoord[1].w, pos, mat3[3]
	out.tc0 = (pos); // MOV result.texcoord, pos
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 waterDynNormalFragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 cross1;
	float4 cross2;
	float4 normal;
	float4 temp;
	float4 height1;
	float4 height2;
	float4 height3;
	float4 height4;
	float4 foam;
	height1 = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX height1, fragment.texcoord[0], texture[0], 2D
	height2 = (OriginalSample(t1,s1,Flip((in.tc1).xy,u.flags.x,1),u,1)); // TEX height2, fragment.texcoord[1], texture[1], 2D
	height3 = (OriginalSample(t2,s2,Flip((in.tc2).xy,u.flags.x,2),u,2)); // TEX height3, fragment.texcoord[2], texture[2], 2D
	height4 = (OriginalSample(t3,s3,Flip((in.tc3).xy,u.flags.x,3),u,3)); // TEX height4, fragment.texcoord[3], texture[3], 2D
	foam.x = ((float4((height1).y) + float4((height2).y))).x; // ADD foam.x, height1.y, height2.y
	cross1 = (u.fe[0]); // MOV cross1,program.env[0]
	cross1.y = ((float4((height1).x) - float4((height2).x))).y; // SUB cross1.y, height1.x, height2.x
	cross2 = (u.fe[1]); // MOV cross2,program.env[1]
	cross2.y = ((float4((height3).x) - float4((height4).x))).y; // SUB cross2.y, height3.x, height4.x
	normal = (float4(cross((cross1).xyz,(cross2).xyz),1.0f)); // XPD normal, cross1,cross2
	temp.x = (float4(dot((normal).xyz,(normal).xyz))).x; // DP3 temp.x, normal, normal
	temp.x = (float4(rsqrt(abs((float4((temp).x)).x)))).x; // RSQ temp.x, temp.x
	normal = ((normal * float4((temp).x))); // MUL normal, normal, temp.x
	color = (normal); // MOV result.color, normal
	color.w = ((float4((foam).x) * float4(0.5f))).w; // MUL result.color.w, foam.x, 0.5
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	return color;
}

vertex DynamicVarying waterDynNormalVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	out.p.x = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	out.tc0 = ((u.ve[9] * float4(1.0f) + float4(float2(v.uv),0.0f,1.0f))); // MAD result.texcoord[0],  program.env[9],  1, vertex.texcoord
	out.tc1 = ((u.ve[9] * float4(-1.0f) + float4(float2(v.uv),0.0f,1.0f))); // MAD result.texcoord[1],  program.env[9], -1, vertex.texcoord
	out.tc2 = ((u.ve[11] * float4(1.0f) + float4(float2(v.uv),0.0f,1.0f))); // MAD result.texcoord[2],  program.env[11], 1, vertex.texcoord
	out.tc3 = ((u.ve[11] * float4(-1.0f) + float4(float2(v.uv),0.0f,1.0f))); // MAD result.texcoord[3],  program.env[11],-1, vertex.texcoord
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 waterDynWaveFragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 temp;
	float4 old;
	float4 midFlow;
	float4 flow1;
	float4 flow2;
	float4 flow3;
	float4 flow4;
	float4 flow;
	float4 backgroundWave;
	old = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX old, fragment.texcoord[0], texture[0], 2D
	midFlow = (OriginalSample(t1,s1,Flip((in.tc1).xy,u.flags.x,1),u,1)); // TEX midFlow, fragment.texcoord[1], texture[1], 2D
	flow1 = (OriginalSample(t2,s2,Flip((in.tc2).xy,u.flags.x,2),u,2)); // TEX flow1, fragment.texcoord[2], texture[2], 2D
	flow2 = (OriginalSample(t3,s3,Flip((in.tc3).xy,u.flags.x,3),u,3)); // TEX flow2, fragment.texcoord[3], texture[3], 2D
	flow3 = (OriginalSample(t4,s4,Flip((in.tc4).xy,u.flags.x,4),u,4)); // TEX flow3, fragment.texcoord[4], texture[4], 2D
	flow4 = (OriginalSample(t5,s5,Flip((in.tc5).xy,u.flags.x,5),u,5)); // TEX flow4, fragment.texcoord[5], texture[5], 2D
	backgroundWave = (OriginalSample(t6,s6,Flip((in.tc6).xy,u.flags.x,6),u,6)); // TEX backgroundWave, fragment.texcoord[6], texture[6], 2D
	old.x = ((float4((old).x) - float4((old).z))).x; // SUB old.x, old.x, old.z
	temp.x = (float4(dot((midFlow).xyzw,(float4(1.0f,1.0f,1.0f,1.0f)).xyzw))).x; // DP4 temp.x, midFlow,{1,1,1,1}
	old.x = ((float4((old).x) - float4((temp).x))).x; // SUB old.x, old.x, temp.x
	flow.x = (float4((flow1).x)).x; // MOV flow.x, flow1.x
	flow.y = (float4((flow2).y)).y; // MOV flow.y, flow2.y
	flow.z = (float4((flow3).z)).z; // MOV flow.z, flow3.z
	flow.w = (float4((flow4).w)).w; // MOV flow.w, flow4.w
	temp.x = (float4(dot((flow).xyzw,(float4(1.0f,1.0f,1.0f,1.0f)).xyzw))).x; // DP4 temp.x, flow,{1,1,1,1}
	old.x = ((float4((old).x) + float4((temp).x))).x; // ADD old.x, old.x, temp.x
	temp = ((midFlow - flow)); // SUB temp, midFlow, flow
	temp.x = (float4(dot((temp).xyzw,(temp).xyzw))).x; // DP4 temp.x, temp, temp
	old.y = ((float4((temp).x) * float4(0.2f) + float4((old).y))).y; // MAD old.y, temp.x, 0.2, old.y
	old.y = (clamp((float4((old).y) * float4(0.99f)),0.0f,1.0f)).y; // MUL_SAT old.y, old.y, 0.99
	temp.x = (min(float4((old).w),float4(2.0f))).x; // MIN temp.x, old.w, 2
	temp.x = (clamp((float4((temp).x) * float4(0.2f)),0.0f,1.0f)).x; // MUL_SAT temp.x, temp.x, 0.2
	old.x = ((float4(0.995f) * float4((old).x) + (float4(1.0f)-float4(0.995f)) * float4((backgroundWave).z))).x; // LRP old.x, 0.995, old.x, backgroundWave.z
	old.x = ((float4((temp).x) * (-float4((old).w)) + (float4(1.0f)-float4((temp).x)) * float4((old).x))).x; // LRP old.x, temp.x, -old.w, old.x
	color = ((old * float4(1.0f,1.0f,0.0f,0.0f))); // MUL result.color, old, {1,1,0,0}
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	return color;
}

vertex DynamicVarying waterDynWaveVertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	out.p.x = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	out.tc0 = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[14])); // ADD result.texcoord[0], vertex.texcoord, program.env[14]
	out.tc1 = (float4(float2(v.uv),0.0f,1.0f)); // MOV result.texcoord[1], vertex.texcoord
	out.tc2 = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[8])); // ADD result.texcoord[2], vertex.texcoord, program.env[8]
	out.tc3 = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[9])); // ADD result.texcoord[3], vertex.texcoord, program.env[9]
	out.tc4 = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[10])); // ADD result.texcoord[4], vertex.texcoord, program.env[10]
	out.tc5 = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[11])); // ADD result.texcoord[5], vertex.texcoord, program.env[11]
	out.tc6 = ((float4(float2(v.uv),0.0f,1.0f) * float4(4.0f) + u.ve[15])); // MAD result.texcoord[6], vertex.texcoord, 4, program.env[15]
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 waterDynWave2Fragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 temp;
	float4 old;
	float4 midHeight;
	float4 height1;
	float4 height2;
	float4 height3;
	float4 height4;
	float4 gheight;
	old = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX old, fragment.texcoord[0], texture[0], 2D
	midHeight = (OriginalSample(t1,s1,Flip((in.tc0).xy,u.flags.x,1),u,1)); // TEX midHeight, fragment.texcoord[0], texture[1], 2D
	height1 = (OriginalSample(t2,s2,Flip((in.tc1).xy,u.flags.x,2),u,2)); // TEX height1, fragment.texcoord[1], texture[2], 2D
	height2 = (OriginalSample(t3,s3,Flip((in.tc2).xy,u.flags.x,3),u,3)); // TEX height2, fragment.texcoord[2], texture[3], 2D
	height3 = (OriginalSample(t4,s4,Flip((in.tc3).xy,u.flags.x,4),u,4)); // TEX height3, fragment.texcoord[3], texture[4], 2D
	height4 = (OriginalSample(t5,s5,Flip((in.tc4).xy,u.flags.x,5),u,5)); // TEX height4, fragment.texcoord[4], texture[5], 2D
	gheight = (OriginalSample(t6,s6,Flip((in.tc5).xy,u.flags.x,6),u,6)); // TEX gheight, fragment.texcoord[5], texture[6], 2D
	temp.x = ((float4((midHeight).x) - float4((height1).x))).x; // SUB temp.x, midHeight.x, height1.x
	old.x = ((float4((temp).x) * float4(0.02f) + float4((old).x))).x; // MAD old.x, temp.x, 0.02, old.x
	temp.x = ((float4((midHeight).x) - float4((height2).x))).x; // SUB temp.x, midHeight.x, height2.x
	old.y = ((float4((temp).x) * float4(0.02f) + float4((old).y))).y; // MAD old.y, temp.x, 0.02, old.y
	temp.x = ((float4((midHeight).x) - float4((height3).x))).x; // SUB temp.x, midHeight.x, height3.x
	old.z = ((float4((temp).x) * float4(0.02f) + float4((old).z))).z; // MAD old.z, temp.x, 0.02, old.z
	temp.x = ((float4((midHeight).x) - float4((height4).x))).x; // SUB temp.x, midHeight.x, height4.x
	old.w = ((float4((temp).x) * float4(0.02f) + float4((old).w))).w; // MAD old.w, temp.x, 0.02, old.w
	temp.x = ((float4((gheight).w) * float4(0.008f) + (float4(1.0f)-float4((gheight).w)) * float4(0.998f))).x; // LRP temp.x, gheight.w, 0.008, 0.998
	old = ((old * float4((temp).x))); // MUL old, old, temp.x
	color = (old); // MOV result.color, old
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	return color;
}

vertex DynamicVarying waterDynWave2Vertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	float4 temp;
	out.p.x = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	temp = ((float4(float2(v.uv),0.0f,1.0f) + u.ve[14])); // ADD temp, vertex.texcoord, program.env[14]
	out.tc0 = (temp); // MOV result.texcoord[0], temp
	out.tc1 = ((temp - u.ve[8])); // SUB result.texcoord[1], temp, program.env[8]
	out.tc2 = ((temp - u.ve[9])); // SUB result.texcoord[2], temp, program.env[9]
	out.tc3 = ((temp - u.ve[10])); // SUB result.texcoord[3], temp, program.env[10]
	out.tc4 = ((temp - u.ve[11])); // SUB result.texcoord[4], temp, program.env[11]
	out.tc5 = ((float4(float2(v.uv),0.0f,1.0f) * u.ve[12] + u.ve[13])); // MAD result.texcoord[5], vertex.texcoord, program.env[12], program.env[13]
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}

fragment float4 waterDynWave3Fragment(DynamicVarying in [[stage_in]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.p,get_sample_position(recoilOriginalSampleID));
	float4 color;
	float4 height;
	height = (OriginalSample(t0,s0,Flip((in.tc0).xy,u.flags.x,0),u,0)); // TEX height, fragment.texcoord[0], texture[0], 2D
	color = (float4((height).x)); // MOV result.color, height.x
	if(u.flags.y)color=clamp(color,0.0f,1.0f);
	return color;
}

vertex DynamicVarying waterDynWave3Vertex(uint id [[vertex_id]],device const DynamicVertex* vertices [[buffer(0)]],constant DynamicUniforms& u [[buffer(1)]],texture2d<float> t0 [[texture(0)]],sampler s0 [[sampler(0)]],texture2d<float> t1 [[texture(1)]],sampler s1 [[sampler(1)]],texture2d<float> t2 [[texture(2)]],sampler s2 [[sampler(2)]],texture2d<float> t3 [[texture(3)]],sampler s3 [[sampler(3)]],texture2d<float> t4 [[texture(4)]],sampler s4 [[sampler(4)]],texture2d<float> t5 [[texture(5)]],sampler s5 [[sampler(5)]],texture2d<float> t6 [[texture(6)]],sampler s6 [[sampler(6)]],texture2d<float> t7 [[texture(7)]],sampler s7 [[sampler(7)]]) {
	const DynamicVertex v=vertices[id];DynamicVarying out;float4 fogResult(u.source.x);
	out.p.x = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,0)).xyzw))).x; // DP4 opos.x, pos, mat[0]
	out.p.y = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,1)).xyzw))).y; // DP4 opos.y, pos, mat[1]
	out.p.z = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,2)).xyzw))).z; // DP4 opos.z, pos, mat[2]
	out.p.w = (float4(dot((float4(float3(v.position),1.0f)).xyzw,(Row(u.mvp,3)).xyzw))).w; // DP4 opos.w, pos, mat[3]
	out.tc0 = (float4(float2(v.uv),0.0f,1.0f)); // MOV result.texcoord[0], vertex.texcoord
	out.fog=fogResult.x;if(u.source.y!=0.0f)out.p.z=(out.p.z+out.p.w)*0.5f;return out;
}
)MSL";
}
