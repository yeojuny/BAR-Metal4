/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeWorldParticleGeometry.h"
#include "Rendering/Backend/NativeLuaNanoRecorder.h"
#include "System/RadixSort.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace rendering;
#define RECOIL_DETAILED_TRACY_ZONE
static unsigned checks=0;
void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<typename F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected,"invalid original range accepted");}
struct float3 {
	float x=0,y=0,z=0;
	float3 operator+(float3 b)const{return {x+b.x,y+b.y,z+b.z};}
	float3 operator-(float3 b)const{return {x-b.x,y-b.y,z-b.z};}
	float3 operator*(float s)const{return {x*s,y*s,z*s};}
};
struct float4 {float x,y,z,w;};
struct SColor {
	uint8_t bytes[4]{};
	SColor()=default;SColor(const uint8_t* p){std::copy_n(p,4,bytes);}
	SColor(uint8_t r,uint8_t g,uint8_t b,uint8_t a):bytes{r,g,b,a}{}
	const uint8_t& operator[](size_t i)const{return bytes[i];}
};
struct VA_TYPE_TC {float3 pos;float s,t;SColor c;};
struct VA_TYPE_PROJ {float3 pos,uvw;float4 uvInfo;float3 aparams;SColor c;};
template<typename T>class TypedRenderBuffer {
public:
	using VertType=T;using IndcType=uint32_t;
	std::vector<T> verts;std::vector<uint32_t> indcs;bool readOnly=false;
	const auto& GetElems()const{return verts;}const auto& GetIndcs()const{return indcs;}
	void AddQuadTriangles(T tl,T tr,T br,T bl){AddQuadTrianglesImpl(tl,tr,br,bl);}
	void Clear(){verts.clear();indcs.clear();}
	template<typename TT>/*@@QUAD@@*/
};
struct RenderBuffer {template<typename T>static auto& GetTypedRenderBuffer(){static TypedRenderBuffer<T> value;return value;}};
class CExpGenSpawnable {
public:
	static void SetGeomFillTarget(TypedRenderBuffer<VA_TYPE_PROJ>*);
	static TypedRenderBuffer<VA_TYPE_PROJ>* GetGeomFillTarget();
	static TypedRenderBuffer<VA_TYPE_PROJ>& GetPrimaryRenderBuffer();
	/*@@SCOPE@@*/;
	static void AddEffectsQuadImpl(uint32_t,const VA_TYPE_TC&,const VA_TYPE_TC&,const VA_TYPE_TC&,const VA_TYPE_TC&,const float3&,const float&);
	static void AddEffectsQuadImpl(uint32_t,const VA_TYPE_TC&,const VA_TYPE_TC&,const VA_TYPE_TC&,const VA_TYPE_TC&);
	template<unsigned I>void AddEffectsQuad(uint32_t page,const VA_TYPE_TC& a,const VA_TYPE_TC& b,const VA_TYPE_TC& c,const VA_TYPE_TC& d){static_assert(I==0);AddEffectsQuadImpl(page,a,b,c,d);}
};
static thread_local TypedRenderBuffer<VA_TYPE_PROJ>* geomFillTarget=nullptr;
/*@@DESTINATION@@*/
/*@@EFFECTS@@*/
/*@@APPEND@@*/
struct Camera {float3 right{1,0,0},up{0,1,0};const auto& GetRight()const{return right;}const auto& GetUp()const{return up;}};
struct GlobalRendering {float timeOffset=0;} renderingValue;
static GlobalRendering* globalRendering=&renderingValue;
static Camera cameraValue;static Camera* camera=&cameraValue;
struct AtlasTexture {float xstart=.125f,xend=.75f,ystart=.25f,yend=.875f;uint32_t pageNum=3;} atlas;
struct ProjectileDrawer {const AtlasTexture* GetSmokeTexture(unsigned)const{return &atlas;}} drawerValue;
static ProjectileDrawer* projectileDrawer=&drawerValue;
struct CSmokeProjectile:CExpGenSpawnable {float color=.5f,age=.25f,size=4,sizeExpansion=2;float3 drawPos{10,20,30};unsigned textureNum=0;void Draw();};
/*@@SMOKE@@*/
struct CProjectile {int drawOrder=0;float distance=0;uint8_t drawFlag=0;float GetSortDist(uint32_t)const{return distance;}};
/*@@SORT@@*/
NativeBufferSnapshotRef Source(NativeBufferLayout layout,std::vector<uint8_t> bytes)
{NativeBufferSnapshotSource source;source.Define(layout);return source.Capture(bytes);}
template<typename T>std::vector<uint8_t> Bytes(const std::vector<T>& values)
{std::vector<uint8_t> result(values.size()*sizeof(T));std::memcpy(result.data(),values.data(),result.size());return result;}
NativeLuaNanoDraw Nano(int shape,bool wide=false)
{
	NativeLuaNanoDraw draw;draw.shape=shape;draw.instanceCount=2;
	NativeBufferLayout slots{NativeBufferKind::Vertex,28,4,{{0,"vertexSlot",NativeScalarType::Float32,NativeValueShape::Scalar,1,0,4,false}}};
	std::vector<float> positions(28);for(unsigned n=0;n<28;++n)positions[n]=n;draw.vertexSlots=Source(slots,Bytes(positions));
	std::vector<uint32_t> topology;
	if(shape==0)for(unsigned q=0;q<6;++q)for(unsigned n:{0u,1u,2u,0u,2u,3u})topology.push_back(q*4+n);
	else for(unsigned n=0;n<24;++n)topology.push_back(n);
	for(unsigned n:{0u,1u,2u,0u,2u,3u})topology.push_back(24+n);
	NativeBufferLayout indices{NativeBufferKind::Index,topology.size(),wide?4u:2u,{{0,"",wide?NativeScalarType::UInt32:NativeScalarType::UInt16,NativeValueShape::Scalar,1,0,wide?4u:2u,false}}};
	if(wide)draw.indices=Source(indices,Bytes(topology));else draw.indices=Source(indices,Bytes(std::vector<uint16_t>(topology.begin(),topology.end())));
	NativeBufferLayout instances{NativeBufferKind::Vertex,3,64,{}};
	for(unsigned n=0;n<4;++n)instances.attributes.push_back({n+1,"",NativeScalarType::Float32,NativeValueShape::Scalar,4,n*16,16,false});
	std::vector<float> seed(48);for(unsigned n=0;n<48;++n)seed[n]=float(n)+.25f;
	draw.instances=Source(instances,Bytes(seed));draw.uniforms={{"drawRadius",1.5f},{"losAlwaysVisible",0}};return draw;
}
int main()
{
	try {
		TypedRenderBuffer<VA_TYPE_PROJ> first,second;Check(CExpGenSpawnable::GetGeomFillTarget()==nullptr,"initial destination");
		{CExpGenSpawnable::ScopedGeomFillTarget outer(&first);Check(CExpGenSpawnable::GetGeomFillTarget()==&first,"outer destination");
			{CExpGenSpawnable::ScopedGeomFillTarget inner(&second);Check(CExpGenSpawnable::GetGeomFillTarget()==&second,"nested destination");}
			Check(CExpGenSpawnable::GetGeomFillTarget()==&first,"nested restore");try{CExpGenSpawnable::ScopedGeomFillTarget inner(&second);throw std::runtime_error("draw error");}catch(const std::exception&){}
			Check(CExpGenSpawnable::GetGeomFillTarget()==&first,"exception restore");}
		Check(CExpGenSpawnable::GetGeomFillTarget()==nullptr,"outer restore");
		for(float age:{0.f,.25f,.9f})for(float time:{0.f,.5f,.99f})for(float size:{1.f,5.f,32.f}) {
			first.Clear();CSmokeProjectile smoke;smoke.age=age;smoke.size=size;globalRendering->timeOffset=time;
			{CExpGenSpawnable::ScopedGeomFillTarget scope(&first);smoke.Draw();}
			Check(first.GetElems().size()==4,"original smoke vertex count");Check(first.GetIndcs()==std::vector<uint32_t>({3,0,1,3,1,2}),"original triangle order");
			const float r=size+2*time;const std::array<float3,4> expected{{{10-r,20-r,30},{10+r,20-r,30},{10+r,20+r,30},{10-r,20+r,30}}};
			const auto alpha=uint8_t((1-age)*255);const auto rgb=uint8_t(.5f*alpha);
			for(unsigned n=0;n<4;++n){const auto& v=first.GetElems()[n];Check(v.pos.x==expected[n].x&&v.pos.y==expected[n].y&&v.pos.z==expected[n].z,"independent original smoke billboard position");
				Check(v.c[0]==rgb&&v.c[1]==rgb&&v.c[2]==rgb&&v.c[3]==alpha,"independent original smoke premultiplied color");Check(v.uvw.z==3&&v.uvInfo.x==.125f&&v.uvInfo.y==.25f&&v.uvInfo.z==.625f&&v.uvInfo.w==.625f,"original page and UV extent");Check(v.aparams.x==1&&v.aparams.y==1&&v.aparams.z==0,"original unanimated parameters");}
			NativeWorldParticleFrame frame;Append(frame,first,{});Append(frame,first,{});Check(frame.vertices.size()==8&&frame.indices==std::vector<uint32_t>({3,0,1,3,1,2,7,4,5,7,5,6}),"independent append bias");Check(frame.ranges[1].firstIndex==6&&frame.ranges[1].indexCount==6,"range addresses");
			const auto old=frame.indices;auto invalid=first.GetIndcs();invalid.back()=99;Reject([&]{AppendNativeParticleGeometry(frame,std::span(frame.vertices).first(4),invalid,{});});Check(frame.indices==old&&frame.vertices.size()==8&&frame.ranges.size()==2,"invalid geometry has no partial append");
			AppendNativeParticleGeometry(frame,std::span(frame.vertices).first(4),std::span(frame.indices).first(6),{});Check(frame.vertices.size()==12&&frame.indices.back()==10&&frame.indices[12]==11,"aliased append stages source before relocation");
		}
		std::vector<CProjectile> particles{{-1,1},{0,5},{0,25},{0,25},{1,100}};std::vector<std::pair<uint64_t,unsigned>> order;
		for(unsigned n=0;n<particles.size();++n)order.push_back({ParticleSortKey(&particles[n],0,true),n});std::vector<std::pair<uint64_t,unsigned>> scratch;
		RadixSortByKey(order,scratch,[](const auto& p){return p.first;});Check(order[0].second==0&&order[1].second==2&&order[2].second==3&&order[3].second==1&&order[4].second==4,"original signed draw order and stable far-to-near ties");
		for(int shape:{0,1})for(bool wide:{false,true}) {
			auto draw=Nano(shape,wide);ValidateNativeLuaNanoDraw(draw);Check(draw.instances->bytes.size()==192,"original capacity not live count");
			NativeLuaNanoRecorder recorder;recorder.Begin();Reject([&]{recorder.Begin();});recorder.Record(draw);auto ghost=draw;ghost.ghostStencilPass=true;recorder.Record(ghost);
			auto bad=draw;bad.instanceCount=4;Reject([&]{recorder.Record(bad);});auto frame=recorder.End();Check(frame->draws.size()==2&&frame->draws[1].ghostStencilPass,"per-draw error preserves prior original submissions");
			Check(frame->draws[0].instances==draw.instances,"immutable actual source retained");Reject([&]{recorder.End();});recorder.Begin();recorder.Abort();Check(!recorder.Active(),"explicit abort");Reject([&]{recorder.End();});
			recorder.Begin();NativeLuaNanoFrame resources;resources.mapSquares={16,8,16,8};resources.cameraViewInverse[0]=2;resources.cameraViewProjection[0]=3;recorder.SetFrameResources(resources);resources.mapSquares[0]=32;Reject([&]{recorder.SetFrameResources(resources);});auto metadata=recorder.End();Check(metadata->mapSquares[0]==16&&metadata->cameraViewInverse[0]==2&&metadata->cameraViewProjection[0]==3,"frame resource rejection preserves prior exact metadata");
			auto altered=std::make_shared<NativeBufferSnapshot>(*draw.indices);altered->bytes[0]=27;bad=draw;bad.indices=altered;Reject([&]{ValidateNativeLuaNanoDraw(bad);});
			altered=std::make_shared<NativeBufferSnapshot>(*draw.instances);altered->layout.attributes[1].offset=20;bad=draw;bad.instances=altered;Reject([&]{ValidateNativeLuaNanoDraw(bad);});
		}
		Check(!IsNativeLuaNanoConsumerAvailable()&&!IsNativeEngineNanoConsumerAvailable(),"no false consumer availability from CPU-only tests");
		std::cout<<"PASS "<<checks<<" original particle geometry/TLS/sort and native nano schema/lifetime checks\n";
	} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
