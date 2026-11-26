
static bool ExpandedTerrainShadowCache(){static const bool enabled=[](){const char* p=std::getenv("BAR_NATIVE_SHADOW_TERRAIN_CACHE_WORKINGSET");return p&&std::string_view(p)=="1";}();return enabled;}
static bool TerrainCacheCounters(){static const bool enabled=[](){const char* p=std::getenv("BAR_NATIVE_SHADOW_TERRAIN_CACHE_COUNTERS");return p&&std::string_view(p)=="1";}();return enabled;}

// Geometry only. Every hit proves exact bytes against an independently owned,
// previously validated immutable shared buffer. Per-draw checks remain.
struct ShadowTerrainGeometryBuffers {
 __strong id<MTLBuffer> vertices=nil,indices=nil;
};
class ShadowTerrainGeometryCache {
public:
 static constexpr std::size_t MaxEntries=128;
 static constexpr std::uint64_t MaxBytes=std::uint64_t{64}<<20;

 static constexpr std::uint64_t MiB=std::uint64_t{1}<<20;
 static constexpr std::uint64_t Quantum=8*MiB;
 // Estimate only sizes; no terrain contents read or reordered. The existing
 // byte-equality/snapshot proof and independent GPU retirement owners remain.
 static std::uint64_t SatAdd(std::uint64_t a,std::uint64_t b,std::uint64_t cap){return b>=cap||a>=cap-b?cap:a+b;}
 static std::uint64_t RoundBudget(std::uint64_t n,std::uint64_t q,std::uint64_t cap){return n>=cap||n>cap-(q-1)?cap:std::min(cap,((n+q-1)/q)*q);}
 static bool Adaptive(){static const bool enabled=[](){const char* p=std::getenv("BAR_NATIVE_SHADOW_TERRAIN_CACHE_ADAPTIVE");return p&&std::string_view(p)=="1";}();return ExpandedTerrainShadowCache()&&enabled;}
 std::size_t EntryLimit()const{return Adaptive()?adaptiveEntries:ExpandedTerrainShadowCache()?512:MaxEntries;}
 std::uint64_t ByteLimit()const{return Adaptive()?adaptiveBytes:ExpandedTerrainShadowCache()?(192*MiB):MaxBytes;}
 std::uint64_t HardLimit()const{return Adaptive()?adaptiveHard:ByteLimit();}
 void BeginWorkingSet(id<MTLDevice> device,const NativeTerrainShadowFrame& frame){
  if(!Adaptive())return;
  if(workingDevice!=device||workingContext!=frame.contextGeneration){workingDevice=device;workingContext=frame.contextGeneration;workingFrame=0;adaptiveEntries=MaxEntries;adaptiveBytes=MaxBytes;smallFrames=0;}
  if(workingFrame==frame.sourceDrawFrame)return;workingFrame=frame.sourceDrawFrame;demandBytes=0;
  const auto recommended=uint64_t(device.recommendedMaxWorkingSetSize);
  // Bound retained cache charge by device budget and an absolute safety cap.
  // Under heavy device allocation pressure halve the cap; only cache refs are
  // evicted, not pending submission owners. The normal path never waits on GPU.
  auto hard=std::clamp(recommended?recommended/32:256*MiB,MaxBytes,512*MiB);
  const bool press=recommended&&uint64_t(device.currentAllocatedSize)>recommended-recommended/10;
  if(press)hard=std::max(MaxBytes,hard/2);
  adaptiveHard=hard;
  const auto count=std::min<size_t>(frame.draws.size(),4096u);
  const auto desiredEntries=std::clamp(count+std::max<size_t>(32,count/4),MaxEntries,size_t(4096));
  std::uint64_t projected=0;for(const auto& draw:frame.draws){
   const auto vertex=uint64_t(draw.Positions().size())*sizeof(draw.Positions()[0]);const auto index=uint64_t(draw.Indices().size())*sizeof(uint32_t);
   for(auto v:{vertex,index})if(v){projected=SatAdd(projected,v,hard);projected=SatAdd(projected,RoundBudget(v,16384,hard),hard);}
  }
  const auto desiredBytes=std::max(MaxBytes,RoundBudget(SatAdd(projected,std::max(8*MiB,projected/4),hard),Quantum,hard));
  if(desiredEntries>adaptiveEntries||desiredBytes>adaptiveBytes){adaptiveEntries=std::max(adaptiveEntries,desiredEntries);adaptiveBytes=std::max(adaptiveBytes,desiredBytes);smallFrames=0;}
  // Don't repeatedly shrink/grow around camera/LOD changes. A new smaller
  // budget needs 120 source frames of demand <= 75% of current capacity.
  else if(desiredEntries*4<=adaptiveEntries*3||desiredBytes<=adaptiveBytes-adaptiveBytes/4){
   if(++smallFrames>=120){adaptiveEntries=desiredEntries;adaptiveBytes=desiredBytes;smallFrames=0;}
  }else smallFrames=0;
  adaptiveBytes=std::min(adaptiveBytes,adaptiveHard);
  Trim(0,0);
 }
 void ObserveActualCost(std::uint64_t cost){
  if(!Adaptive())return;demandBytes=SatAdd(demandBytes,cost,adaptiveHard);
  if(demandBytes>adaptiveBytes){adaptiveBytes=std::min(adaptiveHard,RoundBudget(SatAdd(demandBytes,std::max(8*MiB,demandBytes/4),adaptiveHard),Quantum,adaptiveHard));smallFrames=0;}
 }
 void Trim(std::uint64_t needed,std::size_t additional){
  const auto limit=ByteLimit();const auto count=EntryLimit();
  while(!entries.empty()&&(entries.size()+additional>count||needed>limit||bytes>limit-needed)){
   auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.lastUse<b.second.lastUse;});bytes-=oldest->second.cost;entries.erase(oldest);
  }
 }

 struct Statistics {std::uint64_t hits=0,misses=0;};
 using Key=std::tuple<std::int32_t,std::int32_t,bool,std::size_t>;
 template<class T> static bool Equal(id<MTLBuffer> buffer,const std::vector<T>& data) {
  if(data.empty())return buffer==nil || buffer.length==0;
  if(!buffer || buffer.storageMode!=MTLStorageModeShared || data.size()>buffer.length/sizeof(T) ||
   data.size()*sizeof(T)!=buffer.length || buffer.contents==nullptr)return false;
  // This private cache allocates immutable read-only vertex/index buffers.
  // CPU comparison and GPU reads may share the same retained bytes; no writer
  // exists after upload. Replacement always allocates different storage.
  return std::memcmp(buffer.contents,data.data(),buffer.length)==0;
 }
 ShadowTerrainGeometryBuffers Acquire(RuntimeLease& lease,std::uint64_t generation,
  const Key& key,const NativeTerrainShadowDraw& draw,Statistics* statistics=nullptr) {
  return Acquire(lease.ContextIdentity(),lease.Device(),generation,key,draw,statistics);
 }
 // CPU-only preparation. Old buffers are immutable and independently owned
 // by pending submissions; a miss allocates new shared storage, never writes
 // to the buffer being read by the GPU. No command/residency state is touched.
 ShadowTerrainGeometryBuffers Acquire(const Runtime* selectedContext,id<MTLDevice> selectedDevice,std::uint64_t generation,
  const Key& key,const NativeTerrainShadowDraw& draw,Statistics* statistics=nullptr) {
  Require(generation!=0,"Original MAP shadow geometry has no source context");
  Require(selectedContext&&selectedDevice,"Original MAP shadow geometry has no runtime/device");
  if(context!=selectedContext || device!=selectedDevice || sourceContext!=generation) {
   entries.clear();bytes=0;useSerial=0;context=selectedContext;device=selectedDevice;sourceContext=generation;
  }
  // Recency belongs to this serialized cache owner, not GPU completion.
  // A wrapped counter simply drops cache references; pending owners survive.
  if(++useSerial==0){entries.clear();bytes=0;useSerial=1;}
  if(auto found=entries.find(key);found!=entries.end()) {
   auto& old=found->second;
   if(old.indexed==draw.indexed && ((draw.meshSnapshot&&old.snapshot==draw.meshSnapshot) || (Equal(old.buffers.vertices,draw.Positions()) && Equal(old.buffers.indices,draw.Indices())))) {
    ObserveActualCost(old.cost);old.lastUse=useSerial;
    if(draw.meshSnapshot)old.snapshot=draw.meshSnapshot;
    if(statistics)++statistics->hits;CountPreparationTrace(7,1);return old.buffers;
   }
  }
  if(statistics)++statistics->misses;CountPreparationTrace(8,1);
  const auto count=draw.indexed?draw.Indices().size():draw.Positions().size();
  Require(count && count%3==0 && count<=UINT32_MAX && !draw.Positions().empty() && (draw.indexed || draw.Indices().empty()),
   "Original MAP shadow has a different triangle invocation");
  for(auto index:draw.Indices())Require(index<draw.Positions().size(),"Original MAP shadow retained index exceeds its source vertices");
  for(const auto& vertex:draw.Positions())for(float value:vertex)Require(std::isfinite(value),"Original MAP shadow position is nonfinite");
  ShadowTerrainGeometryBuffers result;
  auto upload=[&](const auto& data,NSString* label) {
   using T=typename std::decay_t<decltype(data)>::value_type;
   Require(!data.empty() && data.size()<=selectedDevice.maxBufferLength/sizeof(T),"Original generic shadow buffer exceeds the actual device bounds");
   auto buffer=[selectedDevice newBufferWithBytes:data.data() length:data.size()*sizeof(T) options:MTLResourceStorageModeShared];
   Require(buffer!=nil,"Cannot allocate world GPU buffer");buffer.label=label;return buffer;
  };
  result.vertices=upload(draw.Positions(),@"Original MAP shadow vertices");
  if(draw.indexed)result.indices=upload(draw.Indices(),@"Original MAP shadow indices");
  // Keep the previous conservative admission charge and eviction behavior,
  // although the separate CPU snapshot is no longer allocated.
  std::uint64_t cost=draw.Positions().size()*sizeof(draw.Positions()[0])+draw.Indices().size()*sizeof(std::uint32_t);
  const auto hard=HardLimit();bool fits=cost<=hard;
  for(auto buffer:{result.vertices,result.indices}) {
   const auto n=std::max(std::uint64_t(buffer.length),std::uint64_t(buffer.allocatedSize));
   if(n>hard || cost>hard-n){fits=false;break;}cost+=n;
  }
  if(fits)ObserveActualCost(cost);const auto byteLimit=ByteLimit();const auto entryLimit=EntryLimit();fits=fits&&cost<=byteLimit;
  // Never mutate old GPU storage. Prepared draws, the request's allocation
  // set and the existing completion graph own it independently of this cache.
  if(auto found=entries.find(key);found!=entries.end()){bytes-=found->second.cost;entries.erase(found);}
  if(fits) {
   while(!entries.empty() && (entries.size()>=entryLimit || bytes>byteLimit-cost)) {
    // Key order is spatial/ordinal, not age: evicting begin() repeatedly
    // drops visible low-key patches while retaining stale camera coverage.
    auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.lastUse<b.second.lastUse;});
    bytes-=oldest->second.cost;entries.erase(oldest);
   }
   entries.emplace(key,Entry{draw.indexed,draw.meshSnapshot,result,cost,useSerial});bytes+=cost;
  }
  return result;
 }
 std::size_t Count()const{return entries.size();}
 std::uint64_t Bytes()const{return bytes;}
private:
 struct Entry {
  bool indexed;
  std::shared_ptr<const NativeTerrainMeshSnapshot> snapshot;
  ShadowTerrainGeometryBuffers buffers;
  std::uint64_t cost,lastUse;
 };
 const Runtime* context=nullptr;
 __strong id<MTLDevice> device=nil;
 std::uint64_t sourceContext=0,bytes=0,useSerial=0;
 std::map<Key,Entry> entries;
 __strong id<MTLDevice> workingDevice=nil;std::uint64_t workingContext=0,workingFrame=0,adaptiveBytes=MaxBytes,adaptiveHard=MaxBytes,demandBytes=0;std::size_t adaptiveEntries=MaxEntries;unsigned smallFrames=0;
};
