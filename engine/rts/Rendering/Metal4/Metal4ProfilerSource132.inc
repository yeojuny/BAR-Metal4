// One selected interval per immutable source. The lease retains replaced
// allocations until completion;unused VBO capacity is never uploaded.
class ProfilerSourceCache132 {
 struct Entry {std::weak_ptr<const NativeBufferSnapshot> owner;uint32_t first=0,count=0;__strong id<MTLBuffer> buffer=nil;size_t cost=0;};
 std::unordered_map<const NativeBufferSnapshot*,Entry> entries;
 const void* context=nullptr;__strong id<MTLDevice> device=nil;size_t bytes=0;
public:
 static constexpr size_t EntryLimit=256,ByteLimit=32u*1024u*1024u;
 void Clear(){entries.clear();context=nullptr;device=nil;bytes=0;}
 void BeginBatch(RuntimeLease& lease){
  if(context!=lease.ContextIdentity()||device!=lease.Device()){Clear();context=lease.ContextIdentity();device=lease.Device();}
  for(auto i=entries.begin();i!=entries.end();)if(i->second.owner.expired()){bytes-=i->second.cost;i=entries.erase(i);}else ++i;
 }
 id<MTLBuffer> Read(RuntimeLease& lease,const NativeProfilerHistogramInput132& input){
  const size_t length=size_t(input.count)*8,offset=size_t(input.first)*8;const auto& source=input.vertices;
  Require(source&&length&&offset<=source->bytes.size()&&length<=source->bytes.size()-offset&&length<=lease.Device().maxBufferLength,"Profiler source range exceeds immutable storage/device");
  auto found=entries.find(source.get());if(found!=entries.end()){
   const auto& e=found->second;if(!e.owner.expired()&&e.first==input.first&&e.count==input.count){lease.AddAllocation(e.buffer);return e.buffer;}
   bytes-=e.cost;entries.erase(found);
  }
  auto value=[lease.Device() newBufferWithBytes:source->bytes.data()+offset length:length options:MTLResourceStorageModeShared];Require(value,"Cannot retain original selected Profiler VBO");lease.AddAllocation(value);
  profilerSource132::Counters().Upload(length);
  const size_t cost=std::max(length,size_t(value.allocatedSize));if(cost<=ByteLimit){
   while(!entries.empty()&&(entries.size()>=EntryLimit||bytes>ByteLimit-cost)){auto i=entries.begin();bytes-=i->second.cost;entries.erase(i);}
   entries.emplace(source.get(),Entry{source,input.first,input.count,value,cost});bytes+=cost;
  }
  return value;
 }
};
profilerSource132::Uniform ProfilerUniform132(const NativeProfilerHistogramInput132& input,const ScreenAlphaTest& alpha,const std::array<double,4>& viewport,unsigned width,unsigned height,unsigned samples){
 profilerSource132::Uniform u;u.box=input.box;u.view={input.view[0],input.view[1],0,0};u.color=input.color;
 for(unsigned k=0;k<4;++k){Require(std::isfinite(viewport[k])&&std::abs(viewport[k])<=32768,"Profiler physical viewport outside source bounds");u.viewport[k]=float(viewport[k]);}
 Require(width&&height&&width<=16384&&height<=16384&&(samples==1||samples==4)&&input.samples==samples,"Profiler actual attachment differs from recorded source mapping");
 u.target={float(width),float(height),input.lineWidth,alpha.reference};u.options={samples,uint32_t(alpha.enabled),uint32_t(alpha.compare),uint32_t(input.zeroToOne)};return u;
}
