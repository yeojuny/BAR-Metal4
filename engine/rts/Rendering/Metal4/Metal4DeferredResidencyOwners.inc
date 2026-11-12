#import <objc/runtime.h>
#include <chrono>
// Only detached strong allocation tables may cross this queue. Never a frame,
// allocator, retained graph, drawable, renderer callback or C++ pool token.
static bool ResidencyReleaseEnabled220(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_RESIDENCY_RELEASE220");return p&&std::string_view(p)=="1";}();return v;}
static bool ResidencyReleaseCounters220(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_RESIDENCY_RELEASE220_COUNTERS");return p&&std::string_view(p)=="1";}();return v;}
static uint64_t ResidencyReleaseNow220(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
@interface M4ResidencyReceipt220:NSObject {@public uint64_t runtimeID,event;bool completed;}
@end
@implementation M4ResidencyReceipt220
@end
@interface M4ResidencyReleaseJob220:NSObject {@public __strong NSHashTable* owners;}
@end
@implementation M4ResidencyReleaseJob220
@end
@interface M4ResidencyReleaseWorker220:NSObject {@public dispatch_queue_t worker;dispatch_semaphore_t capacity;uint64_t identity;bool stopped;unsigned receipts;}
@end
@implementation M4ResidencyReleaseWorker220
-(instancetype)init {if((self=[super init])){static std::atomic<uint64_t> ids{0};identity=++ids;capacity=dispatch_semaphore_create(1);worker=dispatch_queue_create("org.beyondallreason.metal4.completed-allocation-release",DISPATCH_QUEUE_SERIAL);}return self;}
@end
static char residencyWorkerKey220, residencyReceiptKey220;
static M4ResidencyReleaseWorker220* ResidencyWorker220(id queue,bool create){
 if(!queue)return nil;auto worker=(M4ResidencyReleaseWorker220*)objc_getAssociatedObject(queue,&residencyWorkerKey220);
 if(!worker&&create){worker=[M4ResidencyReleaseWorker220 new];objc_setAssociatedObject(queue,&residencyWorkerKey220,worker,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}return worker;
}
static void StopResidencyWorker220(id queue){
 if(!ResidencyReleaseEnabled220()&&!ResidencyReleaseCounters220())return;
 auto worker=ResidencyWorker220(queue,false);if(!worker)return;worker->stopped=true;
 // Runtime destruction is outside requestMutex. Worker never acquires it.
 dispatch_sync(worker->worker,^{});objc_setAssociatedObject(queue,&residencyWorkerKey220,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static void StampResidencyTable220(id queue,NSHashTable* table,uint64_t event){
 if(!ResidencyReleaseEnabled220()&&!ResidencyReleaseCounters220())return;
 auto worker=ResidencyWorker220(queue,true);if(!worker||!table)return;
 auto receipt=[M4ResidencyReceipt220 new];receipt->runtimeID=worker->identity;receipt->event=event;receipt->completed=false;
 objc_setAssociatedObject(table,&residencyReceiptKey220,receipt,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static void CompleteResidencyTable220(id queue,NSHashTable* table,uint64_t event){
 if(!table||(!ResidencyReleaseEnabled220()&&!ResidencyReleaseCounters220()))return;
 auto worker=ResidencyWorker220(queue,false);if(!worker)return;
 auto receipt=(M4ResidencyReceipt220*)objc_getAssociatedObject(table,&residencyReceiptKey220);
 if(receipt&&receipt->runtimeID==worker->identity&&receipt->event==event)receipt->completed=true;
}
@interface M4DeferredResidencyOwners : NSObject {@public __strong NSHashTable* owners;uint64_t runtimeID220,event220;}
@end
@implementation M4DeferredResidencyOwners
@end
static M4DeferredResidencyOwners* DeferredResidencyOwners(NSMutableArray* graph){id first=graph.firstObject;return [first isMemberOfClass:[M4DeferredResidencyOwners class]]?first:nil;}
static void DeferCompletedResidencyOwners(NSMutableArray* graph,NSHashTable* owners,NSHashTable* next,id queue){
 if(!owners)return;auto holder=DeferredResidencyOwners(graph);
 if(!holder){holder=[M4DeferredResidencyOwners new];[graph insertObject:holder atIndex:0];}
 holder->runtimeID220=0;holder->event220=0;
 auto worker=(ResidencyReleaseEnabled220()||ResidencyReleaseCounters220())?ResidencyWorker220(queue,false):nil;
 auto receipt=worker?(M4ResidencyReceipt220*)objc_getAssociatedObject(owners,&residencyReceiptKey220):nil;
 if(owners!=next&&worker&&receipt&&receipt->completed&&receipt->runtimeID==worker->identity){holder->runtimeID220=receipt->runtimeID;holder->event220=receipt->event;}
 // Repeated StageResources replaces the prior holder exactly as before.
 holder->owners=owners;
}
static void InvalidateResidencyTable220(NSHashTable* table){if(table&&(ResidencyReleaseEnabled220()||ResidencyReleaseCounters220()))objc_setAssociatedObject(table,&residencyReceiptKey220,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}
static void ReleaseCompletedResidencyOwners(NSMutableArray* graph){if(auto holder=DeferredResidencyOwners(graph))holder->owners=nil;}
static void ReleaseCompletedResidencyOwners220(NSMutableArray* graph,id queue,uint64_t frameIndex){
 auto holder=DeferredResidencyOwners(graph);if(!holder||!holder->owners)return;
 auto worker=ResidencyWorker220(queue,false);
 const bool observe=worker&&ResidencyReleaseCounters220()&&frameIndex>=600&&worker->receipts++<64;
 const auto start=observe?ResidencyReleaseNow220():0;const auto event=holder->event220;
 const bool eligible=worker&&!worker->stopped&&event&&holder->runtimeID220==worker->identity;
 bool handed=false,saturated=false;
 if(ResidencyReleaseEnabled220()&&eligible){
  if(dispatch_semaphore_wait(worker->capacity,DISPATCH_TIME_NOW)==0){
   auto job=[M4ResidencyReleaseJob220 new];if(job){job->owners=holder->owners;holder->owners=nil;
   // The block captures the job, not the table. The only table field is
   // cleared under the worker autorelease pool; caller retains no table local.
   dispatch_async(worker->worker,^{@autoreleasepool{
    const auto releaseStart=observe?ResidencyReleaseNow220():0;
    job->owners=nil;
#ifdef RECOIL_METAL4_LIVE_SCENE
    if(observe)LOG("[NativeResidencyRelease220] stage=worker completed_event=%llu release_ns=%llu",(unsigned long long)event,(unsigned long long)(ResidencyReleaseNow220()-releaseStart));
#endif
   }
   dispatch_semaphore_signal(worker->capacity);});handed=true;}else dispatch_semaphore_signal(worker->capacity);
  }else saturated=true;
 }
 if(!handed)ReleaseCompletedResidencyOwners(graph);
#ifdef RECOIL_METAL4_LIVE_SCENE
 if(observe)LOG("[NativeResidencyRelease220] stage=caller frame=%llu completed_event=%llu eligible=%u handed=%u saturated=%u synchronous=%u caller_ns=%llu bound=1",(unsigned long long)frameIndex,(unsigned long long)event,unsigned(eligible),unsigned(handed),unsigned(saturated),unsigned(!handed),(unsigned long long)(ResidencyReleaseNow220()-start));
#endif
}
