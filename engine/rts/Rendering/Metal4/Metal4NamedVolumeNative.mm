/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4NamedVolumeNative.h"
#include <algorithm>

namespace rendering::metal4 {
namespace {
struct Resource final:NativeNamedVolumeResource {
	__strong id<MTLTexture> texture=nil;
	uint64_t encodedSubmission=0;
	bool completed=false;
};
id<MTLSamplerState> Sampler(RuntimeLease& lease,const NativeTextureSampling& p)
{
	Require(!p.compareMode&&p.baseLevel==0&&p.maxLevel>=0&&p.lodBias==0&&p.anisotropy==1&&
		p.wrapS==0x2901&&p.wrapT==0x2901&&p.wrapR==0x2901,"Original selected DDS3D sampler requires another source consumer");
	auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;
	switch(p.minFilter){
		case 0x2600:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterNotMipmapped;break;
		case 0x2601:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNotMipmapped;break;
		case 0x2700:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterNearest;break;
		case 0x2701:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNearest;break;
		case 0x2702:d.minFilter=MTLSamplerMinMagFilterNearest;d.mipFilter=MTLSamplerMipFilterLinear;break;
		case 0x2703:d.minFilter=MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterLinear;break;
		default:throw std::runtime_error("Original DDS3D filter is invalid");
	}
	Require(p.magFilter==0x2600||p.magFilter==0x2601,"Original DDS3D magnification filter is invalid");
	d.magFilter=p.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
	d.sAddressMode=d.tAddressMode=d.rAddressMode=MTLSamplerAddressModeRepeat;
	d.lodMinClamp=std::max(0.f,p.minLod);d.lodMaxClamp=std::min(float(p.maxLevel),p.maxLod);
	auto result=[lease.Device()newSamplerStateWithDescriptor:d];Require(result,"Cannot allocate original DDS3D sampler");lease.Retain(result);return result;
}
}
CUSSourceImage ResolveNativeNamedVolume(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission)
{
	Require(submission&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&binding.object->target==NativeCUSTextureTarget::Image3D,"Original volume resolver lacks its actual bound object/submission");
	auto backing=std::dynamic_pointer_cast<const NativeNamedVolumeBacking>(binding.object->backing);
	Require(backing&&backing->pixels,"Original named volume has another source producer");
	const auto& a=binding.object->allocation;const auto& pixels=*backing->pixels;
	Require(a.width==64&&a.height==64&&a.depth==64&&(a.levels==1||a.levels==7)&&a.samples==1&&a.originalInternalFormat==4,"Original named volume allocation differs");
	auto resource=std::dynamic_pointer_cast<Resource>(backing->resource);
	if(!resource){
		Require(!backing->resource,"Original named volume resource owner differs");
		resource=std::make_shared<Resource>();auto descriptor=[MTLTextureDescriptor new];descriptor.textureType=MTLTextureType3D;
		descriptor.width=descriptor.height=descriptor.depth=64;descriptor.mipmapLevelCount=a.levels;descriptor.pixelFormat=MTLPixelFormatBGRA8Unorm;
		descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageShaderRead;
		resource->texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(resource->texture,"Cannot allocate original named DDS3D storage");
		[resource->texture replaceRegion:MTLRegionMake3D(0,0,0,64,64,64) mipmapLevel:0 slice:0 withBytes:pixels.bgraBytes.data() bytesPerRow:pixels.bytesPerRow bytesPerImage:pixels.bytesPerImage];
		resource->completed=a.levels==1;backing->resource=resource;
	}
	lease.AddAllocation(resource->texture);CUSSourceImage out;out.texture=resource->texture;out.sampler=Sampler(lease,binding.object->sampling);out.owner=backing;
	// Encoding occurs only after the common Begin. Keep the actual encoded
	// owner on every exception; RuntimeLease poisons reuse and retains in-flight
	// allocations. No destructor may roll this source mark back for retry.
	out.encodeDependencies=[resource,submission](id<MTL4CommandBuffer> command){
		if(resource->completed||resource->encodedSubmission==submission)return;
		Require(command&&!resource->encodedSubmission,"Original DDS3D mip producer is still owned by another submission");
		auto copy=[command computeCommandEncoder];Require(copy,"Cannot encode original named DDS3D mip generation");
		[copy generateMipmapsForTexture:resource->texture];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];resource->encodedSubmission=submission;
	};
	out.requireReadable=[resource,submission]{Require(resource->completed||resource->encodedSubmission==submission,"Original named DDS3D sampled before its upload/mip dependency");};
	out.didComplete=[resource,submission]{Require(resource->completed||resource->encodedSubmission==submission,"Original named DDS3D retired before its actual dependency");resource->completed=true;};
	return out;
}
} // namespace rendering::metal4
