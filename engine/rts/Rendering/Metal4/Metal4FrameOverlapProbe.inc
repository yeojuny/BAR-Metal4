// Diagnostic only. Sample 1/16 deferred presentations, max128 rows. No game
// console output or file I/O during rendering. Disabled in acceptance controls.
#include <mach/mach_time.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <string>
struct FrameOverlapSample {
 uint64_t frame=0;
 double submit=0,returned=0,drain=0,done=0,gpuStart=0,gpuEnd=0;
 bool ready=false;
 std::atomic<bool> complete{false};
};
static double FrameOverlapHostTime() {
 static const auto scale=[] {mach_timebase_info_data_t info;mach_timebase_info(&info);return double(info.numer)/info.denom/1e9;}();
 return double(mach_absolute_time())*scale;
}
struct FrameOverlapProbe {
 std::string path;std::array<FrameOverlapSample,128> rows;size_t used=0;uint64_t seen=0;
 explicit FrameOverlapProbe(const char* p):path(p){}
 void Flush() noexcept {
  const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return;
  dprintf(fd,"frame,submit,returned,drain,done,gpu_start,gpu_end,event_ready_at_drain\n");
  for(size_t i=0;i<used;++i){const auto& r=rows[i];if(!r.complete.load(std::memory_order_acquire))continue;
   dprintf(fd,"%llu,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d\n",(unsigned long long)r.frame,r.submit,r.returned,r.drain,r.done,r.gpuStart,r.gpuEnd,int(r.ready));}
  close(fd);
 }
};
// Process lifetime when enabled: a late GPU handler must not reference destroyed
// probe storage during abnormal teardown. Memory is bounded and OFF allocates none.
static FrameOverlapProbe*& FrameOverlapProbeStorage(){static FrameOverlapProbe* value=nullptr;return value;}
static FrameOverlapSample* BeginFrameOverlapProbe(uint64_t frame) {
 static const char* path=std::getenv("BAR_NATIVE_FRAME_OVERLAP_PROBE_FILE");
 if(!path||path[0]!='/'||std::strlen(path)>2048)return nullptr;
 auto*& probe=FrameOverlapProbeStorage();
 if(!probe){probe=new FrameOverlapProbe(path);std::atexit([]{FrameOverlapProbeStorage()->Flush();});}
 if((probe->seen++%16)!=0||probe->used==probe->rows.size())return nullptr;
 auto& sample=probe->rows[probe->used++];sample.frame=frame;return &sample;
}
