#include "Rendering/Backend/NativeCUSDeferredData.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct State {bool encoded=false,completed=false;uint64_t preparedSubmission=19;};
struct Color:NativeDeferredColorResource {std::shared_ptr<State> state;explicit Color(std::shared_ptr<State> s):state(s){}bool CanCancelUnsubmitted()const noexcept override{return !state->encoded&&!state->completed;}};
struct Depth:NativeShieldDepthResource {std::shared_ptr<State> state;explicit Depth(std::shared_ptr<State> s):state(s){}bool CanCancelUnsubmitted()const noexcept override{return !state->encoded&&!state->completed;}};
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();
 NativeShieldDepthMetadata metadata{{},{},{0,0,4,4},4,4};
 const auto publish=[&](bool fulfill){
  NativeCUSDeferredPhase phase;phase.role=NativeGeometryTextureRole::Map;
  for(unsigned n=0;n<7;++n){
   auto object=registry.Create(NativeCUSTextureTarget::Image2D,"MRT");registry.Allocate(object->name,{4,4,1,1,1,n==6?0x8CACu:0x8058u});object=registry.Find(object->name);
   if(n==6){auto b=std::make_shared<NativeShieldDepthBacking>(NativeTextureBackingKind::MapDepth,object->identity,object->version+1,7,NativeTextureContent::ProducerVersion,metadata);
    if(fulfill)b->Fulfill(std::make_shared<Depth>(std::make_shared<State>()));registry.PublishContent(object->name,{},b);phase.depth={object,registry.Find(object->name),b};
   }else{auto b=std::make_shared<NativeDeferredColorBacking>(object->identity,object->version+1,7,NativeTextureContent::ProducerVersion,NativeDeferredColorMetadata{NativeGeometryTextureRole::Map,n==5?NativeGeometryTextureNames::CombinedColorAttachment:n,metadata});
    if(fulfill)b->Fulfill(std::make_shared<Color>(std::make_shared<State>()));registry.PublishContent(object->name,{},b);(n==5?phase.combinedColor:phase.colors[n])=NativeDeferredColorVersion{object,registry.Find(object->name),b};}
  }return phase;
 };
 const auto preserved=[&](const auto& p){assert(registry.Find(p.depth.published->name)->backing==p.depth.backing);for(const auto& c:p.colors)assert(registry.Find(c.current->name)->backing==c.backing);assert(registry.Find(p.combinedColor.current->name)->backing==p.combinedColor.backing);};
 for(unsigned i=0;i<100;++i){auto p=publish(i%2);CancelNativeCUSDeferredPhase(registry,p);assert(!registry.Find(p.depth.published->name)->backing);for(const auto& c:p.colors)assert(!registry.Find(c.current->name)->backing);assert(!registry.Find(p.combinedColor.current->name)->backing);}
 // Every output can veto the entire phase, not just itself. Prepared identity
 // alone is deliberately not an execution flag.
 for(unsigned n=0;n<7;++n)for(bool completed:{false,true}){
  auto p=publish(true);auto state=n==6?std::dynamic_pointer_cast<const Depth>(p.depth.backing->Resource())->state:std::dynamic_pointer_cast<const Color>((n==5?p.combinedColor:p.colors[n]).backing->Resource())->state;
  if(completed)state->completed=true;else state->encoded=true;
  CancelNativeCUSDeferredPhase(registry,p);preserved(p);
 }
 auto p=publish(false);p.colors[3].backing->Fulfill(std::make_shared<NativeDeferredColorResource>());CancelNativeCUSDeferredPhase(registry,p);preserved(p);
 auto shield=std::make_shared<NativeShieldDepthFrame>();shield->map=publish(true).depth;
 auto state=std::dynamic_pointer_cast<const Depth>(shield->map.backing->Resource())->state;state->completed=true;
 auto retained=std::shared_ptr<const NativeShieldDepthFrame>(shield);assert(!CancelPendingNativeShieldDepth(registry,shield,retained));assert(shield==retained&&registry.Find(shield->map.published->name)->backing==shield->map.backing);
 auto color=publish(true).colors[0];std::dynamic_pointer_cast<const Color>(color.backing->Resource())->state->encoded=true;CancelNativeDeferredColor(registry,color);assert(registry.Find(color.current->name)->backing==color.backing);
 auto older=publish(true),younger=publish(true);
 std::dynamic_pointer_cast<const Color>(older.colors[2].backing->Resource())->state->completed=true;
 NativeOpaqueModelFrame batch;NativeOpaqueModelPass pass;pass.mrt=std::make_shared<NativeCUSDeferredPhase>(older);batch.passes.push_back(pass);pass.mrt=std::make_shared<NativeCUSDeferredPhase>(younger);batch.passes.push_back(pass);
 CancelNativeOpaqueModels(registry,batch);preserved(older);preserved(younger);
 std::cout<<"MRT cancellation:100 unfulfilled/prepared rollbacks,14 per-output execution vetoes,unknown-resource/shield-retention/standalone-color/opaque-batch passed\n";
}
