/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBlueprintData.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include <cmath>
#include <cstring>
#include <optional>
namespace rendering::blueprintOutline129 {
struct Uniform {std::array<float,16> matrix;std::array<float,4> map,viewport,plane,raster;std::array<uint32_t,4> flags;};
static_assert(sizeof(Uniform)==144);
inline std::optional<Uniform> Admit(const NativeBlueprintOutlineDraw& d,const std::array<double,4>& vp,unsigned width,unsigned height,unsigned samples){
 ValidateNativeBlueprintOutline(d);
 if(!d.instanceCount||d.instanceCount>65536||(samples!=1&&samples!=4)||!width||!height||width>16384||height>16384||d.heights->width>8193||d.heights->height>8193||d.surface.splatPainter)return {};
 if(d.world.depthClamp||d.world.stencilTest||(d.world.lineStipple&&d.world.stipplePattern!=0xffff))return {};
 for(double x:vp)if(!std::isfinite(x)||std::abs(x)>32768)return {};if(vp[2]<=0||vp[3]<=0)return {};
 for(float x:d.view.viewProjection)if(std::abs(x)>65536)return {};
 for(size_t row=d.firstInstance;row<size_t(d.firstInstance)+d.instanceCount;++row){float instance[9];std::memcpy(instance,d.instances->bytes.data()+row*36,36);for(float value:instance)if(std::abs(value)>65536)return {};}
 if(d.world.inheritedClipPlane)for(float x:*d.world.inheritedClipPlane)if(!std::isfinite(x)||std::abs(x)>65536)return {};
 std::array<NativeWorldDrawVertex,2> sentinel{};for(auto& v:sentinel)v.position[3]=1;auto surface=d.surface;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};
 ValidateNativeWorldDrawCommand(MakeNativeWorldDrawCommand(d.stage,NativeDrawPrimitive::Lines,sentinel,surface,d.world));
 Uniform u{};u.matrix=d.view.viewProjection;u.map={d.mapWorldSize[0],d.mapWorldSize[1],0,0};for(unsigned i=0;i<4;++i)u.viewport[i]=float(vp[i]);if(d.world.inheritedClipPlane)u.plane=*d.world.inheritedClipPlane;
 u.raster={float(width),float(height),d.world.lineWidth,0};u.flags={uint32_t(d.view.clipDepth==ClipDepthConvention::ZeroToOne),uint32_t(d.world.inheritedClipPlane.has_value()),samples,0};return u;
}
}
