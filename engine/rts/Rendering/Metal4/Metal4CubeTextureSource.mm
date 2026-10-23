/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CubeTextureSource.h"
#include <algorithm>
namespace rendering::metal4 {
namespace {
struct Resource final:NativeCubeTextureResource {
 __strong id<MTLTexture> texture=nil,depth=nil;
 mutable bool cpuUploaded=false;
};
std::shared_ptr<const Resource> Storage(const NativeCubeTextureBacking& image){auto r=std::dynamic_pointer_cast<const Resource>(image.resource);Require(r&&r->texture,"Original cube lacks its actual native allocation");return r;}
void Readable(const NativeCubeTextureBacking& image,uint64_t submission){
 if(image.content==NativeTextureContent::AllocationOnly)return;
 Require(image.completed.load(std::memory_order_acquire)||(submission&&image.encodedSubmission.load(std::memory_order_acquire)==submission),"Original cube content has not executed before this source consumer");
}
MTLSamplerAddressMode Address(unsigned value){switch(value){case 0x2901:return MTLSamplerAddressModeRepeat;case 0x812F:return MTLSamplerAddressModeClampToEdge;case 0x812D:return MTLSamplerAddressModeClampToBorderColor;default:throw std::runtime_error("Original cube addressing is not represented");}}
id<MTLSamplerState> Sampler(RuntimeLease& lease,const NativeTextureSampling& p){
 Require(!p.compareMode&&p.baseLevel==0&&p.maxLevel>=0&&p.lodBias==0&&p.anisotropy==1,"Original cube sampler requires its actual comparison/base/bias implementation");
 auto* d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;
 switch(p.minFilter){case 0x2600:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterNotMipmapped;break;case 0x2601:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNotMipmapped;break;case 0x2700:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterNearest;break;case 0x2701:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNearest;break;case 0x2702:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterLinear;break;case 0x2703:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterLinear;break;default:throw std::runtime_error("Original cube minification filter is invalid");}
 Require(p.magFilter==0x2600||p.magFilter==0x2601,"Original cube magnification filter is invalid");d.magFilter=p.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
 d.sAddressMode=Address(p.wrapS);d.tAddressMode=Address(p.wrapT);d.rAddressMode=Address(p.wrapR);d.lodMinClamp=std::max(0.f,p.minLod);d.lodMaxClamp=std::min(float(p.maxLevel),p.maxLod);
 Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{0,0,0,1}||p.borderColor==std::array<float,4>{1,1,1,1},"Original cube custom border requires its exact sampler");d.borderColor=p.borderColor[3]==0?MTLSamplerBorderColorTransparentBlack:p.borderColor[0]==1?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorOpaqueBlack;
 auto sampler=[lease.Device()newSamplerStateWithDescriptor:d];Require(sampler,"Cannot allocate original cube sampler");lease.Retain(sampler);return sampler;
}
}
std::shared_ptr<const NativeCubeTextureResource> AllocateNativeCubeTexture(RuntimeLease& lease,NativeCubeTextureKind kind,uint32_t size,uint32_t levels){
 Require(size&&size<=lease.Capabilities().maxTextureDimension2D&&levels,"Original cube allocation extent differs");uint32_t maximum=1;for(auto n=size;n>1;n/=2)++maximum;Require(levels<=maximum,"Original cube mip extent differs");auto r=std::make_shared<Resource>();
 auto* d=[MTLTextureDescriptor new];d.textureType=MTLTextureTypeCube;d.width=d.height=size;d.depth=1;d.arrayLength=1;d.mipmapLevelCount=levels;d.pixelFormat=MTLPixelFormatRGBA8Unorm;d.storageMode=kind==NativeCubeTextureKind::Specular?MTLStorageModeShared:MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
 r->texture=[lease.Device()newTextureWithDescriptor:d];Require(r->texture,"Cannot allocate original named cube");r->texture.label=@"Actual CubeMapHandler logical storage";lease.AddAllocation(r->texture);
 if(kind!=NativeCubeTextureKind::Specular){auto* depth=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:size height:size mipmapped:NO];depth.storageMode=MTLStorageModePrivate;depth.usage=MTLTextureUsageRenderTarget;r->depth=[lease.Device()newTextureWithDescriptor:depth];Require(r->depth,"Cannot allocate original cube framebuffer depth");lease.AddAllocation(r->depth);}
 return r;
}
id<MTLTexture> NativeCubeTexture(const NativeCubeTextureBacking& image){return Storage(image)->texture;}
id<MTLTexture> NativeCubeDepth(const NativeCubeTextureBacking& image){return Storage(image)->depth;}
struct PreparedCubeTexture::Impl {
 std::shared_ptr<const NativeCubeTextureBacking> image;
 std::shared_ptr<const Resource> storage;
 std::shared_ptr<PreparedCubeTexture> prior;
 __strong id<MTLSamplerState> sampler=nil;
 uint64_t submission=0;
 bool completion=false;
};
PreparedCubeTexture::PreparedCubeTexture(std::unique_ptr<Impl> p):impl(std::move(p)){}
PreparedCubeTexture::~PreparedCubeTexture()=default;
std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(RuntimeLease& lease,std::shared_ptr<const NativeCubeTextureBacking> image,const NativeTextureSampling& sampling,uint64_t submission,id<MTLSamplerState> actualSampler){
 Require(image&&submission,"Original cube prepare requires actual storage/submission identity");auto p=std::make_unique<PreparedCubeTexture::Impl>();p->image=std::move(image);p->storage=Storage(*p->image);p->submission=submission;
 if(actualSampler){Require(actualSampler.device==lease.Device(),"Original cube sampler belongs to another actual device");lease.Retain(actualSampler);}
 p->sampler=actualSampler?actualSampler:Sampler(lease,sampling);lease.AddAllocation(p->storage->texture);if(p->storage->depth)lease.AddAllocation(p->storage->depth);
 // Specular CPU bytes originate in CreateSpecularFace/Part, not a second
 // equation implementation. Only defined faces are uploaded; other storage
 // retains its original undefined allocation semantics.
 if(p->image->kind==NativeCubeTextureKind::Specular&&!p->storage->cpuUploaded){for(unsigned f=0;f<6;++f)if(const auto& pixels=p->image->specularFaces[f]){Require(pixels->size()==size_t(p->image->size)*p->image->size*4,"Original specular face byte extent differs");[p->storage->texture replaceRegion:MTLRegionMake2D(0,0,p->image->size,p->image->size) mipmapLevel:0 slice:f withBytes:pixels->data() bytesPerRow:size_t(p->image->size)*4 bytesPerImage:0];}p->storage->cpuUploaded=true;p->image->completed.store(true,std::memory_order_release);}
 if(p->image->prior&&!p->image->completed.load(std::memory_order_acquire))p->prior=PrepareNativeCubeTexture(lease,p->image->prior,sampling,submission,actualSampler);
 return std::shared_ptr<PreparedCubeTexture>(new PreparedCubeTexture(std::move(p)));
}
void PreparedCubeTexture::EncodeDependencies(id<MTL4CommandBuffer> command){
 const auto& image=*impl->image;if(image.content==NativeTextureContent::AllocationOnly||image.completed.load(std::memory_order_acquire))return;
 const auto encoded=image.dependenciesSubmission.load(std::memory_order_acquire);if(encoded){Require(encoded==impl->submission,"Original cube dependencies belong to another unretired submission");return;}
 Require(command&&impl->prior,"Original cube producer has no source storage dependency");impl->prior->EncodeDependencies(command);Readable(*impl->prior->impl->image,impl->submission);
 auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot encode original cube storage copy");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
 auto source=impl->prior->impl->storage->texture;auto target=impl->storage->texture;Require(source.width==target.width&&source.mipmapLevelCount==target.mipmapLevelCount,"Original cube retained storage extent differs");
 for(NSUInteger face=0;face<6;++face)for(NSUInteger mip=0;mip<target.mipmapLevelCount;++mip){auto size=std::max(NSUInteger(1),target.width>>mip);[encoder copyFromTexture:source sourceSlice:face sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(size,size,1) toTexture:target destinationSlice:face destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];}
 if(image.initialMipmaps||(!image.requiresFaceDraw&&image.regenerateMipmaps))[encoder generateMipmapsForTexture:target];[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment|MTLStageVertex|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];image.dependenciesSubmission.store(impl->submission,std::memory_order_release);
 if(!image.requiresFaceDraw)image.encodedSubmission.store(impl->submission,std::memory_order_release);
}
void PreparedCubeTexture::FinishReflection(id<MTL4CommandBuffer> command){
 const auto& image=*impl->image;Require(command&&image.requiresFaceDraw&&image.dependenciesSubmission.load(std::memory_order_acquire)==impl->submission&&!image.encodedSubmission.load(),"Original named cube face completion precedes its actual dependencies or is duplicated");
 auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot finish original named cube face");[encoder barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];if(image.regenerateMipmaps)[encoder generateMipmapsForTexture:impl->storage->texture];[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment|MTLStageVertex|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];image.encodedSubmission.store(impl->submission,std::memory_order_release);
}
void PreparedCubeTexture::DidComplete(){
 if(impl->completion)return;const auto& image=*impl->image;Readable(image,impl->submission);if(impl->prior)impl->prior->DidComplete();if(image.content==NativeTextureContent::ProducerVersion)image.completed.store(true,std::memory_order_release);image.prior.reset();impl->completion=true;
}
CUSSourceImage PreparedCubeTexture::Image()const {
 CUSSourceImage out;out.texture=impl->storage->texture;out.sampler=impl->sampler;out.owner=impl->image;auto image=impl->image;auto submission=impl->submission;out.requireReadable=[image,submission]{Readable(*image,submission);};return out;
}
CUSSourceImage ResolveNativeCubeTexture(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission,id<MTLSamplerState> actualSampler){
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&binding.object->target==NativeCUSTextureTarget::Cube,"Original cube binding is not an actual named cube");auto image=std::dynamic_pointer_cast<const NativeCubeTextureBacking>(binding.object->backing);Require(bool(image),"Original cube binding has another real producer");auto prepared=PrepareNativeCubeTexture(lease,std::move(image),binding.object->sampling,submission,actualSampler);auto out=prepared->Image();out.owner=prepared;out.encodeDependencies=[prepared](id<MTL4CommandBuffer> command){prepared->EncodeDependencies(command);};out.didComplete=[prepared]{prepared->DidComplete();};return out;
}
} // namespace rendering::metal4
