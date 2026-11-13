#include "Rendering/Backend/NativeShieldDepthPublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
#include "Rendering/Backend/NativeMainSourceSettlement.h"
#include "Rendering/Backend/NativeOriginalShadowData.h"
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#include "Rendering/Backend/NativeDynamicWaterData.h"
#include <cassert>
#include <iostream>
using namespace rendering;
template<class F>void Reject(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}assert(threw);}
struct BusyResource:NativeShieldDepthResource{};
int main(){
 NativeTextureContext context;context.Initialize([]{return 1;},{});context.History().ActiveTexture(0);
 auto& map=context.Geometry(NativeGeometryTextureRole::Map);auto& models=context.Geometry(NativeGeometryTextureRole::Models);
 map.Update(8,8,NativeCUSTextureTarget::Image2D,1,false);models.Update(8,8,NativeCUSTextureTarget::Image2D,1,false);
 WorldCamera camera;camera.viewport={0,0,8,8};
 PublishNativeShieldDepth(context.Registry(),map,NativeTextureBackingKind::MapDepth,1,camera,true);
 PublishNativeShieldDepth(context.Registry(),models,NativeTextureBackingKind::ModelDepth,1,camera,true);
 NativeModelDepthSelection selection;selection.drawDeferredEnabled=selection.geometryBufferValid=selection.canDrawDeferred=selection.unitsDeferred=true;
 for(unsigned i=1;i<=100;++i){
  NativeShieldCaptureJournal journal;WorldFrame frame;frame.sourceDrawFrame=i;frame.viewCamera=camera;
  Reject([&]{journal.Begin(context,i,camera,{},0);});assert(!journal.Receipt());
  auto partial=journal.Begin(context,i,camera,{},i+100);assert(partial&&!partial->sealed&&!partial->map.Changed());
  Reject([&]{journal.Begin(context,i,camera,{},i+100);});assert(journal.Receipt()==partial);
  auto wrong=frame;wrong.sourceDrawFrame++;Reject([&]{journal.PublishModels(context,wrong,selection);});
  wrong=frame;wrong.viewCamera.viewport[2]++;Reject([&]{journal.PublishModels(context,wrong,selection);});
  assert(!partial->sealed&&!partial->models.published);
  const auto prior=context.Registry().Find(models.Name(5));
  auto sealed=journal.PublishModels(context,frame,selection);assert(sealed==partial&&sealed->sealed&&sealed->models.Changed());
  Reject([&]{journal.PublishModels(context,frame,selection);});Reject([&]{journal.BorrowModels(context,frame);});
  NativeMainSourceSettlement settlement;
  std::shared_ptr<const NativeOpaqueModelFrame> o=std::make_shared<NativeOpaqueModelFrame>();
  std::shared_ptr<const NativeOriginalShadowFrame> sh=std::make_shared<NativeOriginalShadowFrame>();
  std::shared_ptr<const NativeTerrainSceneSourceFrame> gr=std::make_shared<NativeTerrainSceneSourceFrame>();
  std::shared_ptr<const NativeDynamicWaterFrame> wa=std::make_shared<NativeDynamicWaterFrame>();
  NativeEnvironmentStageReceipt env{std::make_shared<NativeWorldEnvironmentFrame>(),true};
  const auto oldO=o;const auto oldS=sh;const auto oldG=gr;const auto oldW=wa;const auto oldE=env.frame;
  const auto epoch=context.Registry().ContextGeneration();
  settlement.Take(epoch,i,o,sh,gr,wa,env,journal);
  assert(settlement.detached&&!o&&!sh&&!gr&&!wa&&!env.frame&&!journal.Receipt());
  assert(settlement.shield.Receipt()==sealed&&settlement.shield.CanCancel());
  Reject([&]{settlement.Take(epoch,i,o,sh,gr,wa,env,journal);});
  Reject([&]{settlement.ValidateRestore(epoch+1,o,sh,gr,wa,env,journal);});
  // Every destination is preflighted before any owner is moved back.
  o=oldO;Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});o.reset();
  sh=oldS;Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});sh.reset();
  gr=oldG;Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});gr.reset();
  wa=oldW;Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});wa.reset();
  env.frame=oldE;Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});env={};
  journal.Begin(context,i+1000,camera,{},i+1000);
  Reject([&]{settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);});journal.Reset();
  assert(settlement.shield.Receipt()==sealed&&settlement.opaque==oldO);
  settlement.ValidateRestore(epoch,o,sh,gr,wa,env,journal);
  settlement.RestoreValidated(o,sh,gr,wa,env,journal);
  assert(!settlement.detached&&settlement.Empty()&&o==oldO&&sh==oldS&&gr==oldG&&wa==oldW&&env.frame==oldE&&env.shaderValidation&&journal.Receipt()==sealed);
  // Failed-Begun ownership survives the local throwing scope in a durable slot.
  std::shared_ptr<NativeMainSourceSettlement> durable;
  try{auto job=std::make_shared<NativeMainSourceSettlement>();
   job->Take(epoch,0,o,sh,gr,wa,env,journal);durable=job;throw 7;
  }catch(int){}
  assert(durable&&durable->detached&&durable->draw==0&&durable->shield.Receipt()==sealed&&!journal.Receipt());
  durable->ValidateRestore(epoch,o,sh,gr,wa,env,journal);
  durable->RestoreValidated(o,sh,gr,wa,env,journal);durable.reset();

  assert(journal.Cancel()&&!journal.Receipt());assert(context.Registry().Find(models.Name(5))->backing==prior->backing);
  assert(sealed->models.backing&&sealed->sealed); // Consumer survives journal release.
  assert(journal.Cancel());
  journal.Begin(context,i,camera,{},i+200);
  auto opaque=std::make_shared<NativeOpaqueModelFrame>();opaque->sourceDrawFrame=i;frame.opaqueModelFrame=opaque;
  const auto borrowedObject=context.Registry().Find(models.Name(5));
  auto borrowed=journal.BorrowModels(context,frame);
  assert(borrowed->sealed&&!borrowed->models.Changed()&&borrowed->models.published==borrowedObject);
  assert(journal.Cancel());assert(context.Registry().Find(models.Name(5))==borrowedObject);
 }
 NativeShieldCaptureJournal journal;WorldFrame frame;frame.sourceDrawFrame=500;frame.viewCamera=camera;
 auto partial=journal.Begin(context,500,camera,{},900);
 auto depth=context.Registry().Find(models.Name(5));auto invalid=depth->allocation;invalid.originalInternalFormat=0x1908;
 context.Registry().Allocate(depth->name,invalid);
 Reject([&]{journal.PublishModels(context,frame,selection);});assert(journal.Receipt()==partial&&!partial->sealed&&!partial->models.published);
 context.Registry().Allocate(depth->name,depth->allocation);
 auto sealed=journal.PublishModels(context,frame,selection);
 sealed->models.backing->Fulfill(std::make_shared<BusyResource>());
 assert(!journal.CanCancel()&&!journal.Cancel()&&journal.Receipt()==sealed);
 journal.Reset();assert(!journal.Receipt()&&sealed->models.backing->Resource()); // Explicit caller retirement/reset contract.
 journal.Begin(context,501,camera,{},901);
 context.Destroy();context.Initialize([]{return 2;},{});
 frame.sourceDrawFrame=501;
 Reject([&]{journal.PublishModels(context,frame,selection);});
 assert(!journal.CanCancel()&&!journal.Cancel()&&journal.Receipt());
 journal.Reset();context.Destroy();
 std::cout<<"Actual shield journal+context+registry:100 partial/seal/borrow/rollback/retry/retention cycles; busy refusal PASS\n";
}
