#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include "Rendering/Backend/NativeRenderDispatchReceipt.h"
#include <cassert>
#include <iostream>
using namespace rendering;
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
struct Owner {std::thread::id source;unsigned* destroyed;~Owner(){assert(source==std::this_thread::get_id());++*destroyed;}};
struct MoveProbe {unsigned* moved;MoveProbe(unsigned& n):moved(&n){}MoveProbe(MoveProbe&& p):moved(p.moved){++*moved;}void operator()()const{}};
int main(){
 const auto source=std::this_thread::get_id();NativeSerialRenderWorker worker;unsigned destroyed=0;
 for(unsigned i=0;i<1000;++i){
  auto owner=std::make_shared<Owner>();owner->source=source;owner->destroyed=&destroyed;
  std::promise<void> release;auto wait=release.get_future();
  auto task=worker.Enqueue([owner=std::move(owner),wait=std::move(wait)]()mutable{wait.wait();return 17;});
  assert(!task->Ready());unsigned moved=0;MoveProbe probe(moved);
  Reject([&]{worker.Enqueue(std::move(probe));});assert(moved==0);
  release.set_value();while(!task->Ready())std::this_thread::yield();
  assert(destroyed==i);Reject([&]{worker.Invoke([]{});}); // Finished but still credited.
  std::async(std::launch::async,[&]{Reject([&]{task->Join();});Reject([&]{task->Ready();});}).get();
  assert(task->Join()==17&&destroyed==i+1);Reject([&]{task->Join();});
  assert(worker.Invoke([]{return 19;})==19);
 }
 // Shared state, not a dangling worker pointer, supports joined teardown order.
 {auto w=std::make_unique<NativeSerialRenderWorker>();auto task=w->Enqueue([]{return 23;});w.reset();assert(task->Join()==23);}
 // Discard cannot reopen admission or claim an unobserved failure succeeded.
 for(bool fail:{false,true}){NativeSerialRenderWorker w;{auto task=w.Enqueue([fail]{if(fail)throw std::runtime_error("unobserved");});}Reject([&]{w.Invoke([]{});});}
 for(unsigned mode=0;mode<3;++mode){
  NativeSerialRenderWorker w;auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  std::promise<void> release;auto wait=release.get_future();
  auto task=w.Enqueue([receipt,mode,wait=std::move(wait)]()mutable{
   struct Finish{std::shared_ptr<NativeRenderDispatchReceipt> value;~Finish(){value->MarkWorkerFinished();}} finish{receipt};
   if(mode==0)throw std::runtime_error("pre Begin");
   receipt->MarkBegun(31);receipt->PublishBeginReady();wait.wait();
   if(mode==2)throw std::runtime_error("post Begin");
  });
  const auto state=receipt->WaitForBeginOrFinished();unsigned notice=0;
  if(mode){assert(state&1);receipt->DeliverTarget([&](auto,uint64_t id,auto)noexcept{assert(id==31);++notice;});assert(!task->Ready());}
  else assert(state==2);
  release.set_value();bool failed=false;try{task->Join();}catch(const std::runtime_error&){failed=true;}
  assert(failed==(mode!=1));assert(receipt->Progress()&2);
  receipt->DeliverTarget([&](auto,auto,auto)noexcept{++notice;});assert(notice==(mode?1:0));
 }
 std::cout<<"1000 owned task credit/lifetime cycles; rejected capture move, foreign Join, terminal discard, worker teardown, Begin-ready/error handshake PASS\n";
}
