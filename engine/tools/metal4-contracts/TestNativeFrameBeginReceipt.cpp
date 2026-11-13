#include "Rendering/Backend/NativeFrameBeginReceipt.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
using namespace rendering;
int main(int argc,char** argv){
 if(argc>1){NativeFrameBeginReceipt r(1,2);if(std::string_view(argv[1])=="zero")r.MarkBegun(0);else{r.MarkBegun(3);r.MarkBegun(3);}return 99;}
 NativeSerialRenderWorker worker;
 for(int round=0;round<1000;++round)for(int failure=0;failure<3;++failure){
  auto owned=std::make_shared<NativeFrameBeginReceipt>(round,round+1);std::shared_ptr<const NativeFrameBeginReceipt> retained;
  assert(owned->Matches(round,round+1));assert(!owned->Matches(round+1,round+1));
  try {
   worker.Invoke([owned,failure]{if(failure==1)throw std::runtime_error("before Begin");owned->MarkBegun(71);if(failure==2)throw std::runtime_error("after Begin");});
  }catch(...){owned->MarkFailed();retained=owned;}
  assert(owned->Submission()==(failure==1?0:71));
  assert(owned->FailureStage()==(failure==0?NativeFrameBeginReceipt::Failure::None:failure==1?NativeFrameBeginReceipt::Failure::BeforeBegin:NativeFrameBeginReceipt::Failure::AfterBegin));
  if(failure){owned.reset();assert(retained->FailureStage()!=NativeFrameBeginReceipt::Failure::None);}
 }
 std::cout<<"3000 actual serial-worker owned Begin receipt lifecycles passed\n";
}
