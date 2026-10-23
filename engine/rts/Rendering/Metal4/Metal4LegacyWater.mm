/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4LegacyWater.h"
#include "NativeLegacyWaterShaders.h"
#include <cstring>
#include <map>
#include <tuple>
namespace rendering::metal4 {
namespace {
struct alignas(16) Uniforms {
 WorldMatrix viewProjection,view;
 std::array<float,4> forward,fogColor,fogParameters;
 std::array<uint32_t,4> flags;
 std::array<float,4> refractExtent,mapExtent;
 std::array<uint32_t,4> refractFlags;
};
static_assert(sizeof(NativeLegacyWaterVertex)==24);
static_assert(sizeof(Uniforms)==240);
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size){
 auto result=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
 Require(result!=nil,"Cannot allocate original Water1 buffer");lease.AddAllocation(result);return result;
}
}
struct LegacyRefractionImage final:NativeLegacyRefractionResource {
 __strong id<MTLTexture> texture=nil;
 uint64_t encodedSubmission=0;bool completed=false;
 bool Completed()const noexcept override{return completed;}
};
// Resolve the immutable source version during preparation, regardless of
// whether an inherited reader or the original copy producer prepares first.
std::shared_ptr<LegacyRefractionImage> EnsureLegacyRefractionImage(RuntimeLease& lease,const std::shared_ptr<const NativeCUSTextureObject>& object){
 Require(object&&(object->target==NativeCUSTextureTarget::Rectangle||object->target==NativeCUSTextureTarget::Image2D),"Refractive image has no original target-aware texture object");
 auto backing=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(object->backing);
 auto expectedSampling=InitialNativeTextureSampling(object->target);expectedSampling.minFilter=expectedSampling.magFilter=0x2600;
 Require(object->sampling==expectedSampling,"Refractive sampler state differs from the exact LoadGfx writes");
 const auto& allocation=object->allocation;
 Require(backing&&backing->storageGeneration==object->identity&&backing->content==NativeTextureContent::ProducerVersion&&object->storage==NativeTextureStorageKnowledge::Published&&
  allocation.width&&allocation.height&&allocation.depth==1&&allocation.levels==1&&allocation.samples==1&&allocation.originalInternalFormat==3,"Refractive image has no exact RGB allocation/content publication");
 std::shared_ptr<LegacyRefractionImage> resource;
 if(backing->Resource())resource=std::const_pointer_cast<LegacyRefractionImage>(std::dynamic_pointer_cast<const LegacyRefractionImage>(backing->Resource()));
 else{
  resource=std::make_shared<LegacyRefractionImage>();auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:allocation.width height:allocation.height mipmapped:NO];
  d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  resource->texture=[lease.Device() newTextureWithDescriptor:d];Require(resource->texture!=nil,"Cannot allocate original refractive RGB copy");backing->Fulfill(resource);
 }
 Require(resource&&resource->texture&&resource->texture.width==allocation.width&&resource->texture.height==allocation.height,"Refractive image has a foreign or mismatched native resource");
 lease.AddAllocation(resource->texture);return resource;
}
struct PreparedLegacyWater::Impl {
 std::shared_ptr<const NativeLegacyWaterFrame> frame;
 __strong id<MTL4ArgumentTable> arguments=nil;
 __strong id<MTLBuffer> indices=nil;
 __strong id<MTLRenderPipelineState> pipeline=nil;
 __strong id<MTLDepthStencilState> depth=nil;
 bool encoded=false;
 CUSSourceImage shading;
 std::shared_ptr<LegacyRefractionImage> copiedColor;uint64_t submission=0;
 __strong id<MTLComputePipelineState> copyPipeline=nil;
 __strong id<MTL4ArgumentTable> copyArguments=nil;
};
PreparedLegacyWater::PreparedLegacyWater(std::unique_ptr<Impl> p):impl(std::move(p)){}
PreparedLegacyWater::~PreparedLegacyWater()=default;
void PreparedLegacyWater::EncodeDependencies(id<MTL4CommandBuffer> command){if(impl->shading.encodeDependencies)impl->shading.encodeDependencies(command);}
void PreparedLegacyWater::DidComplete(){
 if(impl->copiedColor){Require(impl->copiedColor->completed||impl->copiedColor->encodedSubmission==impl->submission,"Refractive copy completion has no exact submission");impl->copiedColor->completed=true;}
 if(impl->shading.didComplete)impl->shading.didComplete();
}
void PreparedLegacyWater::EncodeCopy(id<MTL4CommandBuffer> command,id<MTLTexture> source,id<MTLBuffer> origin){
 auto& image=impl->copiedColor;Require(image&&source&&origin,"Refractive copy has no real source/destination/origin");if(image->completed)return;
 Require(!image->encodedSubmission,"Refractive source copy encoded twice");
 auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot encode original refractive copy");
 [copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
 [impl->copyArguments setAddress:origin.gpuAddress atIndex:0];[impl->copyArguments setTexture:source.gpuResourceID atIndex:0];[impl->copyArguments setTexture:image->texture.gpuResourceID atIndex:1];
 [copy setComputePipelineState:impl->copyPipeline];[copy setArgumentTable:impl->copyArguments];
 [copy dispatchThreads:MTLSizeMake(image->texture.width,image->texture.height,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
 [copy barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];image->encodedSubmission=impl->submission;
}
void PreparedLegacyWater::Encode(id<MTL4RenderCommandEncoder> e){
 if(impl->copiedColor)Require(impl->copiedColor->completed||impl->copiedColor->encodedSubmission==impl->submission,"Refractive surface precedes its source copy");
 if(impl->shading.requireReadable)impl->shading.requireReadable();
 Require(!impl->encoded,"Original Water1 encoded twice");impl->encoded=true;
 if(impl->frame->indices.empty()||impl->frame->cull==LegacyWaterBumpCull::FrontAndBack)return;
 [e setRenderPipelineState:impl->pipeline];[e setDepthStencilState:impl->depth];
 [e setFrontFacingWinding:MTLWindingCounterClockwise];
 [e setCullMode:impl->frame->cull==LegacyWaterBumpCull::Front?MTLCullModeFront:impl->frame->cull==LegacyWaterBumpCull::Back?MTLCullModeBack:MTLCullModeNone];[e setDepthClipMode:MTLDepthClipModeClip];[e setDepthBias:0 slopeScale:0 clamp:0];
 [e setTriangleFillMode:impl->frame->wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
 [e setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:impl->frame->indices.size() indexType:MTLIndexTypeUInt32 indexBuffer:impl->indices.gpuAddress indexBufferLength:impl->indices.length];
}
struct LegacyWaterGPU::Impl {
 using Key=std::tuple<MTLPixelFormat,unsigned,bool,bool>;
 std::map<Key,id<MTLRenderPipelineState>> pipelines;
 __strong id<MTLDepthStencilState> depth=nil;
 std::map<bool,id<MTLComputePipelineState>> copyPipelines;
};
LegacyWaterGPU::LegacyWaterGPU():impl(std::make_unique<Impl>()){}
LegacyWaterGPU::~LegacyWaterGPU()=default;
std::unique_ptr<PreparedLegacyWater> LegacyWaterGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeLegacyWaterFrame> frame,
 id<MTLTexture> reflection,id<MTLTexture> bump,bool bumpFlipV,MTLPixelFormat color,unsigned samples,bool validation,double& compileMs,CUSSourceImage shading,uint32_t renderHeight,uint64_t submission){
 Require(frame&&frame->bump&&reflection&&bump,"Water1 missing actual same-frame producers");
 Require(reflection.width==512&&reflection.height==512&&bump.width==128&&bump.height==128,"Water1 source resource extents differ");
 auto p=std::make_unique<PreparedLegacyWater::Impl>();p->frame=frame;p->shading=std::move(shading);
 const auto refract=frame->refraction;
 id<MTLTexture> subsurface=nil;
 if(refract){
  Require(submission&&refract->copiedColor,"Refractive copy lacks exact source publication/submission");
  auto backing=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(refract->copiedColor->backing);Require(backing&&backing->sourceDrawFrame==frame->sourceDrawFrame,"Refractive copy source owner differs");
  Require(refract->copiedColor->target==(refract->target==LegacyRefractionTarget::Rectangle?NativeCUSTextureTarget::Rectangle:NativeCUSTextureTarget::Image2D),"Refractive captured target differs from its source publication");
  p->copiedColor=EnsureLegacyRefractionImage(lease,refract->copiedColor);
  Require(p->copiedColor&&(!p->copiedColor->encodedSubmission||p->copiedColor->completed),"Refractive unfinished copy cannot be reused");
  p->submission=submission;subsurface=p->copiedColor->texture;lease.AddAllocation(subsurface);
  if(!p->copiedColor->completed){
   auto foundCopy=impl->copyPipelines.find(validation);
   if(foundCopy==impl->copyPipelines.end()){
    const auto start=Clock::now();NSError* err=nil;auto options=[MTLCompileOptions new];
    auto lib=lease.NewLibraryWithSource(@"#include <metal_stdlib>\nusing namespace metal;\nkernel void LegacyRGBSourceCopy(texture2d<float,access::read> src [[texture(0)]],texture2d<float,access::write> dst [[texture(1)]],constant uint2& origin [[buffer(0)]],uint2 p [[thread_position_in_grid]]){if(any(p>=uint2(dst.get_width(),dst.get_height())))return;dst.write(float4(src.read(p+origin).rgb,1.0f),p);}",options,&err);Require(lib!=nil,"Refractive RGB source copy compile failed: "+Error(err));
    auto d=[MTL4ComputePipelineDescriptor new];auto f=[MTL4LibraryFunctionDescriptor new];f.library=lib;f.name=@"LegacyRGBSourceCopy";d.computeFunctionDescriptor=f;auto po=[MTL4PipelineOptions new];po.shaderValidation=ShaderValidationMode(validation);d.options=po;
    auto pipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:d compilerTaskOptions:nil error:&err];Require(pipeline!=nil,"Refractive RGB copy pipeline failed: "+Error(err));compileMs+=Milliseconds(start);foundCopy=impl->copyPipelines.emplace(validation,pipeline).first;
   }
   p->copyPipeline=foundCopy->second;lease.Retain(p->copyPipeline);auto a=[MTL4ArgumentTableDescriptor new];a.maxBufferBindCount=1;a.maxTextureBindCount=2;a.initializeBindings=YES;NSError* err=nil;p->copyArguments=[lease.Device() newArgumentTableWithDescriptor:a error:&err];Require(p->copyArguments!=nil,"Refractive copy arguments unavailable");lease.Retain(p->copyArguments);
  }
 }

 if(refract)Require(subsurface&&p->shading.texture&&p->shading.sampler&&p->shading.owner&&renderHeight&&
  subsurface.width==refract->allocation[0]&&subsurface.height==refract->allocation[1],"Refractive same-frame color/shading owners missing");
 if(frame->indices.empty())return std::unique_ptr<PreparedLegacyWater>(new PreparedLegacyWater(std::move(p)));
 NSError* error=nil;
 const auto key=Impl::Key{color,lease.SampleShadingKey(samples),validation,bool(refract)};auto found=impl->pipelines.find(key);
 if(found==impl->pipelines.end()){
 const auto start=Clock::now();auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
 auto lib=lease.NewLibraryWithSource(String(refract?NativeLegacyRefractiveWaterShaders:NativeLegacyWaterShaders),options,&error);Require(lib!=nil,"Water1 shader compile failed: "+Error(error));
 auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
 auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=lib;v.name=@"LegacyWaterVertex";f.name=@"LegacyWaterFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;
 auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=refract?MTL4BlendStateDisabled:MTL4BlendStateEnabled;a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;
 a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 auto po=[MTL4PipelineOptions new];po.shaderValidation=ShaderValidationMode(validation);d.options=po;
 p->pipeline=lease.NewRenderPipeline(d,&error);Require(p->pipeline!=nil,"Water1 pipeline compile failed: "+Error(error));compileMs+=Milliseconds(start);found=impl->pipelines.emplace(key,p->pipeline).first;
 }
 p->pipeline=found->second;lease.Retain(p->pipeline);
 if(!impl->depth){auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionLessEqual;depth.depthWriteEnabled=NO;impl->depth=[lease.Device() newDepthStencilStateWithDescriptor:depth];Require(impl->depth!=nil,"Water1 depth state missing");}
 p->depth=impl->depth;lease.Retain(p->depth);
 Uniforms uniforms{frame->viewProjection,frame->captureCamera.view,{frame->forward[0],frame->forward[1],0,0},frame->fogColor,frame->fogParameters,
  {uint32_t(frame->captureCamera.clipDepth==ClipDepthConvention::MinusOneToOne),1,uint32_t(bumpFlipV),uint32_t(frame->fogEnabled)},{},{},{}};
 if(refract){uniforms.refractExtent={float(refract->allocation[0]),float(refract->allocation[1]),float(renderHeight),0};uniforms.mapExtent={refract->inverseMapExtent[0],refract->inverseMapExtent[1],0,0};uniforms.refractFlags={1,uint32_t(refract->target==LegacyRefractionTarget::Rectangle),uint32_t(p->shading.flipV),0};}
 auto vertexBuffer=Buffer(lease,frame->vertices.data(),frame->vertices.size()*sizeof(NativeLegacyWaterVertex));
 p->indices=Buffer(lease,frame->indices.data(),frame->indices.size()*sizeof(uint32_t));auto uniformBuffer=Buffer(lease,&uniforms,sizeof(uniforms));
 auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=2;ad.maxTextureBindCount=4;ad.maxSamplerStateBindCount=4;ad.initializeBindings=YES;
 p->arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(p->arguments!=nil,"Water1 argument table missing");lease.Retain(p->arguments);
 [p->arguments setAddress:vertexBuffer.gpuAddress atIndex:0];[p->arguments setAddress:uniformBuffer.gpuAddress atIndex:1];
 for(unsigned slot=0;slot<2;++slot){auto sd=[MTLSamplerDescriptor new];sd.supportArgumentBuffers=YES;sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.mipFilter=MTLSamplerMipFilterNotMipmapped;
  sd.sAddressMode=sd.tAddressMode=slot?MTLSamplerAddressModeRepeat:MTLSamplerAddressModeClampToEdge;
  auto sampler=[lease.Device() newSamplerStateWithDescriptor:sd];Require(sampler!=nil,"Water1 sampler missing");lease.Retain(sampler);
  [p->arguments setSamplerState:sampler.gpuResourceID atIndex:slot];[p->arguments setTexture:(slot?bump:reflection).gpuResourceID atIndex:slot];}
 if(refract){
  auto sd=[MTLSamplerDescriptor new];sd.supportArgumentBuffers=YES;sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterNearest;sd.mipFilter=MTLSamplerMipFilterNotMipmapped;
  sd.sAddressMode=sd.tAddressMode=refract->target==LegacyRefractionTarget::Rectangle?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;
  auto sampler=[lease.Device() newSamplerStateWithDescriptor:sd];Require(sampler!=nil,"Refractive nearest source sampler missing");lease.Retain(sampler);
  [p->arguments setTexture:subsurface.gpuResourceID atIndex:2];[p->arguments setSamplerState:sampler.gpuResourceID atIndex:2];
  [p->arguments setTexture:p->shading.texture.gpuResourceID atIndex:3];[p->arguments setSamplerState:p->shading.sampler.gpuResourceID atIndex:3];
  lease.AddAllocation(subsurface);
 }
 lease.AddAllocation(reflection);lease.AddAllocation(bump);
 return std::unique_ptr<PreparedLegacyWater>(new PreparedLegacyWater(std::move(p)));
}
CUSSourceImage LegacyWaterGPU::ResolveCopy(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission){
 Require(submission&&binding.object,"Refractive inherited image lacks source/submission");auto backing=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(binding.object->backing);
 Require(bool(backing),"Refractive inherited image has no original copy publication");auto resource=EnsureLegacyRefractionImage(lease,binding.object);
 const auto& state=binding.object->sampling;Require(state.minFilter==0x2600&&state.magFilter==0x2600&&state.baseLevel==0&&!state.compareMode,"Refractive source sampler changed outside supported caller");
 const bool rectangle=binding.object->target==NativeCUSTextureTarget::Rectangle;const unsigned wrap=rectangle?0x812F:0x2901;Require(state.wrapS==wrap&&state.wrapT==wrap,"Refractive source wrapping changed outside supported caller");
 auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;s.minFilter=s.magFilter=MTLSamplerMinMagFilterNearest;s.mipFilter=MTLSamplerMipFilterNotMipmapped;s.sAddressMode=s.tAddressMode=s.rAddressMode=rectangle?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;
 auto sampler=[lease.Device() newSamplerStateWithDescriptor:s];Require(sampler!=nil,"Refractive inherited sampler unavailable");lease.Retain(sampler);lease.AddAllocation(resource->texture);
 CUSSourceImage out;out.texture=resource->texture;out.sampler=sampler;out.owner=binding.object;out.flipV=true;out.requireReadable=[resource,submission]{Require(resource->completed||resource->encodedSubmission==submission,"Refractive inherited image precedes its exact copy");};return out;
}

}
