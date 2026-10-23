/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4SSAO.h"
#include "NativeSSAOShaders.h"
#include "Metal4DeferredColorNative.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4x4 view,projection,inverseProjection,fuseInverseProjection;simd_float4 kernel[64],weights[12],offsets[12],directionStrength,alphaConfig;simd_uint4 flip,swizzle[8],depthMode[2];simd_float4 framebuffer;};static_assert(sizeof(Uniform)==1888);
struct ImageState final:NativeSSAOImageResource {std::weak_ptr<const NativeSSAOImageVersion>owner;__strong id<MTLTexture>texture=nil;__strong id<MTLSamplerState>sampler=nil;std::shared_ptr<NativeSSAOProducerState>producer;NativeDeferredPostprocessEpoch sourceModelEpoch;};
using ImageKey=std::pair<uint64_t,uint64_t>;
// Validate the captured original backing as well as the resolved DTO: texture
// names and a matching producer pointer alone do not identify an MRT attachment.
void ValidateModelColorInput(const NativeCUSTextureBinding& binding,ResolvedSSAOImage& image,unsigned attachment,uint64_t submission)
{
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"SSAO model color requires its captured original binding");
 auto backing=std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing);
 Require(backing&&backing->Metadata().role==NativeGeometryTextureRole::Models&&backing->Metadata().attachment==attachment,"SSAO model color names the wrong original MRT role/attachment");
 const auto actual=GetDeferredColorBackingImage(*backing);
 Require(actual.producer&&actual.producer->role==ShieldDepthRole::ModelsOnly&&image.texture==actual.texture&&image.texture.pixelFormat==MTLPixelFormatRGBA8Unorm&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1,"SSAO model color is not its actual RGBA8 MRT output");
 Require(image.deferredEpoch.actualProducer==actual.producer&&image.deferredEpoch.allocatedUndefined==(actual.producer->operation==ShieldDepthOperation::AllocationOnly),"SSAO model color resolver changed its actual producer epoch");
 Require(bool(image.requireReadable),"SSAO model color lacks actual encode-time readability");
 auto readable=std::move(image.requireReadable);
 image.requireReadable=[readable=std::move(readable),actual,submission]{readable();Require(IsDeferredColorReadable(actual,submission),"SSAO actual model color has not encoded or retired");};
}
void ValidateDepthInput(const NativeCUSTextureBinding& binding,ResolvedSSAOImage& image,bool models,uint64_t submission)
{
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"SSAO depth requires its captured original binding");
 auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);
 const auto kind=models?NativeTextureBackingKind::ModelDepth:NativeTextureBackingKind::MapDepth;
 const auto role=models?ShieldDepthRole::ModelsOnly:ShieldDepthRole::TerrainOnly;
 Require(backing&&backing->Kind()==kind,"SSAO depth names the wrong original model/map attachment5");
 const auto actual=GetShieldDepthBackingImage(*backing);
 Require(actual.producer&&actual.role==role&&actual.producer->role==role&&image.texture==actual.texture&&image.texture.pixelFormat==MTLPixelFormatDepth32Float&&image.texture.textureType==MTLTextureType2D&&image.texture.sampleCount==1,"SSAO depth is not its actual independent D32F source");
 Require(image.deferredEpoch.actualProducer==actual.producer&&image.deferredEpoch.allocatedUndefined==(actual.producer->operation==ShieldDepthOperation::AllocationOnly),"SSAO depth resolver changed its actual producer epoch");
 Require(bool(image.requireReadable),"SSAO depth lacks actual encode-time readability");
 auto readable=std::move(image.requireReadable);
 image.requireReadable=[readable=std::move(readable),actual,submission]{readable();const auto& p=*actual.producer;Require(IsNativeShieldDepthReadable(p.encoded,p.completed,p.operation==ShieldDepthOperation::AllocationOnly,p.allocationVerified,p.submissionIdentity,submission),"SSAO actual depth has not encoded or retired");};
}
bool DepthFormat(MTLPixelFormat f){return f==MTLPixelFormatDepth32Float||f==MTLPixelFormatDepth16Unorm||f==MTLPixelFormatDepth32Float_Stencil8;}
id<MTLBuffer>Upload(RuntimeLease&l,const void*p,size_t n){Require(n&&n<=l.Device().maxBufferLength,"SSAO immutable buffer exceeds device capacity");auto b=[l.Device()newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate actual SSAO uniforms/vertices");l.AddAllocation(b);return b;}
std::string Defines(const NativeSSAODefines&d,bool modelColor,bool mapColor,bool normalColor){std::ostringstream s;s<<std::setprecision(9)<<"#define MODEL_COLOR "<<modelColor<<"\n#define MAP_COLOR "<<mapColor<<"\n#define NORMAL_COLOR "<<normalColor<<"\n";for(auto&[k,v]:d)s<<"#define "<<k<<' '<<v<<"\n";return s.str();}
NativeSSAODefines DefaultDefines(){return {{"VSX",64},{"VSY",48},{"HSX",64},{"HSY",48},{"DOWNSAMPLE",1},{"TEXPADDINGX",0},{"TEXPADDINGY",0},{"DEPTH_CLIP01",1},{"MERGE_MISC",0},{"SSAO_FIBONACCI",1},{"SSAO_KERNEL_MINZ",.04f},{"SSAO_RANDOM_LENGTH",.6f},{"SSAO_KERNEL_SIZE",28},{"SSAO_RADIUS",7},{"SSAO_RADIUS_FAR_SCALE",4.5f},{"SSAO_MIN",.61f},{"SSAO_OCCLUSION_POWER",4},{"SSAO_FADE_DIST_0",9000},{"SSAO_FADE_DIST_1",4500},{"DEBUG_SSAO",0},{"BRIGHTEN",30},{"BLUR_HALF_KERNEL_SIZE",5},{"BLUR_SIGMA",3.2f},{"MINCOSANGLE",.75f},{"ZTHRESHOLD",.005f},{"MINSELFWEIGHT",.2f},{"OUTLIERCORRECTIONFACTOR",.1f},{"BLUR_POWER",1.6f},{"BLUR_CLAMP",.145f},{"DEBUG_BLUR",0},{"USE_STENCIL",0},{"ENABLE",1},{"SLOWFUSE",0},{"NOFUSE",1},{"SSAO_ALPHA_POW",10},{"BLUR_COMPOSITE",0},{"TILE_PX",32},{"TILE_TEXELS",8}};}
const char*Entry(NativeSSAOProgram p){switch(p){case NativeSSAOProgram::Fuse:return "SSAOFuse";case NativeSSAOProgram::Sample:return "SSAOSample";case NativeSSAOProgram::Blur:case NativeSSAOProgram::BlurComposite:return "SSAOBlur";case NativeSSAOProgram::TexRect:return "SSAOTexRect";case NativeSSAOProgram::Composite:return "SSAOComposite";}throw std::runtime_error("Unknown SSAO program");}
std::vector<unsigned>InputUnits(const NativeSSAODraw&d){const bool fuse=d.defines.empty()?false:d.defines.at("NOFUSE")==0,stencil=d.defines.empty()?false:d.defines.at("USE_STENCIL")==1;std::vector<unsigned>r;switch(d.program){case NativeSSAOProgram::Fuse:r={1,4};break;case NativeSSAOProgram::Sample:r={0};if(fuse)r.push_back(5);else{r.push_back(1);r.push_back(4);}break;case NativeSSAOProgram::Blur:r={0};break;case NativeSSAOProgram::BlurComposite:r={0,1,4};break;case NativeSSAOProgram::TexRect:r={0};break;case NativeSSAOProgram::Composite:r={0,1,4};if(fuse&&d.defines.at("DOWNSAMPLE")>1)r.push_back(5);break;}if(d.defines.contains("TILE_CULL")||(stencil&&d.program!=NativeSSAOProgram::Composite&&d.program!=NativeSSAOProgram::TexRect))r.push_back(7);return r;}
uint64_t BlendKey(const ScreenBlendState&b,const std::array<bool,4>&m){return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;}
MTLBlendOperation Equation(ScreenBlendEquation e){switch(e){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown SSAO blend equation");}
}
struct PreparedSSAO::Impl {
 struct Draw {uint64_t sequence;__strong id<MTL4ArgumentTable>arguments;__strong id<MTLRenderPipelineState>pipeline;__strong id<MTLDepthStencilState>depth;__strong id<MTLTexture>color,z,stencil;std::shared_ptr<ImageState>output,previous;std::vector<ResolvedSSAOImage>inputs;std::vector<std::shared_ptr<ImageState>>localInputs;NativeWorldDrawState world;effectstate::Raster raster;uint64_t submission=0;mutable bool encoded=false;bool noop=false,clear=false;__strong id<MTLBuffer>indices=nil;NSUInteger instanceCount=0;std::array<float,4>clearColor{};};
 std::vector<Draw>draws;std::shared_ptr<const NativeSSAOFrame>owner;
};
PreparedSSAO::PreparedSSAO():impl(std::make_unique<Impl>()){}PreparedSSAO::~PreparedSSAO()=default;size_t PreparedSSAO::DrawCount()const{return impl->draws.size();}uint64_t PreparedSSAO::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
void PreparedSSAO::EncodeDraw(size_t index,id<MTL4CommandBuffer>command)const{
 const auto&d=impl->draws.at(index);Require(command!=nil,"SSAO operation has no actual command buffer");Require(!d.encoded,"SSAO original operation was encoded twice");
 if(d.noop){
  if(d.previous)Require(d.previous->producer->completed||(d.previous->producer->encoded&&d.previous->producer->submissionIdentity==d.submission),"SSAO no-op previous contents have an outstanding original writer");
  if(d.output&&d.previous){auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot retain original SSAO no-coverage image contents");[copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[copy copyFromTexture:d.previous->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(d.color.width,d.color.height,1) toTexture:d.color destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];}
  d.encoded=true;if(d.output){d.output->producer->submissionIdentity=d.submission;d.output->producer->encoded=true;}return;
 }
 for(auto&i:d.inputs)i.requireReadable();for(auto&i:d.localInputs)Require(i->producer->completed||(i->producer->encoded&&i->producer->submissionIdentity==d.submission),"SSAO samples unencoded original FBO generation");
 if(d.output&&d.previous){Require(d.previous->producer->completed||(d.previous->producer->encoded&&d.previous->producer->submissionIdentity==d.submission),"SSAO partial update previous contents are not produced");auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot preserve actual SSAO partial image contents");[copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[copy copyFromTexture:d.previous->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(d.color.width,d.color.height,1) toTexture:d.color destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];}
 auto p=[MTL4RenderPassDescriptor new];p.defaultRasterSampleCount=d.color.sampleCount;p.renderTargetWidth=d.color.width;p.renderTargetHeight=d.color.height;p.colorAttachments[0].texture=d.color;p.colorAttachments[0].loadAction=d.output?(d.previous?MTLLoadActionLoad:MTLLoadActionDontCare):MTLLoadActionLoad;p.colorAttachments[0].storeAction=MTLStoreActionStore;
 if(d.z){p.depthAttachment.texture=d.z;p.depthAttachment.loadAction=MTLLoadActionLoad;p.depthAttachment.storeAction=MTLStoreActionStore;}
 if(d.stencil){p.stencilAttachment.texture=d.stencil;p.stencilAttachment.loadAction=MTLLoadActionLoad;p.stencilAttachment.storeAction=MTLStoreActionStore;}
 auto e=[command renderCommandEncoderWithDescriptor:p];Require(e!=nil,"Cannot encode actual SSAO render operation");[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];effectstate::RasterState(e,d.world,d.raster);[e setCullMode:d.world.cullEnabled?(d.world.cull==NativeWorldCull::Front?MTLCullModeFront:d.world.cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone):MTLCullModeNone];[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];if(d.indices){[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length instanceCount:d.instanceCount];}else{[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];}[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];d.encoded=true;if(d.output){d.output->producer->submissionIdentity=d.submission;d.output->producer->encoded=true;}
}
void PreparedSSAO::DidComplete()const{
 // Validate the whole frame before publishing any retirement. A late missing
 // operation must not leave earlier outputs marked completed during rollback.
 for(const auto&d:impl->draws){Require(d.encoded,"Unencoded original SSAO operation cannot retire");if(d.output)Require(d.output->producer->encoded&&d.output->producer->submissionIdentity==d.submission,"Unencoded or unrelated SSAO output cannot retire");}
 for(const auto&d:impl->draws)if(d.output)d.output->producer->completed=true;
 impl->owner->completed=true;
}
struct SSAOGPU::Impl {
 bool initialized=false;
 std::map<ImageKey,std::shared_ptr<ImageState>>images;
 struct ProgramKey {std::string definitions;unsigned program;MTLPixelFormat color,depth,stencil;uint64_t blend;bool clear,validation;unsigned samples;bool coverage;auto operator<=>(const ProgramKey&)const=default;};
 std::map<std::string,id<MTLLibrary>>libraries;std::map<ProgramKey,id<MTLRenderPipelineState>>pipelines;
 std::shared_ptr<ImageState>Image(RuntimeLease&l,std::shared_ptr<const NativeSSAOImageVersion>v){Require(v&&v->image&&v->generation,"Missing actual SSAO image version");if(v->Resource()){auto image=std::dynamic_pointer_cast<const ImageState>(v->Resource());Require(image&&image->texture.device==l.Device()&&image->owner.lock()==v,"Original SSAO image resource belongs to another device/version");return std::const_pointer_cast<ImageState>(image);}ImageKey k{v->image->identity,v->generation};if(auto i=images.find(k);i!=images.end()){Require(i->second->owner.lock()==v,"SSAO image identity/generation reused by unrelated owner");return i->second;}auto r=std::make_shared<ImageState>();r->owner=v;r->producer=v->producer;auto&im=*v->image;Require(im.width&&im.height&&im.width<=l.Capabilities().maxTextureDimension2D&&im.height<=l.Capabilities().maxTextureDimension2D,"SSAO original FBO exceeds actual device limits");auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:im.format==NativeSSAOFormat::RGBA8?MTLPixelFormatRGBA8Unorm:MTLPixelFormatRGBA16Float width:im.width height:im.height mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;r->texture=[l.Device()newTextureWithDescriptor:d];Require(r->texture!=nil,"Cannot allocate actual SSAO FBO texture");auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;s.minFilter=im.sampling.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.magFilter=im.sampling.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.mipFilter=MTLSamplerMipFilterNotMipmapped;s.sAddressMode=s.tAddressMode=MTLSamplerAddressModeClampToEdge;r->sampler=[l.Device()newSamplerStateWithDescriptor:s];Require(r->sampler!=nil,"Cannot allocate actual SSAO sampler");v->Fulfill(r);images.emplace(k,r);return r;}
 id<MTLRenderPipelineState>Pipeline(RuntimeLease&l,const NativeSSAODraw&draw,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool modelColor,bool mapColor,bool normalColor,bool validation,double&ms,unsigned samples){auto d=draw.defines.empty()?DefaultDefines():draw.defines;std::string defs=Defines(d,modelColor,mapColor,normalColor);ProgramKey key{defs,unsigned(draw.program),color,depth,stencil,BlendKey(draw.surface.blend,draw.world.colorWrite),draw.clear,validation,l.SampleShadingKey(samples),!draw.clear&&draw.world.alphaToCoverage};if(auto p=pipelines.find(key);p!=pipelines.end())return p->second;auto start=Clock::now();NSError*error=nil;id<MTLLibrary>library=nil;if(auto i=libraries.find(defs);i!=libraries.end())library=i->second;else{auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Fixed31657 complete SSAO program family";descriptor.source=String(defs+ssao::SHADER+"\nfragment float4 SSAOClear(constant SSAOUniform& u [[buffer(0)]]){return u.directionStrength;}\n");auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;library=l.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile exact original SSAO shaders: "+Error(error));libraries.emplace(defs,library);}
 auto p=[MTL4RenderPipelineDescriptor new];p.rasterSampleCount=samples;p.alphaToCoverageState=key.coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=@"SSAOVertex";f.name=draw.clear?@"SSAOClear":String(Entry(draw.program));p.vertexFunctionDescriptor=v;p.fragmentFunctionDescriptor=f;p.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=p.colorAttachments[0];a.pixelFormat=color;auto b=draw.surface.blend;if(draw.clear)b.enabled=false;a.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);auto m=draw.world.colorWrite;a.writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);p.options=options;auto pipeline=l.NewRenderPipeline(p,&error);Require(pipeline!=nil,"Cannot compile exact SSAO actual attachment pipeline: "+Error(error));pipelines.emplace(key,pipeline);ms+=Milliseconds(start);return pipeline;}
};
SSAOGPU::SSAOGPU():impl(std::make_unique<Impl>()){}SSAOGPU::~SSAOGPU()=default;
void SSAOGPU::Initialize(RuntimeLease&l,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double&ms,unsigned samples){
 NativeSSAODraw d;d.defines=DefaultDefines();
 for(auto p:{NativeSSAOProgram::Fuse,NativeSSAOProgram::Sample,NativeSSAOProgram::Blur,NativeSSAOProgram::TexRect,NativeSSAOProgram::Composite,NativeSSAOProgram::BlurComposite}){
  d.program=p;d.defines["BLUR_COMPOSITE"]=p==NativeSSAOProgram::BlurComposite?1:0;
  const bool screen=p==NativeSSAOProgram::Composite||p==NativeSSAOProgram::BlurComposite;
  impl->Pipeline(l,d,p==NativeSSAOProgram::Fuse?MTLPixelFormatRGBA16Float:screen?color:MTLPixelFormatRGBA8Unorm,screen?depth:MTLPixelFormatInvalid,screen?stencil:MTLPixelFormatInvalid,false,false,true,validation,ms,screen?samples:1);
 }
 d.clear=true;d.defines["BLUR_COMPOSITE"]=0;
 impl->Pipeline(l,d,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,true,validation,ms,1);impl->initialized=true;
}
bool SSAOGPU::Initialized()const noexcept{return impl->initialized;}
void SSAOGPU::ClearResources(){impl->images.clear();}
ResolvedSSAOImage SSAOGPU::Resolve(RuntimeLease&l,std::shared_ptr<const NativeSSAOImageVersion>v,uint64_t submission){auto im=impl->Image(l,std::move(v));l.AddAllocation(im->texture);l.Retain(im->sampler);return {im->texture,im->sampler,im,[im,submission]{Require(im->producer->completed||(submission&&im->producer->encoded&&im->producer->submissionIdentity==submission),"SSAO immutable output is not readable before its real producer");},true,im->sourceModelEpoch};}
std::unique_ptr<PreparedSSAO>SSAOGPU::Prepare(RuntimeLease&l,std::shared_ptr<const NativeSSAOFrame>frame,const WorldCamera&camera,uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture>sceneColor,id<MTLTexture>sceneDepth,id<MTLTexture>sceneStencil,const SSAOImageResolver&resolver,bool validation,double&ms,uint64_t submission){
 for(auto i=impl->images.begin();i!=impl->images.end();)if(i->second.use_count()==1&&i->second->owner.expired())i=impl->images.erase(i);else++i;
 if(!frame||frame->draws.empty())return{};Require(submission!=0,"SSAO requires the common actual submission identity");Require(sceneColor!=nil,"SSAO final composite needs actual scene color");Require(frame->publication&&frame->publication->matrices,"SSAO requires original uploaded camera UBO");auto result=std::make_unique<PreparedSSAO>();result->impl->owner=frame;uint64_t prior=0;
 for(auto&draw:frame->draws){ValidateNativeSSAODraw(draw);Require(draw.sequence>prior,"SSAO source operations out of order");prior=draw.sequence;PreparedSSAO::Impl::Draw d{};d.sequence=draw.sequence;d.submission=submission;d.world=draw.world;Require(d.world.frontPolygonMode==NativePolygonMode::Fill&&d.world.backPolygonMode==NativePolygonMode::Fill,"Original SSAO non-fill raster consumer is not yet connected");d.clear=draw.clear;d.clearColor=draw.clearColor;d.color=sceneColor;d.z=sceneDepth;d.stencil=sceneStencil;
  if(draw.output){d.output=impl->Image(l,draw.output);d.color=d.output->texture;d.z=d.stencil=nil;d.world.depthTest=d.world.depthWrite=d.world.stencilTest=false;if(draw.previous)d.previous=impl->Image(l,draw.previous);}
  // These inputs are original immutable named images. Only the final output
  // uses the main sample count; do not resolve or replace named map/model depth.
  Require(d.color.sampleCount && [l.Device() supportsTextureSampleCount:d.color.sampleCount],"SSAO output sample count is unsupported");
  for(auto attachment:{d.z,d.stencil})if(attachment)Require(attachment.device==d.color.device&&
   attachment.width==d.color.width&&attachment.height==d.color.height&&attachment.sampleCount==d.color.sampleCount,
   "SSAO output depth/stencil differs from the actual color storage");
  auto original=camera;if(draw.surface.viewport){auto&v=*draw.surface.viewport;original.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
  // RawBindFBO does NOT alter GL viewport. Private images use the same actual
  // window-coordinate viewport, clipped by their own attachment dimensions.
  auto scissor=draw.surface.scissor;if(scissor&&!scissor->windowCoordinates){scissor->x+=camera.viewport[0];scissor->y+=camera.viewport[1];scissor->windowCoordinates=true;}
  if(draw.clear)original.viewport={0,0,int32_t(d.color.width),int32_t(d.color.height)};
  auto raster=effectstate::Viewport(original,uint32_t(d.color.width),uint32_t(d.color.height),draw.output?uint32_t(d.color.width):logicalWidth,draw.output?uint32_t(d.color.height):logicalHeight,scissor);d.noop=!raster||(!draw.clear&&d.world.cullEnabled&&d.world.cull==NativeWorldCull::FrontAndBack);if(raster)d.raster=*raster;
  // A no-coverage Fuse copies the existing fused storage, so its model-depth
  // provenance follows that retained storage rather than the unsampled input.
  if(d.noop&&d.output&&d.previous)d.output->sourceModelEpoch=d.previous->sourceModelEpoch;
  // Source no-coverage/cull-both has no fragment samples or writes. Retain
  // actual prior storage, including its undefined-storage state; no clear is invented.
  // Storage follows the immutable version; never replace an image already
  // retained by a later generic UI argument table.
  if(draw.output&&draw.clear){d.raster.viewport={0,0,double(d.color.width),double(d.color.height),0,1};d.world.depthTest=d.world.depthWrite=d.world.stencilTest=d.world.cullEnabled=false;}
  Uniform u{};auto&bytes=frame->publication->matrices->bytes;std::memcpy(&u.view,bytes.data()+3*64,64);std::memcpy(&u.projection,bytes.data()+4*64,64);std::memcpy(&u.inverseProjection,bytes.data()+8*64,64);std::memcpy(&u.fuseInverseProjection,draw.uniforms.inverseProjection.data(),64);for(unsigned i=0;i<64;++i)u.kernel[i]=simd_make_float4(draw.uniforms.samplingKernel[i][0],draw.uniforms.samplingKernel[i][1],draw.uniforms.samplingKernel[i][2],0);for(unsigned i=0;i<12;++i){u.weights[i].x=draw.uniforms.weights[i];u.offsets[i].x=draw.uniforms.offsets[i];}u.directionStrength=draw.clear?simd_make_float4(draw.clearColor[0],draw.clearColor[1],draw.clearColor[2],draw.clearColor[3]):simd_make_float4(draw.uniforms.direction[0],draw.uniforms.direction[1],draw.uniforms.strength,0);
  // Convert Metal attachment coordinates back to the captured source window
  // coordinates for gl_FragCoord and exact texelFetch; private targets are 1:1.
  u.framebuffer=simd_make_float4(draw.output?1.0f:float(logicalWidth)/float(d.color.width),draw.output?1.0f:float(logicalHeight)/float(d.color.height),float(d.color.height),0);
  u.alphaConfig=simd_make_float4(draw.surface.alphaTest.enabled,draw.surface.alphaTest.reference,unsigned(draw.surface.alphaTest.compare),0);
  auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=3;desc.maxTextureBindCount=desc.maxSamplerStateBindCount=8;desc.initializeBindings=YES;NSError*error=nil;d.arguments=[l.Device()newArgumentTableWithDescriptor:desc error:&error];Require(d.arguments!=nil,"Cannot allocate SSAO original texture table: "+Error(error));bool modelColor=false,mapColor=false,normalColor=true;NativeDeferredPostprocessEpoch normalEpoch,modelEpoch;
  for(unsigned unit=0;unit<8;++unit){const auto&input=draw.inputs[unit];const auto&s=input.local?input.local->image->sampling:input.actual.object?input.actual.object->sampling:input.actual.defaultSampling;u.swizzle[unit]=simd_make_uint4(s.swizzle[0],s.swizzle[1],s.swizzle[2],s.swizzle[3]);u.depthMode[unit/4][unit%4]=s.depthMode;}
  if(!draw.clear&&!d.noop)for(unsigned unit:InputUnits(draw)){auto&input=draw.inputs[unit];ResolvedSSAOImage image;if(input.local){auto im=impl->Image(l,input.local);d.localInputs.push_back(im);image=Resolve(l,input.local,submission);if(unit==5){Require(input.local->image->format==NativeSSAOFormat::RGB16Float&&bool(im->sourceModelEpoch.actualProducer),"SSAO fused depth requires retained original Fuse storage/provenance");modelEpoch=im->sourceModelEpoch;}}else{Require(bool(resolver),"SSAO genuine GBuffer/stencil resolver is not installed");image=resolver(l,input.actual);}if(unit==0&&draw.program==NativeSSAOProgram::Sample){Require(!input.local,"SSAO normal0 cannot use private effect storage");ValidateModelColorInput(input.actual,image,0,submission);}
   if(unit==1||unit==4){Require(!input.local,"SSAO original model/map depth5 cannot use private effect storage");ValidateDepthInput(input.actual,image,unit==1,submission);}
   if(unit==5)Require(bool(input.local),"SSAO fused depth cannot use a scene/screen substitute");
   Require(image.texture&&image.sampler&&image.owner,"SSAO actual sampler owner/image missing");Require(bool(image.requireReadable),"SSAO sampled image lacks its actual producer readability check");Require(image.texture.textureType==MTLTextureType2D,"SSAO original sampler2D cannot silently sample an unresolved multisample image");if(unit==0&&draw.program==NativeSSAOProgram::Sample)normalEpoch=image.deferredEpoch;if(unit==1)modelEpoch=image.deferredEpoch;if(unit==0&&draw.program==NativeSSAOProgram::Sample)normalColor=!DepthFormat(image.texture.pixelFormat);if(unit==1)modelColor=!DepthFormat(image.texture.pixelFormat);if(unit==4){Require(bool(image.deferredEpoch.actualProducer),"SSAO map depth lacks independent actual producer provenance");mapColor=!DepthFormat(image.texture.pixelFormat);}if(image.flipV)u.flip[unit/4]|=1u<<(unit%4);[d.arguments setTexture:image.texture.gpuResourceID atIndex:unit];[d.arguments setSamplerState:image.sampler.gpuResourceID atIndex:unit];l.AddAllocation(image.texture);l.Retain(image.sampler);d.inputs.push_back(std::move(image));}
  if(!draw.clear&&!d.noop){
   if(draw.program==NativeSSAOProgram::Fuse){Require(bool(modelEpoch.actualProducer),"Original SSAO fuse lacks actual model-depth provenance");if(d.output)d.output->sourceModelEpoch=modelEpoch;}
   if(draw.program==NativeSSAOProgram::Sample){const std::array<NativeDeferredPostprocessEpoch,2> epochs{normalEpoch,modelEpoch};ValidateNativeDeferredModelEpochs(epochs);}
  }
  auto vertices=draw.clear?MakeNativeSSAOTexRect(-1,-1,1,1,0,0,1,1):draw.vertices;auto vb=Upload(l,vertices.data(),sizeof(vertices)),ub=Upload(l,&u,sizeof(u));[d.arguments setAddress:ub.gpuAddress atIndex:0];[d.arguments setAddress:vb.gpuAddress atIndex:1];
  if(draw.tiles){
   const auto&tiles=*draw.tiles;
   d.indices=Upload(l,tiles.indices.data(),sizeof(tiles.indices));d.instanceCount=tiles.instances.size();
   auto instances=Upload(l,tiles.instances.data(),tiles.instances.size()*sizeof(tiles.instances.front()));
   [d.arguments setAddress:instances.gpuAddress atIndex:2];
  }
  d.pipeline=impl->Pipeline(l,draw,d.color.pixelFormat,d.z?d.z.pixelFormat:MTLPixelFormatInvalid,d.stencil?d.stencil.pixelFormat:MTLPixelFormatInvalid,modelColor,mapColor,normalColor,validation,ms,unsigned(d.color.sampleCount));d.depth=effectstate::Depth(l,d.world,d.stencil?d.stencil.pixelFormat:MTLPixelFormatInvalid);l.Retain(d.arguments);l.Retain(d.pipeline);l.AddAllocation(d.color);if(d.z)l.AddAllocation(d.z);if(d.stencil)l.AddAllocation(d.stencil);if(d.previous)l.AddAllocation(d.previous->texture);result->impl->draws.push_back(std::move(d));
 }return result;
}
}
