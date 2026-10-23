/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeEngineModelData.h"
#include "Rendering/UniformConstants.h"
#import <simd/simd.h>
#include <cstring>
#include <cstddef>
namespace rendering::metal4::engineoriginal {
struct Uniform {
 simd_float4x4 viewProjection,world,piece,normal;
	simd_float4 team,nano,colorMultiplier,alphaControl,clip0,clip1,clip2,clip3;
 simd_uint4 flags,image0,swizzle0,image1,swizzle1;
 simd_float4 samplerParameters;
 simd_float4 root0q,root0t,root1q,root1t,piece0q,piece0t,piece1q,piece1t;
 simd_float4x4 shadow;
 simd_float4 camera,sunDirection,sunAmbient,sunDiffuse,sunSpecular,fogColor,fogParameters;
 simd_float4 lightingParameters; // shadowDensity, alphaReference, reserved
 simd_uint4 lightingFlags; // shadows, alphaEnabled, alphaFunction, shadingMode
 simd_uint4 lightImage,lightSwizzle;
 simd_float4 lightSampler;
};
static_assert(sizeof(Uniform)==864);
static_assert(offsetof(Uniform,clip3)==368);
static_assert(offsetof(Uniform,flags)==384);
static_assert(offsetof(Uniform,samplerParameters)==464);
static_assert(offsetof(Uniform,root0q)==480);
static_assert(offsetof(Uniform,shadow)==608);
static_assert(offsetof(Uniform,camera)==672);
static_assert(offsetof(Uniform,lightSampler)==848);
inline simd_float4x4 Matrix(const WorldMatrix& m){simd_float4x4 v;std::memcpy(&v,m.data(),sizeof(v));return v;}
inline simd_float4x4 Matrix(const float* m){simd_float4x4 v;std::memcpy(&v,m,sizeof(v));return v;}
inline simd_float4 Vec(const std::array<float,4>& v){return{v[0],v[1],v[2],v[3]};}
inline simd_uint4 Vec(const std::array<unsigned,4>& v){return{v[0],v[1],v[2],v[3]};}
inline void Transform(const NativeCUSPublishedBuffer& source,size_t index,simd_float4& q,simd_float4& t){
 Require(source.kind==NativeCUSPublishedKind::Transforms&&source.stride==32,"Original model transform ABI differs");
 std::array<uint8_t,32> bytes;ReadNativeCUSPublishedElement(source,index,bytes);
 std::memcpy(&q,bytes.data(),16);std::memcpy(&t,bytes.data()+16,16);
}
inline Uniform DrawUniform(const NativeEngineModelPhase& phase,const NativeEngineModelDraw& draw){
 Uniform u{};const auto& p=draw.program;
 u.viewProjection=Matrix(draw.sourceProjection);u.world=Matrix(draw.sourceModelView);
 u.team=Vec(p.team);u.nano=Vec(p.nano);u.colorMultiplier=Vec(p.colorMultiplier);u.alphaControl=Vec(p.alphaControl);
 u.clip0=draw.clipEnabled[0]?Vec(draw.storedClipPlanes[0]):simd_float4{0,0,0,1};u.clip1=draw.clipEnabled[1]?Vec(draw.storedClipPlanes[1]):simd_float4{0,0,0,1};
 u.clip2=draw.clipEnabled[2]?Vec(draw.storedClipPlanes[2]):simd_float4{0,0,0,1};
	u.clip3=phase.alphaClipPlane?Vec(*phase.alphaClipPlane):simd_float4{0,0,0,1};
 u.flags.x=phase.viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne;
 u.shadow=Matrix(p.shadowMatrix);u.sunDirection=Vec(p.sunDirection);u.sunAmbient=Vec(p.sunAmbient);u.sunDiffuse=Vec(p.sunDiffuse);u.sunSpecular=Vec(p.sunSpecular);u.fogColor=Vec(p.fogColor);u.fogParameters=Vec(p.fogParameters);
 u.lightingParameters={p.shadowDensity,draw.sourceSurface.alphaTest.reference,0,0};
 u.lightingFlags={unsigned(p.shadows),unsigned(draw.sourceSurface.alphaTest.enabled),unsigned(draw.sourceSurface.alphaTest.compare),unsigned(p.shadingMode)};
 const auto camera=simd_mul(simd_inverse(u.viewProjection),simd_float4{0,0,0,1});u.camera=camera/camera.w;
 if(draw.shader==NativeEngineModelShader::GL4){
  const auto& pose=draw.pose;ValidateNativeCUSUniformPublication(*pose.engineUniforms);
  UniformMatricesBuffer matrices;UniformParamsBuffer params;
  std::memcpy(&matrices,pose.engineUniforms->matrices->bytes.data(),sizeof(matrices));
  // Only prefix fields are read here; team row is checked independently.
  std::memcpy(&params,pose.engineUniforms->params->bytes.data(),NativeCUSParamsPrefixBytes);
		u.flags.y=1;u.flags.z=pose.arrayModel?2:unsigned(pose.staticModel);u.flags.x=!(params.renderCaps&1u);
  u.samplerParameters.z=params.timeInfo.w;
  const auto bytes=ReadNativeCUSTeamColor(*pose.engineUniforms,pose.instance[2]&0x7ffu);std::memcpy(&u.team,bytes.data(),16);u.team.w=p.teamAlpha;
  u.viewProjection=Matrix(p.cameraMode==1?matrices.reflectionViewProj.m:matrices.cameraViewProj.m);
  u.camera=simd_mul(Matrix(matrices.cameraViewInv.m),simd_float4{0,0,0,1});
  u.shadow=Matrix(matrices.shadowView.m);
  std::memcpy(&u.sunDirection,&params.sunDir,16);std::memcpy(&u.sunAmbient,&params.sunAmbientModel,16);std::memcpy(&u.sunDiffuse,&params.sunDiffuseModel,16);std::memcpy(&u.sunSpecular,&params.sunSpecularModel,16);std::memcpy(&u.fogColor,&params.fogColor,16);std::memcpy(&u.fogParameters,&params.fogParams,16);u.lightingParameters.x=params.shadowDensity.y;
		if(pose.staticModel&&!pose.arrayModel)u.world=Matrix(p.staticModelMatrix);
		else if(pose.arrayModel)Transform(*pose.transforms,pose.instance[0],u.root0q,u.root0t);
  else{Transform(*pose.transforms,pose.instance[0],u.root0q,u.root0t);Transform(*pose.transforms,size_t(pose.instance[0])+1,u.root1q,u.root1t);}
 }
 return u;
}
inline void PieceUniform(Uniform& u,const NativeEngineModelDraw& draw,size_t piece){
 if(draw.shader==NativeEngineModelShader::GL4){
  const auto& pose=draw.pose;
  if(pose.staticModel){Transform(*pose.transforms,size_t(pose.instance[3])+piece,u.piece0q,u.piece0t);u.piece1q=u.piece0q;u.piece1t=u.piece0t;}
  else{const size_t index=size_t(pose.instance[0])+2*(1+piece);Transform(*pose.transforms,index,u.piece0q,u.piece0t);Transform(*pose.transforms,index+1,u.piece1q,u.piece1t);}
 }else{
  u.piece=Matrix(draw.object->pieces[piece].modelSpace);const auto model=simd_mul(u.world,u.piece);u.normal=simd_transpose(simd_inverse(model));
 }
}
} // namespace rendering::metal4::engineoriginal
