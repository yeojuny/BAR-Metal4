/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSerialRenderWorker.h"
#include "NativeRenderDispatchReceipt.h"
#include <optional>
namespace rendering {
// Direct/readback compatibility path. Draw scopes must unwind before the source
// notice; a notice failure is never replayed as a second failure notification.
template<class Draw,class Notify> auto InvokeNativeSynchronousCPUJob(Draw&& draw,Notify&& notify){
 bool notified=false;
 try{auto result=std::forward<Draw>(draw)();notified=true;notify(&result,{});return result;}
 catch(...){const auto failure=std::current_exception();if(!notified){try{notify(nullptr,failure);}catch(...){}}std::rethrow_exception(failure);}
}

// Main-thread CPU settlement only. GPU/source-frame acceptance is a later,
// separate transaction; a completed task must never imply presentation.
template<class Result> class NativeRenderJobSettlement {
 enum class State {Empty, Running, Settling, Settled, Failed, Consumed};
 State state=State::Empty;
 const std::thread::id source=std::this_thread::get_id();
 std::unique_ptr<typename NativeSerialRenderWorker::template Task<Result>> task;
 std::shared_ptr<NativeRenderDispatchReceipt> receipt;
 std::function<void()> target;
 std::function<void(const Result*,std::exception_ptr)> settled;
 std::optional<Result> result;
 std::exception_ptr failure;
 void Notify()noexcept{
  try{if(settled)settled(result?&*result:nullptr,failure);}catch(...){if(!failure)failure=std::current_exception();}
 }
 void Source()const{if(source!=std::this_thread::get_id())throw std::logic_error("CPU settlement requires its source thread");}
public:
 NativeRenderJobSettlement(std::shared_ptr<NativeRenderDispatchReceipt> notice,std::function<void()> deliver,std::function<void(const Result*,std::exception_ptr)> completed={}):receipt(std::move(notice)),target(std::move(deliver)),settled(std::move(completed)){
  if(!receipt||!target)throw std::logic_error("CPU settlement lacks exact dispatch receipt");
 }
 ~NativeRenderJobSettlement(){
  if(source!=std::this_thread::get_id())std::terminate();
  // Abandonment drains the task before destroying its reservation. Task marks
  // worker admission terminal; no target/retirement success is fabricated.
  task.reset();receipt->ReleaseHandoff();
 }
 template<class F>void Submit(NativeSerialRenderWorker& worker,F&& work){
  Source();if(state!=State::Empty)throw std::logic_error("CPU job already submitted");
  try{task=worker.Enqueue(std::forward<F>(work));state=State::Running;}
  catch(...){failure=std::current_exception();state=State::Settling;receipt->ReleaseHandoff();Notify();state=State::Failed;std::rethrow_exception(failure);}
 }
 void Finish(){
  Source();
  if(state==State::Failed)std::rethrow_exception(failure);
  if(state==State::Settled||state==State::Consumed)return;
  if(state!=State::Running)throw std::logic_error("CPU job settlement is absent or recursive");
  state=State::Settling;
  try{result.emplace(task->Join());}catch(...){failure=std::current_exception();}
  // Join has destroyed worker callable on source and released its credit even
  // on exception. Detach the ticket before invoking any source callback.
  task.reset();receipt->ReleaseHandoff();
  try{target();}catch(...){if(!failure)failure=std::current_exception();}
  try{receipt->DeliverRetirement();}catch(...){if(!failure)failure=std::current_exception();}
  Notify(); // Exact scene acceptance/quarantine precedes any later control.
  state=failure?State::Failed:State::Settled;
  if(failure)std::rethrow_exception(failure);
 }
 Result Consume(){
  Finish();if(state!=State::Settled)throw std::logic_error("CPU job result already consumed");
  static_assert(std::is_nothrow_move_constructible_v<Result>);
  state=State::Consumed;return std::move(*result);
 }
};
}
