// Pool physical images only; each logical Resource owns its own upload state.
#include <mutex>
#include <vector>
@interface M4CubeStorageLifetime : NSObject {
@public
 std::shared_ptr<const void> owner;
}
@end
@implementation M4CubeStorageLifetime
@end
namespace rendering::metal4 { namespace {
struct CubeStorage {
 __strong id<MTLTexture> texture=nil,depth=nil;
};
struct CubeStoragePool {
 struct Slot {const Runtime* context;NativeCubeTextureKind kind;std::shared_ptr<CubeStorage> owner;uint64_t bytes;};
 std::mutex mutex;std::vector<Slot> slots;uint64_t bytes=0;
 static constexpr uint64_t MaxBytes=uint64_t{256}<<20;
 static constexpr size_t MaxSlots=8;
};
CubeStoragePool& CubePool(){static CubeStoragePool pool;return pool;}
void RetainCubeStorage(RuntimeLease& lease,const std::shared_ptr<CubeStorage>& storage)
{
 auto lifetime=[M4CubeStorageLifetime new];lifetime->owner=storage;lease.Retain(lifetime);
}
std::shared_ptr<CubeStorage> ReserveCubeStorage(const Runtime& runtime,NativeCubeTextureKind kind,uint32_t size,uint32_t levels)
{
 Require(size&&size<=runtime.Capabilities().maxTextureDimension2D&&levels,"Original cube allocation extent differs");
 uint32_t maximum=1;for(auto n=size;n>1;n/=2)++maximum;Require(levels<=maximum,"Original cube mip extent differs");
 auto& pool=CubePool();std::lock_guard<std::mutex> lock(pool.mutex);std::shared_ptr<CubeStorage> result;
 for(const auto& slot:pool.slots){
  if(slot.owner.use_count()==1&&slot.context==&runtime&&slot.kind==kind&&slot.owner->texture.device==runtime.Device()&&slot.owner->texture.width==size&&slot.owner->texture.mipmapLevelCount==levels){result=slot.owner;break;}
 }
 if(!result){
  result=std::make_shared<CubeStorage>();
  auto* d=[MTLTextureDescriptor new];d.textureType=MTLTextureTypeCube;d.width=d.height=size;d.depth=1;d.arrayLength=1;d.mipmapLevelCount=levels;d.pixelFormat=MTLPixelFormatRGBA8Unorm;d.storageMode=kind==NativeCubeTextureKind::Specular?MTLStorageModeShared:MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
  result->texture=[runtime.Device()newTextureWithDescriptor:d];Require(result->texture,"Cannot allocate original named cube");result->texture.label=@"Actual CubeMapHandler logical storage";
  if(kind!=NativeCubeTextureKind::Specular){auto* depth=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:size height:size mipmapped:NO];depth.storageMode=MTLStorageModePrivate;depth.usage=MTLTextureUsageRenderTarget;result->depth=[runtime.Device()newTextureWithDescriptor:depth];Require(result->depth,"Cannot allocate original cube framebuffer depth");}
  const uint64_t bytes=result->texture.allocatedSize+result->depth.allocatedSize;
  if(bytes<=CubeStoragePool::MaxBytes){
   for(auto it=pool.slots.begin();it!=pool.slots.end()&&(pool.slots.size()>=CubeStoragePool::MaxSlots||pool.bytes>CubeStoragePool::MaxBytes-bytes);){
    if(it->owner.use_count()==1){pool.bytes-=it->bytes;it=pool.slots.erase(it);}else ++it;
   }
   if(pool.slots.size()<CubeStoragePool::MaxSlots&&pool.bytes<=CubeStoragePool::MaxBytes-bytes){pool.slots.push_back({&runtime,kind,result,bytes});pool.bytes+=bytes;}
  }
 }
 // Reservation changes no texture contents or shared submission state. Every
 // logical Resource owns this pin; preparation adds its independent GPU pin.
 return result;
}
std::shared_ptr<CubeStorage> AcquireCubeStorage(RuntimeLease& lease,NativeCubeTextureKind kind,uint32_t size,uint32_t levels)
{
 auto result=ReserveCubeStorage(*lease.ContextIdentity(),kind,size,levels);
 RetainCubeStorage(lease,result);return result;
}
} }
