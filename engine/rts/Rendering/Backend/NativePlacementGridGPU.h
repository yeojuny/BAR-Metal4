/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePlacementData.h"
#include "NativeWorldDrawRecorder.h"
#include <cmath>
#include <optional>
namespace rendering::placementGrid128 {
// Literal source shader inputs plus the viewport and API line adaptation.
// No CPU endpoints, raster plans, per-pixel work estimates or double emulation.
struct Uniform {
 std::array<float,16> matrix;
 std::array<float,4> mouse,mapLines,viewport,plane,raster;
 std::array<uint32_t,4> flags;
};
static_assert(sizeof(Uniform)==160);
inline std::optional<Uniform> Admit(const NativeBuildingGridDraw& d,const std::array<double,4>& vp,uint32_t width,uint32_t height,unsigned samples) {
 ValidateNativeBuildingGrid(d);
 if(!d.generatedPatch31657||!d.vertexCount||(samples!=1&&samples!=4)||!width||!height||width>16384||height>16384||d.heights->width>8193||d.heights->height>8193||d.surface.splatPainter)return {};
 // This is the pinned source's always-pass, no-depth-write line family.
 // Other inherited states retain their existing renderer.
 if(d.world.depthWrite||(d.world.depthTest&&d.world.depthCompare!=ScreenAlphaCompare::Always)||d.world.depthClamp||d.world.stencilTest||(d.world.lineStipple&&d.world.stipplePattern!=0xffff))return {};
 if(!std::isfinite(d.world.lineWidth)||d.world.lineWidth<=0||d.world.lineWidth>4)return {};
 for(double v:vp)if(!std::isfinite(v)||std::abs(v)>32768)return {};
 if(vp[2]<=0||vp[3]<=0)return {};
 for(float v:d.mousePosition)if(std::abs(v)>65536)return {};
 for(float v:d.view.viewProjection)if(std::abs(v)>65536)return {};
 if(d.world.inheritedClipPlane)for(float v:*d.world.inheritedClipPlane)if(!std::isfinite(v)||std::abs(v)>65536)return {};
 // Preserve generic state validation without evaluating source vertices.
 std::array<NativeWorldDrawVertex,2> sentinel{};for(auto& v:sentinel)v.position[3]=1;
 auto surface=d.surface;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};
 ValidateNativeWorldDrawCommand(MakeNativeWorldDrawCommand(d.stage,NativeDrawPrimitive::Lines,sentinel,surface,d.world));
 Uniform u{};u.matrix=d.view.viewProjection;u.mouse={d.mousePosition[0],d.mousePosition[1],d.mousePosition[2],d.cameraFade};u.mapLines={d.mapWorldSize[0],d.mapWorldSize[1],d.lineParameters[0],d.lineParameters[1]};
 for(unsigned i=0;i<4;++i)u.viewport[i]=float(vp[i]);
 if(d.world.inheritedClipPlane)u.plane=*d.world.inheritedClipPlane;
 u.raster={float(width),float(height),d.world.lineWidth,d.lineParameters[2]};
 u.flags={d.firstVertex,uint32_t(d.view.clipDepth==ClipDepthConvention::ZeroToOne),uint32_t(d.world.inheritedClipPlane.has_value()),samples};return u;
}
}
