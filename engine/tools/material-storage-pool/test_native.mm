/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Optional Apple-SDK test of the ACTUAL material header. No GPU commands are
// submitted; submitted/late-feedback owners are modeled by binding copies.
#import <Foundation/Foundation.h>
#include "Rendering/Metal4/Metal4ModelMaterial156.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace rendering::metal4;
struct TestLease {
 __strong id<MTLDevice> device;
 const void* context;
 std::vector<id<MTLBuffer>> nativePins;
 id<MTLDevice> Device() const {return device;}
 const void* ContextIdentity() const {return context;}
 void AddAllocation(id<MTLBuffer> buffer) {assert(buffer);nativePins.push_back(buffer);}
};
std::vector<unsigned char> Bytes(const ModelMaterialBinding156& b){
 assert(b&&b.buffer.contents);const auto* p=static_cast<const unsigned char*>(b.buffer.contents);
 return {p,p+b.buffer.length};
}
int main(){@autoreleasepool{
 id<MTLDevice> device=MTLCreateSystemDefaultDevice();
 if(!device){std::cerr<<"No Metal device\n";return 77;}
 int firstContext=0,secondContext=0;TestLease lease{device,&firstContext,{}};
 ModelMaterials156 materials;
 const bool pool=materials.UsesStoragePool();
 std::array<id<MTLSamplerState>,12> samplers{};
 auto samplerDescriptor=[MTLSamplerDescriptor new];
 samplers[0]=[device newSamplerStateWithDescriptor:samplerDescriptor];assert(samplers[0]);
 std::vector<id<MTLTexture>> assets;
 auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
 descriptor.storageMode=MTLStorageModeShared;
 for(unsigned i=0;i<200;++i){auto texture=[device newTextureWithDescriptor:descriptor];assert(texture);assets.push_back(texture);}
 std::vector<ModelMaterialBinding156> current,previous,feedback;
 ModelMaterialBinding156 quarantine;std::vector<unsigned char> snapshot;
 std::size_t created=0,reused=0,encodes=0,pins=0;
 for(unsigned batch=0;batch<300;++batch){
  materials.BeginBatch(lease);lease.nativePins.clear();current.clear();
  for(unsigned i=0;i<16;++i){
   std::array<id<MTLTexture>,12> textures{};textures[0]=assets[(batch*8+i%8)%assets.size()];
   auto binding=materials.Get(lease,textures,samplers);assert(binding);
   assert(bool(binding.storage)==pool);current.push_back(binding);
   if(batch==0&&i==0){quarantine=binding;snapshot=Bytes(binding);}
   assert(Bytes(quarantine)==snapshot);
  }
  const auto& stats=materials.Stats();assert(stats.hits+stats.created==16);
  assert(stats.newBuffers+stats.reusedBuffers==stats.created);
  assert(lease.nativePins.size()==16);
  created+=stats.newBuffers;reused+=stats.reusedBuffers;encodes+=stats.created;pins+=lease.nativePins.size();
  feedback=std::move(previous);previous=current;
 }
 assert(materials.Entries()<=128&&materials.StoragePoolCount()<=256);
 if(pool)assert(reused>0&&created<encodes);else assert(reused==0&&created==encodes&&materials.StoragePoolCount()==0);
 // A different source context drops only cache/pool ownership, never the held draw.
 lease.context=&secondContext;materials.BeginBatch(lease);
 assert(materials.Entries()==0&&materials.StoragePoolCount()==0&&Bytes(quarantine)==snapshot);
 materials.Clear();assert(Bytes(quarantine)==snapshot);
 std::cout<<"native material preparation pool="<<pool<<" new_buffers="<<created<<" reused="<<reused
          <<" encodes="<<encodes<<" registrations="<<pins<<" old_bytes_unchanged=1 PASS\n";
}}
