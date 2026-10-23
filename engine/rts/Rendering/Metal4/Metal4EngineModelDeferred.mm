/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4EngineModelDeferred.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "Rendering/Backend/NativeLegacyMaterialData.h"
#include "Metal4CUSSource.h"
#include "Metal4NativeEffectState.h"
#include "NativeEngineDeferredShaders.h"
#include "NativeEngineTextureABI.h"
#include "Metal4EngineModelState.h"
#import <simd/simd.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace rendering::metal4 {
namespace {
using engineoriginal::Uniform;using engineoriginal::Matrix;using engineoriginal::Vec;
static_assert(sizeof(WorldModelVertex)==76);
bool Empty(const NativeCUSTextureBinding& b){return ui::NativeUITextureHistory::SamplingIncomplete(b);}
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Original engine MRT blend equation unsupported");}
const NativeTextureSampling& Sampling(const NativeCUSTextureBinding& b){Require(b.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original engine MRT texture history is unobserved");if(b.knowledge==NativeCUSBindingKnowledge::DefaultObject)return b.defaultSampling;Require(bool(b.object),"Original engine MRT binding lacks actual object");return b.object->sampling;}
}
struct PreparedEngineModelDeferred::Impl {
 struct Piece {EngineDeferredPieceBuffers geometry;__strong id<MTL4ArgumentTable> table=nil;};
 struct Draw {NativeWorldDrawState state;__strong id<MTLDepthStencilState> sourceDepth=nil;uint64_t sequence=0;std::optional<effectstate::Raster> raster;std::vector<Piece> pieces;std::array<CUSSourceImage,2> images;__strong id<MTLRenderPipelineState> pipeline=nil;bool wire=false,cull=true;float bias=0,slope=0;};
 std::shared_ptr<const NativeEngineModelPhase> source;
 std::vector<Draw> before,after;
 __strong id<MTLDepthStencilState> depth=nil;
 bool dependencies=false,beforeEncoded=false,afterEncoded=false,completed=false;
 bool ordered=false;size_t orderedEncoded=0;
 const Draw& At(size_t index)const{Require(index<before.size()+after.size(),"Engine MRT draw index exceeds actual source");return index<before.size()?before[index]:after[index-before.size()];}
 void EncodeDraw(id<MTL4RenderCommandEncoder> e,const Draw& draw) {
  Require(e&&dependencies&&!completed,"Engine MRT source lacks real dependencies");
  if(!draw.raster||(draw.state.cullEnabled&&draw.state.cull==NativeWorldCull::FrontAndBack))return; // Actual captured zero viewport/scissor produces no samples.
  effectstate::RasterState(e,draw.state,*draw.raster);
  [e setDepthStencilState:draw.sourceDepth];[e setCullMode:MTLCullModeBack];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setDepthClipMode:MTLDepthClipModeClip];
   for(const auto& image:draw.images)if(image.requireReadable)image.requireReadable();
   [e setCullMode:!draw.state.cullEnabled||draw.state.cull==NativeWorldCull::None?MTLCullModeNone:draw.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];[e setRenderPipelineState:draw.pipeline];[e setTriangleFillMode:draw.wire?MTLTriangleFillModeLines:MTLTriangleFillModeFill];[e setDepthBias:draw.bias slopeScale:draw.slope clamp:0];
   for(const auto& piece:draw.pieces){[e setArgumentTable:piece.table atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry.indices.gpuAddress indexBufferLength:piece.geometry.indices.length];}
 }
 void Encode(id<MTL4RenderCommandEncoder> e,const std::vector<Draw>& draws){Require(e&&dependencies&&!completed,"Engine MRT source lacks real dependencies");if(draws.empty()){[e setDepthStencilState:depth];[e setCullMode:MTLCullModeBack];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setDepthClipMode:MTLDepthClipModeClip];}for(const auto& draw:draws)EncodeDraw(e,draw);}
};
PreparedEngineModelDeferred::PreparedEngineModelDeferred():impl(std::make_unique<Impl>()){}
PreparedEngineModelDeferred::~PreparedEngineModelDeferred()=default;
void PreparedEngineModelDeferred::EncodeDependencies(id<MTL4CommandBuffer> command){Require(command&&!impl->dependencies,"Engine MRT dependencies missing or repeated");std::set<const void*> seen;for(const auto* list:{&impl->before,&impl->after})for(const auto& draw:*list)for(const auto& image:draw.images)if(image.owner&&seen.insert(image.owner.get()).second&&image.encodeDependencies)image.encodeDependencies(command);impl->dependencies=true;}
void PreparedEngineModelDeferred::EncodeBeforeLua(id<MTL4RenderCommandEncoder> e){Require(!impl->ordered&&!impl->beforeEncoded,"Engine MRT before-Lua list repeated or mixed with ordered draws");impl->Encode(e,impl->before);impl->beforeEncoded=true;}
void PreparedEngineModelDeferred::EncodeAfterLua(id<MTL4RenderCommandEncoder> e){Require(!impl->ordered&&impl->beforeEncoded&&!impl->afterEncoded,"Engine MRT after-Lua order differs");impl->Encode(e,impl->after);impl->afterEncoded=true;}
size_t PreparedEngineModelDeferred::DrawCount()const{return impl->before.size()+impl->after.size();}
uint64_t PreparedEngineModelDeferred::SequenceAt(size_t index)const{return impl->At(index).sequence;}
void PreparedEngineModelDeferred::EncodeOrderedDraw(size_t index,id<MTL4RenderCommandEncoder> e){Require(!impl->beforeEncoded&&!impl->afterEncoded&&index==impl->orderedEncoded,"Engine MRT ordered draw is repeated, skipped or mixed with grouped lists");const auto& draw=impl->At(index);Require(draw.sequence,"Engine MRT ordered draw lacks its real source sequence");impl->ordered=true;impl->EncodeDraw(e,draw);++impl->orderedEncoded;}
void PreparedEngineModelDeferred::FinishOrderedDraws(){Require(impl->dependencies&&!impl->beforeEncoded&&!impl->afterEncoded&&!impl->completed&&impl->orderedEncoded==DrawCount(),"Engine MRT ordered source list did not finish exactly once");impl->ordered=true;impl->beforeEncoded=impl->afterEncoded=true;}
void PreparedEngineModelDeferred::DidComplete(){Require(impl->beforeEncoded&&impl->afterEncoded&&!impl->completed,"Engine MRT retirement occurred before actual source pass");std::set<const void*> seen;for(const auto* list:{&impl->before,&impl->after})for(const auto& draw:*list)for(const auto& image:draw.images)if(image.owner&&seen.insert(image.owner.get()).second&&image.didComplete)image.didComplete();impl->completed=true;}
struct EngineModelDeferredGPU::Impl {
 struct Original {bool modern=false,shadows=false;CUSSourceTarget target;__strong id<MTLRenderPipelineState> pipeline=nil;};
 std::map<const void*,std::shared_ptr<const Original>> originalPrograms;
 void RequireOriginal(const NativeEngineModelDraw& draw,const CUSSourceTarget& target)const{
  const auto found=originalPrograms.find(draw.compiledOriginalProgram.get());
  Require(found!=originalPrograms.end(),"Engine MRT draw did not retain this actual compiled program owner");const auto& original=*found->second;
  Require(original.modern==(draw.shader==NativeEngineModelShader::GL4)&&original.shadows==draw.program.shadows&&original.target.colors==target.colors&&original.target.depth==target.depth&&original.target.stencil==target.stencil&&original.target.samples==target.samples,"Engine MRT actual program/target profile differs from source");
 }

 std::map<std::string,id<MTLRenderPipelineState>> pipelines;
 std::map<uint32_t,id<MTLLibrary>> libraries;__strong id<MTLDepthStencilState> depth=nil;
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,bool modern,bool validation,double& ms,const NativeEngineModelDraw* source=nullptr,uint32_t imageKey=0){
  ScreenDrawState initial;initial.blend.enabled=false;const auto& surface=source?source->sourceSurface:initial;const auto& blend=surface.blend;
  std::ostringstream keyStream;keyStream<<modern<<':'<<validation<<':'<<imageKey<<':'<<blend.enabled<<':'<<unsigned(blend.sourceColor)<<':'<<unsigned(blend.destinationColor)<<':'<<unsigned(blend.sourceAlpha)<<':'<<unsigned(blend.destinationAlpha)<<':'<<unsigned(blend.colorEquation)<<':'<<unsigned(blend.alphaEquation);for(bool write:surface.colorWrite)keyStream<<':'<<write;const auto key=keyStream.str();auto it=pipelines.find(key);if(it!=pipelines.end()){lease.Retain(it->second);lease.Retain(libraries.at(imageKey));return it->second;}
  const auto start=Clock::now();NSError* error=nil;
  auto& library=libraries[imageKey];
  if(!library){auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;library=lease.NewLibraryWithSource(String(NativeEngineTextureShaderDefines(imageKey)+NativeEngineDeferredShaders),options,&error);Require(library,"Cannot compile actual engine five-output shader: "+Error(error));}
  auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=@"NativeEngineDeferredVertex";f.name=modern?@"NativeEngineDeferredGL4":@"NativeEngineDeferredLegacy";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
  for(unsigned i=0;i<5;++i){auto a=d.colorAttachments[i];a.pixelFormat=MTLPixelFormatRGBA8Unorm;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
   a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);
   unsigned mask=0;for(unsigned c=0;c<4;++c)if(surface.colorWrite[c])mask|=MTLColorWriteMaskRed>>c;a.writeMask=MTLColorWriteMask(mask);
  }
  auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
  auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline,"Cannot compile actual engine five-output pipeline: "+Error(error));pipelines.emplace(key,pipeline);lease.Retain(pipeline);lease.Retain(library);ms+=Milliseconds(start);return pipeline;
 }
};
EngineModelDeferredGPU::EngineModelDeferredGPU():impl(std::make_unique<Impl>()){}
EngineModelDeferredGPU::~EngineModelDeferredGPU()=default;
std::shared_ptr<const void> EngineModelDeferredGPU::CompileOriginalProgram(RuntimeLease& lease,bool modern,bool shadows,const CUSSourceTarget& target,bool validation,double& ms){
 Require(target.samples==1&&target.depth==MTLPixelFormatDepth32Float&&target.stencil==MTLPixelFormatInvalid&&std::all_of(target.colors.begin(),target.colors.end(),[](auto f){return f==MTLPixelFormatRGBA8Unorm;}),"Original engine MRT construction requires its actual five-color/D32F target");
 for(const auto& [key,old]:impl->originalPrograms)if(old->modern==modern&&old->shadows==shadows&&old->target.colors==target.colors&&old->target.depth==target.depth&&old->target.stencil==target.stencil&&old->target.samples==target.samples)return old;
 auto token=std::make_shared<Impl::Original>();token->modern=modern;token->shadows=shadows;token->target=target;token->pipeline=impl->Pipeline(lease,modern,validation,ms);impl->originalPrograms.emplace(token.get(),token);return token;
}
std::shared_ptr<PreparedEngineModelDeferred> EngineModelDeferredGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeEngineModelPhase> source,
 const CUSSourceTarget& actualTarget,const EngineDeferredGeometryProvider& geometry,const CUSSourceImageProvider& images,bool validation,double& ms)
{
 Require(source&&source->engineObserved&&source->legacyObserved&&geometry&&images,"Engine MRT requires actual before/after source observations and shared resource owners");
 // Never accept an absent arbitrary Lua-material implementation as no geometry.
 Require(source->legacyExecution&&source->legacyExecution->Matches(*source,true),"Original legacy Lua material bin still requires its actual native shader/state consumer");
 Require(source->viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Engine MRT original clip-depth convention is unobserved");
 Require(actualTarget.width&&actualTarget.height&&actualTarget.logicalWidth&&actualTarget.logicalHeight&&actualTarget.samples==1&&actualTarget.depth==MTLPixelFormatDepth32Float&&actualTarget.stencil==MTLPixelFormatInvalid&&std::all_of(actualTarget.colors.begin(),actualTarget.colors.end(),[](auto f){return f==MTLPixelFormatRGBA8Unorm;}),"Engine MRT requires its actual single-sample five-color/D32F target");
 auto out=std::shared_ptr<PreparedEngineModelDeferred>(new PreparedEngineModelDeferred);auto& p=*out->impl;p.source=std::move(source);
 if(!impl->depth){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionLessEqual;d.depthWriteEnabled=YES;impl->depth=[lease.Device()newDepthStencilStateWithDescriptor:d];Require(impl->depth,"Cannot create actual engine MRT depth state");}p.depth=impl->depth;lease.Retain(p.depth);
 std::map<const WorldModel*,std::vector<EngineDeferredPieceBuffers>> sharedGeometry;
 auto prepareList=[&](const auto& list,auto& target){for(const auto& sourceDraw:list){
  ValidateNativeEngineModelDraw(sourceDraw);impl->RequireOriginal(sourceDraw,actualTarget);const auto& object=*sourceDraw.object;const bool modern=sourceDraw.shader==NativeEngineModelShader::GL4;
  Require(sourceDraw.raster.observed&&sourceDraw.raster.viewport,"Engine MRT draw lacks its actual source raster snapshot");
  const auto& viewport=*sourceDraw.raster.viewport;
  Require(viewport.width<=uint32_t(std::numeric_limits<int32_t>::max())&&viewport.height<=uint32_t(std::numeric_limits<int32_t>::max()),"Engine MRT captured viewport exceeds original GLsizei range");
  auto camera=actualTarget.camera;camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
  PreparedEngineModelDeferred::Impl::Draw draw;draw.sequence=sourceDraw.sequence;
  if(viewport.width&&viewport.height)draw.raster=effectstate::Viewport(camera,actualTarget.width,actualTarget.height,actualTarget.logicalWidth,actualTarget.logicalHeight,sourceDraw.raster.scissor);
  if(!draw.raster){target.push_back(std::move(draw));continue;}
  draw.state=sourceDraw.sourceWorld;draw.sourceDepth=effectstate::Depth(lease,draw.state,actualTarget.stencil);draw.wire=sourceDraw.wireframe;draw.cull=object.modelType!=0;draw.bias=sourceDraw.polygonOffset?sourceDraw.polygonOffsetUnits:0;draw.slope=sourceDraw.polygonOffset?sourceDraw.polygonOffsetFactor:0;
  Uniform common=engineoriginal::DrawUniform(*p.source,sourceDraw);common.flags.x|=uint32_t(actualTarget.invertClipY)<<1;
  for(unsigned slot=0;slot<2;++slot){const auto& binding=sourceDraw.textures[slot];const auto& sampler=Sampling(binding);auto& meta=slot?common.image1:common.image0;auto& swizzle=slot?common.swizzle1:common.swizzle0;swizzle=Vec(sampler.swizzle);meta.z=sampler.depthMode;common.samplerParameters[slot]=sampler.lodBias;
   if(Empty(binding))continue;
   auto image=images(lease,binding,slot);Require(image.texture&&image.sampler&&image.owner&&image.requireReadable,"Engine MRT actual image lost its owner/sampler/producer");Require(image.texture.textureType==MTLTextureType2D,"Engine MRT original sampler2D received another target");Require(!image.depthImage||sampler.compareMode==0,"Engine MRT sampler2D cannot substitute shadow comparison");meta.x=image.depthImage?2:1;meta.y=image.flipV;lease.AddAllocation(image.texture);lease.Retain(image.sampler);draw.images[slot]=std::move(image);
  }
  const uint32_t imageKey=common.image0.x|(common.image1.x<<2);
  draw.pipeline=impl->Pipeline(lease,modern,validation,ms,&sourceDraw,imageKey);
  auto [it,inserted]=sharedGeometry.try_emplace(object.model.get());if(inserted)it->second=geometry(lease,object.model);Require(it->second.size()==object.pieces.size(),"Engine MRT shared model piece slots differ");
  for(size_t i=0;i<object.pieces.size();++i){const auto& mesh=it->second[i];if((!modern&&!object.pieces[i].visible)||!mesh.indexCount)continue;Require(mesh.vertices&&mesh.indices&&mesh.indices.length>=size_t(mesh.indexCount)*4,"Engine MRT shared geometry is incomplete");lease.AddAllocation(mesh.vertices);lease.AddAllocation(mesh.indices);
   Uniform u=common;engineoriginal::PieceUniform(u,sourceDraw,i);
   auto buffer=[lease.Device()newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];Require(buffer,"Cannot allocate actual engine MRT uniforms");lease.AddAllocation(buffer);
   auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=4;descriptor.maxSamplerStateBindCount=2;descriptor.initializeBindings=YES;
   auto table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:nil];Require(table,"Cannot allocate actual engine MRT arguments");lease.Retain(table);[table setAddress:mesh.vertices.gpuAddress atIndex:0];[table setAddress:buffer.gpuAddress atIndex:1];
   for(unsigned slot=0;slot<2;++slot){const auto& image=draw.images[slot];if(!image.texture)continue;[table setTexture:image.texture.gpuResourceID atIndex:image.depthImage?slot+2:slot];[table setSamplerState:image.sampler.gpuResourceID atIndex:slot];}
   draw.pieces.push_back({mesh,table});
  }
  target.push_back(std::move(draw));
 }};
 prepareList(p.source->beforeLua,p.before);prepareList(p.source->afterLua,p.after);return out;
}
} // namespace rendering::metal4
