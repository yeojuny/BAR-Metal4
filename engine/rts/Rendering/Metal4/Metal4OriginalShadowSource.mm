/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4OriginalShadowSource.h"
#include <algorithm>
#include <exception>
namespace rendering::metal4 {
CUSSourceTarget OriginalShadowSourceTarget(const NativeShadowTextureBacking& depth,const NativeShadowTextureBacking& color,const WorldCamera& drawing){
 Require(CheckNativeShadowFramebuffer(depth,color),"Original shadow source target is not its actual named attachment pair");
 auto d=NativeShadowTextureImage(depth),c=NativeShadowTextureImage(color);
 const auto storage=QueryNativeShadowDepthStorage(depth);
 Require(storage.actualMetalPixelFormat==d.pixelFormat,"Original shadow target differs from queried physical storage");
 CUSSourceTarget out;out.colors.fill(MTLPixelFormatInvalid);out.colors[0]=c.pixelFormat;out.depth=d.pixelFormat;out.samples=d.sampleCount;out.width=out.height=out.logicalWidth=out.logicalHeight=depth.request.size;out.camera=drawing;out.invertClipY=false;
 // Run95 Zink units conversion is local to this actual shadow destination.
 out.shadowPolygonOffsetUnitsScale=2.f;return out;
}
struct PreparedOriginalShadowSource::Impl{
 struct Draw{uint64_t sequence;size_t index;int family;};
 std::shared_ptr<const NativeOriginalShadowFrame> source;CUSSourceTarget target;
 std::unique_ptr<PreparedCUSSource> cus;std::array<std::shared_ptr<PreparedEngineShadow>,3> models;
 std::shared_ptr<PreparedOriginalShadowParticles> particles;
 std::shared_ptr<PreparedGenericShadow> generic;
 std::shared_ptr<PreparedEngineShadow> terrain;
 std::vector<Draw> draws;uint64_t submission=0;size_t next=0;
 __strong id<MTLTexture> depth=nil,color=nil;__strong id<MTL4CommandBuffer> command=nil;
 bool dependencies=false,encoded=false,completed=false;
};
PreparedOriginalShadowSource::PreparedOriginalShadowSource():impl(std::make_unique<Impl>()){}
PreparedOriginalShadowSource::~PreparedOriginalShadowSource()=default;
id<MTLTexture> PreparedOriginalShadowSource::Depth()const{return impl->depth;}
id<MTLTexture> PreparedOriginalShadowSource::Color()const{return impl->color;}
const CUSSourceTarget& PreparedOriginalShadowSource::Target()const{return impl->target;}
uint64_t PreparedOriginalShadowSource::BeginSequence()const{return impl->source->beginSequence;}
uint64_t PreparedOriginalShadowSource::EndSequence()const{return impl->source->endSequence;}
size_t PreparedOriginalShadowSource::DrawCount()const{return impl->draws.size();}
uint64_t PreparedOriginalShadowSource::SequenceAt(size_t i)const{Require(i<DrawCount(),"Original shadow source draw index is outside retained source");return impl->draws[i].sequence;}
void PreparedOriginalShadowSource::EncodeDependencies(id<MTL4CommandBuffer> command){
 Require(command&&!impl->dependencies&&!impl->encoded,"Original shadow dependencies are missing/repeated");impl->command=command;
 const auto& retained=impl->source->textures->retainedColor;
 if(retained){if(retained->operation==NativeShadowTextureOperation::InitialClear)EncodeNativeShadowInitialClear(retained,command,impl->submission);
  Require(retained->completed.load()||retained->encodedSubmission.load()==impl->submission,"Original dark-sun retained color has no actual completed/same-submit producer");}
 if(impl->cus)impl->cus->EncodeDependencies(command);for(const auto& model:impl->models)if(model)model->EncodeDependencies(command);
 if(impl->particles)impl->particles->EncodeDependencies(command);
 if(impl->generic)impl->generic->EncodeDependencies(command);
 if(impl->terrain)impl->terrain->EncodeDependencies(command);impl->dependencies=true;
}
void PreparedOriginalShadowSource::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder){
 Require(encoder&&impl->dependencies&&!impl->encoded&&index==impl->next&&index<DrawCount(),"Original shadow source draw skipped/repeated or has no dependencies");const auto& draw=impl->draws[index];
 if(draw.family<0){Require(bool(impl->cus),"Original ordered CUS shadow draw lost its prepared owner");impl->cus->EncodeDraw(draw.index,encoder);}
 else if(draw.family==3)impl->particles->EncodeDraw(draw.index,encoder);
 else if(draw.family==4)impl->generic->EncodeDraw(draw.index,encoder);
 else if(draw.family==5)impl->terrain->EncodeDraw(draw.index,encoder);
 else impl->models[draw.family]->EncodeDraw(draw.index,encoder);++impl->next;
}
void PreparedOriginalShadowSource::EndEncoding(id<MTL4CommandBuffer> command,id<MTL4RenderCommandEncoder> encoder){
 Require(command&&command==impl->command&&encoder&&impl->dependencies&&!impl->encoded&&impl->next==DrawCount(),"Original shadow source cannot publish before all ordered actual draws and final common encoder");
 for(const auto& model:impl->models)if(model)model->Finish();
 if(impl->particles)impl->particles->Finish();
 if(impl->generic)impl->generic->Finish();
 if(impl->terrain)impl->terrain->Finish();
 [encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
 MarkNativeShadowTextureEncoded(*impl->source->textures->depth.backing,impl->submission);
 if(impl->source->textures->color.backing)MarkNativeShadowTextureEncoded(*impl->source->textures->color.backing,impl->submission);impl->encoded=true;
}
void PreparedOriginalShadowSource::DidComplete(){
 Require(impl->encoded&&!impl->completed,"Original shadow retirement preceded its actual graph encoding or repeated");
 // Complete shared content status FIRST after the actual common wait. Other
 // image-owner callbacks may throw; they cannot erase this genuine retirement.
 CompleteNativeShadowTexture(*impl->source->textures->depth.backing);
 if(impl->source->textures->color.backing)CompleteNativeShadowTexture(*impl->source->textures->color.backing);
 const auto& retained=impl->source->textures->retainedColor;if(retained&&retained->encodedSubmission.load()==impl->submission&&!retained->completed.load())CompleteNativeShadowTexture(*retained);
 impl->completed=true;
 std::vector<std::exception_ptr> failures;
 const auto retire=[&](const auto& complete){try{complete();}catch(...){failures.push_back(std::current_exception());}};
 if(impl->cus)retire([&]{impl->cus->DidComplete();});
 for(const auto& model:impl->models)if(model)retire([&]{model->DidComplete();});
 if(impl->particles)retire([&]{impl->particles->DidComplete();});
 if(impl->generic)retire([&]{impl->generic->DidComplete();});
 if(impl->terrain)retire([&]{impl->terrain->DidComplete();});
 if(failures.size()==1)std::rethrow_exception(failures.front());
 if(!failures.empty()){
  std::string message="Original shadow child retirement reported multiple failures:";
  for(const auto& error:failures)try{std::rethrow_exception(error);}catch(const std::exception& e){message+="\n";message+=e.what();}catch(...){message+="\nUnknown original shadow retirement failure";}
  throw std::runtime_error(message);
 }
}
std::unique_ptr<PreparedOriginalShadowSource> PrepareOriginalShadowSource(RuntimeLease& lease,std::shared_ptr<const NativeOriginalShadowFrame> source,std::shared_ptr<const NativeWorldDrawFrame> commonFrame,CUSSourceGPU& cus,EngineShadowGPU& engine,OriginalShadowParticlesGPU& particles,const EngineDeferredGeometryProvider& geometry,const CUSSourceImageProvider& images,std::span<const NativeWorldAttachmentClear> actualClearQueue,uint64_t frame,uint64_t submission,bool validation,double& milliseconds){
 Require(source&&source->textures&&source->cus&&source->sourceDrawFrame==frame&&source->textures->sourceDrawFrame==frame&&source->cus->sourceDrawFrame==frame&&submission&&source->beginSequence&&source->endSequence>source->beginSequence,"Original shadow graph lost its actual immutable frame/source ordinals/submission");
 Require(bool(source->clearProof),"Original shadow source has no actual clear initialization owner");source->clearProof->RequireQueue(source->textures,actualClearQueue,source->beginSequence,source->endSequence);
 const auto& textures=*source->textures;
 const auto check=[&](const NativeShadowTextureWrite& w,NativeShadowTextureKind kind,NativeShadowTextureOperation op){Require(w.previous&&w.current&&w.backing&&w.previous->name==w.current->name&&w.previous->identity==w.current->identity&&w.current->backing==w.backing&&w.backing->request.kind==kind&&w.backing->operation==op&&w.backing->sourceDrawFrame==frame&&!w.backing->encodedSubmission.load()&&!w.backing->completed.load(),"Original shadow write is not this unencoded actual source publication");};
 check(textures.depth,NativeShadowTextureKind::Depth,NativeShadowTextureOperation::DepthPass);
 std::shared_ptr<const NativeShadowTextureBacking> color;
 if(textures.redrawColor){check(textures.color,NativeShadowTextureKind::Color,NativeShadowTextureOperation::ColorPass);Require(!textures.retainedColor&&source->colorClearSequence>source->beginSequence&&source->colorClearSequence<source->endSequence,"Positive-sun shadow graph lost its actual original color-clear boundary");color=textures.color.backing;}
 else{Require(!textures.color.backing&&!textures.color.current&&!textures.color.previous&&textures.retainedColor&&!source->colorClearSequence&&!source->models[0]&&!source->models[1]&&!source->projectiles&&!source->terrain&&source->cus->draws.empty()&&!source->transparent&&!source->transparentLua,"Dark-sun shadow source must retain exact color and execute no geometry/callins");color=textures.retainedColor;}
 auto result=std::unique_ptr<PreparedOriginalShadowSource>(new PreparedOriginalShadowSource);auto& out=*result->impl;out.source=std::move(source);out.submission=submission;out.target=OriginalShadowSourceTarget(*textures.depth.backing,*color,out.source->camera);out.depth=NativeShadowTextureImage(*textures.depth.backing);out.color=NativeShadowTextureImage(*color);Require(out.depth.device==lease.Device()&&out.color.device==lease.Device(),"Original shadow graph has another actual device owner");lease.AddAllocation(out.depth);lease.AddAllocation(out.color);
 for(const auto& draw:out.source->cus->draws)Require(draw.program&&(draw.program->originalPass==16||HasNoNativeCUSAttachmentEffects(draw)),"Original shadow source contains drawable CUS output from another original pass");
 // The actual shared factory returns null for a genuinely empty captured
 // CUS scope. Retain that original CPU scope; no dummy prepared/ready owner.
 out.cus=cus.Prepare(lease,out.source->cus,out.target,images,validation,milliseconds);
 Require(out.cus||out.source->cus->draws.empty(),"Nonempty original CUS shadow lost its actual prepared owner");
 if(out.cus)for(size_t i=0;i<out.cus->DrawCount();++i)out.draws.push_back({out.cus->SequenceAt(i),i,-1});
 for(unsigned f=0;f<2;++f)if(out.source->models[f]){Require(out.source->models[f]->sourceDrawFrame==frame&&out.source->models[f]->feature==bool(f),"Original default shadow frame changed source/object class");out.models[f]=engine.Prepare(lease,out.source->models[f],out.target,geometry,images,validation,milliseconds);for(size_t i=0;i<out.models[f]->DrawCount();++i)out.draws.push_back({out.models[f]->SequenceAt(i),i,int(f)});}
 if(out.source->projectiles){Require(out.source->projectiles->sourceDrawFrame==frame&&out.source->projectiles->projectile,"Original projectile shadow frame changed its source/family");out.models[2]=engine.Prepare(lease,out.source->projectiles,out.target,geometry,images,validation,milliseconds);for(size_t i=0;i<out.models[2]->DrawCount();++i)out.draws.push_back({out.models[2]->SequenceAt(i),i,2});}
 if(out.source->terrain){
  Require(out.source->terrain->sourceDrawFrame==frame&&out.source->terrain->contextGeneration==textures.depth.current->contextGeneration,"Original MAP shadow frame changed its actual source/context");
  out.terrain=engine.PrepareTerrain(lease,out.source->terrain,out.target,images);
  for(size_t i=0;i<out.terrain->DrawCount();++i)out.draws.push_back({out.terrain->SequenceAt(i),i,5});
 }
 if(out.source->transparent){
  Require(out.source->transparent->sourceDrawFrame==frame,"Original transparent particles changed their actual source frame");
  out.particles=particles.PrepareOriginalShadowParticles(lease,out.source->transparent,out.target,images,validation,milliseconds);
  Require(out.particles||out.source->transparent->draws.empty(),"Nonempty original transparent particles lost their prepared owner");
  if(out.particles)for(size_t i=0;i<out.particles->DrawCount();++i)out.draws.push_back({out.particles->SequenceAt(i),i,3});
 }
 if(commonFrame){
  const auto genericCount=std::count_if(commonFrame->draws.begin(),commonFrame->draws.end(),[](const auto& draw){return draw.stage==NativeWorldDrawStage::ShadowOpaque;});
  Require(textures.redrawColor||!genericCount,"Dark-sun shadow source contains generic geometry");
  out.generic=engine.PrepareGeneric(lease,std::move(commonFrame),frame,textures.depth.current->contextGeneration,out.target,validation,milliseconds);
  Require((out.generic?out.generic->DrawCount():0)==size_t(genericCount),"Original common shadow commands lack their actual prepared consumer");
  if(out.generic)for(size_t i=0;i<out.generic->DrawCount();++i)out.draws.push_back({out.generic->SequenceAt(i),i,4});
 }
 std::sort(out.draws.begin(),out.draws.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
 uint64_t previous=out.source->beginSequence;for(const auto& draw:out.draws){Require(draw.sequence>previous&&draw.sequence<out.source->endSequence,"Original shadow source order is duplicated/outside retained source boundary");previous=draw.sequence;}
 return result;
}
}
