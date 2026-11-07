/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CubeTextureSource.h"
#include "Metal4CubeCopy206.h"
#include "Rendering/Backend/NativeInitialClearLoad.h"
#include <algorithm>
#include "Metal4CubeStoragePool.inc"
namespace rendering::metal4 {
namespace {
struct Resource final:NativeCubeTextureResource {
 std::shared_ptr<CubeStorage> storage;
 mutable bool cpuUploaded=false;
 // A per-version proof stamp; previous versions and pool ownership are unchanged.
 mutable std::atomic<uint64_t> overwriteSubmission206{0};
};
std::shared_ptr<const Resource> Storage(const NativeCubeTextureBacking& image){auto r=std::dynamic_pointer_cast<const Resource>(image.resource);Require(r&&r->storage&&r->storage->texture,"Original cube lacks its actual native allocation");return r;}
// An ordered predecessor is this immutable version's exact successful producer,
// not permission to consume any encoded cube. The runtime verifies its actual
// retirement after N's event, feedback and receipt, before submitting N+1.
void Readable(const NativeCubeTextureBacking& image,uint64_t submission,uint64_t predecessor=0){
 if(predecessor){
  Require(predecessor!=submission&&image.content==NativeTextureContent::ProducerVersion&&
   image.dependenciesSubmission.load(std::memory_order_acquire)==predecessor&&
   image.encodedSubmission.load(std::memory_order_acquire)==predecessor,
   "Ordered cube input lost its exact successfully encoded predecessor");
  return;
 }
 if(image.content==NativeTextureContent::AllocationOnly)return;
 Require(image.completed.load(std::memory_order_acquire)||(submission&&image.encodedSubmission.load(std::memory_order_acquire)==submission),"Original cube content has not executed before this source consumer");
}
void RequireCubePredecessorComplete158(const NativeCubeTextureBacking& image,uint64_t submission,uint64_t predecessor){
 Require(predecessor!=0,"Missing ordered cube predecessor identity");
 Readable(image,submission,predecessor);
 Require(image.completed.load(std::memory_order_acquire),"Ordered cube predecessor did not actually complete before successor submission/retirement");
}
uint64_t OrderCubePredecessor158(RuntimeLease& lease,const std::shared_ptr<const NativeCubeTextureBacking>& image,uint64_t submission){
 Require(image&&submission,"Cube predecessor ordering lacks its actual version/submission");
 if(image->content==NativeTextureContent::AllocationOnly||image->completed.load(std::memory_order_acquire))return 0;
 const auto dependencies=image->dependenciesSubmission.load(std::memory_order_acquire);
 const auto encoded=image->encodedSubmission.load(std::memory_order_acquire);
 if((dependencies&&dependencies!=submission)||(encoded&&encoded!=submission)){
  // A face whose copy began but whose real face/mip producer never finished
  // cannot be borrowed or replayed by a later submission.
  Require(image->content==NativeTextureContent::ProducerVersion&&dependencies&&dependencies==encoded&&
   image->resource&&image->storageGeneration&&image->contentVersion,
   "Cube predecessor has no complete encoded version/storage identity");
  if(lease.OrderAfterPreviousSubmission(image.get(),[image,submission,encoded]{
   RequireCubePredecessorComplete158(*image,submission,encoded);
  }))return encoded;
  // With recording overlap disabled, StageResources still retires N before
  // Begin. Keep the original strict read checks instead of an early CPU wait.
 }
 return 0;
}
bool CubeDependenciesAlreadyEncoded158(const NativeCubeTextureBacking& image,uint64_t submission,uint64_t predecessor){
 if(predecessor){Readable(image,submission,predecessor);return true;}
 if(image.content==NativeTextureContent::AllocationOnly||image.completed.load(std::memory_order_acquire))return true;
 const auto encoded=image.dependenciesSubmission.load(std::memory_order_acquire);
 if(encoded){Require(encoded==submission,"Original cube dependencies belong to another unretired submission");return true;}
 return false;
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
 r->storage=AcquireCubeStorage(lease,kind,size,levels);
 lease.AddAllocation(r->storage->texture);if(r->storage->depth)lease.AddAllocation(r->storage->depth);
 return r;
}
std::shared_ptr<const NativeCubeTextureResource> ReserveNativeCubeTextureForCapture(NativeCubeTextureKind kind,uint32_t size,uint32_t levels)
{
 Require([NSThread isMainThread],"Original cube capture reservation requires the engine main thread");
 const auto& runtime=SharedRuntime();const auto capabilities=runtime.Capabilities();
 Require(capabilities.directMetal4 && capabilities.unifiedMemory && runtime.Device()!=nil,
  "Original cube capture has no supported actual native device");
 auto resource=std::make_shared<Resource>();
 resource->storage=ReserveCubeStorage(runtime,kind,size,levels);
 // cpuUploaded remains false. Original specular bytes are uploaded only by
 // PrepareNativeCubeTexture under the retirement-gated RuntimeLease.
 return resource;
}
id<MTLTexture> NativeCubeTexture(const NativeCubeTextureBacking& image){return Storage(image)->storage->texture;}
id<MTLTexture> NativeCubeDepth(const NativeCubeTextureBacking& image){return Storage(image)->storage->depth;}
void AuthorizeCubeFaceOverwrite206(const NativeCubeTextureBacking& image,
 MTL4RenderPassDescriptor* pass,const NativeWorldAttachmentClear& clear,
 uint64_t sourceFrame,uint64_t submission)
{
 // Fail closed. Preparation has already validated the actual original clear.
 // Generic cube consumers never authorize a discard, and an already encoded
 // dependency keeps its original copy plan.
 if(!submission||!sourceFrame||image.sourceDrawFrame!=sourceFrame||
  image.kind!=NativeCubeTextureKind::Reflection||image.content!=NativeTextureContent::ProducerVersion||
  !image.requiresFaceDraw||image.initialMipmaps||image.sourceFace>=6||!image.prior||
  image.completed.load(std::memory_order_acquire)||image.dependenciesSubmission.load(std::memory_order_acquire)||
  image.encodedSubmission.load(std::memory_order_acquire)||!pass||
  clear.stage!=NativeWorldDrawStage::TerrainReflection||!clear.sequence||
  !(NativeInitialClearDiscardMask(clear,image.size,image.size)&ScreenAttachmentColor))return;
 auto resource=Storage(image);auto attachment=pass.colorAttachments[0];auto texture=resource->storage->texture;
 if(!texture||texture!=attachment.texture||texture==Storage(*image.prior)->storage->texture||
  texture.textureType!=MTLTextureTypeCube||texture.pixelFormat!=MTLPixelFormatRGBA8Unorm||
  texture.width!=image.size||texture.height!=image.size||texture.mipmapLevelCount!=image.levels||
  texture.sampleCount!=1||
  pass.renderTargetWidth!=image.size||pass.renderTargetHeight!=image.size||
  attachment.slice!=image.sourceFace||attachment.level||attachment.depthPlane||attachment.resolveTexture||
  attachment.loadAction!=MTLLoadActionLoad||attachment.storeAction!=MTLStoreActionStore)return;
 resource->overwriteSubmission206.store(submission,std::memory_order_release);
}
struct PreparedCubeTexture::Impl {
 std::shared_ptr<const NativeCubeTextureBacking> image;
 std::shared_ptr<const Resource> storage;
 std::shared_ptr<PreparedCubeTexture> prior;
 __strong id<MTLSamplerState> sampler=nil;
 uint64_t submission=0,predecessor=0;
 bool completion=false;
};
PreparedCubeTexture::PreparedCubeTexture(std::unique_ptr<Impl> p):impl(std::move(p)){}
PreparedCubeTexture::~PreparedCubeTexture()=default;
std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(RuntimeLease& lease,std::shared_ptr<const NativeCubeTextureBacking> image,const NativeTextureSampling& sampling,uint64_t submission,id<MTLSamplerState> actualSampler){
 Require(image&&submission,"Original cube prepare requires actual storage/submission identity");auto p=std::make_unique<PreparedCubeTexture::Impl>();p->image=std::move(image);p->storage=Storage(*p->image);p->submission=submission;
 RetainCubeStorage(lease,p->storage->storage);
 if(actualSampler){Require(actualSampler.device==lease.Device(),"Original cube sampler belongs to another actual device");lease.Retain(actualSampler);}
 p->sampler=actualSampler?actualSampler:Sampler(lease,sampling);lease.AddAllocation(p->storage->storage->texture);if(p->storage->storage->depth)lease.AddAllocation(p->storage->storage->depth);
 // Specular CPU bytes originate in CreateSpecularFace/Part, not a second
 // equation implementation. Only defined faces are uploaded; other storage
 // retains its original undefined allocation semantics.
 if(p->image->kind==NativeCubeTextureKind::Specular&&!p->storage->cpuUploaded){for(unsigned f=0;f<6;++f)if(const auto& pixels=p->image->specularFaces[f]){Require(pixels->size()==size_t(p->image->size)*p->image->size*4,"Original specular face byte extent differs");[p->storage->storage->texture replaceRegion:MTLRegionMake2D(0,0,p->image->size,p->image->size) mipmapLevel:0 slice:f withBytes:pixels->data() bytesPerRow:size_t(p->image->size)*4 bytesPerImage:0];}p->storage->cpuUploaded=true;p->image->completed.store(true,std::memory_order_release);}
 p->predecessor=OrderCubePredecessor158(lease,p->image,submission);
 // N owns its full dependency/completion graph. A borrowed N version only
 // retains the actual image/storage; it must never republish N's completion.
 if(!p->predecessor&&p->image->prior&&!p->image->completed.load(std::memory_order_acquire))
  p->prior=PrepareNativeCubeTexture(lease,p->image->prior,sampling,submission,actualSampler);
 return std::shared_ptr<PreparedCubeTexture>(new PreparedCubeTexture(std::move(p)));
}
SourceDependencyResult160 PreparedCubeTexture::InspectDependencies160()const{
 return {CubeDependenciesAlreadyEncoded158(*impl->image,impl->submission,impl->predecessor)?
  SourceDependencyWork160::NoCommands:SourceDependencyWork160::CommandsRequired,true};
}
void PreparedCubeTexture::EncodeDependencies(id<MTL4CommandBuffer> command){
 const auto& image=*impl->image;
 if(CubeDependenciesAlreadyEncoded158(image,impl->submission,impl->predecessor))return;
 Require(command&&impl->prior,"Original cube producer has no source storage dependency");
 impl->prior->EncodeDependencies(command);
 Readable(*impl->prior->impl->image,impl->submission,impl->prior->impl->predecessor);
 auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot encode original cube storage copy");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
 auto source=impl->prior->impl->storage->storage->texture;auto target=impl->storage->storage->texture;Require(source.width==target.width&&source.mipmapLevelCount==target.mipmapLevelCount,"Original cube retained storage extent differs");
 // The new face is cleared before any legal read of this producer version.
 // On regeneration frames FinishReflection overwrites every lower mip before
 // encodedSubmission is published. Other faces/base levels keep their copies.
 const bool overwrite206=impl->storage->overwriteSubmission206.load(std::memory_order_acquire)==impl->submission;
 for(NSUInteger face=0;face<6;++face)for(NSUInteger mip=0;mip<target.mipmapLevelCount;++mip){if(!PreserveCubeSubresource206(unsigned(face),unsigned(mip),image.sourceFace,overwrite206,image.regenerateMipmaps))continue;auto size=std::max(NSUInteger(1),target.width>>mip);[encoder copyFromTexture:source sourceSlice:face sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(size,size,1) toTexture:target destinationSlice:face destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];}
 if(image.initialMipmaps||(!image.requiresFaceDraw&&image.regenerateMipmaps))[encoder generateMipmapsForTexture:target];[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment|MTLStageVertex|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];image.dependenciesSubmission.store(impl->submission,std::memory_order_release);
 if(!image.requiresFaceDraw)image.encodedSubmission.store(impl->submission,std::memory_order_release);
}
void PreparedCubeTexture::FinishReflection(id<MTL4CommandBuffer> command){
 const auto& image=*impl->image;Require(command&&!impl->predecessor&&image.requiresFaceDraw&&image.dependenciesSubmission.load(std::memory_order_acquire)==impl->submission&&!image.encodedSubmission.load(),"Original named cube face completion precedes its actual dependencies or is duplicated");
 auto encoder=[command computeCommandEncoder];Require(encoder,"Cannot finish original named cube face");[encoder barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];if(image.regenerateMipmaps)[encoder generateMipmapsForTexture:impl->storage->storage->texture];[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment|MTLStageVertex|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];image.encodedSubmission.store(impl->submission,std::memory_order_release);
}
void PreparedCubeTexture::DidComplete(){
 if(impl->completion)return;
 const auto& image=*impl->image;
 if(impl->predecessor){
  RequireCubePredecessorComplete158(image,impl->submission,impl->predecessor);
  impl->completion=true;return; // Local receipt only; N owns image completion/prior release.
 }
 Readable(image,impl->submission);
 if(impl->prior)impl->prior->DidComplete();
 if(image.content==NativeTextureContent::ProducerVersion)image.completed.store(true,std::memory_order_release);
 image.prior.reset();impl->completion=true;
}
CUSSourceImage PreparedCubeTexture::Image()const {
 CUSSourceImage out;out.texture=impl->storage->storage->texture;out.sampler=impl->sampler;out.owner=impl->image;auto image=impl->image;auto submission=impl->submission,predecessor=impl->predecessor;out.requireReadable=[image,submission,predecessor]{Readable(*image,submission,predecessor);};return out;
}
CUSSourceImage ResolveNativeCubeTexture(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission,id<MTLSamplerState> actualSampler){
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&binding.object->target==NativeCUSTextureTarget::Cube,"Original cube binding is not an actual named cube");auto image=std::dynamic_pointer_cast<const NativeCubeTextureBacking>(binding.object->backing);Require(bool(image),"Original cube binding has another real producer");auto prepared=PrepareNativeCubeTexture(lease,std::move(image),binding.object->sampling,submission,actualSampler);auto out=prepared->Image();out.owner=prepared;out.encodeDependencies=[prepared](id<MTL4CommandBuffer> command){prepared->EncodeDependencies(command);};out.didComplete=[prepared]{prepared->DidComplete();};
 out.dependencyQuery160={prepared.get(),[](const void* context){return static_cast<const PreparedCubeTexture*>(context)->InspectDependencies160();}};return out;
}
} // namespace rendering::metal4
