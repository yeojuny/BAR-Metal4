// Separate from the legacy CPU/residency census: no per-resource introspection,
// GPU timing reads, locks, heap allocation or log output inside timed regions.
namespace {
struct PreparationTraceState {
 bool active=false;
 std::uint64_t began=0, ordinal=0, samples=0, last=0;
 std::int64_t frame=0;
 std::array<std::uint64_t,40> times{};
 std::array<std::uint64_t,46> counters{};
};
thread_local PreparationTraceState preparationTrace;
const char* ResourceProbe91Path() {
 static const char* value=[] {const char* p=std::getenv("BAR_NATIVE_RESOURCE_PROBE_FILE");return p&&p[0]=='/'&&std::strlen(p)<=2048?p:nullptr;}();
 return value;
}
struct ResourceProbe91Rows {
 std::array<PreparationTraceState,128> rows;std::size_t used=0;
 void Flush() noexcept {
  FILE* f=std::fopen(ResourceProbe91Path(),"wx");if(!f)return;
  std::fprintf(f,"frame");for(unsigned i=0;i<40;++i)std::fprintf(f,",t%u_us",i);for(unsigned i=0;i<46;++i)std::fprintf(f,",c%u",i);std::fprintf(f,"\n");
  for(std::size_t n=0;n<used;++n){const auto& r=rows[n];std::fprintf(f,"%lld",(long long)r.frame);for(auto t:r.times)std::fprintf(f,",%.3f",double(t)/1000);for(auto c:r.counters)std::fprintf(f,",%llu",(unsigned long long)c);std::fprintf(f,"\n");}std::fclose(f);
 }
};
ResourceProbe91Rows*& ResourceProbe91Storage(){static ResourceProbe91Rows* rows=nullptr;return rows;}
void StoreResourceProbe91(const PreparationTraceState& sample) {
 auto*& p=ResourceProbe91Storage();if(!p){p=new ResourceProbe91Rows;std::atexit([]{ResourceProbe91Storage()->Flush();});}
 if(p->used<p->rows.size())p->rows[p->used++]=sample;
}
}
bool BeginPreparationTrace(std::int64_t frame) noexcept {
 static const bool enabled=[] {
  const char* p=std::getenv("BAR_NATIVE_PREPARATION_TRACE");
  return ResourceProbe91Path() || (p && p[0]=='1' && p[1]=='\0');
 }();
 auto& s=preparationTrace;
 if (!enabled || frame<=0 || s.active || s.samples>=(ResourceProbe91Path()?128u:256u)) return false;
 if (++s.ordinal%16!=1) return false;
 const auto now=SubmissionTraceNow();
 if (!s.began) s.began=now;
 if (now-s.began>=std::uint64_t{300}*1000000000) return false;
 s.active=true;s.last=now;s.frame=frame;s.times.fill(0);s.counters.fill(0);++s.samples;
 return true;
}
void MarkPreparationTrace(unsigned category) noexcept {
 auto& s=preparationTrace;
 if (!s.active || category>=s.times.size()) return;
 const auto now=SubmissionTraceNow();s.times[category]+=now-s.last;s.last=now;
}
std::uint64_t StartPreparationScope() noexcept {
 return preparationTrace.active?SubmissionTraceNow():0;
}
void EndPreparationScope(unsigned category,std::uint64_t start) noexcept {
 auto& s=preparationTrace;
 if (s.active && start && category<s.times.size()) s.times[category]+=SubmissionTraceNow()-start;
}
void CountPreparationTrace(unsigned counter,std::uint64_t amount) noexcept {
 auto& s=preparationTrace;
 if (s.active && counter<s.counters.size()) s.counters[counter]+=amount;
}
void EndPreparationTrace() noexcept {
 auto& s=preparationTrace;if(!s.active)return;s.active=false;
 if(ResourceProbe91Path()){StoreResourceProbe91(s);return;}
#ifdef RECOIL_METAL4_LIVE_SCENE
 char text[2048];std::size_t used=0;
 auto append=[&](const char* format,auto... values) {
  const int n=std::snprintf(text+used,sizeof(text)-used,format,values...);
  if(n<0 || std::size_t(n)>=sizeof(text)-used)return false;
  used+=std::size_t(n);return true;
 };
 if(!append("[NativePrepDetail] sample=%llu frame=%lld",(unsigned long long)s.samples,(long long)s.frame))return;
 for(unsigned i=0;i<32;++i)if(!append(" t%u=%.3f",i,double(s.times[i])/1000.0))return;
 for(unsigned i=0;i<12;++i)if(!append(" c%u=%llu",i,(unsigned long long)s.counters[i]))return;
 // At most 256 bounded rows (under 512 KiB), after preparation and submission.
 LOG("%s",text);
#endif
}
