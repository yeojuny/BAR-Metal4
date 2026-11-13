#include "Rendering/Backend/NativeShieldDepthPublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
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
