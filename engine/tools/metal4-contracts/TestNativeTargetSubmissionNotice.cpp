#include "Rendering/Backend/NativeTargetSubmissionNotice.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace rendering;
int main(){
 for(int failure=0;failure<5;++failure){
  std::vector<ScreenRenderTargetPass> passes(2);ScreenFrameView view;view.targetPasses=passes;
  view.postprocess=std::make_shared<NativePostprocessFrame>();auto post=view.postprocess;
  NativeTargetSubmissionNotice notice(&view);passes.clear();view.postprocess.reset();
  int deliveries=0;bool receiptAttempted=false;
  auto apply=[&](auto owned,uint64_t id,auto frame) noexcept {assert(owned.size()==2);assert(id==17);assert(frame==post);++deliveries;};
  auto deliver=[&]() noexcept {notice.Deliver(apply);};
  auto prior=[&]{receiptAttempted=true;assert(deliveries==1);if(failure==4)throw std::runtime_error("prior receipt");};
  try {
   try {
    if(failure==1)throw std::runtime_error("before Begin");
    notice.MarkBegun(17);
    if(failure==2)throw std::runtime_error("after Begin");
    if(failure==3)throw std::runtime_error("completion");
    deliver();prior();
   }catch(...){deliver();if(failure!=1 && !receiptAttempted)prior();throw;}
  }catch(const std::runtime_error&){}
  deliver();assert(deliveries==(failure==1?0:1));
 }
 NativeSerialRenderWorker worker;const auto mainThread=std::this_thread::get_id();
 for(int failure=0;failure<3;++failure){
  NativeTargetSubmissionNotice notice(nullptr);int delivered=0;
  try {worker.Invoke([&]{assert(std::this_thread::get_id()!=mainThread);if(failure==1)throw std::runtime_error("pre Begin");notice.MarkBegun(23);if(failure==2)throw std::runtime_error("post Begin");});}catch(const std::runtime_error&){}
  notice.Deliver([&](auto,uint64_t id,auto) noexcept {assert(std::this_thread::get_id()==mainThread);assert(id==23);++delivered;});
  assert(delivered==(failure==1?0:1));
 }
 NativeTargetSubmissionNotice empty(nullptr);int deliveries=0;
 empty.Deliver([&](auto,auto,auto) noexcept {++deliveries;});assert(deliveries==0);
 std::cout<<"Owned Begin notice: pre/post Begin, completion/prior receipt failures, ownership, idempotence passed\n";
}
