#import <objc/runtime.h>
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#include <algorithm>
#include <array>
#include <chrono>

// The worker payload is exactly immutable numeric uniform snapshots. Mixed
// source frames stay on the caller and are never captured by the worker block.
static bool TerrainUniformReleaseEnabled220(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_TERRAIN_UNIFORM_RELEASE220");return p&&std::string_view(p)=="1";}();return v;}
static bool TerrainUniformReleaseCounters220(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_TERRAIN_UNIFORM_RELEASE220_COUNTERS");return p&&std::string_view(p)=="1";}();return v;}
static uint64_t TerrainUniformNow220(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
using TerrainUniformOwner220=std::shared_ptr<const rendering::NativeTerrainUniformSnapshot>;
static constexpr size_t TerrainUniformMaxSquares220=4096;
@interface M4TerrainUniformSources220:NSObject {@public std::array<std::shared_ptr<const rendering::NativeWaterGroundSourceFrame>,2> sources;}
@end
@implementation M4TerrainUniformSources220
@end
@interface M4TerrainUniformJob220:NSObject {@public std::vector<TerrainUniformOwner220> owners;dispatch_semaphore_t capacity;}
@end
@implementation M4TerrainUniformJob220
-(void)dealloc {
 // A collection/allocation failure releases the reservation synchronously.
 // Worker success explicitly clears owners before its last job reference.
 owners.clear();
 if(capacity)dispatch_semaphore_signal(capacity);
}
@end
@interface M4TerrainUniformWorker220:NSObject {@public dispatch_queue_t worker;dispatch_semaphore_t capacity;uint64_t identity;bool stopped;unsigned receipts;}
@end
@implementation M4TerrainUniformWorker220
-(instancetype)init {if((self=[super init])){static std::atomic<uint64_t> ids{0};identity=++ids;capacity=dispatch_semaphore_create(1);worker=dispatch_queue_create("org.beyondallreason.metal4.completed-terrain-uniform-release",DISPATCH_QUEUE_SERIAL);}return self;}
@end
static char terrainUniformWorkerKey220,terrainUniformSourcesKey220;
static M4TerrainUniformWorker220* TerrainUniformWorker220(id queue,bool create){
 if(!queue)return nil;auto worker=(M4TerrainUniformWorker220*)objc_getAssociatedObject(queue,&terrainUniformWorkerKey220);
 if(!worker&&create){worker=[M4TerrainUniformWorker220 new];objc_setAssociatedObject(queue,&terrainUniformWorkerKey220,worker,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}return worker;
}
static M4TerrainUniformSources220* TerrainUniformSources220(NSArray* graph){
 if(!TerrainUniformReleaseEnabled220()&&!TerrainUniformReleaseCounters220())return nil;
 return (M4TerrainUniformSources220*)objc_getAssociatedObject(graph,&terrainUniformSourcesKey220);
}
static void StopTerrainUniformWorker220(id queue){
 if(!TerrainUniformReleaseEnabled220()&&!TerrainUniformReleaseCounters220())return;
 auto worker=TerrainUniformWorker220(queue,false);if(!worker)return;worker->stopped=true;
 // Runtime destruction holds no requestMutex here. Worker never acquires it.
 dispatch_sync(worker->worker,^{});objc_setAssociatedObject(queue,&terrainUniformWorkerKey220,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
struct TerrainUniformReceipt220 {uint64_t runtime=0,event=0;bool completed=false;};
static TerrainUniformReceipt220 StampTerrainUniform220(id queue,M4TerrainUniformSources220* sources,uint64_t event){
 if(!sources||!event)return {};
 auto worker=TerrainUniformWorker220(queue,true);return worker?TerrainUniformReceipt220{worker->identity,event,false}:TerrainUniformReceipt220{};
}
static void CompleteTerrainUniform220(id queue,TerrainUniformReceipt220& receipt,uint64_t event){
 if(!receipt.event)return;
 auto worker=TerrainUniformWorker220(queue,false);
 if(worker&&!worker->stopped&&receipt.runtime==worker->identity&&receipt.event==event)receipt.completed=true;
}
struct TerrainUniformAttempt220 {
 bool observed=false,eligible=false,saturated=false,overflow=false,allocationFailure=false;
 uint64_t event=0,frame=0,start=0,collectNS=0;size_t squares=0,unique=0;
};
static M4TerrainUniformJob220* PrepareTerrainUniformRelease220(id queue,M4TerrainUniformSources220* sources,
 const TerrainUniformReceipt220& receipt,uint64_t event,uint64_t frame,TerrainUniformAttempt220& attempt) noexcept {
 if(!sources)return nil;
 auto worker=TerrainUniformWorker220(queue,false);if(!worker)return nil;
 attempt.observed=TerrainUniformReleaseCounters220()&&frame>=600&&worker->receipts++<64;
 attempt.event=event;attempt.frame=frame;attempt.start=attempt.observed?TerrainUniformNow220():0;
 attempt.eligible=!worker->stopped&&receipt.completed&&event&&receipt.event==event&&receipt.runtime==worker->identity;
 if(!TerrainUniformReleaseEnabled220()||!attempt.eligible)return nil;
 // Admission happens before scanning or retaining any per-draw leaf.
 if(dispatch_semaphore_wait(worker->capacity,DISPATCH_TIME_NOW)!=0){attempt.saturated=true;return nil;}
 auto job=[M4TerrainUniformJob220 new];
 if(!job){dispatch_semaphore_signal(worker->capacity);attempt.allocationFailure=true;return nil;}
 job->capacity=worker->capacity;
 try {
  for(const auto& source:sources->sources)if(source){
   if(source->squares.size()>TerrainUniformMaxSquares220-attempt.squares){attempt.overflow=true;return nil;}
   attempt.squares+=source->squares.size();
  }
  if(!attempt.squares)return nil;
  // Raw pointers refer to shared_ptr fields in the still-owned immutable
  // sources. Sort/dedup does not churn their reference counts or copy maps.
  std::vector<const TerrainUniformOwner220*> refs;refs.reserve(attempt.squares);
  for(const auto& source:sources->sources)if(source)for(const auto& draw:source->squares)
   if(draw.program.uniforms)refs.push_back(&draw.program.uniforms);
  std::sort(refs.begin(),refs.end(),[](auto a,auto b){return std::less<const void*>{}(a->get(),b->get());});
  refs.erase(std::unique(refs.begin(),refs.end(),[](auto a,auto b){return a->get()==b->get();}),refs.end());
  job->owners.reserve(refs.size());
  for(auto ref:refs)job->owners.push_back(*ref);
  attempt.unique=job->owners.size();
  if(attempt.observed)attempt.collectNS=TerrainUniformNow220()-attempt.start;
  if(job->owners.empty())return nil;
  return job;
 }catch(...){attempt.allocationFailure=true;return nil;}
}
static void DispatchTerrainUniformRelease220(id queue,M4TerrainUniformJob220* job,const TerrainUniformAttempt220& attempt){
 auto worker=job?TerrainUniformWorker220(queue,false):nil;
 const bool handed=job&&worker&&!worker->stopped;
 const bool observe=attempt.observed;const auto event=attempt.event;
 size_t sole=0;
 if(observe&&job)for(const auto& owner:job->owners)sole+=owner.use_count()==1;
 if(handed){
  // Captures only the typed CPU job, scalar receipt, and diagnostic scalars.
  // No frame/source, callback, program, backing, pool token or runtime pointer.
  dispatch_async(worker->worker,^{@autoreleasepool{
   const auto start=observe?TerrainUniformNow220():0;
   job->owners.clear();
#ifdef RECOIL_METAL4_LIVE_SCENE
   if(observe)LOG("[NativeTerrainUniformRelease220] stage=worker completed_event=%llu release_ns=%llu",(unsigned long long)event,(unsigned long long)(TerrainUniformNow220()-start));
#endif
  }});
 }else if(job)job->owners.clear();
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(observe)LOG("[NativeTerrainUniformRelease220] stage=caller frame=%llu completed_event=%llu eligible=%u handed=%u saturated=%u overflow=%u allocation_failure=%u synchronous=%u squares=%zu unique=%zu sole_after_graph=%zu collect_ns=%llu caller_ns=%llu bound=1 max_squares=4096",
  (unsigned long long)attempt.frame,(unsigned long long)event,unsigned(attempt.eligible),unsigned(handed),unsigned(attempt.saturated),unsigned(attempt.overflow),unsigned(attempt.allocationFailure),unsigned(!handed),attempt.squares,attempt.unique,sole,(unsigned long long)attempt.collectNS,(unsigned long long)(TerrainUniformNow220()-attempt.start));
#endif
}
