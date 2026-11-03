// Geometry only. Every hit proves exact bytes against an independently owned,
// previously validated immutable shared buffer. Per-draw checks remain.
struct ShadowTerrainGeometryBuffers {
 __strong id<MTLBuffer> vertices=nil,indices=nil;
};
class ShadowTerrainGeometryCache {
public:
 static constexpr std::size_t MaxEntries=128;
 static constexpr std::uint64_t MaxBytes=std::uint64_t{64}<<20;
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
  const Key& key,const NativeTerrainShadowDraw& draw) {
  return Acquire(lease.ContextIdentity(),lease.Device(),generation,key,draw);
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
    old.lastUse=useSerial;
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
  bool fits=cost<=MaxBytes;
  for(auto buffer:{result.vertices,result.indices}) {
   const auto n=std::max(std::uint64_t(buffer.length),std::uint64_t(buffer.allocatedSize));
   if(n>MaxBytes || cost>MaxBytes-n){fits=false;break;}cost+=n;
  }
  // Never mutate old GPU storage. Prepared draws, the request's allocation
  // set and the existing completion graph own it independently of this cache.
  if(auto found=entries.find(key);found!=entries.end()){bytes-=found->second.cost;entries.erase(found);}
  if(fits) {
   while(!entries.empty() && (entries.size()>=MaxEntries || bytes>MaxBytes-cost)) {
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
};
