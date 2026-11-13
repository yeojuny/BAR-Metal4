#include "Rendering/Backend/NativeOwnedWorldGeometry.h"
#include <cassert>
#include <cstring>
#include <bit>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <new>
using namespace rendering;
static bool denyAllocation=false;
void* operator new(std::size_t n){if(denyAllocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
static const NativeWorldDrawCommand* expectedSource;
static NativeOwnedWorldGeometry::Target expectedTarget;
static int calls=0,fail=0;
struct Marker{};
namespace rendering {
// Explicit mock: verifies payload ownership/forwarding, not raster correctness.
NativeWorldRasterMesh BuildNativeWorldDrawGeometry(const NativeWorldDrawCommand& d,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){
 ++calls;assert(&d==expectedSource);assert(std::memcmp(vp.data(),expectedTarget.viewport.data(),sizeof(double)*4)==0);assert(w==expectedTarget.width&&h==expectedTarget.height&&s==expectedTarget.samples);
 if(fail==1)throw Marker{};if(fail==2)throw std::bad_alloc();
 NativeWorldRasterMesh result;result.vertices.resize(2);result.indices={1,0};result.pointDisks.push_back({1,2,3,4});result.fullViewport=true;return result;
}}
template<class F> void Logic(F&& f){bool threw=false;try{f();}catch(const std::logic_error&){threw=true;}assert(threw);}
int main(){
 static_assert(std::is_nothrow_constructible_v<NativeOwnedWorldGeometry,std::shared_ptr<const NativeWorldDrawCommand>,NativeOwnedWorldGeometry::Target>);
 static_assert(!std::is_copy_constructible_v<NativeOwnedWorldGeometry> && !std::is_move_constructible_v<NativeOwnedWorldGeometry>);
 for(int mode=0;mode<3;++mode){
  auto frame=std::make_shared<NativeWorldDrawFrame>();frame->draws.resize(2);frame->draws[1].sequence=17;
  std::weak_ptr<const NativeWorldDrawFrame> weak=frame;
  denyAllocation=true;auto source=std::shared_ptr<const NativeWorldDrawCommand>(frame,&frame->draws[1]);denyAllocation=false;expectedSource=source.get();
  expectedTarget={{-0.0,std::bit_cast<double>(uint64_t{0x7ff8000000000017}),13.25,-71.5},1023,517,1001,509,4};calls=0;fail=mode;
  {
   // Alias construction and payload construction allocate no storage.
   denyAllocation=true;NativeOwnedWorldGeometry job(std::move(source),expectedTarget);denyAllocation=false;
   frame.reset();assert(!weak.expired());Logic([&]{job.Take();});assert(calls==0);
   job.Execute();assert(calls==1);Logic([&]{job.Execute();});assert(calls==1);
   if(mode==0){auto result=job.Take();assert(result.vertices.size()==2&&result.indices==std::vector<uint32_t>({1,0})&&result.pointDisks.size()==1&&result.fullViewport);}
   else {bool caught=false;try{job.Take();}catch(const Marker&){assert(mode==1);caught=true;}catch(const std::bad_alloc&){assert(mode==2);caught=true;}assert(caught);}
   Logic([&]{job.Take();});Logic([&]{job.Execute();});assert(calls==1);
  }
  assert(weak.expired());
 }
 // An explicitly joined external executor is a helper exercise only, not an
 // assertion that typed production inputs are eligible for concurrent access.
 auto frame=std::make_shared<NativeWorldDrawFrame>();frame->draws.resize(1);auto source=std::shared_ptr<const NativeWorldDrawCommand>(frame,&frame->draws[0]);expectedSource=source.get();expectedTarget={{0,0,32,32},32,32,32,32,1};calls=0;fail=0;
 NativeOwnedWorldGeometry job(source,expectedTarget);std::thread t([&]{job.Execute();});t.join();auto result=job.Take();assert(calls==1&&result.indices.size()==2);
}
