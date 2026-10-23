/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ShadowTextureNative.h"
#include <cmath>

namespace rendering::metal4 {
namespace {
struct Resource final:NativeShadowTextureResource {__strong id<MTLTexture> texture=nil;NativeShadowTextureRequest request;};
MTLSamplerAddressMode Address(unsigned v)
{
	switch(v){case 0x2901:return MTLSamplerAddressModeRepeat;case 0x812F:return MTLSamplerAddressModeClampToEdge;
		case 0x812D:return MTLSamplerAddressModeClampToBorderColor;default:throw std::runtime_error("Original shadow addressing has no exact native sampler");}
}
bool Readable(const NativeShadowTextureBacking& backing,uint64_t submission)
{
	return backing.completed.load()||(submission&&backing.encodedSubmission.load()==submission);
}
}
std::shared_ptr<const NativeShadowTextureResource> AllocateNativeShadowTexture(RuntimeLease& lease,const NativeShadowTextureRequest& request)
{
	Require(request.size&&request.size<=lease.Capabilities().maxTextureDimension2D,"Original shadow size exceeds actual native allocation bounds");
	MTLPixelFormat format;
	if(request.kind==NativeShadowTextureKind::Depth){
		Require(request.originalFormat==0x81A5||request.originalFormat==0x81A6||request.originalFormat==0x8CAC,
			"Original shadow depth request has no supported native storage mapping");
		// Accepted Apple donor mapping: retain the requested D24 identity while
		// allocating real D32Float. Fixed32 remains unsupported; it has a
		// finer fixed grid near one than binary32 can represent.
		format=request.originalFormat==0x81A5?MTLPixelFormatDepth16Unorm:MTLPixelFormatDepth32Float;
	}else{
		Require(request.originalFormat==(request.colored?0x8051u:0x8229u),"Original shadow RGB8/R8 format differs");
		format=request.colored?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR8Unorm;
	}
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:request.size height:request.size mipmapped:NO];
	d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget|MTLTextureUsagePixelFormatView;
	auto resource=std::make_shared<Resource>();resource->request=request;resource->texture=[lease.Device()newTextureWithDescriptor:d];
	Require(resource->texture!=nil,"Cannot allocate actual original named shadow storage");lease.AddAllocation(resource->texture);return resource;
}
id<MTLTexture> NativeShadowTextureImage(const NativeShadowTextureBacking& backing)
{
	auto resource=std::dynamic_pointer_cast<const Resource>(backing.resource);
	Require(resource&&resource->texture&&resource->request.kind==backing.request.kind&&resource->request.size==backing.request.size&&resource->request.originalFormat==backing.request.originalFormat,
		"Original shadow backing has no matching real native resource");return resource->texture;
}
NativeShadowDepthStorage QueryNativeShadowDepthStorage(const NativeShadowTextureBacking& backing)
{
	Require(backing.request.kind==NativeShadowTextureKind::Depth,"Original shadow depth query received a color owner");
	const auto texture=NativeShadowTextureImage(backing);
	Require(texture.textureType==MTLTextureType2D&&texture.width==backing.request.size&&texture.height==backing.request.size&&texture.sampleCount==1,"Original shadow depth storage differs from its actual allocation");
	NativeShadowDepthStorage result;result.requestedGLInternalFormat=backing.request.originalFormat;result.actualMetalPixelFormat=texture.pixelFormat;
	if(texture.pixelFormat==MTLPixelFormatDepth16Unorm){
		Require(backing.request.originalFormat==0x81A5,"Original D16 storage has another requested depth format");
		result.actualComponentType=NativeShadowDepthComponent::UNorm;result.actualComponentStorageBits=16;result.exactRequestedFixedPointEncoding=true;
	}else{
		Require(texture.pixelFormat==MTLPixelFormatDepth32Float&&(backing.request.originalFormat==0x81A6||backing.request.originalFormat==0x8CAC),"Original shadow request/storage tuple is unsupported");
		result.actualComponentType=NativeShadowDepthComponent::Float;result.actualComponentStorageBits=32;
	}
	return result;
}
bool CheckNativeShadowFramebuffer(const NativeShadowTextureBacking& depth,const NativeShadowTextureBacking& color)
{
	auto d=NativeShadowTextureImage(depth),c=NativeShadowTextureImage(color);
	return depth.request.kind==NativeShadowTextureKind::Depth&&color.request.kind==NativeShadowTextureKind::Color&&d.width==c.width&&d.height==c.height&&d.sampleCount==1&&c.sampleCount==1&&
		(d.pixelFormat==MTLPixelFormatDepth16Unorm||d.pixelFormat==MTLPixelFormatDepth32Float)&&
		c.pixelFormat==(color.request.colored?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR8Unorm);
}
void MarkNativeShadowTextureEncoded(const NativeShadowTextureBacking& backing,uint64_t submission)
{
	Require(submission&&!backing.completed.load(),"Original shadow completion state or submission identity differs");
	uint64_t expected=0;
	Require(backing.encodedSubmission.compare_exchange_strong(expected,submission)||expected==submission,"Original shadow version was encoded by another submission");
}
void CompleteNativeShadowTexture(const NativeShadowTextureBacking& backing)
{
	Require(backing.encodedSubmission.load()!=0,"Cannot complete original shadow content before its real producer executed");backing.completed=true;
}
void EncodeNativeShadowInitialClear(const std::shared_ptr<const NativeShadowTextureBacking>& backing,
	id<MTL4CommandBuffer> command,uint64_t submission)
{
	Require(backing&&command&&submission,"Original shadow initialization has no actual shared command");
	if(Readable(*backing,submission))return;
	Require(backing->operation==NativeShadowTextureOperation::InitialClear,"Original shadow geometry must execute at its source pass, not as a consumer clear");
	// The caller registered this actual allocation during Prepare, before
	// the common lease committed residency. Encoding cannot add resources.
	auto texture=NativeShadowTextureImage(*backing);
	auto p=[MTL4RenderPassDescriptor new];p.renderTargetWidth=p.renderTargetHeight=backing->request.size;
	if(backing->request.kind==NativeShadowTextureKind::Depth){p.depthAttachment.texture=texture;p.depthAttachment.loadAction=MTLLoadActionClear;p.depthAttachment.storeAction=MTLStoreActionStore;p.depthAttachment.clearDepth=1;}
	else {p.colorAttachments[0].texture=texture;p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].storeAction=MTLStoreActionStore;p.colorAttachments[0].clearColor=MTLClearColorMake(1,1,1,0);}
	auto e=[command renderCommandEncoderWithDescriptor:p];Require(e!=nil,"Cannot encode original shadow initial clear");
	[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];MarkNativeShadowTextureEncoded(*backing,submission);
}
CUSSourceImage ResolveNativeShadowTexture(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission,id<MTLSamplerState> actualSampler)
{
	Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Original shadow lookup did not bind a real object");
	auto backing=std::dynamic_pointer_cast<const NativeShadowTextureBacking>(binding.object->backing);Require(bool(backing),"Binding is not the original shadow resource");
	const auto& p=binding.object->sampling;const bool depth=backing->request.kind==NativeShadowTextureKind::Depth;
	id<MTLSamplerState> sampler=actualSampler;
	if(actualSampler)Require(actualSampler.device==lease.Device(),"Original shadow sampler belongs to another actual device");
	else{
	Require(p.baseLevel==0&&p.maxLevel>=0&&p.minLod<=0&&p.maxLod>=0&&p.lodBias==0&&p.anisotropy==1&&
		(p.minFilter==0x2600||p.minFilter==0x2601)&&(p.magFilter==0x2600||p.magFilter==0x2601)&&
		(p.compareMode==0||(depth&&p.compareMode==0x884E&&p.compareFunction==0x0203)),"Original shadow sampling requires an unimplemented exact state");
	Require(p.borderColor==std::array<float,4>{1,1,1,1}||p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{0,0,0,1},"Original shadow border color is not representable exactly");
	auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;d.minFilter=p.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
	d.magFilter=p.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;d.mipFilter=MTLSamplerMipFilterNotMipmapped;
	d.sAddressMode=Address(p.wrapS);d.tAddressMode=Address(p.wrapT);d.rAddressMode=Address(p.wrapR);
	d.borderColor=p.borderColor[3]==0?MTLSamplerBorderColorTransparentBlack:p.borderColor[0]==1?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorOpaqueBlack;
	d.compareFunction=p.compareMode?MTLCompareFunctionLessEqual:MTLCompareFunctionNever;
	sampler=[lease.Device()newSamplerStateWithDescriptor:d];Require(sampler!=nil,"Cannot create original shadow sampler");
	}
	auto texture=NativeShadowTextureImage(*backing);lease.AddAllocation(texture);lease.Retain(sampler);
	CUSSourceImage out;out.texture=texture;out.sampler=sampler;out.owner=backing;out.flipV=true;out.depthImage=depth;
	if(depth){const auto storage=QueryNativeShadowDepthStorage(*backing);out.clampDepthCompare=storage.requestedGLInternalFormat==0x81A5||storage.requestedGLInternalFormat==0x81A6;}
	if(backing->operation==NativeShadowTextureOperation::InitialClear)
		out.encodeDependencies=[backing,submission](id<MTL4CommandBuffer> command){EncodeNativeShadowInitialClear(backing,command,submission);};
	out.requireReadable=[backing,submission]{Require(Readable(*backing,submission),"Original shadow content has not been encoded in this actual submit or retired earlier");};
	out.didComplete=[backing]{if(backing->encodedSubmission.load())CompleteNativeShadowTexture(*backing);};return out;
}
} // namespace rendering::metal4
