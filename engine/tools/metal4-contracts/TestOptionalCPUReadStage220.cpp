#include "Rendering/Backend/NativeCUSCPUReadWorker220.h"
#include <cassert>
#include <future>
#include <iostream>
#include <vector>
using namespace rendering;
using Worker=NativeCUSCPUReadWorker220;
using Source=std::shared_ptr<const NativeCUSSourceFrame>;
struct Injected {int source;};
Source Frame(unsigned id){auto p=std::make_shared<NativeCUSSourceFrame>();p->sourceDrawFrame=id;return p;}
int main(){
 for(unsigned iteration=0;iteration<1000;++iteration){
  auto a=Frame(1),b=Frame(2),c=Frame(3),d=Frame(4);
  // Publication and cancellation exercise the actual mutex/condition transition.
  {
   Worker::Job job;job.sources={a,b};job.ExpectTail();job.measure=true;
   std::vector<unsigned> visited;
   auto task=std::async(std::launch::async,[&]{return Worker::VisitSources(job,[&](const Source& s){visited.push_back(s->sourceDrawFrame);});});
   job.PublishTail({c},{d,a}); // Preserve order and duplicates for scope-level dedup.
   assert(task.get());assert((visited==std::vector<unsigned>{1,2,3,4,1}));
   assert(!job.optionalFailed&&!job.error);
   bool twice=false;try{job.PublishTail({},{});}catch(const std::logic_error&){twice=true;}assert(twice);
  }
  {
   Worker::Job job;job.sources={a};job.ExpectTail();
   std::vector<unsigned> visited;
   auto task=std::async(std::launch::async,[&]{return Worker::VisitSources(job,[&](const Source& s){visited.push_back(s->sourceDrawFrame);});});
   job.CancelTail();assert(task.get());assert((visited==std::vector<unsigned>{1}));
   bool rejected=false;try{job.PublishTail({c},{d});}catch(const std::logic_error&){rejected=true;}assert(rejected);
   assert(!job.optionalFailed&&!job.error);
  }
  for(unsigned failure:{1u,3u,4u}){
   Worker::Job job;job.sources={a,b};job.ExpectTail();job.PublishTail({c},{d,a});
   std::vector<unsigned> visited;bool accepted=false,caught=false,sealed=false;
   try{
    accepted=Worker::VisitSources(job,[&](const Source& s){
     visited.push_back(s->sourceDrawFrame);if(s->sourceDrawFrame==failure)throw Injected{int(failure)};
    });
    if(accepted)sealed=true; // Same guard as Build: optional failure cannot seal.
   }catch(const Injected& e){caught=true;assert(e.source==int(failure));job.error=std::current_exception();}
   assert(!accepted&&!sealed);
   if(failure==4){assert(job.optionalFailed&&!job.error&&!caught);assert((visited==std::vector<unsigned>{1,2,3,4}));}
   else {assert(!job.optionalFailed&&job.error&&caught);assert(visited.back()==failure);}
  }
  // Catch all allocation and runtime failures in only the optional stage.
  for(unsigned kind=0;kind<2;++kind){
   Worker::Job job;job.ExpectTail();job.PublishTail({}, {d});
   assert(!Worker::VisitSources(job,[&](const Source&){if(kind)throw std::bad_alloc();throw std::runtime_error("speculative");}));
   assert(job.optionalFailed&&!job.error&&job.packet.Count()==0);
  }
  // Old API/default disabled-tail behavior is unchanged.
  {
   Worker::Job job;job.sources={a};unsigned visits=0;
   assert(Worker::VisitSources(job,[&](const Source&){++visits;}));assert(visits==1);
  }
  {
   Worker::Job job;job.ExpectTail();job.PublishTail({c});unsigned visits=0;
   assert(Worker::VisitSources(job,[&](const Source&){++visits;}));assert(visits==1&&job.optionalSources.empty());
  }
 }
 // Neither successful visitation nor failed speculation transfers source owners
 // to a worker. Main destroys the final job/vector ownership after synchronization.
 const auto main=std::this_thread::get_id();bool destroyed=false;
 {
  Worker::Job job;job.ExpectTail();
  Source owned(new NativeCUSSourceFrame,[&](auto* p){assert(std::this_thread::get_id()==main);destroyed=true;delete p;});
  job.PublishTail({}, {owned});owned.reset();
  auto task=std::async(std::launch::async,[&]{return Worker::VisitSources(job,[](const Source&){throw std::bad_alloc();});});
  assert(!task.get()&&job.optionalFailed&&!destroyed);
 }
 assert(destroyed);
 std::cout<<"Optional CPU-read stage:1000 ordered publication/cancel/default/mandatory-error/optional-fallback cycles; source lifetime passed\n";
}
