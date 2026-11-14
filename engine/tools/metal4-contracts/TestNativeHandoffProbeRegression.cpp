#include "Rendering/Backend/NativeRenderJobSettlement.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <string>

// Host-only regression adapted from the 2026-11-14 runtime audit appendix A.
// Compiles against the actual production headers. Success requires zero conflicts.
// Repeated source Try probes enlarge the probability of the production
// LegacyCopyExit -> CanAbortUnsubmittedSource window. They do not model its
// observed frequency, any Metal call, or the user's actual crash stack.
int main() {
  using namespace rendering;
  NativeSerialRenderWorker worker;
  unsigned conflicts=0,otherErrors=0,successes=0,probes=0;
  constexpr unsigned iterations=20000;
  for(unsigned n=0;n<iterations;++n) {
    NativeRendererAccessGate gate;
    auto receipt=std::make_shared<NativeRenderDispatchReceipt>(
      nullptr,std::function<void()>{});
    {
      NativeRendererAccessGate::Scope owner(&gate);
      receipt->Reserve(gate);
    }
    NativeRenderJobSettlement<int> job(receipt,[]{});
    std::atomic<bool> sourceReturned{false};
    job.Submit(worker,[receipt,&sourceReturned] {
      struct End {
        std::shared_ptr<NativeRenderDispatchReceipt> receipt;
        ~End(){receipt->MarkWorkerFinished();}
      } end{receipt};
      while(!sourceReturned.load(std::memory_order_acquire))
        std::this_thread::yield();
      NativeRendererAccessGate::Scope admitted(receipt->Handoff());
      receipt->MarkBegun(1);
      receipt->PublishBeginReady();
      return 1;
    });
    assert(job.ReleaseForPreparation());
    sourceReturned.store(true,std::memory_order_release);
    while(!(receipt->Progress()&2)) {
      NativeRendererAccessGate::Scope canAbort(
        &gate,NativeRendererAccessGate::Try{});
      assert(!canAbort.Acquired());
      ++probes;
    }
    try {
      job.Finish();
      assert(job.Consume()==1);
      ++successes;
    } catch(const std::logic_error& e) {
      if(std::string(e.what())=="Renderer handoff access conflict")
        ++conflicts;
      else {
        ++otherErrors;
        std::cerr<<e.what()<<"\n";
      }
    }
  }
  std::cout
    <<"host-only admission regression: iterations="<<iterations
    <<" rejected_source_probes="<<probes
    <<" handoff_conflicts="<<conflicts
    <<" successes="<<successes
    <<" other_errors="<<otherErrors<<"\n";
  return otherErrors||conflicts||successes!=iterations;
}
