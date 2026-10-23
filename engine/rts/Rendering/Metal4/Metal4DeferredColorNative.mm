/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4DeferredColorNative.h"
namespace rendering::metal4 {
namespace {
struct Resource final:NativeDeferredColorResource {DeferredColorImage image;__strong id<MTLTexture> retained=nil;};
MTLSamplerAddressMode Address(unsigned v){switch(v){case 0x2901:return MTLSamplerAddressModeRepeat;case 0x812F:return MTLSamplerAddressModeClampToEdge;case 0x812D:return MTLSamplerAddressModeClampToBorderColor;default:throw std::runtime_error("Original deferred image addressing requires its exact sampler consumer");}}
}
void FulfillDeferredColorBacking(const NativeDeferredColorBacking& backing,const DeferredColorImage& image)
{
	const auto& m=backing.Metadata();Require(image.texture&&image.storageOwner&&image.producer&&image.texture.pixelFormat==MTLPixelFormatRGBA8Unorm&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1,"Original deferred color has no actual RGBA8 single-sample backing");
	Require(image.texture.width==m.view.width&&image.texture.height==m.view.height&&image.metadata.role==m.role&&image.metadata.attachment==m.attachment&&image.metadata.view.view==m.view.view&&image.metadata.view.projection==m.view.projection&&image.metadata.view.viewport==m.view.viewport,"Original deferred color camera/extent differs");
	Require(image.producer->sourceDrawFrame==backing.sourceDrawFrame,"Original deferred color producer frame differs");auto resource=std::make_shared<Resource>();resource->image=image;resource->retained=image.texture;backing.Fulfill(std::move(resource));
}
const DeferredColorImage& GetDeferredColorBackingImage(const NativeDeferredColorBacking& backing){auto resource=std::dynamic_pointer_cast<const Resource>(backing.Resource());Require(bool(resource),"Original deferred color resource is not fulfilled");return resource->image;}
bool IsDeferredColorReadable(const DeferredColorImage& image,uint64_t submission)noexcept{return image.producer&&IsNativeShieldDepthReadable(image.producer->encoded,image.producer->completed,image.producer->operation==ShieldDepthOperation::AllocationOnly,image.producer->allocationVerified,image.producer->submissionIdentity,submission);}
CUSSourceImage ResolveDeferredColorImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission)
{
	Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Original deferred binding is unobserved/default; use its actual source empty-sampler branch");auto backing=std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing);Require(bool(backing),"Binding is not an original deferred color resource");const auto image=GetDeferredColorBackingImage(*backing);const auto& p=binding.object->sampling;
	Require(p.compareMode==0&&p.baseLevel==0&&p.maxLevel>=0&&p.minLod<=0&&p.maxLod>=0&&p.lodBias==0&&p.anisotropy==1&&(p.minFilter==0x2600||p.minFilter==0x2601)&&(p.magFilter==0x2600||p.magFilter==0x2601),"Original deferred sampler/LOD requires its observed general sampler implementation");
	Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{0,0,0,1}||p.borderColor==std::array<float,4>{1,1,1,1},"Original deferred border color is not representable by this sampler");auto* d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;d.minFilter=p.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.magFilter=p.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNotMipmapped;d.sAddressMode=Address(p.wrapS);d.tAddressMode=Address(p.wrapT);d.rAddressMode=Address(p.wrapR);d.borderColor=p.borderColor[3]==0?MTLSamplerBorderColorTransparentBlack:p.borderColor[0]==1?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorOpaqueBlack;
	id<MTLSamplerState> sampler=[lease.Device()newSamplerStateWithDescriptor:d];Require(sampler,"Cannot allocate original deferred sampler");lease.AddAllocation(image.texture);lease.Retain(sampler);CUSSourceImage out;out.texture=image.texture;out.sampler=sampler;out.owner=backing;out.flipV=true;out.requireReadable=[image,submission]{Require(IsDeferredColorReadable(image,submission),"Original deferred color source has not executed in this actual submission or retired earlier");};return out;
}
}
