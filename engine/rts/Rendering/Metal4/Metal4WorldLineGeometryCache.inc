// Only single-sample canonical lines. Cache entries own an exact CPU input
// snapshot and immutable GPU output. No buffer is ever overwritten. The caller
// validates every incoming command before lookup and retains every returned
// allocation in its normal completion graph, independently of cache eviction.
struct WorldLineGeometryBuffers {
 __strong id<MTLBuffer> vertices=nil,indices=nil;
 std::uint32_t count=0;
 std::shared_ptr<std::atomic<bool>> completed; // Null for original CPU-written buffers.
};
class WorldLineGeometryCache {
public:
 static constexpr std::size_t MaxEntries=32;
 static constexpr std::uint64_t MaxBytes=std::uint64_t{32}<<20;
 using Key=std::pair<NativeWorldDrawStage,std::size_t>;
 static bool Eligible(const NativeWorldDrawCommand& d,unsigned samples) {
  return samples==1 && d.primitive==NativeDrawPrimitive::Lines &&
   d.surface.material==ScreenMaterial::Solid && !d.surface.secondary.texture;
 }
 template<class T> static bool Equal(const std::vector<T>& a,const std::vector<T>& b) {
  return a.size()==b.size() && (a.empty() || std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);
 }
 static bool SameWorld(const NativeWorldDrawState& a,const NativeWorldDrawState& b) {
  // Semantic equality covers every scalar/enum/point/stencil field. Preserve
  // bit identity as well for the floating values used by clipping/raster math.
  if(!(a==b))return false;
  if(std::memcmp(&a.lineWidth,&b.lineWidth,sizeof(float)) || std::memcmp(&a.pointSize,&b.pointSize,sizeof(float)))return false;
  return !a.inheritedClipPlane || std::memcmp(a.inheritedClipPlane->data(),b.inheritedClipPlane->data(),sizeof(float)*4)==0;
 }
 std::shared_ptr<const WorldLineGeometryBuffers> Find(RuntimeLease& lease,const NativeWorldDrawCommand& d,
  std::size_t ordinal,const std::array<double,4>& viewport,std::uint32_t width,std::uint32_t height,unsigned samples) {
  ResetContext(lease);
  if(!Eligible(d,samples))return {};
  const auto found=entries.find({d.stage,ordinal});if(found==entries.end())return {};
  const auto& e=found->second;
  if(e.buffers->completed&&!e.buffers->completed->load(std::memory_order_acquire))return {};
  if(e.sourcePrimitive!=d.sourcePrimitive || !SameWorld(e.world,d.world) || e.width!=width || e.height!=height ||
   std::memcmp(e.viewport.data(),viewport.data(),sizeof(double)*4) || !Equal(e.vertices,d.vertices) || !Equal(e.indices,d.indices))return {};
  CountPreparationTrace(31,1);return e.buffers;
 }
 void Store(RuntimeLease& lease,const NativeWorldDrawCommand& d,std::size_t ordinal,
  const std::array<double,4>& viewport,std::uint32_t width,std::uint32_t height,unsigned samples,
  const NativeWorldRasterMesh& mesh,id<MTLBuffer> vertices,id<MTLBuffer> indices) {
  ResetContext(lease);if(!Eligible(d,samples))return;
  Require(!mesh.fullViewport && mesh.pointDisks.empty() && vertices && indices &&
   mesh.indices.size()<=UINT32_MAX && mesh.indices.size()*sizeof(std::uint32_t)==indices.length &&
   mesh.vertices.size()*sizeof(NativeWorldDrawVertex)==vertices.length,
   "Immutable world line cache received a different raster output");
  StoreGenerated(lease,d,ordinal,viewport,width,height,samples,uint32_t(mesh.indices.size()),vertices,indices,{});
 }
 void StoreGenerated(RuntimeLease& lease,const NativeWorldDrawCommand& d,std::size_t ordinal,
  const std::array<double,4>& viewport,std::uint32_t width,std::uint32_t height,unsigned samples,
  uint32_t count,id<MTLBuffer> vertices,id<MTLBuffer> indices,std::shared_ptr<std::atomic<bool>> completed) {
  ResetContext(lease);if(!Eligible(d,samples))return;
  Require(vertices&&indices&&count&&count%6==0&&uint64_t(count)*sizeof(uint32_t)==indices.length&&
   uint64_t(count/6)*4*sizeof(NativeWorldDrawVertex)==vertices.length,
   "World line cache generated output bounds differ");
  const Key key{d.stage,ordinal};
  if(auto old=entries.find(key);old!=entries.end()){bytes-=old->second.cost;entries.erase(old);}
  std::uint64_t cost=d.vertices.size()*sizeof(NativeWorldDrawVertex)+d.indices.size()*sizeof(std::uint32_t);
  for(auto buffer:{vertices,indices}) {
   const auto n=std::max(std::uint64_t(buffer.length),std::uint64_t(buffer.allocatedSize));
   if(n>MaxBytes || cost>MaxBytes-n)return;cost+=n;
  }
  if(cost>MaxBytes)return;
  Entry e;e.sourcePrimitive=d.sourcePrimitive;e.world=d.world;e.viewport=viewport;e.width=width;e.height=height;e.cost=cost;
  e.vertices=d.vertices;e.indices=d.indices;e.buffers=std::make_shared<WorldLineGeometryBuffers>();
  e.buffers->vertices=vertices;e.buffers->indices=indices;e.buffers->count=count;e.buffers->completed=std::move(completed);
  while(!entries.empty() && (entries.size()>=MaxEntries || bytes>MaxBytes-cost)) {
   auto old=entries.begin();bytes-=old->second.cost;entries.erase(old);
  }
  entries.emplace(key,std::move(e));bytes+=cost;
 }
 std::size_t Count()const{return entries.size();}
 std::uint64_t Bytes()const{return bytes;}
private:
 struct Entry {
  NativeDrawPrimitive sourcePrimitive;
  NativeWorldDrawState world;
  std::array<double,4> viewport;
  std::uint32_t width=0,height=0;
  std::uint64_t cost=0;
  std::vector<NativeWorldDrawVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::shared_ptr<WorldLineGeometryBuffers> buffers;
 };
 void ResetContext(RuntimeLease& lease) {
  if(context==lease.ContextIdentity() && device==lease.Device())return;
  entries.clear();bytes=0;context=lease.ContextIdentity();device=lease.Device();
 }
 const Runtime* context=nullptr;
 __strong id<MTLDevice> device=nil;
 std::uint64_t bytes=0;
 std::map<Key,Entry> entries;
};
