#include "Rendering/Backend/NativeClippedFillPrecompute.h"
#include <cassert>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>
#include <iostream>
#include <chrono>
using namespace rendering;
static const auto caller=std::this_thread::get_id();
static std::atomic<bool> failWorker{false};
static thread_local bool failCaller=false;
static std::atomic<bool> pauseWorker{false},enteredWorker{false},releaseWorker{false};
void* operator new(std::size_t n){
 if(std::this_thread::get_id()!=caller&&pauseWorker.exchange(false)){enteredWorker=true;while(!releaseWorker.load())std::this_thread::yield();}
 if(failCaller||(failWorker.load()&&std::this_thread::get_id()!=caller))throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
static auto Frame(){
 auto frame=std::make_shared<NativeWorldDrawFrame>();frame->draws.resize(3);
 for(auto& d:frame->draws){d.primitive=d.sourcePrimitive=NativeDrawPrimitive::Triangles;d.world.frontPolygonMode=d.world.backPolygonMode=NativePolygonMode::Fill;d.surface.material=ScreenMaterial::Solid;d.world.inheritedClipPlane=std::array<float,4>{1,0,0,0};d.vertices.resize(3);d.vertices[0].position={-1.1f,0,0,1};d.vertices[1].position={1.3f,1,0,2};d.vertices[2].position={1.7f,-1,0,3};d.indices={0,1,2};}
 return frame;
}
static void Same(const NativeWorldRasterMesh& a,const NativeWorldRasterMesh& b){assert(a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&a.fullViewport==b.fullViewport&&a.pointDisks==b.pointDisks);assert(a.vertices.empty()||std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(NativeWorldDrawVertex))==0);}
static auto all=[](const auto&){return true;};
int main(){
 using Job=NativeClippedFillPrecompute;
 for(int rounding:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  if(std::fesetround(rounding)!=0)continue;
  auto f=Frame();Job job(f,all,{});assert(job.CandidateCount()==3);assert(job.Start());assert(!job.Start());
  NativeWorldRasterMesh out;std::fenv_t captured{};std::fegetenv(&captured);
  auto unrelated=Frame();assert(!job.TryTake(unrelated,&unrelated->draws[0],out));
  // Same address but unrelated control block must not match.
  std::shared_ptr<const NativeWorldDrawFrame> falseOwner(f.get(),[](auto*){});
  assert(!job.TryTake(falseOwner,&f->draws[0],out));
  NativeWorldDrawCommand foreign;assert(!job.TryTake(f,&foreign,out));
  for(size_t i:{2,0,1}){std::fesetenv(&captured);assert(job.TryTake(f,&f->draws[i],out));const int actualFlags=std::fetestexcept(FE_ALL_EXCEPT);std::fesetenv(&captured);auto ref=BuildValidatedNativeWorldClippedFill(f->draws[i].vertices,f->draws[i].indices,*f->draws[i].world.inheritedClipPlane);Same(out,ref);assert(actualFlags==std::fetestexcept(FE_ALL_EXCEPT));assert(!job.TryTake(f,&f->draws[i],out));}
  assert(std::fegetround()==rounding);
 }
 std::fesetround(FE_TONEAREST);
 {auto f=Frame();Job job(f,all,{});assert(job.Start());if(std::fesetround(FE_UPWARD)==0){NativeWorldRasterMesh out;assert(!job.TryTake(f,&f->draws[0],out));}std::fesetround(FE_TONEAREST);}
 {auto f=Frame();Job job(f,all,{});job.CancelAndJoin();assert(job.Status()==Job::State::Cancelled);assert(!job.Start());NativeWorldRasterMesh out;assert(!job.TryTake(f,&f->draws[0],out));}
 {auto f=Frame();Job job(f,all,{});failWorker=true;assert(job.Start());NativeWorldRasterMesh out;assert(!job.TryTake(f,&f->draws[0],out));failWorker=false;assert(job.Status()==Job::State::Ready);}
 {auto f=Frame();failCaller=true;Job job(f,all,{});failCaller=false;assert(job.CandidateCount()==0&&!job.Start());}
 {auto f=Frame();Job job(f,[](const auto&)->bool{throw std::runtime_error("optional predicate");},{});assert(!job.Start());}
 {auto f=Frame();f->draws[0].indices={99,0,1};f->draws[1].vertices[1].position[0]=std::numeric_limits<float>::infinity();f->draws[2].world.inheritedClipPlane.reset();Job job(f,all,{});assert(!job.Start());}
 {auto f=Frame();Job::Limits limit;limit.candidates=1;Job job(f,all,limit);assert(job.CandidateCount()==1);}
 {auto f=Frame();Job::Limits limit;limit.indices=2;Job job(f,all,limit);assert(job.CandidateCount()==0);}
 {auto f=Frame();Job::Limits limit;limit.logicalOutputBytes=1;Job job(f,all,limit);assert(job.CandidateCount()==0);}
 {auto f=Frame();Job::Limits limit;limit.scanCommands=1;Job job(f,all,limit);assert(job.CandidateCount()==1);}
 {auto f=Frame();Job job(f,all,{});failCaller=true;assert(!job.Start());failCaller=false;assert(job.Status()==Job::State::Cancelled);}
 {auto f=Frame();std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_DIVBYZERO);const int flags=std::fetestexcept(FE_ALL_EXCEPT);
  f->draws[0].vertices[0].position[0]=std::bit_cast<float>(uint32_t{0x7f800001});
  Job job(f,all,{});assert(job.CandidateCount()==2);assert(std::fetestexcept(FE_ALL_EXCEPT)==flags);assert(job.Start());
  std::feraiseexcept(FE_INVALID);const int changed=std::fetestexcept(FE_ALL_EXCEPT);NativeWorldRasterMesh out;
  assert(!job.TryTake(f,&f->draws[1],out));assert(std::fetestexcept(FE_ALL_EXCEPT)==changed);job.CancelAndJoin();assert(std::fetestexcept(FE_ALL_EXCEPT)==changed);std::feclearexcept(FE_ALL_EXCEPT);
 }
 {auto f=Frame();f->draws[0].indices[0]=99;Job::Limits limit;limit.indices=3;Job job(f,all,limit);assert(job.CandidateCount()==0&&!job.Start());}
 {auto f=Frame();Job job(f,all,{});enteredWorker=false;releaseWorker=false;pauseWorker=true;assert(job.Start());
  while(!enteredWorker.load())std::this_thread::yield();assert(job.Status()==Job::State::Running);
  std::atomic<bool> cancellationRequested{false};
  std::thread release([&]{while(!cancellationRequested.load())std::this_thread::yield();releaseWorker=true;});
  cancellationRequested=true;job.CancelAndJoin();release.join();assert(job.Status()==Job::State::Ready);NativeWorldRasterMesh out;assert(!job.TryTake(f,&f->draws[0],out));
 }
 // Main-owned alias destruction is after worker settlement, including discarded
 // pending/running/ready results. No worker ever owns a final shared_ptr copy.
 struct Holder{NativeWorldDrawFrame frame;bool* destroyed;~Holder(){assert(std::this_thread::get_id()==caller);*destroyed=true;}};
 for(int n=0;n<100;++n){bool destroyed=false;auto holder=std::make_shared<Holder>();holder->frame=*Frame();holder->destroyed=&destroyed;std::shared_ptr<const NativeWorldDrawFrame> owner(holder,&holder->frame);{
  Job job(owner,all,{});assert(job.Start());owner.reset();holder.reset();assert(!destroyed);job.CancelAndJoin();assert(!destroyed);
 }assert(destroyed);}
 std::cout<<"Actual pure kernel: owner identity, optional failures, bounded selection, pre-start cancel, join-before-release, 100 teardown cycles and rounding-mode parity PASS\n";
}
