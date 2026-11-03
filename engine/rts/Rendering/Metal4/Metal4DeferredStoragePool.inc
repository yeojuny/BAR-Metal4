// Physical storage only. Logical versions, copies, clears and producer state
// remain owned by PreparedCUSDeferred and its original publication graph.
#include <mutex>
#include <vector>
@interface M4DeferredStorageLifetime : NSObject {
@public
 std::shared_ptr<const void> owner;
}
@end
@implementation M4DeferredStorageLifetime
@end
namespace rendering::metal4 { namespace {
struct Storage {
 std::array<id<MTLTexture>,5> colors{};
 __strong id<MTLTexture> depth=nil,combinedColor=nil;
};
struct DeferredStoragePool {
 struct Slot {const Runtime* context;std::shared_ptr<Storage> owner;uint64_t bytes;};
 std::mutex mutex;std::vector<Slot> slots;uint64_t bytes=0;
 static constexpr uint64_t MaxBytes=uint64_t{256}<<20;
 static constexpr size_t MaxSlots=8;
};
DeferredStoragePool& DeferredPool(){static DeferredStoragePool pool;return pool;}
std::shared_ptr<Storage> AcquireDeferredStorage(RuntimeLease& lease,uint32_t width,uint32_t height,bool combined)
{
 Require(width&&height&&width<=lease.Capabilities().maxTextureDimension2D&&height<=lease.Capabilities().maxTextureDimension2D,"Original MRT extent exceeds actual native storage limit");
 auto& pool=DeferredPool();std::lock_guard<std::mutex> lock(pool.mutex);
 std::shared_ptr<Storage> result;
 for(const auto& slot:pool.slots){
  if(slot.owner.use_count()==1&&slot.context==lease.ContextIdentity()&&slot.owner->depth.device==lease.Device()&&slot.owner->depth.width==width&&slot.owner->depth.height==height&&bool(slot.owner->combinedColor)==combined){result=slot.owner;break;}
 }
 if(!result){
  result=std::make_shared<Storage>();
  auto allocate=[&](MTLPixelFormat format){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto image=[lease.Device()newTextureWithDescriptor:d];Require(image!=nil,"Cannot allocate original deferred output storage");return image;};
  for(auto& color:result->colors)color=allocate(MTLPixelFormatRGBA8Unorm);
  if(combined)result->combinedColor=allocate(MTLPixelFormatRGBA8Unorm);
  result->depth=allocate(MTLPixelFormatDepth32Float);
  uint64_t bytes=result->depth.allocatedSize+result->combinedColor.allocatedSize;
  for(auto color:result->colors)bytes+=color.allocatedSize;
  if(bytes<=DeferredStoragePool::MaxBytes){
   for(auto it=pool.slots.begin();it!=pool.slots.end()&&(pool.slots.size()>=DeferredStoragePool::MaxSlots||pool.bytes>DeferredStoragePool::MaxBytes-bytes);){
    if(it->owner.use_count()==1){pool.bytes-=it->bytes;it=pool.slots.erase(it);}else ++it;
   }
   if(pool.slots.size()<DeferredStoragePool::MaxSlots&&pool.bytes<=DeferredStoragePool::MaxBytes-bytes){pool.slots.push_back({lease.ContextIdentity(),result,bytes});pool.bytes+=bytes;}
  }
 }
 // Even after prepared frames release, a pending or poisoned completion graph
 // must prevent pool reuse. Preserve the existing RuntimeLease failure rules.
 auto lifetime=[M4DeferredStorageLifetime new];lifetime->owner=result;lease.Retain(lifetime);
 return result;
}
} }
