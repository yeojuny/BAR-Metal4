#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
using namespace rendering;
static unsigned checks=0;
static void Check(bool ok){++checks;if(!ok)throw std::runtime_error("completion mailbox mismatch");}
static void Reject(const std::function<void()>& f){bool rejected=false;try{f();}catch(const std::logic_error&){rejected=true;}Check(rejected);}
static NativeFrameCompletionNotice Notice(std::uint64_t draw){return {true,draw,draw+100,1,2,draw,{3,4},{5,6,7}};}
int main(){
 Reject([]{NativeFrameCompletionMailbox invalid(0);});
 auto box=std::make_shared<NativeFrameCompletionMailbox>(17);Check(box->Epoch()==17);
 Check(!box->Pop());auto a=Notice(10),b=Notice(20);
 Reject([&]{box->Prepare(18,10,110);});auto ta=box->Prepare(17,10,110),tb=box->Prepare(17,20,120),tc=box->Prepare(17,30,130);auto bad=a;bad.sourceFrameCompleted=false;
 Reject([&]{box->Publish(ta,bad);});Reject([&]{box->Publish(box->Prepare(17,11,110),a);});Reject([&]{box->Publish(box->Prepare(17,10,111),a);});
 Check(!box->Pop());box->Publish(ta,a);box->Publish(tb,b);
 Reject([&]{box->Publish(tc,Notice(30));});
 Reject([&]{box->Publish(ta,a);});
 auto first=box->Pop();Check(first&&first->sourceDrawFrame==10&&first->dynamicWaterSourceDrawFrame==10&&first->completedWorldInfo==a.completedWorldInfo&&first->completedWaterUpdates==a.completedWaterUpdates);
 auto second=box->Pop();Check(second&&second->sourceDrawFrame==20);Check(!box->Pop());Reject([&]{box->Publish(ta,a);});
 // Callback lifetime is solely the mailbox, not a destroyed Session or global.
 auto callback=[owned=box,ticket=box->Prepare(17,10,110)](const auto& value){owned->Publish(ticket,value);};
 box->Close();box.reset();Reject([&]{callback(a);});
 NativeFrameCompletionMailbox replacement(18);Reject([&]{replacement.Publish(ta,a);});Check(!replacement.Pop());
 // One producer and consumer: bounded scheduling, every field copied faithfully.
 NativeFrameCompletionMailbox concurrent(19);std::atomic<unsigned> consumed=0;std::atomic<bool> failed=false;
 constexpr unsigned total=20000;
 std::thread producer([&]{try{for(unsigned i=1;i<=total;++i){while(i-consumed.load(std::memory_order_acquire)>2)std::this_thread::yield();auto ticket=concurrent.Prepare(19,i,i+100);concurrent.Publish(ticket,Notice(i));}}catch(...){failed=true;}});
 for(unsigned i=1;i<=total;++i){std::optional<NativeFrameCompletionNotice> value;while(!(value=concurrent.Pop())){if(failed)break;std::this_thread::yield();}Check(value&&value->sourceDrawFrame==i&&value->completedPostprocessSerial==i+100&&value->dynamicWaterSourceDrawFrame==i);consumed.store(i,std::memory_order_release);}
 producer.join();Check(!failed&&!concurrent.Pop());
 // Close races with publication and consumption. Either operation linearizes
 // before Close or is rejected. No wrong-epoch result may escape into a new box.
 for(unsigned round=0;round<1000;++round){
  auto raced=std::make_shared<NativeFrameCompletionMailbox>(100+round);
  std::atomic<bool> go=false;std::atomic<unsigned> accepted=0,rejected=0,popped=0;
  const auto epoch=raced->Epoch();const auto ticket=raced->Prepare(epoch,10,110);
  std::thread writer([&]{while(!go.load(std::memory_order_acquire)){}try{raced->Publish(ticket,a);++accepted;}catch(const std::logic_error&){++rejected;}});
  std::thread reader([&]{while(!go.load(std::memory_order_acquire)){}try{if(auto v=raced->Pop()){if(v->sourceDrawFrame==10)++popped;else failed=true;}}catch(const std::logic_error&) {}});
  go.store(true,std::memory_order_release);raced->Close();writer.join();reader.join();
  Check(accepted+rejected==1&&popped<=accepted&&!failed);
  Reject([&]{raced->Publish(ticket,a);});Reject([&]{raced->Pop();});
 }
 std::cout<<checks<<" production completion mailbox checks passed\n";
}
