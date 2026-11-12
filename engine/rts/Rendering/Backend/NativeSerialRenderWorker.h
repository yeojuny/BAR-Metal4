/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace rendering {
// One bounded worker, one outstanding invocation. Invoke waits: this first
// intervention migrates preparation/encoding, but does not claim next-sim overlap.
// Main-thread source receipts are delivered by the caller after Invoke returns.
class NativeSerialRenderWorker {
public:
 NativeSerialRenderWorker(): worker([this]{Run();}) {}
 ~NativeSerialRenderWorker(){
  {std::lock_guard lock(mutex);stopping=true;} ready.notify_one();worker.join();
 }
 NativeSerialRenderWorker(const NativeSerialRenderWorker&)=delete;
 NativeSerialRenderWorker& operator=(const NativeSerialRenderWorker&)=delete;
 template<class F> auto Invoke(F&& function)->std::invoke_result_t<F> {
  using R=std::invoke_result_t<F>;
  if(std::this_thread::get_id()==worker.get_id())throw std::logic_error("Recursive native render worker call");
  auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(function));
  auto result=task->get_future();
  {std::lock_guard lock(mutex);if(stopping||busy)throw std::logic_error("Native render worker has an outstanding invocation");
   busy=true;work=[task]{(*task)();};}
  ready.notify_one();
  try {
   if constexpr(std::is_void_v<R>){result.get();WaitIdle();}
   else {auto value=result.get();WaitIdle();return value;}
  }catch(...){WaitIdle();throw;}
 }
private:
 void WaitIdle(){std::unique_lock lock(mutex);idle.wait(lock,[this]{return !busy;});}
 void Run(){
  for(;;){std::function<void()> next;{
   std::unique_lock lock(mutex);ready.wait(lock,[this]{return stopping||bool(work);});
   if(stopping&&!work)return;
   next=std::move(work);work={};}
   next();{std::lock_guard lock(mutex);busy=false;}idle.notify_all();
  }
 }
 std::mutex mutex;std::condition_variable ready,idle;
 std::function<void()> work;bool stopping=false,busy=false;std::thread worker;
};
}
