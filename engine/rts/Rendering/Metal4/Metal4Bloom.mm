/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Bloom.h"
#include "NativeBloomShaders.h"
#include "Metal4DeferredColorNative.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>
#include <algorithm>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4 source,bright,temporal,alphaConfig;simd_uint4 flip,swizzle[4],depthMode;};static_assert(sizeof(Uniform)==160);
struct ImageState final:NativeBloomImageResource {std::weak_ptr<const NativeBloomImageVersion>owner;__strong id<MTLTexture>texture=nil;__strong id<MTLSamplerState>sampler=nil;std::shared_ptr<NativeBloomProducerState>producer;};
using ImageKey=std::pair<uint64_t,uint64_t>;
// Validate the captured original backing as well as the resolved DTO: texture
// names and a matching producer pointer alone do not identify an MRT attachment.
void ValidateModelColorInput(const NativeCUSTextureBinding& binding,ResolvedBloomImage& image,unsigned attachment,uint64_t submission)
{
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Bloom model color requires its captured original binding");
 auto backing=std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing);
 Require(backing&&backing->Metadata().role==NativeGeometryTextureRole::Models&&backing->Metadata().attachment==attachment,"Bloom model color names the wrong original MRT role/attachment");
 const auto actual=GetDeferredColorBackingImage(*backing);
 Require(actual.producer&&actual.producer->role==ShieldDepthRole::ModelsOnly&&image.texture==actual.texture&&image.texture.pixelFormat==MTLPixelFormatRGBA8Unorm&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1,"Bloom model color is not its actual RGBA8 MRT output");
 Require(image.deferredEpoch.actualProducer==actual.producer&&image.deferredEpoch.allocatedUndefined==(actual.producer->operation==ShieldDepthOperation::AllocationOnly),"Bloom model color resolver changed its actual producer epoch");
 Require(bool(image.requireReadable),"Bloom model color lacks actual encode-time readability");
 auto readable=std::move(image.requireReadable);
 image.requireReadable=[readable=std::move(readable),actual,submission]{readable();Require(IsDeferredColorReadable(actual,submission),"Bloom actual model color has not encoded or retired");};
}
void ValidateDepthInput(const NativeCUSTextureBinding& binding,ResolvedBloomImage& image,bool models,uint64_t submission)
{
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Bloom depth requires its captured original binding");
 auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);
 const auto kind=models?NativeTextureBackingKind::ModelDepth:NativeTextureBackingKind::MapDepth;
 const auto role=models?ShieldDepthRole::ModelsOnly:ShieldDepthRole::TerrainOnly;
 Require(backing&&backing->Kind()==kind,"Bloom depth names the wrong original model/map attachment5");
 const auto actual=GetShieldDepthBackingImage(*backing);
 Require(actual.producer&&actual.role==role&&actual.producer->role==role&&image.texture==actual.texture&&image.texture.pixelFormat==MTLPixelFormatDepth32Float&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1,"Bloom depth is not its actual independent D32F source");
 Require(image.deferredEpoch.actualProducer==actual.producer&&image.deferredEpoch.allocatedUndefined==(actual.producer->operation==ShieldDepthOperation::AllocationOnly),"Bloom depth resolver changed its actual producer epoch");
 Require(bool(image.requireReadable),"Bloom depth lacks actual encode-time readability");
 auto readable=std::move(image.requireReadable);
 image.requireReadable=[readable=std::move(readable),actual,submission]{readable();const auto& p=*actual.producer;Require(IsNativeShieldDepthReadable(p.encoded,p.completed,p.operation==ShieldDepthOperation::AllocationOnly,p.allocationVerified,p.submissionIdentity,submission),"Bloom actual depth has not encoded or retired");};
}
MTLPixelFormat ColorFormat(NativeBloomFormat format){switch(format){case NativeBloomFormat::RG11B10Float:return MTLPixelFormatRG11B10Float;case NativeBloomFormat::RGBA16Float:return MTLPixelFormatRGBA16Float;}throw std::runtime_error("Unknown original31657 Bloom HDR format");}
bool DepthFormat(MTLPixelFormat f){return f==MTLPixelFormatDepth32Float||f==MTLPixelFormatDepth16Unorm||f==MTLPixelFormatDepth32Float_Stencil8;}
id<MTLBuffer>Upload(RuntimeLease&l,const void*p,size_t n){Require(n&&n<=l.Device().maxBufferLength,"Bloom immutable buffer exceeds device capacity");auto b=[l.Device()newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate actual Bloom uniforms/vertices");l.AddAllocation(b);return b;}
std::string Defines(const NativeBloomDefines&d,unsigned depthMask,bool combine){std::ostringstream s;s<<std::setprecision(9)<<"#define DEPTH_MASK "<<depthMask<<"\n";if(combine)s<<"#define BLOOM_COMBINE 1\n";for(auto&[k,v]:d)s<<"#define "<<k<<' '<<v<<"\n";return s.str();}
NativeBloomDefines DefaultDefines(){return {{"VSX",64},{"VSY",48},{"HSX",32},{"HSY",24},{"IHSX",1.f/32},{"IHSY",1.f/24},{"PADX",0},{"PADY",0},{"DOWNSCALE",2},{"MIPCOUNT",5}};}
const char*Entry(NativeBloomProgram p){switch(p){case NativeBloomProgram::Bright:return "BloomBright";case NativeBloomProgram::Downsample:return "BloomDownsample";case NativeBloomProgram::Upsample:return "BloomUpsample";case NativeBloomProgram::FinalUpsample:return "BloomFinalUpsample";case NativeBloomProgram::Combine:return "BloomCombine";}throw std::runtime_error("Unknown Bloom program");}
std::vector<unsigned>InputUnits(const NativeBloomDraw&d){switch(d.program){case NativeBloomProgram::Bright:return{0,1,2,3};case NativeBloomProgram::FinalUpsample:return{0,1,2};default:return{0};}}
uint64_t BlendKey(const ScreenBlendState&b,const std::array<bool,4>&m){return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;}
MTLBlendOperation Equation(ScreenBlendEquation e){switch(e){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown Bloom blend equation");}
}
struct PreparedBloom::Impl {
 struct Draw {uint64_t sequence;__strong id<MTL4ArgumentTable>arguments;__strong id<MTLRenderPipelineState>pipeline;__strong id<MTLDepthStencilState>depth;__strong id<MTLTexture>color,z,stencil;std::shared_ptr<ImageState>output,previous;std::vector<ResolvedBloomImage>inputs;std::vector<std::shared_ptr<ImageState>>localInputs;NativeWorldDrawState world;effectstate::Raster raster;uint64_t submission=0;mutable bool encoded=false;bool noop=false,clear=false,copyPrevious=true;std::array<float,4>clearColor{};};
 std::vector<Draw>draws;std::shared_ptr<const NativeBloomFrame>owner;
};
PreparedBloom::PreparedBloom():impl(std::make_unique<Impl>()){}PreparedBloom::~PreparedBloom()=default;size_t PreparedBloom::DrawCount()const{return impl->draws.size();}uint64_t PreparedBloom::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
void PreparedBloom::EncodeDraw(size_t index,id<MTL4CommandBuffer>command)const{
 const auto&d=impl->draws.at(index);Require(command!=nil,"Bloom operation has no actual command buffer");Require(!d.encoded,"Bloom original operation was encoded twice");
 if(d.noop){
  if(d.previous)Require(d.previous->producer->completed||(d.previous->producer->encoded&&d.previous->producer->submissionIdentity==d.submission),"Bloom no-op previous contents have an outstanding original writer");
  if(d.output&&d.previous){auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot retain original Bloom no-coverage image contents");[copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[copy copyFromTexture:d.previous->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(d.color.width,d.color.height,1) toTexture:d.color destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];}
  d.encoded=true;if(d.output){d.output->producer->submissionIdentity=d.submission;d.output->producer->encoded=true;}return;
 }
 for(auto&i:d.inputs)i.requireReadable();for(auto&i:d.localInputs)Require(i->producer->completed||(i->producer->encoded&&i->producer->submissionIdentity==d.submission),"Bloom samples unencoded original FBO generation");
 if(d.output&&d.previous&&d.copyPrevious){Require(d.previous->producer->completed||(d.previous->producer->encoded&&d.previous->producer->submissionIdentity==d.submission),"Bloom partial update previous contents are not produced");auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot preserve actual Bloom partial image contents");[copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[copy copyFromTexture:d.previous->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(d.color.width,d.color.height,1) toTexture:d.color destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];}
 auto p=[MTL4RenderPassDescriptor new];p.defaultRasterSampleCount=d.color.sampleCount;p.renderTargetWidth=d.color.width;p.renderTargetHeight=d.color.height;p.colorAttachments[0].texture=d.color;p.colorAttachments[0].loadAction=d.output?(d.previous&&d.copyPrevious?MTLLoadActionLoad:MTLLoadActionDontCare):MTLLoadActionLoad;p.colorAttachments[0].storeAction=MTLStoreActionStore;
 if(d.z){p.depthAttachment.texture=d.z;p.depthAttachment.loadAction=MTLLoadActionLoad;p.depthAttachment.storeAction=MTLStoreActionStore;}
 if(d.stencil){p.stencilAttachment.texture=d.stencil;p.stencilAttachment.loadAction=MTLLoadActionLoad;p.stencilAttachment.storeAction=MTLStoreActionStore;}
 auto e=[command renderCommandEncoderWithDescriptor:p];Require(e!=nil,"Cannot encode actual Bloom render operation");[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];effectstate::RasterState(e,d.world,d.raster);[e setCullMode:d.world.cullEnabled?(d.world.cull==NativeWorldCull::Front?MTLCullModeFront:d.world.cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone):MTLCullModeNone];[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];d.encoded=true;if(d.output){d.output->producer->submissionIdentity=d.submission;d.output->producer->encoded=true;}
}
void PreparedBloom::DidComplete()const{
 // Validate the whole frame before publishing any retirement. A late missing
 // operation must not leave earlier outputs marked completed during rollback.
 for(const auto&d:impl->draws){Require(d.encoded,"Unencoded original Bloom operation cannot retire");if(d.output)Require(d.output->producer->encoded&&d.output->producer->submissionIdentity==d.submission,"Unencoded or unrelated Bloom output cannot retire");}
 for(const auto&d:impl->draws)if(d.output)d.output->producer->completed=true;
 impl->owner->completed=true;
}
struct BloomGPU::Impl {
 bool initialized=false;
 std::map<ImageKey,std::shared_ptr<ImageState>>images;
 std::vector<id<MTLTexture>> freeImages;
 std::map<std::pair<unsigned,unsigned>,id<MTLSamplerState>> samplers;
 void Recycle(){for(auto i=images.begin();i!=images.end();){auto& image=i->second;if(image.use_count()!=1||!image->owner.expired()||(image->producer->encoded&&!image->producer->completed)){++i;continue;}
  const bool aliased=std::any_of(images.begin(),images.end(),[&](const auto& other){return other.first!=i->first&&other.second->texture==image->texture;});
  if(!aliased&&std::find(freeImages.begin(),freeImages.end(),image->texture)==freeImages.end())freeImages.push_back(image->texture);i=images.erase(i);
 }}
 struct ProgramKey {std::string definitions;unsigned program;MTLPixelFormat color,depth,stencil;uint64_t blend;bool clear,validation;unsigned samples;bool coverage;auto operator<=>(const ProgramKey&)const=default;};
 std::map<std::string,id<MTLLibrary>>libraries;std::map<ProgramKey,id<MTLRenderPipelineState>>pipelines;
 std::shared_ptr<ImageState>Image(RuntimeLease&l,std::shared_ptr<const NativeBloomImageVersion>v){Require(v&&v->image&&v->generation,"Missing actual Bloom image version");if(v->Resource()){auto image=std::dynamic_pointer_cast<const ImageState>(v->Resource());Require(image&&image->texture.device==l.Device()&&image->owner.lock()==v,"Original Bloom image resource belongs to another device/version");return std::const_pointer_cast<ImageState>(image);}ImageKey k{v->image->identity,v->generation};if(auto i=images.find(k);i!=images.end()){Require(i->second->owner.lock()==v,"Bloom image identity/generation reused by unrelated owner");return i->second;}auto r=std::make_shared<ImageState>();r->owner=v;r->producer=v->producer;auto&im=*v->image;Require(im.width&&im.height&&im.width<=l.Capabilities().maxTextureDimension2D&&im.height<=l.Capabilities().maxTextureDimension2D,"Bloom original FBO exceeds actual device limits");auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ColorFormat(im.format) width:im.width height:im.height mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto reusable=std::find_if(freeImages.begin(),freeImages.end(),[&](id<MTLTexture> texture){return texture.width==im.width&&texture.height==im.height&&texture.pixelFormat==ColorFormat(im.format);});if(reusable!=freeImages.end()){r->texture=*reusable;freeImages.erase(reusable);}else r->texture=[l.Device()newTextureWithDescriptor:d];Require(r->texture!=nil,"Cannot allocate actual Bloom FBO texture");const auto key=std::pair{im.sampling.minFilter,im.sampling.magFilter};if(auto cached=samplers.find(key);cached!=samplers.end())r->sampler=cached->second;else{auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;s.minFilter=im.sampling.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.magFilter=im.sampling.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.mipFilter=MTLSamplerMipFilterNotMipmapped;s.sAddressMode=s.tAddressMode=MTLSamplerAddressModeClampToEdge;r->sampler=[l.Device()newSamplerStateWithDescriptor:s];Require(r->sampler!=nil,"Cannot allocate actual Bloom sampler");samplers.emplace(key,r->sampler);}v->Fulfill(r);images.emplace(k,r);return r;}
 id<MTLRenderPipelineState>Pipeline(RuntimeLease&l,const NativeBloomDraw&draw,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned depthMask,bool validation,double&ms,unsigned samples){auto d=draw.defines.empty()?DefaultDefines():draw.defines;std::string defs=Defines(d,depthMask,draw.program==NativeBloomProgram::Combine);ProgramKey key{defs,unsigned(draw.program),color,depth,stencil,BlendKey(draw.surface.blend,draw.world.colorWrite),draw.clear,validation,l.SampleShadingKey(samples),!draw.clear&&draw.world.alphaToCoverage};if(auto p=pipelines.find(key);p!=pipelines.end())return p->second;auto start=Clock::now();NSError*error=nil;id<MTLLibrary>library=nil;if(auto i=libraries.find(defs);i!=libraries.end())library=i->second;else{auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 complete Bloom program family";descriptor.source=String(defs+bloom::SHADER);auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;library=l.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile exact original Bloom shaders: "+Error(error));libraries.emplace(defs,library);}
 auto p=[MTL4RenderPipelineDescriptor new];p.rasterSampleCount=samples;p.alphaToCoverageState=key.coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=@"BloomVertex";f.name=String(Entry(draw.program));p.vertexFunctionDescriptor=v;p.fragmentFunctionDescriptor=f;p.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=p.colorAttachments[0];a.pixelFormat=color;auto b=draw.surface.blend;if(draw.clear)b.enabled=false;a.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);auto m=draw.world.colorWrite;a.writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);p.options=options;auto pipeline=l.NewRenderPipeline(p,&error);Require(pipeline!=nil,"Cannot compile exact Bloom actual attachment pipeline: "+Error(error));pipelines.emplace(key,pipeline);ms+=Milliseconds(start);return pipeline;}
};
BloomGPU::BloomGPU():impl(std::make_unique<Impl>()){}BloomGPU::~BloomGPU()=default;
void BloomGPU::Initialize(RuntimeLease&l,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double&ms,unsigned samples){NativeBloomDraw d;d.defines=DefaultDefines();for(auto hdr:{MTLPixelFormatRG11B10Float,MTLPixelFormatRGBA16Float})for(auto p:{NativeBloomProgram::Bright,NativeBloomProgram::Downsample,NativeBloomProgram::Upsample,NativeBloomProgram::FinalUpsample,NativeBloomProgram::Combine}){d.program=p;impl->Pipeline(l,d,p==NativeBloomProgram::Combine?color:hdr,p==NativeBloomProgram::Combine?depth:MTLPixelFormatInvalid,p==NativeBloomProgram::Combine?stencil:MTLPixelFormatInvalid,p==NativeBloomProgram::Bright?12u:0u,validation,ms,p==NativeBloomProgram::Combine?samples:1);}impl->initialized=true;}

bool BloomGPU::Initialized()const noexcept{return impl->initialized;}
void BloomGPU::ClearResources(){impl->images.clear();impl->freeImages.clear();}
ResolvedBloomImage BloomGPU::Resolve(RuntimeLease&l,std::shared_ptr<const NativeBloomImageVersion>v,uint64_t submission){auto im=impl->Image(l,std::move(v));l.AddAllocation(im->texture);l.Retain(im->sampler);return {im->texture,im->sampler,im,[im,submission]{Require(im->producer->completed||(submission&&im->producer->encoded&&im->producer->submissionIdentity==submission),"Bloom immutable output is not readable before its real producer");},true,{}};}
std::unique_ptr<PreparedBloom>BloomGPU::Prepare(RuntimeLease&l,std::shared_ptr<const NativeBloomFrame>frame,const WorldCamera&camera,uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture>sceneColor,id<MTLTexture>sceneDepth,id<MTLTexture>sceneStencil,const BloomImageResolver&resolver,bool validation,double&ms,uint64_t submission){
 impl->Recycle();
 if(!frame||frame->draws.empty())return{};Require(submission!=0,"Bloom requires the common actual submission identity");Require(sceneColor!=nil,"Bloom final composite needs actual scene color");auto result=std::make_unique<PreparedBloom>();result->impl->owner=frame;uint64_t prior=0;
 for(auto&draw:frame->draws){ValidateNativeBloomDraw(draw);Require(draw.sequence>prior,"Bloom source operations out of order");prior=draw.sequence;PreparedBloom::Impl::Draw d{};d.sequence=draw.sequence;d.submission=submission;d.world=draw.world;Require(d.world.frontPolygonMode==NativePolygonMode::Fill&&d.world.backPolygonMode==NativePolygonMode::Fill,"Original Bloom non-fill raster consumer is not yet connected");d.clear=draw.clear;d.clearColor=draw.clearColor;d.color=sceneColor;d.z=sceneDepth;d.stencil=sceneStencil;
  if(draw.output){d.output=impl->Image(l,draw.output);d.color=d.output->texture;d.z=d.stencil=nil;d.world.depthTest=d.world.depthWrite=d.world.stencilTest=false;if(draw.previous)d.previous=impl->Image(l,draw.previous);}
  // These inputs are original immutable named images. Only the final output
  // uses the main sample count; do not resolve or replace named map/model depth.
  Require(d.color.sampleCount && [l.Device() supportsTextureSampleCount:d.color.sampleCount],"Bloom output sample count is unsupported");
  for(auto attachment:{d.z,d.stencil})if(attachment)Require(attachment.device==d.color.device&&
   attachment.width==d.color.width&&attachment.height==d.color.height&&attachment.sampleCount==d.color.sampleCount,
   "Bloom output depth/stencil differs from the actual color storage");
  auto original=camera;if(draw.surface.viewport){auto&v=*draw.surface.viewport;original.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
  // Original RenderToTexture supplies the actual private image viewport;
  // the final combine retains the original main framebuffer viewport.
  auto scissor=draw.surface.scissor;if(scissor&&!scissor->windowCoordinates){scissor->x+=camera.viewport[0];scissor->y+=camera.viewport[1];scissor->windowCoordinates=true;}
  if(draw.clear)original.viewport={0,0,int32_t(d.color.width),int32_t(d.color.height)};
  auto raster=effectstate::Viewport(original,uint32_t(d.color.width),uint32_t(d.color.height),draw.output?uint32_t(d.color.width):logicalWidth,draw.output?uint32_t(d.color.height):logicalHeight,scissor);d.noop=!raster||(!draw.clear&&d.world.cullEnabled&&(d.world.cull==NativeWorldCull::FrontAndBack||(d.world.cull==NativeWorldCull::Front&&draw.vertices==MakeNativeBloomTexRect(-1,-1,1,1,0,0,1,1))));if(raster)d.raster=*raster;
  // Source no-coverage/cull-both has no fragment samples or writes. Retain
  // actual prior storage, including its undefined-storage state; no clear is invented.
  // Storage follows the immutable version; never replace an image already
  // retained by a later generic UI argument table.
  if(draw.output&&draw.clear){d.raster.viewport={0,0,double(d.color.width),double(d.color.height),0,1};d.world.depthTest=d.world.depthWrite=d.world.stencilTest=d.world.cullEnabled=false;}
  Uniform u{};u.source=simd_make_float4(draw.uniforms.sourceTexelSize[0],draw.uniforms.sourceTexelSize[1],draw.uniforms.filterRadius,0);u.bright=simd_make_float4(draw.uniforms.illuminationThreshold,draw.uniforms.kneeWidth,draw.uniforms.glowAmplifier,draw.uniforms.maxBrightContribution);u.temporal=simd_make_float4(draw.uniforms.historyMix,draw.uniforms.bloomNorm,float(draw.uniforms.debugDraw),camera.clipDepth==ClipDepthConvention::ZeroToOne?1:0);
  u.alphaConfig=simd_make_float4(draw.surface.alphaTest.enabled,draw.surface.alphaTest.reference,unsigned(draw.surface.alphaTest.compare),0);
  // A proved whole-attachment overwrite needs no old-image copy. Additive
  // upsampling, masks and partial/scissored coverage retain the real prior contents.
  const auto& v=d.raster.viewport;const auto& sc=d.raster.scissor;
  const auto expected=MakeNativeBloomTexRect(-1,-1,1,1,0,0,1,1);
  d.copyPrevious=!(draw.output&&!d.noop&&!draw.surface.blend.enabled&&!draw.surface.alphaTest.enabled&&!draw.world.alphaToCoverage&&draw.vertices==expected&&std::all_of(draw.world.colorWrite.begin(),draw.world.colorWrite.end(),[](bool x){return x;})&&v.originX==0&&v.originY==0&&v.width==d.color.width&&v.height==d.color.height&&sc.x==0&&sc.y==0&&sc.width==d.color.width&&sc.height==d.color.height);
  auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=2;desc.maxTextureBindCount=desc.maxSamplerStateBindCount=4;desc.initializeBindings=YES;NSError*error=nil;d.arguments=[l.Device()newArgumentTableWithDescriptor:desc error:&error];Require(d.arguments!=nil,"Cannot allocate Bloom original texture table: "+Error(error));unsigned depthMask=0;std::array<NativeDeferredPostprocessEpoch,3> modelEpochs;
  for(unsigned unit=0;unit<4;++unit){const auto&input=draw.inputs[unit];const auto&s=input.local?input.local->image->sampling:input.actual.object?input.actual.object->sampling:input.actual.defaultSampling;u.swizzle[unit]=simd_make_uint4(s.swizzle[0],s.swizzle[1],s.swizzle[2],s.swizzle[3]);u.depthMode[unit]=s.depthMode;}
  if(!draw.clear&&!d.noop)for(unsigned unit:InputUnits(draw)){auto&input=draw.inputs[unit];ResolvedBloomImage image;if(input.local){auto im=impl->Image(l,input.local);d.localInputs.push_back(im);image=Resolve(l,input.local,submission);}else{Require(bool(resolver),"Bloom genuine GBuffer/stencil resolver is not installed");image=resolver(l,input.actual);}
   // Original Bright sampler units: diffuse1, emission3, modeldepth5, mapdepth5.
   if(draw.program==NativeBloomProgram::Bright){Require(!input.local,"Bloom Bright GBuffer inputs cannot use private effect storage");if(unit<2)ValidateModelColorInput(input.actual,image,unit==0?1:3,submission);else ValidateDepthInput(input.actual,image,unit==2,submission);}
   Require(image.texture&&image.sampler&&image.owner,"Bloom actual sampler owner/image missing");Require(bool(image.requireReadable),"Bloom sampled image lacks its actual producer readability check");Require(image.texture.textureType==MTLTextureType2D,"Bloom original sampler2D cannot silently sample an unresolved multisample image");if(draw.program==NativeBloomProgram::Bright&&unit<3)modelEpochs[unit]=image.deferredEpoch;if(draw.program==NativeBloomProgram::Bright&&unit==3)Require(bool(image.deferredEpoch.actualProducer),"Bloom map depth lacks independent actual producer provenance");
   // Original31657 vertex shaders derive these flat values from textureSize(source,0).
   if(unit==0&&(draw.program==NativeBloomProgram::Downsample||draw.program==NativeBloomProgram::Upsample)){Require(image.texture.width&&image.texture.height,"Bloom source textureSize is empty");u.source.x=1.f/float(image.texture.width);u.source.y=1.f/float(image.texture.height);u.source.w=draw.program==NativeBloomProgram::Downsample&&image.texture.width==draw.defines.at("HSX")&&image.texture.height==draw.defines.at("HSY")?1.f:0.f;}
   if(DepthFormat(image.texture.pixelFormat))depthMask|=1u<<unit;if(image.flipV)u.flip.x|=1u<<unit;[d.arguments setTexture:image.texture.gpuResourceID atIndex:unit];[d.arguments setSamplerState:image.sampler.gpuResourceID atIndex:unit];l.AddAllocation(image.texture);l.Retain(image.sampler);d.inputs.push_back(std::move(image));}
  if(!draw.clear&&!d.noop&&draw.program==NativeBloomProgram::Bright)ValidateNativeDeferredModelEpochs(modelEpochs);
  auto vertices=draw.clear?MakeNativeBloomTexRect(-1,-1,1,1,0,0,1,1):draw.vertices;auto vb=Upload(l,vertices.data(),sizeof(vertices)),ub=Upload(l,&u,sizeof(u));[d.arguments setAddress:ub.gpuAddress atIndex:0];[d.arguments setAddress:vb.gpuAddress atIndex:1];d.pipeline=impl->Pipeline(l,draw,d.color.pixelFormat,d.z?d.z.pixelFormat:MTLPixelFormatInvalid,d.stencil?d.stencil.pixelFormat:MTLPixelFormatInvalid,depthMask,validation,ms,unsigned(d.color.sampleCount));d.depth=effectstate::Depth(l,d.world,d.stencil?d.stencil.pixelFormat:MTLPixelFormatInvalid);l.Retain(d.arguments);l.Retain(d.pipeline);l.AddAllocation(d.color);if(d.z)l.AddAllocation(d.z);if(d.stencil)l.AddAllocation(d.stencil);if(d.previous)l.AddAllocation(d.previous->texture);result->impl->draws.push_back(std::move(d));
 }return result;
}
}
