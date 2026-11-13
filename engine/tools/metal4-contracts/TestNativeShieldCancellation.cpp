#include "Rendering/Backend/NativeShieldDepthPublication.h"
#include <cassert>
#include <iostream>
using namespace rendering;
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();
 auto object=registry.Create(NativeCUSTextureTarget::Image2D,"shield-model");
 registry.Allocate(object->name,{4,4,1,1,1,0x8CAC});
 const auto publish=[&](std::uint64_t draw){
  auto frame=std::make_shared<NativeShieldDepthFrame>();frame->sourceDrawFrame=draw;
  frame->models.previous=registry.Find(object->name);
  auto backing=std::make_shared<NativeShieldDepthBacking>(NativeTextureBackingKind::ModelDepth,object->identity,draw,draw,NativeTextureContent::ProducerVersion,NativeShieldDepthMetadata{{},{},{0,0,4,4},4,4});
  registry.PublishContent(object->name,{},backing);
  frame->models.published=registry.Find(object->name);frame->models.backing=backing;
  return frame;
 };
 for(unsigned i=1;i<=100;++i){
  auto pending=publish(i);auto expected=std::shared_ptr<const NativeShieldDepthFrame>(pending);
  auto wrong=std::make_shared<NativeShieldDepthFrame>(*pending);
  CancelPendingNativeShieldDepth(registry,pending,wrong);assert(pending==expected);
  CancelPendingNativeShieldDepth(registry,pending,{});assert(pending==expected);
  CancelPendingNativeShieldDepth(registry,pending,expected);assert(!pending);
  assert(registry.Find(object->name)->backing==expected->models.previous->backing);
  assert(registry.Find(object->name)->storage==expected->models.previous->storage);
  assert(registry.Find(object->name)->version>expected->models.published->version);
  CancelPendingNativeShieldDepth(registry,pending,expected);assert(!pending);
 }
 auto earlier=publish(101),pending=publish(102);
 CancelPendingNativeShieldDepth(registry,pending,earlier);assert(pending && registry.Find(object->name)==pending->models.published);
 // Partial map-only capture has no models but still releases its exact owner.
 auto partial=std::make_shared<NativeShieldDepthFrame>();auto expected=std::shared_ptr<const NativeShieldDepthFrame>(partial);
 CancelPendingNativeShieldDepth(registry,partial,expected);assert(!partial);
 std::cout<<"Shield actual registry rollback:100 exact/absent/replay/wrong identity cycles, newer and partial capture passed\n";
}
