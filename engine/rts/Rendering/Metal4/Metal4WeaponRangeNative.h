/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeWeaponRangeData.h"
#include <algorithm>
#include <cmath>

namespace rendering::metal4 {
struct WeaponRangeRaster {MTLViewport viewport;MTLScissorRect scissor;};
// GL viewport maps/clips centerlines. Only glScissor and the attachment
// bound the width of an aliased line after that clipping.
inline std::optional<WeaponRangeRaster> MapWeaponRangeRaster(const std::array<std::int32_t,4>& viewport,
	std::array<std::uint32_t,2> logical,std::array<std::uint32_t,2> target,const std::optional<ScreenScissor>& requested)
{
	Require(logical[0] && logical[1] && target[0] && target[1],"Original range raster requires actual source/target extents");
	if(viewport[2]<=0 || viewport[3]<=0)return {};
	const double sx=double(target[0])/logical[0],sy=double(target[1])/logical[1];
	WeaponRangeRaster out{{viewport[0]*sx,(double(logical[1])-double(viewport[1])-viewport[3])*sy,viewport[2]*sx,viewport[3]*sy,0,1},{0,0,target[0],target[1]}};
	if(requested){const auto& sc=*requested;
		const auto x0=NSUInteger(std::clamp(std::floor(double(sc.x)*sx),0.,double(target[0]))),x1=NSUInteger(std::clamp(std::ceil((double(sc.x)+sc.width)*sx),0.,double(target[0])));
		const auto y0=NSUInteger(std::clamp(std::floor((double(logical[1])-sc.y-sc.height)*sy),0.,double(target[1]))),y1=NSUInteger(std::clamp(std::ceil((double(logical[1])-sc.y)*sy),0.,double(target[1])));
		if(x1<=x0 || y1<=y0)return {};out.scissor={x0,y0,x1-x0,y1-y0};
	}
	return out;
}
class PreparedWeaponRangeDraw {
public:
	~PreparedWeaponRangeDraw();
	bool Empty() const;
	void Encode(id<MTL4RenderCommandEncoder>) const;
private:
	PreparedWeaponRangeDraw();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class WeaponRangeGPU;
};

// Original source vertices are evaluated in the native VS. The existing
// map owner and render pass retain producer order; no new compute pass,
// source tessellation, framebuffer copy, submission or host wait is added.
class WeaponRangeGPU {
public:
	WeaponRangeGPU();~WeaponRangeGPU();
	void ClearResources(); // Only after serialized GPU retirement.
	std::unique_ptr<PreparedWeaponRangeDraw> Prepare(RuntimeLease&,const NativeWeaponRangeDraw&,
		const StartBoxImageResolver&,MTLViewport,MTLScissorRect,std::array<std::uint32_t,2> targetExtent,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds,
		std::optional<NativePostprocessConsumer> consumer=std::nullopt);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
