#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include <cassert>
#include <cstring>
#include <random>
#include <cstdlib>
#include <new>
#include <cmath>
static int failAfter=-1,allocations=0;
void* operator new(std::size_t n){++allocations;if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#include "clipped-world-fill.inc"
using namespace rendering;
void Same(const NativeWorldRasterMesh& a,const NativeWorldRasterMesh& b){assert(a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&a.pointDisks==b.pointDisks&&a.fullViewport==b.fullViewport);if(!a.vertices.empty())assert(std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(NativeWorldDrawVertex))==0);}
int main(){std::mt19937 rng(0xc11f);
 for(int run=0;run<4000;++run){NativeWorldDrawCommand d;d.vertices.resize(3*(rng()%12));for(auto& v:d.vertices){for(auto* values:{&v.position,&v.color,&v.uv})for(auto& x:*values)x=(int(rng()%2001)-1000)/37.f;v.position[3]=(rng()%1000+1)/103.f;}
  for(size_t n=0;n<d.vertices.size();++n)d.indices.push_back(rng()%d.vertices.size());std::array<float,4> plane{};for(auto& x:plane)x=(int(rng()%31)-15)/7.f;d.world.inheritedClipPlane=plane;
  allocations=0;auto ref=Reference(d);int count=allocations;allocations=0;auto actual=BuildValidatedNativeWorldClippedFill(d.vertices,d.indices,plane);assert(allocations==count);Same(ref,actual);
 }
 // Explicit boundary vertices, duplicate corners, zero-area triangles, all-out,
 // all-on-plane and perspective-varying homogeneous w.
 for(auto positions:{std::array<float,3>{0,0,0},{-1,0,1},{-1,-1,-1},{1,1,1},{0,-1,0}}){NativeWorldDrawCommand d;d.vertices.resize(3);d.indices={0,1,2,2,1,0,0,0,0};for(int i=0;i<3;++i){d.vertices[i].position={positions[i],float(i),0,float(i+1)};d.vertices[i].color={1,.5,0,1};d.vertices[i].uv={float(i),0,1,0};}d.world.inheritedClipPlane=std::array<float,4>{1,0,0,0};Same(Reference(d),BuildValidatedNativeWorldClippedFill(d.vertices,d.indices,*d.world.inheritedClipPlane));}
 NativeWorldDrawCommand d;d.vertices.resize(3);d.vertices[0].position={-1,0,0,1};d.vertices[1].position={1,1,0,2};d.vertices[2].position={1,-1,0,3};d.indices={0,1,2,2,1,0};d.world.inheritedClipPlane=std::array<float,4>{1,0,0,0};allocations=0;auto baseline=Reference(d);const int total=allocations;
 for(int f=0;f<=total;++f){bool a=false,b=false;int ac,bc;allocations=0;failAfter=f;try{auto r=Reference(d);}catch(const std::bad_alloc&){a=true;}failAfter=-1;ac=allocations;allocations=0;failAfter=f;try{auto r=BuildValidatedNativeWorldClippedFill(d.vertices,d.indices,*d.world.inheritedClipPlane);}catch(const std::bad_alloc&){b=true;}failAfter=-1;bc=allocations;assert(a==b&&ac==bc);}
}
