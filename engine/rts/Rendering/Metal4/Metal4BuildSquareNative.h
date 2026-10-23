/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeBuildSquareData.h"
#include <algorithm>
#include <cmath>
namespace rendering::metal4 {
struct BuildSquareRaster {MTLViewport viewport;MTLScissorRect scissor;};
inline std::optional<BuildSquareRaster> MapBuildSquareRaster(const std::array<std::int32_t,4>& v,
	std::array<std::uint32_t,2> logical,std::array<std::uint32_t,2> target,const std::optional<ScreenScissor>& requested)
{
	Require(logical[0]&&logical[1]&&target[0]&&target[1],"Original BuildSquare raster has no source/target extent");if(v[2]<=0||v[3]<=0)return {};
	const double sx=double(target[0])/logical[0],sy=double(target[1])/logical[1];
	BuildSquareRaster out{{v[0]*sx,(double(logical[1])-double(v[1])-v[3])*sy,v[2]*sx,v[3]*sy,0,1},{0,0,target[0],target[1]}};
	if(requested){const auto& s=*requested;const auto x0=NSUInteger(std::clamp(std::floor(double(s.x)*sx),0.,double(target[0]))),x1=NSUInteger(std::clamp(std::ceil((double(s.x)+s.width)*sx),0.,double(target[0])));
		const auto y0=NSUInteger(std::clamp(std::floor((double(logical[1])-s.y-s.height)*sy),0.,double(target[1]))),y1=NSUInteger(std::clamp(std::ceil((double(logical[1])-s.y)*sy),0.,double(target[1])));
		if(x1<=x0||y1<=y0)return {};out.scissor={x0,y0,x1-x0,y1-y0};}
	return out;
}
class PreparedBuildSquareDraw {
public:
	~PreparedBuildSquareDraw();bool Empty()const;
	void Encode(id<MTL4RenderCommandEncoder>)const;
private:
	PreparedBuildSquareDraw();struct Impl;std::unique_ptr<Impl> impl;friend class BuildSquareGPU;
};
class BuildSquareGPU {
public:
	BuildSquareGPU();~BuildSquareGPU();
	void ClearResources(); // Existing serialized GPU-retirement boundary only.
	std::unique_ptr<PreparedBuildSquareDraw> Prepare(RuntimeLease&,const NativeBuildSquareDraw&,
		const StartBoxImageResolver&,MTLViewport,MTLScissorRect,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds,std::optional<NativePostprocessConsumer> consumer=std::nullopt);
private:struct Impl;std::unique_ptr<Impl> impl;
};
}
