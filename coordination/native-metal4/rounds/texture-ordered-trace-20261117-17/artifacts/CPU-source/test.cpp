#include "NativeTextureOrderedTrace.h"
#include <string_view>
#include <cstdlib>
#include <thread>
#include <vector>
#include <cassert>
#include <stdexcept>
using namespace rendering;
#ifdef RECOIL_METAL4_LIVE_SCENE
static unsigned logs=0;
template<class... T> void FakeLog(T...){++logs;throw std::runtime_error("injected logger failure");}
#define LOG(...) FakeLog(__VA_ARGS__)
#endif
#include "wrapper.h"
int main(int argc,char** argv){
 assert(argc==2);const bool enabled=argv[1][0]=='1';
 NativeTextureOrderedTraceBudget budget;std::atomic<unsigned> accepted{0};std::vector<std::thread> threads;
 for(int i=0;i<8;++i)threads.emplace_back([&]{for(int k=0;k<1000;++k)accepted+=budget.Take(NativeTextureOrderedTraceEvent::Admitted);});
 for(auto& t:threads){t.join();}
 assert(accepted==64);
 assert(budget.Take(NativeTextureOrderedTraceEvent::Armed));assert(!budget.Take(NativeTextureOrderedTraceEvent::Armed));
 for(int i=0;i<64;++i)assert(budget.Take(NativeTextureOrderedTraceEvent::ProducerRetiredCheck));
 assert(!budget.Take(NativeTextureOrderedTraceEvent::ProducerRetiredCheck));assert(!budget.Take(NativeTextureOrderedTraceEvent::Count));
 for(int i=0;i<1000;++i){TraceTextureOrdered(NativeTextureOrderedTraceEvent::Armed);TraceTextureOrdered(NativeTextureOrderedTraceEvent::Admitted,1,2);TraceTextureOrdered(NativeTextureOrderedTraceEvent::EncodePermitCheck,1,2);TraceTextureOrdered(NativeTextureOrderedTraceEvent::ProducerRetiredCheck,1,2);}
#ifdef RECOIL_METAL4_LIVE_SCENE
 assert(logs==(enabled?193u:0u));
#else
 assert(!enabled);
#endif
}
