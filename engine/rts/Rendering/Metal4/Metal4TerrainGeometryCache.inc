// Immutable buffers only: a cache hit never overwrites existing GPU storage.
#include <map>
#include <tuple>
#include <cstring>
#include <cmath>
namespace rendering::metal4 { namespace {
struct TerrainGeometryBuffers {__strong id<MTLBuffer> positions=nil,colors=nil,indices=nil;};
class TerrainGeometryCache {
public:
 static constexpr size_t MaxEntries=128;
 static constexpr uint64_t MaxBytes=uint64_t{96}<<20; // CPU snapshots plus actual GPU allocations.
 using Key=std::tuple<uint32_t,uint32_t,unsigned,bool>;
 template<class T>static bool Equal(const std::vector<T>& a,const std::vector<T>& b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
 TerrainGeometryBuffers Acquire(RuntimeLease& lease,const Key& key,const NativeTerrainMeshGeometry& mesh,const std::shared_ptr<const NativeTerrainMeshSnapshot>& snapshot={}){
  Require(!snapshot||&mesh==&snapshot->Geometry(),"Terrain snapshot and geometry must have the same immutable owner");
  if(context!=lease.ContextIdentity()||device!=lease.Device()){entries.clear();bytes=0;context=lease.ContextIdentity();device=lease.Device();}
  if(auto it=entries.find(key);it!=entries.end()){
   if(snapshot&&it->second.snapshot==snapshot)return it->second.buffers;
   const auto& old=it->second.snapshot?it->second.snapshot->Geometry():it->second.source;
   if(old.indexed==mesh.indexed&&Equal(old.positions,mesh.positions)&&Equal(old.colors,mesh.colors)&&Equal(old.indices,mesh.indices)){
    if(snapshot){it->second.source={};it->second.snapshot=snapshot;}
    return it->second.buffers;
   }
  }
  // Cache entries are admitted only after complete geometry validation.
  // A hit uses the retained immutable producer version, or compares every
  // position/color/index byte for callers without that version proof.
		for(const auto& position:mesh.positions)for(float component:position)
			Require(std::isfinite(component),"Original terrain submitted a nonfinite local vertex position");
		for(uint32_t element:mesh.indices)Require(element<mesh.positions.size(),"Original terrain index exceeds its retained source vertices");
  TerrainGeometryBuffers result;
  auto upload=[&](const auto& data,const char* message){
   using T=typename std::decay_t<decltype(data)>::value_type;
   Require(!data.empty()&&data.size()<=lease.Device().maxBufferLength/sizeof(T),"Original terrain stream exceeds actual device capacity");
   auto buffer=[lease.Device()newBufferWithBytes:data.data() length:data.size()*sizeof(T) options:MTLResourceStorageModeShared];Require(buffer!=nil,message);return buffer;
  };
  result.positions=upload(mesh.positions,"Cannot retain original terrain position stream");
  if(!mesh.colors.empty())result.colors=upload(mesh.colors,"Cannot retain original border normalized UByte color stream");
  if(mesh.indexed)result.indices=upload(mesh.indices,"Cannot retain original terrain index order");
  // Count the exact vector-owned elements and physical GPU byte sizes. Keep
  // oversized requests fully functional without adding them to this cache.
  const uint64_t cpu=mesh.positions.size()*sizeof(mesh.positions.front())+mesh.colors.size()*sizeof(mesh.colors.front())+mesh.indices.size()*sizeof(uint32_t);
  uint64_t cost=cpu;bool fits=cost<=MaxBytes;
  for(auto buffer:{result.positions,result.colors,result.indices}){const auto n=uint64_t(buffer.allocatedSize);if(n>MaxBytes||cost>MaxBytes-n){fits=false;break;}cost+=n;}
  if(auto it=entries.find(key);it!=entries.end()){bytes-=it->second.bytes;entries.erase(it);}
  if(fits){
   while(!entries.empty()&&(entries.size()>=MaxEntries||bytes>MaxBytes-cost)){auto it=entries.begin();bytes-=it->second.bytes;entries.erase(it);}
   entries.emplace(key,Entry{snapshot?NativeTerrainMeshGeometry{}:mesh,snapshot,result,cost});bytes+=cost;
  }
  return result;
 }
 size_t Count()const{return entries.size();}uint64_t Bytes()const{return bytes;}
private:
 struct Entry {NativeTerrainMeshGeometry source;std::shared_ptr<const NativeTerrainMeshSnapshot> snapshot;TerrainGeometryBuffers buffers;uint64_t bytes;};
 const Runtime* context=nullptr;__strong id<MTLDevice> device=nil;
 std::map<Key,Entry> entries;uint64_t bytes=0;
};
} }
