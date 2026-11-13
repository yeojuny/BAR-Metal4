#pragma once
#include <condition_variable>
#include <chrono>
#include <utility>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace rendering {
// One worker and one outstanding owned task credit. Completion does not release
// the credit: the submitting thread must join and release source captures first.
class NativeSerialRenderWorker {
 struct State {
  std::mutex mutex;std::condition_variable ready,idle;
  std::function<void()> work;bool stopping=false,busy=false,claimed=false,abandoned=false;
 };
 std::shared_ptr<State> state=std::make_shared<State>();
 std::thread worker;
public:
 template<class R> class Task {
  friend class NativeSerialRenderWorker;
  std::shared_ptr<State> state;
  std::shared_ptr<std::packaged_task<R()>> task;
  std::future<R> result;
  const std::thread::id source=std::this_thread::get_id();
  bool published=false,joined=false;
  void Release(bool observed)noexcept {
   if(!published||joined)return;
   if(std::this_thread::get_id()!=source)std::terminate();
   {std::unique_lock lock(state->mutex);state->idle.wait(lock,[this]{return !state->busy;});}
   // Worker releases its callable before idle. Final captured source destruction
   // happens here, on the submitting thread, before another task gets credit.
   result={}; // Drop the future state before the final callable owner.
   task.reset();
   {std::lock_guard lock(state->mutex);state->abandoned|=!observed;state->claimed=false;}
   joined=true;
  }
  Task(std::shared_ptr<State> s,std::shared_ptr<std::packaged_task<R()>> t,std::future<R> f):state(std::move(s)),task(std::move(t)),result(std::move(f)) {}
 public:
  ~Task(){Release(false);} // Abandonment releases resources but terminally closes admission.
  Task(const Task&)=delete;Task& operator=(const Task&)=delete;
  // Future readiness is not the worker-release barrier; Join is still required.
  bool Ready()const {if(std::this_thread::get_id()!=source)throw std::logic_error("Render task readiness belongs to its source owner");return result.valid()&&result.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
  R Join(){
   if(std::this_thread::get_id()!=source||joined||!published)throw std::logic_error("Render task must be joined once by its source owner");
   try{
    if constexpr(std::is_void_v<R>){result.get();Release(true);}
    else {auto value=result.get();Release(true);return value;}
   }catch(...){Release(true);throw;}
  }
 };
 NativeSerialRenderWorker():worker([s=state]{Run(s);}) {}
 ~NativeSerialRenderWorker(){
  if(std::this_thread::get_id()==worker.get_id())std::terminate();
  {std::lock_guard lock(state->mutex);state->stopping=true;}state->ready.notify_one();worker.join();
 }
 NativeSerialRenderWorker(const NativeSerialRenderWorker&)=delete;
 NativeSerialRenderWorker& operator=(const NativeSerialRenderWorker&)=delete;
 template<class F> auto Enqueue(F&& function)->std::unique_ptr<Task<std::invoke_result_t<F>>> {
  using R=std::invoke_result_t<F>;
  if(std::this_thread::get_id()==worker.get_id())throw std::logic_error("Recursive native render worker call");
  // Reject before moving caller captures. Construction consumes the sole credit;
  // every allocation failure restores it after local captured owners unwind.
  struct Claim {
   std::shared_ptr<State> state;bool published=false;
   ~Claim(){if(!published){std::lock_guard lock(state->mutex);state->claimed=false;}}
  } claim{state};
  {std::lock_guard lock(state->mutex);if(state->stopping||state->claimed||state->abandoned){claim.published=true;throw std::logic_error("Native render worker has no task credit");}state->claimed=true;}
  auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(function));
  auto result=task->get_future();
  // Allocate the owning ticket/callable before publication. Claim restores
  // construction credit on failure; no throw follows publication.
  std::unique_ptr<Task<R>> ticket(new Task<R>(state,task,std::move(result)));
  std::function<void()> submitted=[task]{(*task)();};
  {std::lock_guard lock(state->mutex);if(state->stopping)throw std::logic_error("Native render worker stopped during task construction");
   state->work=std::move(submitted);state->busy=true;ticket->published=true;claim.published=true;}
  state->ready.notify_one();return ticket;
 }
 template<class F> auto Invoke(F&& function)->std::invoke_result_t<F> {
  auto ticket=Enqueue(std::forward<F>(function));return ticket->Join();
 }
private:
 static void Run(const std::shared_ptr<State>& state){
  for(;;){std::function<void()> next;{
   std::unique_lock lock(state->mutex);state->ready.wait(lock,[&]{return state->stopping||bool(state->work);});
   if(state->stopping&&!state->work)return;
   next=std::move(state->work);state->work={};}
   next();next={};
   {std::lock_guard lock(state->mutex);state->busy=false;}state->idle.notify_all();
  }
 }
};
}
