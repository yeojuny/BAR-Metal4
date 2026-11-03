// Physical storage has no producer state. Logical versions and each submission
// retain this token independently; only a sole pool owner may be reused.
#include <mutex>
#include <vector>
@interface M4SSAOStorageLifetime : NSObject {
@public
 std::shared_ptr<const void> owner;
}
@end
@implementation M4SSAOStorageLifetime
@end
namespace rendering::metal4 { namespace {
struct SSAOStorage { __strong id<MTLTexture> texture=nil; };
struct SSAOStoragePool {
 struct Slot {const Runtime* context;std::shared_ptr<SSAOStorage> owner;uint64_t bytes;};
 std::mutex mutex;std::vector<Slot> slots;uint64_t bytes=0;
 static constexpr uint64_t MaxBytes=uint64_t{128}<<20;
 static constexpr size_t MaxSlots=16;
 void Clear(){std::lock_guard<std::mutex> lock(mutex);slots.clear();bytes=0;}
};
void RetainSSAOStorage(RuntimeLease& lease,const std::shared_ptr<SSAOStorage>& storage)
{
 auto lifetime=[M4SSAOStorageLifetime new];lifetime->owner=storage;lease.Retain(lifetime);
}
std::shared_ptr<SSAOStorage> AcquireSSAOStorage(SSAOStoragePool& pool,RuntimeLease& lease,uint32_t width,uint32_t height,MTLPixelFormat format)
{
 Require(width&&height&&width<=lease.Capabilities().maxTextureDimension2D&&height<=lease.Capabilities().maxTextureDimension2D,"SSAO original FBO exceeds actual device limits");
 Require(format==MTLPixelFormatRGBA8Unorm||format==MTLPixelFormatRGBA16Float,"SSAO physical storage format is unsupported");
 std::lock_guard<std::mutex> lock(pool.mutex);std::shared_ptr<SSAOStorage> result;
 for(const auto& slot:pool.slots){
  const auto texture=slot.owner->texture;
  if(slot.owner.use_count()==1&&slot.context==lease.ContextIdentity()&&texture.device==lease.Device()&&texture.width==width&&texture.height==height&&texture.pixelFormat==format){result=slot.owner;break;}
 }
 if(!result){
  result=std::make_shared<SSAOStorage>();
  auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
  descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
  result->texture=[lease.Device()newTextureWithDescriptor:descriptor];
  Require(result->texture!=nil,"Cannot allocate actual SSAO FBO texture");
  const uint64_t bytes=result->texture.allocatedSize;
  if(bytes<=SSAOStoragePool::MaxBytes){
   for(auto it=pool.slots.begin();it!=pool.slots.end()&&(pool.slots.size()>=SSAOStoragePool::MaxSlots||pool.bytes>SSAOStoragePool::MaxBytes-bytes);){
    if(it->owner.use_count()==1){pool.bytes-=it->bytes;it=pool.slots.erase(it);}else ++it;
   }
   if(pool.slots.size()<SSAOStoragePool::MaxSlots&&pool.bytes<=SSAOStoragePool::MaxBytes-bytes){pool.slots.push_back({lease.ContextIdentity(),result,bytes});pool.bytes+=bytes;}
  }
 }
 RetainSSAOStorage(lease,result);return result;
}
} }
