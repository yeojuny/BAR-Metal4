/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeScreenLine.h"
#include "NativeMultisampleLine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
bool Diamond(double ax,double ay,double bx,double by,int x,int y)
{
	// GL's endpoint perturbation removes exact diamond-boundary ambiguity.
	constexpr double E=1e-7;ax-=E;ay-=E*E;bx-=E;by-=E*E;
	const std::array<double,2> start{ax+ay,ax-ay},delta{bx+by-start[0],bx-by-start[1]};
	const std::array<double,2> center{double(x+y+1),double(x-y)};
	double enter=0,leave=1;
	for(size_t axis=0;axis<2;++axis) {
		const double lo=center[axis]-.5,hi=center[axis]+.5;
		if(delta[axis]==0){if(start[axis]<=lo||start[axis]>=hi)return false;continue;}
		double a=(lo-start[axis])/delta[axis],b=(hi-start[axis])/delta[axis];
		if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);
	}
	// A fragment is generated only when the segment exits its diamond; the
	// final endpoint's diamond is excluded even if the segment enters it.
	return leave>enter && leave>=0 && enter<=1 && leave<1;
}
}
void AppendNativeScreenLines(ScreenDrawList& list,std::span<const ScreenLine> lines,const ScreenDrawState& state,uint32_t width,uint32_t height,unsigned samples)
{
	if(!samples||samples>32||!width||!height||width>16384||height>16384||list.IsRecording())throw std::runtime_error("Native line requires completed primitive and exact bounded raster extent");
	if(state.material!=ScreenMaterial::Solid)throw std::runtime_error("Native aliased lines do not support textured lines");
	std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<uint32_t> indices;
	uint64_t stippleCounter=0;
	for(const auto& original:lines) {
		auto line=original;
		if(line.resetStipple)stippleCounter=0;
		for(float value:{line.start.x,line.start.y,line.start.z,line.end.x,line.end.y,line.end.z,line.width})if(!std::isfinite(value))throw std::runtime_error("Native line requires finite endpoints/width");
		if(line.width<=0||line.width>64)throw std::runtime_error("Native aliased line width/position outside supported bounds");
		for(float c:line.start.color)if(!std::isfinite(c))throw std::runtime_error("Nonfinite line color");
		for(float c:line.end.color)if(!std::isfinite(c))throw std::runtime_error("Nonfinite line color");
		// Homogeneous w=1 clip before rasterization. GL leaves the initial
		// stipple counter indeterminate after clipping; this implementation
		// retains the counter and starts at the first clipped unit fragment.
		double enter=0,leave=1;
		const auto clip=[&](double a,double b,double low,double high) {
			if(a==b)return a>=low&&a<=high;
			double lo=(low-a)/(b-a),hi=(high-a)/(b-a);if(lo>hi)std::swap(lo,hi);
			enter=std::max(enter,lo);leave=std::min(leave,hi);return enter<=leave;
		};
		if(!clip(line.start.x,line.end.x,0,width)||!clip(line.start.y,line.end.y,0,height)||!clip(line.start.z,line.end.z,-1,1))continue;
		const auto interpolate=[&](double t){ScreenVertex v{};v.x=float(original.start.x+(double(original.end.x)-original.start.x)*t);v.y=float(original.start.y+(double(original.end.y)-original.start.y)*t);v.z=float(original.start.z+(double(original.end.z)-original.start.z)*t);for(size_t c=0;c<4;++c)v.color[c]=float(original.start.color[c]+(double(original.end.color[c])-original.start.color[c])*t);return v;};
		line.start=interpolate(enter);line.end=interpolate(leave);
		const double dx=double(line.end.x)-line.start.x,dy=double(line.end.y)-line.start.y,length=dx*dx+dy*dy;
		if(length==0)continue;
		if(samples>1){
			const double distance=std::sqrt(length),nx=-dy/distance*line.width*.5,ny=dx/distance*line.width*.5;
			NativeMultisampleLineIntervals(distance,line.stippled,line.pattern,line.factor,stippleCounter,[&](double t0,double t1){
				const auto first=uint32_t(vertices.size());
				for(const auto& corner:std::array<std::array<double,2>,4>{{{t0,1},{t0,-1},{t1,-1},{t1,1}}}){
					const double t=corner[0],side=corner[1];ScreenVertex v{};
					v.x=float(line.start.x+dx*t+nx*side);v.y=float(line.start.y+dy*t+ny*side);v.z=float(line.start.z+(double(line.end.z)-line.start.z)*t);
					for(unsigned c=0;c<4;++c)v.color[c]=float(line.start.color[c]+(double(line.end.color[c])-line.start.color[c])*t);
					vertices.push_back(v);uv.push_back({0,0});
				}
				for(uint32_t offset:{0u,1u,2u,0u,2u,3u})indices.push_back(first+offset);
				if(vertices.size()>2'000'000)throw std::runtime_error("Native multisample line output exceeds bounds");
			});
			continue;
		}
		// Use a fixed subpixel grid for coverage, retaining the original
		// endpoints for interpolation. This keeps a projected 45-degree edge
		// from changing its major axis because x/y rounded differently.
		const auto snap=[](double value){return std::floor(value*256+.5)/256;};
		const double ax=snap(line.start.x),ay=snap(line.start.y),bx=snap(line.end.x),by=snap(line.end.y);
		const double rasterDx=bx-ax,rasterDy=by-ay;
		if(rasterDx==0&&rasterDy==0)continue;
		const bool xMajor=std::abs(rasterDx)>=std::abs(rasterDy);const int count=std::max(1,int(std::floor(line.width+.5f)));
		// Retain the unit-width segment's half-open major coverage for every
		// width, then offset/replicate in the minor direction (GL2.1 3.4).
		// Re-testing shifted endpoints against diamonds changes the line's
		// length and double-blends some connected wide-line corners.
		const double shift=-double(count-1)*.5;
		const double majorA=xMajor?ax:ay,majorB=xMajor?bx:by;
		const int begin=std::max(0,int(std::floor(std::min(majorA,majorB)))-1);
		const int end=std::min(int(xMajor?width:height)-1,int(std::ceil(std::max(majorA,majorB)))+1);
		std::vector<int> unitMajors;
		for(int step=0;step<=end-begin;++step) {
			const int major=majorB>=majorA?begin+step:end-step;
			const double tAt=(major+.5-majorA)/(majorB-majorA);
			const int center=int(std::floor((xMajor?ay:ax)+tAt*(xMajor?rasterDy:rasterDx)));
			for(int minor=center-1;minor<=center+1;++minor)
				if(Diamond(ax,ay,bx,by,xMajor?major:minor,xMajor?minor:major)){unitMajors.push_back(major);break;}
		}
		if(unitMajors.empty())continue;
		const int direction=majorB>=majorA?1:-1;
		std::vector<int> covered=unitMajors;
		if(line.stippled || line.polygonEdge) {
			// Legacy stipple/polygon edges have sample-center endpoint caps.
			// Preserve the virtual unit-line counter independently: adding or
			// removing an endpoint sample must not shift every later stipple.
			covered.clear();
			const int first=direction>0?int(std::ceil(majorA-.5)):int(std::floor(majorA-.5));
			const int last=direction>0?int(std::ceil(majorB-.5))-1:int(std::floor(majorB-.5))+1;
			for(int major=first;direction>0?major<=last:major>=last;major+=direction)covered.push_back(major);
		}
		for(int major:covered) {
			const auto ordinal=uint64_t(std::max(0,(major-unitMajors.front())*direction));
			const bool visible=!line.stippled || ((line.pattern>>(((stippleCounter+ordinal)/uint64_t(std::clamp(line.factor,1,256)))%16))&1)!=0;
			if(!visible)continue;
			const double tAt=(major+.5-majorA)/(majorB-majorA);
			const double minor=(xMajor?ay:ax)+tAt*(xMajor?rasterDy:rasterDx);
			const int lower=int(std::floor(minor+shift+((line.stippled||line.polygonEdge)?1e-9:-1e-9)));
			for(int replica=0;replica<count;++replica) {
				const int x=xMajor?major:lower+replica,y=xMajor?lower+replica:major;
				if(x<0||y<0||x>=int(width)||y>=int(height))continue;
				const double t=(xMajor?(x+.5-line.start.x)/dx:(y+.5-line.start.y)/dy);
				const float z=float(line.start.z+(line.end.z-line.start.z)*t);if(z < -1 || z > 1)continue;
				ScreenColor color;for(size_t c=0;c<4;++c)color[c]=float(line.start.color[c]+(line.end.color[c]-line.start.color[c])*t);
				const uint32_t first=uint32_t(vertices.size());for(const auto& p:std::array<std::array<float,2>,4>{{{float(x),float(y)},{float(x+1),float(y)},{float(x+1),float(y+1)},{float(x),float(y+1)}}}){vertices.push_back({p[0],p[1],z,color});uv.push_back({0,0});}
				for(uint32_t i:{0u,1u,2u,0u,2u,3u})indices.push_back(first+i);
				if(vertices.size()>2'000'000)throw std::runtime_error("Native aliased line output exceeds source geometry bounds");
			}
		}
		stippleCounter+=unitMajors.size();
	}
	list.AppendTriangles(vertices,uv,indices,state);
}
}
