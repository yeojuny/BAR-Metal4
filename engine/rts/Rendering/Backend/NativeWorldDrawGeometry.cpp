/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldDrawGeometry.h"
#include "NativeWorldDrawRecorder.h"
#include "NativeScreenLine.h"
#include "NativeMultisampleLine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace rendering {
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
NativeWorldDrawVertex Mix(const NativeWorldDrawVertex& a,const NativeWorldDrawVertex& b,double t)
{
	NativeWorldDrawVertex r;
	for(size_t i=0;i<4;++i){r.position[i]=float(a.position[i]+(double(b.position[i])-a.position[i])*t);r.color[i]=float(a.color[i]+(double(b.color[i])-a.color[i])*t);}
	for(size_t i=0;i<4;++i)r.uv[i]=float(a.uv[i]+(double(b.uv[i])-a.uv[i])*t);
	return r;
}
double PlaneDistance(const NativeWorldDrawVertex& vertex,const std::array<float,4>& plane)
{
	double distance=0;
	for(size_t i=0;i<4;++i)distance+=double(vertex.position[i])*plane[i];
	return distance;
}
bool ClipUserPlane(NativeWorldDrawVertex& a,NativeWorldDrawVertex& b,const std::array<float,4>& plane)
{
	const double da=PlaneDistance(a,plane),db=PlaneDistance(b,plane);
	if(da<0 && db<0)return false;
	if((da<0)!=(db<0)) {
		const auto intersection=Mix(a,b,da/(da-db));
		if(da<0)a=intersection;else b=intersection;
	}
	return true;
}
std::vector<NativeWorldDrawVertex> ClipUserPolygon(const std::vector<NativeWorldDrawVertex>& polygon,const std::array<float,4>& plane)
{
	std::vector<NativeWorldDrawVertex> result;
	if(polygon.empty())return result;
	result.reserve(polygon.size()+1);
	auto previous=polygon.back();double before=PlaneDistance(previous,plane);
	for(const auto& vertex:polygon) {
		const double distance=PlaneDistance(vertex,plane);
		if((before<0)!=(distance<0)) {
			// An endpoint on the plane is already retained. Do not duplicate it
			// as an extra boundary corner for polygon-line/point rasterization.
			if(before!=0 && distance!=0)result.push_back(Mix(previous,vertex,before/(before-distance)));
		}
		if(distance>=0)result.push_back(vertex);
		previous=vertex;before=distance;
	}
	return result;
}
bool Clip(NativeWorldDrawVertex& a,NativeWorldDrawVertex& b,bool depthClamp)
{
	double enter=0,leave=1;
	for(size_t axis=0;axis<(depthClamp?2u:3u);++axis)for(double sign:{-1.,1.}) {
		const double da=a.position[3]+sign*a.position[axis],db=b.position[3]+sign*b.position[axis];
		if(da<0 && db<0)return false;
		if(da<0)enter=std::max(enter,da/(da-db));
		if(db<0)leave=std::min(leave,da/(da-db));
	}
	if(enter>leave)return false;
	const auto first=a,last=b;a=Mix(first,last,enter);b=Mix(first,last,leave);
	return a.position[3]>0 && b.position[3]>0;
}
std::vector<NativeWorldDrawVertex> ClipPolygon(std::vector<NativeWorldDrawVertex> polygon,bool depthClamp)
{
	for(size_t axis=0;axis<(depthClamp?2u:3u);++axis)for(double sign:{-1.,1.}) {
		std::vector<NativeWorldDrawVertex> next;
		if(polygon.empty())return next;
		auto previous=polygon.back();double before=previous.position[3]+sign*previous.position[axis];
		for(const auto& vertex:polygon) {
			const double distance=vertex.position[3]+sign*vertex.position[axis];
			if((before<0)!=(distance<0)) {
				auto intersection=Mix(previous,vertex,before/(before-distance));
				// The generated vertex lies on this exact homogeneous plane;
				// avoid rounding it just outside again during point/edge clipping.
				intersection.position[axis]=float(-sign*intersection.position[3]);
				next.push_back(intersection);
			}
			if(distance>=0)next.push_back(vertex);
			previous=vertex;before=distance;
		}
		polygon=std::move(next);
	}
	return polygon;
}
}
static NativeWorldRasterMesh BuildGeometry(const NativeWorldDrawCommand& d,const std::array<double,4>& viewport,uint32_t width,uint32_t height,bool polygonEdge,unsigned samples)
{
	ValidateNativeWorldDrawCommand(d);
	Require(samples&&samples<=32&&width&&height&&width<=16384&&height<=16384&&viewport[2]>0&&viewport[3]>0,"Native world raster extent invalid");
	for(double value:viewport)Require(std::isfinite(value),"Native world viewport non-finite");
	if(d.world.inheritedClipPlane)for(float value:*d.world.inheritedClipPlane)Require(std::isfinite(value),"Native inherited clip plane non-finite");
	if(d.primitive==NativeDrawPrimitive::Triangles) {
		if(d.world.frontPolygonMode==NativePolygonMode::Fill && d.world.backPolygonMode==NativePolygonMode::Fill) {
			if(!d.world.inheritedClipPlane)return {d.vertices,d.indices};
			NativeWorldRasterMesh result;
			// Keep the existing source fill triangulation and perspective
			// interpolants. The Metal vertex ABI remains homogeneous GL xyzw.
			for(size_t i=0;i<d.indices.size();i+=3) {
				const auto polygon=ClipUserPolygon({d.vertices[d.indices[i]],d.vertices[d.indices[i+1]],d.vertices[d.indices[i+2]]},*d.world.inheritedClipPlane);
				if(polygon.size()<3)continue;
				const auto first=uint32_t(result.vertices.size());
				result.vertices.insert(result.vertices.end(),polygon.begin(),polygon.end());
				for(uint32_t j=1;j+1<polygon.size();++j)result.indices.insert(result.indices.end(),{first,first+j,first+j+1});
			}
			return result;
		}
		Require(!d.world.polygonOffset,"Native non-fill polygon offset requires its original plane-depth contract");
		NativeWorldRasterMesh result;
		const auto polygon=[&](const std::vector<uint32_t>& corners) {
			std::vector<NativeWorldDrawVertex> original;for(auto index:corners)original.push_back(d.vertices[index]);
			auto clipped=ClipPolygon(d.world.inheritedClipPlane?ClipUserPolygon(original,*d.world.inheritedClipPlane):original,d.world.depthClamp);if(clipped.size()<3)return;
			double area=0;for(size_t i=0;i<clipped.size();++i){const auto& a=clipped[i].position;const auto& b=clipped[(i+1)%clipped.size()].position;if(a[3]<=0||b[3]<=0)return;area+=double(a[0])/a[3]*double(b[1])/b[3]-double(b[0])/b[3]*double(a[1])/a[3];}
			if(area==0)return;const bool front=area>0;
			if(d.world.cullEnabled && (d.world.cull==NativeWorldCull::FrontAndBack || (front&&d.world.cull==NativeWorldCull::Front)||(!front&&d.world.cull==NativeWorldCull::Back)))return;
			const auto mode=front?d.world.frontPolygonMode:d.world.backPolygonMode;
			auto state=d.world;state.frontPolygonMode=state.backPolygonMode=NativePolygonMode::Fill;state.cullEnabled=false;
			// Offset on rasterized edges/points needs the original polygon plane,
			// not the slope of the pixel quads used for native coverage.
			Require(mode==NativePolygonMode::Fill || !state.polygonOffset,"Native non-fill polygon offset requires its original plane-depth contract");
			const auto primitive=mode==NativePolygonMode::Fill?(corners.size()>4?NativeDrawPrimitive::Polygon:corners.size()==4?NativeDrawPrimitive::Quads:NativeDrawPrimitive::Triangles):mode==NativePolygonMode::Line?NativeDrawPrimitive::LineLoop:NativeDrawPrimitive::Points;
			if(mode==NativePolygonMode::Fill&&samples>1){
				// The final mixed-mode mesh is remapped to the full attachment.
				// Clip filled faces to the original homogeneous volume first;
				// otherwise that remap would admit formerly out-of-view pixels.
				// Preserve each original fill triangle (including quad diagonals)
				// before clipping; never retriangulate a whole clipped quad.
				auto command=MakeNativeWorldDrawCommand(d.stage,primitive,original,d.surface,state);
				for(size_t triangle=0;triangle<command.indices.size();triangle+=3){
					std::vector<NativeWorldDrawVertex> face{command.vertices[command.indices[triangle]],command.vertices[command.indices[triangle+1]],command.vertices[command.indices[triangle+2]]};
					if(state.inheritedClipPlane)face=ClipUserPolygon(face,*state.inheritedClipPlane);
					face=ClipPolygon(std::move(face),state.depthClamp);if(face.size()<3)continue;
					const auto base=uint32_t(result.vertices.size());result.vertices.insert(result.vertices.end(),face.begin(),face.end());
					for(uint32_t n=1;n+1<face.size();++n)result.indices.insert(result.indices.end(),{base,base+n,base+n+1});
				}
				return;
			}
			// GL clips polygons before polygon-mode rasterization. Clip-created
			// edges are boundary edges (OpenGL2.1 section2.12); retain them, but
			// never introduce native fill-triangulation diagonals.
			auto boundary=mode==NativePolygonMode::Fill?original:clipped;
			// Polygon clipping precedes boundary coverage. The new edge/point
			// quads may cross the user plane, just as original wide GL edges do.
			if(mode!=NativePolygonMode::Fill)state.inheritedClipPlane.reset();
			if(mode==NativePolygonMode::Line)std::rotate(boundary.begin(),boundary.end()-1,boundary.end());
			auto command=MakeNativeWorldDrawCommand(d.stage,primitive,boundary,d.surface,state);
			auto mesh=BuildGeometry(command,viewport,width,height,mode==NativePolygonMode::Line,samples);
			const auto base=uint32_t(result.vertices.size());result.vertices.insert(result.vertices.end(),mesh.vertices.begin(),mesh.vertices.end());
			for(size_t i=0;i<mesh.indices.size();i+=3) {
				result.indices.push_back(base+mesh.indices[i]);
				// Pixel quads retain the original polygon face for two-sided stencil.
				const bool reverse=!front&&mode!=NativePolygonMode::Fill;
				result.indices.push_back(base+mesh.indices[i+(reverse?2:1)]);result.indices.push_back(base+mesh.indices[i+(reverse?1:2)]);
			}
		};
		const auto n=uint32_t(d.vertices.size());
		switch(d.sourcePrimitive) {
			case NativeDrawPrimitive::Polygon:{std::vector<uint32_t> corners;corners.reserve(n);for(uint32_t i=0;i<n;++i)corners.push_back(i);polygon(corners);break;}
			case NativeDrawPrimitive::Quads:for(uint32_t i=0;i+3<n;i+=4)polygon({i,i+1,i+2,i+3});break;
			case NativeDrawPrimitive::Triangles:for(uint32_t i=0;i+2<n;i+=3)polygon({i,i+1,i+2});break;
			case NativeDrawPrimitive::TriangleStrip:for(uint32_t i=0;i+2<n;++i)if(i&1)polygon({i+1,i,i+2});else polygon({i,i+1,i+2});break;
			case NativeDrawPrimitive::TriangleFan:for(uint32_t i=1;i+1<n;++i)polygon({0,i,i+1});break;
			default:throw std::runtime_error("Native polygon source topology invalid");
		}
		return result;
	}
	NativeWorldRasterMesh result;
	const auto screen=[&](const NativeWorldDrawVertex& v){return ScreenVertex{float(viewport[0]+(v.position[0]/v.position[3]+1)*viewport[2]*.5),float(double(height)-viewport[1]-viewport[3]+(v.position[1]/v.position[3]+1)*viewport[3]*.5),v.position[2]/v.position[3],v.color};};
	const auto emit=[&](float x,float y,float z,ScreenColor color,ScreenTexCoord uv){result.vertices.push_back({{float((x-viewport[0])*2/viewport[2]-1),float((y-(double(height)-viewport[1]-viewport[3]))*2/viewport[3]-1),z,1},color,uv});};
	if(d.primitive==NativeDrawPrimitive::Points) {
		const int size=std::max(1,int(std::floor(d.world.pointSize+.5f)));Require(size<=64,"Native world aliased point size exceeds validated bounds");
		for(auto index:d.indices) {
			const auto& v=d.vertices[index];const auto& p=v.position;
			if(p[3]<=0||std::abs(p[0])>p[3]||std::abs(p[1])>p[3]||(!d.world.depthClamp&&std::abs(p[2])>p[3]))continue;
			if(d.world.inheritedClipPlane && PlaneDistance(v,*d.world.inheritedClipPlane)<0)continue;
			const auto at=screen(v);const float cx=size&1?std::floor(at.x)+.5f:std::floor(at.x+.5f),cy=size&1?std::floor(at.y)+.5f:std::floor(at.y+.5f);
			const float x=cx-size*.5f,y=cy-size*.5f;const auto first=uint32_t(result.vertices.size());
			for(auto offset:std::array<ScreenTexCoord,4>{{{0,0},{float(size),0},{float(size),float(size)},{0,float(size)}}})emit(x+offset[0],y+offset[1],at.z,v.color,v.uv);
			for(uint32_t i:{0u,1u,2u,0u,2u,3u})result.indices.push_back(first+i);
		}return result;
	}
	Require(d.surface.material==ScreenMaterial::Solid && !d.surface.secondary.texture,"Native world textured lines require an independent original raster/sampling oracle");
	struct Segment{NativeWorldDrawVertex a,b;};
	std::vector<Segment> segments;std::vector<ScreenLine> lines;uint64_t multisampleStippleCounter=0;
	for(size_t index=0;index<d.indices.size();index+=2) {
		auto a=d.vertices[d.indices[index]],b=d.vertices[d.indices[index+1]];
		if(d.world.inheritedClipPlane && !ClipUserPlane(a,b,*d.world.inheritedClipPlane))continue;
		if(!Clip(a,b,d.world.depthClamp))continue;
		const auto sa=screen(a),sb=screen(b);
		if(samples>1){
			Require(d.world.lineWidth<=64,"Native multisample line width exceeds supported range");
			if(d.sourcePrimitive==NativeDrawPrimitive::Lines)multisampleStippleCounter=0;
			const double dx=double(sb.x)-sa.x,dy=double(sb.y)-sa.y,length=std::hypot(dx,dy);if(length==0)continue;
			const double nx=-dy/length*d.world.lineWidth*.5,ny=dx/length*d.world.lineWidth*.5;
			NativeMultisampleLineIntervals(length,d.world.lineStipple,d.world.stipplePattern,d.world.stippleFactor,multisampleStippleCounter,[&](double t0,double t1){
				const auto first=uint32_t(result.vertices.size());
				for(const auto& corner:std::array<std::array<double,2>,4>{{{t0,1},{t0,-1},{t1,-1},{t1,1}}}){
					const double t=corner[0],side=corner[1],inverseW=(1-t)/a.position[3]+t/b.position[3];
					Require(std::isfinite(inverseW)&&inverseW>0,"Native multisample line interpolation invalid");
					auto v=Mix(a,b,(t/b.position[3])/inverseW);const double w=1/inverseW;
					const double x=sa.x+dx*t+nx*side,y=sa.y+dy*t+ny*side;
					v.position={float(((x-viewport[0])*2/viewport[2]-1)*w),float(((y-(double(height)-viewport[1]-viewport[3]))*2/viewport[3]-1)*w),float((sa.z+(double(sb.z)-sa.z)*t)*w),float(w)};
					result.vertices.push_back(v);
				}
				for(uint32_t offset:{0u,1u,2u,0u,2u,3u})result.indices.push_back(first+offset);
				Require(result.vertices.size()<=4*1024*1024,"Native multisample line output exceeds geometry bounds");
			});
			continue;
		}
		const float tag=float(segments.size());segments.push_back({a,b});
		// Carry the segment id and linear interpolation parameter through the
		// common rasterizer. Its snapped major axis must also select the
		// perspective interpolation axis, including every wide-line replica.
		auto taggedA=sa,taggedB=sb;taggedA.color={tag,0,0,1};taggedB.color={tag,1,0,1};
		lines.push_back({taggedA,taggedB,d.world.lineWidth,d.world.lineStipple,d.world.stipplePattern,d.world.stippleFactor,lines.empty()||d.sourcePrimitive==NativeDrawPrimitive::Lines,polygonEdge});
	}
	if(samples>1)return result;
	ScreenDrawList raster;AppendNativeScreenLines(raster,lines,{},width,height);
	for(size_t index=0;index<raster.Vertices().size();index+=4) {
		const auto pixels=raster.Vertices().subspan(index,4);const auto& segment=segments.at(size_t(pixels[0].color[0]));
		const double t=pixels[0].color[1];
		const double inverseW=(1-t)/segment.a.position[3]+t/segment.b.position[3];
		Require(std::isfinite(inverseW)&&inverseW>0,"Native world line perspective interpolation invalid");
		const auto sample=Mix(segment.a,segment.b,(t/segment.b.position[3])/inverseW);
		for(const auto& pixel:pixels)emit(pixel.x,pixel.y,pixel.z,sample.color,sample.uv);
	}
	result.indices.assign(raster.Indices().begin(),raster.Indices().end());return result;
}
NativeWorldRasterMesh BuildNativeWorldDrawGeometry(const NativeWorldDrawCommand& d,const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned samples)
{
	auto mesh=BuildGeometry(d,viewport,width,height,false,samples);
	if(samples>1&&(d.primitive==NativeDrawPrimitive::Lines||(d.primitive==NativeDrawPrimitive::Triangles&&(d.world.frontPolygonMode!=NativePolygonMode::Fill||d.world.backPolygonMode!=NativePolygonMode::Fill)))){
		// Clip the source primitive first, then expand its raster footprint.
		// A viewport is not a scissor: the wide rectangle may extend past it.
		// Draw the expanded mesh in full attachment space so the GPU does not
		// incorrectly clip those expanded corners to the original NDC box.
		for(auto& vertex:mesh.vertices){auto& p=vertex.position;
			p[0]=float((viewport[0]*2/width-1)*p[3]+(double(p[0])+p[3])*viewport[2]/width);
			p[1]=float(((double(height)-viewport[1]-viewport[3])*2/height-1)*p[3]+(double(p[1])+p[3])*viewport[3]/height);
		}
		mesh.fullViewport=true;
	}
	return mesh;
}
}
