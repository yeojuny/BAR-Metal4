/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeReclaimFieldData.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>
namespace rendering::reclaimSource131 {
struct alignas(16) Uniform {
 std::array<float,16> viewProj,viewInv;
 std::array<float,4> fillMetal,fillEnergy,edgeMetal,edgeEnergy,togglesOffsetGradient,fadeEnergy;
 std::array<uint32_t,4> options;
 std::array<float,4> alpha;
};
static_assert(sizeof(Uniform)==256&&offsetof(Uniform,options)==224&&offsetof(Uniform,alpha)==240);
struct Raster {std::array<float,4> viewport,target;std::array<uint32_t,4> options;};
static_assert(sizeof(Raster)==48);
inline Uniform OriginalUniform(const NativeReclaimFieldInput& d,const ScreenAlphaTest& alpha){
 ValidateNativeReclaimFieldInput(d);Uniform u{};
 std::memcpy(u.viewProj.data(),d.original->matrices->bytes.data()+reclaimfield::ViewProjectionByte,64);std::memcpy(u.viewInv.data(),d.original->matrices->bytes.data()+reclaimfield::ViewInverseByte,64);
 const auto& s=d.uniforms;u.fillMetal=s.fillMetal;u.fillEnergy=s.fillEnergy;u.edgeMetal=s.edgeMetal;u.edgeEnergy=s.edgeEnergy;u.togglesOffsetGradient={s.toggles[0],s.toggles[1],s.yOffset,s.gradientAlpha};u.fadeEnergy={s.fadeDist[0],s.fadeDist[1],s.energyOpacity,0};uint32_t caps;std::memcpy(&caps,d.original->params->bytes.data()+12,4);u.options={s.vertsPerChunk,caps&1u,uint32_t(alpha.enabled),uint32_t(alpha.compare)};u.alpha={alpha.reference,0,0,0};return u;
}
inline NativeWorldDrawState LineState(NativeWorldDrawState state){state.cullEnabled=false;state.polygonOffset=false;state.backStencil=state.frontStencil;return state;}
inline std::optional<Raster> Admit(const NativeReclaimFieldInput& d,const NativeWorldDrawState& state,const std::array<double,4>& viewport,unsigned width,unsigned height,unsigned samples){
 ValidateNativeReclaimFieldInput(d);
 if(!d.edges||d.count>4u*1024u*1024u||!width||!height||width>16384||height>16384||(samples!=1&&samples!=4))return {};
 if(state.inheritedClipPlane||(state.lineStipple&&state.stipplePattern!=0xffff)||(state.depthTest&&(state.depthCompare!=ScreenAlphaCompare::Always||state.depthWrite)))return {};
 if(!std::isfinite(state.lineWidth)||state.lineWidth<=0||state.lineWidth>64)return {};
 for(double x:viewport)if(!std::isfinite(x)||std::abs(x)>32768)return {};
 if(viewport[2]<=0||viewport[3]<=0)return {};
 Raster r{};for(unsigned i=0;i<4;++i)r.viewport[i]=float(viewport[i]);r.target={float(width),float(height),state.lineWidth,0};r.options={samples,uint32_t(state.depthClamp),0,0};return r;
}
}
