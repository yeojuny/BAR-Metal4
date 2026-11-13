#include "Rendering/Backend/NativeEnvironmentStageReceipt.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <iostream>
using namespace rendering;
int main() {
 unsigned checks=0;
 NativeEnvironmentStageReceipt empty;
 assert(!empty.HasCubeSource() && !empty.HasPendingCubeSource()); ++checks;
 auto first=std::make_shared<NativeWorldEnvironmentFrame>();first->captureSerial=1;
 auto update=std::make_shared<NativeCubeReflectionUpdate>();
 auto backing=std::make_shared<NativeCubeTextureBacking>();
 update->backing=backing;first->sourceCubeUpdate=update;
 NativeEnvironmentStageReceipt capture{first,true};
 const auto packet=capture;
 assert(packet.Matches(first) && packet.HasCubeSource() && packet.HasPendingCubeSource());checks+=3;
 auto next=std::make_shared<NativeWorldEnvironmentFrame>();next->captureSerial=2;
 capture={next,false};first.reset();update.reset();
 assert(packet.frame->captureSerial==1 && packet.shaderValidation);checks+=2;
 assert(!packet.Matches(capture.frame) && !capture.shaderValidation);checks+=2;
 NativeSerialRenderWorker worker;
 worker.Invoke([packet]{assert(packet.frame->captureSerial==1 && packet.HasPendingCubeSource());});++checks;
 backing->completed.store(true,std::memory_order_release);
 assert(packet.HasCubeSource() && !packet.HasPendingCubeSource());checks+=2;
 // Completed cube source must remain a source requirement: only the pending
 // source-selection query observes completion, matching the original distinction.
 for(unsigned i=0;i<1000;++i) {
  auto fresh=std::make_shared<NativeWorldEnvironmentFrame>();fresh->captureSerial=i+3;
  capture={fresh,bool(i%2)};
  assert(packet.frame->captureSerial==1 && packet.shaderValidation);
  assert(!packet.Matches(capture.frame));checks+=2;
 }
 capture={};
 assert(packet.frame->captureSerial==1);++checks;
 std::cout<<checks<<" environment stage receipt checks passed\n";
}
