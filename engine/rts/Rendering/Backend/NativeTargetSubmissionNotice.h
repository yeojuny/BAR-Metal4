/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <exception>
#include <type_traits>
namespace rendering {
// One synchronous worker invocation owns this journal. Main waits for Invoke
// before reading it; this is not a concurrent mailbox or a lookahead permit.
class NativeTargetSubmissionNotice {
public:
 explicit NativeTargetSubmissionNotice(const ScreenFrameView* frame):
  passes(frame?std::vector<ScreenRenderTargetPass>(frame->targetPasses.begin(),frame->targetPasses.end()):std::vector<ScreenRenderTargetPass>{}),
  postprocess(frame?frame->postprocess:nullptr) {}
 void MarkBegun(uint64_t value) noexcept {
  if(!value || (submission && submission!=value))std::terminate();
  submission=value;
 }
 template<class Apply> void Deliver(Apply&& apply) noexcept {
  static_assert(std::is_nothrow_invocable_v<Apply,std::span<const ScreenRenderTargetPass>,uint64_t,std::shared_ptr<const NativePostprocessFrame>>);
  if(!submission || delivered)return;
  delivered=true;
  apply(passes,submission,postprocess);
 }
private:
 std::vector<ScreenRenderTargetPass> passes;
 std::shared_ptr<const NativePostprocessFrame> postprocess;
 uint64_t submission=0;
 bool delivered=false;
};
} // namespace rendering
