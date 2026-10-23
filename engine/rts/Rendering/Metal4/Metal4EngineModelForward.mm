/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4EngineModelForward.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "Rendering/Backend/NativeLegacyMaterialData.h"
#include "Metal4CUSSource.h"
#include "Metal4NativeEffectState.h"
#include "Metal4EngineModelState.h"
#include "NativeEngineDeferredShaders.h"
#include "NativeEngineTextureABI.h"
#include "NativeEngineForwardShaders.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
namespace rendering::metal4 {
namespace {
using engineoriginal::Uniform;using engineoriginal::Vec;
bool Empty(const NativeCUSTextureBinding& b){return ui::NativeUITextureHistory::SamplingIncomplete(b);}
const NativeTextureSampling& Sampling(const NativeCUSTextureBinding& b){Require(b.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original forward texture history is unobserved");if(b.knowledge==NativeCUSBindingKnowledge::DefaultObject)return b.defaultSampling;Require(bool(b.object),"Original forward binding has no retained object");return b.object->sampling;}
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Original forward blend equation unsupported");}
}
struct PreparedEngineModelForward::Impl {
 struct Piece{EngineDeferredPieceBuffers geometry;__strong id<MTL4ArgumentTable> table=nil;};
 struct Draw{
  uint64_t sequence=0;std::optional<effectstate::Raster> raster;NativeWorldDrawState state;
  std::vector<Piece> pieces;std::array<CUSSourceImage,6> images;
  __strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
 };
 std::shared_ptr<const NativeEngineModelPhase> source;std::vector<Draw> draws;
 bool dependencies=false,finished=false,completed=false;size_t encoded=0;
};
PreparedEngineModelForward::PreparedEngineModelForward():impl(std::make_unique<Impl>()){}
PreparedEngineModelForward::~PreparedEngineModelForward()=default;
size_t PreparedEngineModelForward::DrawCount()const{return impl->draws.size();}
uint64_t PreparedEngineModelForward::SequenceAt(size_t i)const{Require(i<DrawCount(),"Original forward draw index exceeds source");return impl->draws[i].sequence;}
void PreparedEngineModelForward::EncodeDependencies(id<MTL4CommandBuffer> command){
 Require(command&&!impl->dependencies&&!impl->completed,"Original forward dependencies missing or repeated");std::set<const void*> seen;
 for(const auto& d:impl->draws)for(const auto& image:d.images)if(image.owner&&seen.insert(image.owner.get()).second&&image.encodeDependencies)image.encodeDependencies(command);impl->dependencies=true;
}
void PreparedEngineModelForward::EncodeOrderedDraw(size_t i,id<MTL4RenderCommandEncoder> encoder){
 Require(encoder&&impl->dependencies&&!impl->finished&&!impl->completed&&i==impl->encoded&&i<DrawCount(),"Original forward draw repeated/skipped or lacks dependencies");
 const auto& d=impl->draws[i];
 if(d.raster&&(!d.state.cullEnabled||d.state.cull!=NativeWorldCull::FrontAndBack)){
  effectstate::RasterState(encoder,d.state,*d.raster);[encoder setDepthStencilState:d.depth];[encoder setRenderPipelineState:d.pipeline];
  [encoder setCullMode:!d.state.cullEnabled||d.state.cull==NativeWorldCull::None?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
  [encoder setTriangleFillMode:d.state.frontPolygonMode==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
  for(const auto& image:d.images)if(image.requireReadable)image.requireReadable();
  for(const auto& piece:d.pieces){[encoder setArgumentTable:piece.table atStages:MTLRenderStageVertex|MTLRenderStageFragment];[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry.indices.gpuAddress indexBufferLength:piece.geometry.indices.length];}
 }
 ++impl->encoded;
}
void PreparedEngineModelForward::FinishOrderedDraws(){Require(impl->dependencies&&!impl->finished&&!impl->completed&&impl->encoded==DrawCount(),"Original forward source list did not finish once");impl->finished=true;}
void PreparedEngineModelForward::DidComplete(){Require(impl->finished&&!impl->completed,"Original forward retirement preceded its real submission");std::set<const void*> seen;for(const auto& d:impl->draws)for(const auto& image:d.images)if(image.owner&&seen.insert(image.owner.get()).second&&image.didComplete)image.didComplete();impl->completed=true;}
struct EngineModelForwardGPU::Impl {
 struct Original {
  bool modern=false,shadows=false;
  std::map<uint32_t,id<MTLLibrary>> libraries;
  // FBO selection does not create another original GL program or uniform
  // owner. Each entry is a real PSO of this owner's same source functions.
  std::map<std::string,id<MTLRenderPipelineState>> pipelines;
 };
 std::map<const void*,std::shared_ptr<Original>> originalPrograms;
 Original& RequireOriginal(const NativeEngineModelDraw& draw)const{
  const auto found=originalPrograms.find(draw.compiledOriginalProgram.get());Require(found!=originalPrograms.end(),"Original forward draw lacks this actual compiled program owner");const auto& original=*found->second;
  Require(original.modern==(draw.shader==NativeEngineModelShader::GL4)&&original.shadows==draw.program.shadows&&original.libraries.contains(0)&&original.libraries.at(0),"Original forward actual program/source profile differs from source");
  return *found->second;
 }

 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,Original& original,const CUSSourceTarget& target,const NativeEngineModelDraw& source,bool validation,double& ms,uint32_t imageKey=0){
  const auto& surface=source.sourceSurface;const auto& blend=surface.blend;const bool modern=source.shader==NativeEngineModelShader::GL4;
  Require(original.modern==modern&&original.shadows==source.program.shadows,"Original forward target implementation has a foreign source profile");
  Require(target.colors[0]!=MTLPixelFormatInvalid&&target.depth==MTLPixelFormatDepth32Float&&(target.samples>=1&&target.samples<=32&&[lease.Device() supportsTextureSampleCount:target.samples])&&std::all_of(target.colors.begin()+1,target.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;}),"Original forward target implementation requires an actual one-color/D32F target");
  std::ostringstream k;k<<modern<<':'<<validation<<':'<<imageKey<<':'<<target.colors[0]<<':'<<target.depth<<':'<<target.stencil<<':'<<lease.SampleShadingKey(target.samples)<<':'<<blend.enabled<<':'<<unsigned(blend.sourceColor)<<':'<<unsigned(blend.destinationColor)<<':'<<unsigned(blend.sourceAlpha)<<':'<<unsigned(blend.destinationAlpha)<<':'<<unsigned(blend.colorEquation)<<':'<<unsigned(blend.alphaEquation);for(bool write:surface.colorWrite)k<<':'<<write;
  k<<':'<<surface.alphaToCoverage;const auto key=k.str();if(auto i=original.pipelines.find(key);i!=original.pipelines.end()){lease.Retain(i->second);lease.Retain(original.libraries.at(imageKey));return i->second;}
  const auto start=Clock::now();NSError* error=nil;
  auto& library=original.libraries[imageKey];
  if(!library){auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;library=lease.NewLibraryWithSource(String(NativeEngineTextureShaderDefines(imageKey)+NativeEngineDeferredShaders+NativeEngineForwardShaders),options,&error);Require(library,"Cannot compile full original engine forward shader: "+Error(error));}
  auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=@"NativeEngineDeferredVertex";f.name=modern?@"NativeEngineForwardGL4":@"NativeEngineForwardLegacy";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
  d.rasterSampleCount=target.samples;d.alphaToCoverageState=surface.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
  auto a=d.colorAttachments[0];a.pixelFormat=target.colors[0];a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
  a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);
  unsigned mask=0;for(unsigned c=0;c<4;++c)if(surface.colorWrite[c])mask|=MTLColorWriteMaskRed>>c;a.writeMask=MTLColorWriteMask(mask);
  auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
  auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline,"Cannot compile full original engine forward pipeline: "+Error(error));original.pipelines.emplace(key,pipeline);lease.Retain(pipeline);lease.Retain(library);ms+=Milliseconds(start);return pipeline;
 }
};
EngineModelForwardGPU::EngineModelForwardGPU():impl(std::make_unique<Impl>()){}
EngineModelForwardGPU::~EngineModelForwardGPU()=default;
std::shared_ptr<const void> EngineModelForwardGPU::CompileOriginalProgram(RuntimeLease& lease,bool modern,bool shadows,const CUSSourceTarget& target,bool validation,double& ms){
 Require(target.colors[0]!=MTLPixelFormatInvalid&&target.depth==MTLPixelFormatDepth32Float&&(target.samples>=1&&target.samples<=32&&[lease.Device() supportsTextureSampleCount:target.samples])&&std::all_of(target.colors.begin()+1,target.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;}),"Original forward construction requires its actual one-color/D32F target");
 NativeEngineModelDraw profile;profile.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;profile.program.shadows=shadows;profile.sourceSurface.blend.enabled=false;
 for(const auto& [key,old]:impl->originalPrograms)if(old->modern==modern&&old->shadows==shadows){impl->Pipeline(lease,*old,target,profile,validation,ms);return old;}
 auto token=std::make_shared<Impl::Original>();token->modern=modern;token->shadows=shadows;impl->Pipeline(lease,*token,target,profile,validation,ms);impl->originalPrograms.emplace(token.get(),token);return token;
}
std::shared_ptr<PreparedEngineModelForward> EngineModelForwardGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeEngineModelPhase> source,
 const CUSSourceTarget& target,const EngineDeferredGeometryProvider& geometry,const CUSSourceImageProvider& images,bool validation,double& ms){
 Require(source&&source->engineObserved&&source->legacyObserved&&geometry&&images,"Original forward source or common resource owners are missing");
 Require(source->legacyExecution&&source->legacyExecution->Matches(*source,false),"Original legacy Lua material bin still requires its actual native shader/state consumer");
 Require(source->viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original forward clip convention is unobserved");
 Require(target.width&&target.height&&target.logicalWidth&&target.logicalHeight&&(target.samples>=1&&target.samples<=32&&[lease.Device() supportsTextureSampleCount:target.samples])&&target.colors[0]!=MTLPixelFormatInvalid&&target.depth==MTLPixelFormatDepth32Float,"Original forward requires its real one-color/D32F target");
 Require(std::all_of(target.colors.begin()+1,target.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;}),"Original forward target accidentally aliases MRT attachments");
 auto out=std::shared_ptr<PreparedEngineModelForward>(new PreparedEngineModelForward);auto& p=*out->impl;p.source=std::move(source);
 std::map<const WorldModel*,std::vector<EngineDeferredPieceBuffers>> sharedGeometry;
 std::map<const NativeModelLightFrame*,id<MTLBuffer>> sharedLights;__strong id<MTLBuffer> modernLights=nil;
 for(const auto* list:{&p.source->beforeLua,&p.source->afterLua})for(const auto& sd:*list){
  ValidateNativeEngineModelDraw(sd);auto& original=impl->RequireOriginal(sd);const auto& object=*sd.object;const bool modern=sd.shader==NativeEngineModelShader::GL4;
  Require(sd.raster.observed&&sd.raster.viewport,"Original forward source raster is missing");const auto& viewport=*sd.raster.viewport;
  Require(viewport.width<=uint32_t(INT32_MAX)&&viewport.height<=uint32_t(INT32_MAX),"Original forward viewport exceeds GLsizei");
  PreparedEngineModelForward::Impl::Draw draw;draw.sequence=sd.sequence;draw.state=sd.sourceWorld;
  auto camera=target.camera;camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
  if(viewport.width&&viewport.height)draw.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,sd.raster.scissor);
  if(!draw.raster){p.draws.push_back(std::move(draw));continue;}
  draw.depth=effectstate::Depth(lease,draw.state,target.stencil);
  Uniform common=engineoriginal::DrawUniform(*p.source,sd);common.flags.x|=uint32_t(target.invertClipY)<<1;
  for(unsigned slot=0;slot<6;++slot){
   // GL4's actual fragment has no specular-cube sampler; disabled shadows
   // likewise do not observe/read a shadow source.
   if((slot==5&&modern)||((slot==2||slot==3)&&!sd.program.shadows))continue;
   const auto& binding=slot<2?sd.textures[slot]:sd.lightingTextures[slot-2];const auto& sampling=Sampling(binding);
   if(slot<2){auto& meta=slot?common.image1:common.image0;auto& swizzle=slot?common.swizzle1:common.swizzle0;meta.z=sampling.depthMode;swizzle=Vec(sampling.swizzle);common.samplerParameters[slot]=sampling.lodBias;}
   if(slot==3){common.lightImage.z=sampling.depthMode;common.lightSwizzle=Vec(sampling.swizzle);common.lightSampler.x=sampling.lodBias;}
   if(slot>=4){Require(sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original cube callback swizzle requires its actual cube-sampling consumer");common.lightSampler[slot-3]=sampling.lodBias;}
   if(Empty(binding))continue;
   auto image=images(lease,binding,slot);Require(image.texture&&image.sampler&&image.owner&&image.requireReadable,"Original forward image lost its genuine owner/sampler/producer");
   if(slot>=4)Require(image.texture.textureType==MTLTextureTypeCube&&!image.depthImage&&!image.flipV,"Original forward cube owner has incompatible actual storage");
   else Require(image.texture.textureType==MTLTextureType2D,"Original forward sampler2D has another actual target");
   if(slot==2){Require(image.depthImage&&sampling.compareMode==0x884e&&sampling.lodBias==0,"Original shadow comparison requires its actual supported depth sampler");common.flags.w|=1;if(image.clampDepthCompare)common.flags.w|=16;common.lightSampler.w=image.flipV;}
   else if(slot>=4)common.flags.w|=1u<<(slot-2);
   else{Require(!image.depthImage||sampling.compareMode==0,"Original forward non-shadow texture cannot substitute comparison sampling");auto& meta=slot==0?common.image0:slot==1?common.image1:common.lightImage;meta.x=image.depthImage?2:1;meta.y=image.flipV;}
   lease.AddAllocation(image.texture);lease.Retain(image.sampler);draw.images[slot]=std::move(image);
  }
  uint32_t imageKey=0;
  for(unsigned slot=0;slot<draw.images.size();++slot){const auto& image=draw.images[slot];
   const uint32_t kind=!image.texture?0u:slot>=4?3u:image.depthImage?2u:1u;imageKey|=kind<<(slot*2);}
  draw.pipeline=impl->Pipeline(lease,original,target,sd,validation,ms,imageKey);
  id<MTLBuffer> lights=nil;
  if(!modern){
   Require(sd.program.lights&&sd.program.lights->sourceDrawFrame==p.source->sourceDrawFrame,"Original legacy Enable has no real derived-light publication");
   auto [light,inserted]=sharedLights.try_emplace(sd.program.lights.get());
   if(inserted){const auto bytes=PackNativeModelLightFrame(*sd.program.lights);light->second=[lease.Device()newBufferWithBytes:bytes.data() length:bytes.size() options:MTLResourceStorageModeShared];Require(light->second,"Cannot retain original derived model lights");lease.AddAllocation(light->second);}lights=light->second;
  }else{
   if(!modernLights){const std::array<uint32_t,4> noLoop{};modernLights=[lease.Device()newBufferWithBytes:noLoop.data() length:sizeof(noLoop) options:MTLResourceStorageModeShared];Require(modernLights,"Cannot allocate original GL4 no-light-loop header");lease.AddAllocation(modernLights);}lights=modernLights;
  }
  auto [it,inserted]=sharedGeometry.try_emplace(object.model.get());if(inserted)it->second=geometry(lease,object.model);Require(it->second.size()==object.pieces.size(),"Original forward shared piece slots differ");
  for(size_t piece=0;piece<object.pieces.size();++piece){const auto& mesh=it->second[piece];if((!modern&&!object.pieces[piece].visible)||!mesh.indexCount)continue;
   Require(mesh.vertices&&mesh.indices&&mesh.indices.length>=size_t(mesh.indexCount)*4,"Original forward shared geometry is incomplete");lease.AddAllocation(mesh.vertices);lease.AddAllocation(mesh.indices);
   Uniform u=common;engineoriginal::PieceUniform(u,sd,piece);auto uniforms=[lease.Device()newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];Require(uniforms,"Cannot retain actual original forward uniforms");lease.AddAllocation(uniforms);
   auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;descriptor.maxTextureBindCount=9;descriptor.maxSamplerStateBindCount=6;descriptor.initializeBindings=YES;
   auto table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:nil];Require(table,"Cannot allocate original forward argument table");lease.Retain(table);[table setAddress:mesh.vertices.gpuAddress atIndex:0];[table setAddress:uniforms.gpuAddress atIndex:1];[table setAddress:lights.gpuAddress atIndex:2];
   for(unsigned slot=0;slot<6;++slot){const auto& image=draw.images[slot];if(!image.texture)continue;unsigned texture=slot<2?(image.depthImage?slot+2:slot):slot==2?4:slot==3?(image.depthImage?6:5):slot+3;[table setTexture:image.texture.gpuResourceID atIndex:texture];[table setSamplerState:image.sampler.gpuResourceID atIndex:slot];}
   draw.pieces.push_back({mesh,table});
  }
  p.draws.push_back(std::move(draw));
 }
 return out;
}
} // namespace rendering::metal4
