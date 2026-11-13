/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <utility>

namespace rendering {
struct ScreenFrameStorage {
 ScreenDrawList root;
 std::vector<ScreenRenderTargetPass> targets;
};

// Single producer cache, not a queue or a synchronization primitive. A view
// retains immutable execution data independently of the live binding journal.
class NativeOwnedScreenFrame {
 std::shared_ptr<ScreenFrameStorage> cached;
public:
 void Recycle(ScreenDrawList& output) {
  if(cached && cached.use_count()==1) {
   // Preserve the producer's logical state; only reclaim geometry capacity.
   const auto state=output.CaptureState();
   output=std::move(cached->root);
   output.RestoreState(state);
   cached->targets.clear();
  }
 }
 ScreenFrameView Seal(ScreenDrawList& output,const std::vector<ScreenRenderTargetPass>& targets,
                      std::uint32_t width,std::uint32_t height) {
  auto next=(cached && cached.use_count()==1)?cached:std::make_shared<ScreenFrameStorage>();
  // Allocate descriptors before transferring geometry: allocation failure must
  // leave the producer and every previously returned view intact.
  next->targets=targets;
  const auto state=output.CaptureState();
  next->root=std::move(output);
  output.RestoreState(state);
  cached=std::move(next);
  return {&cached->root,width,height,{ },cached->targets,{ },cached};
 }
};
}
