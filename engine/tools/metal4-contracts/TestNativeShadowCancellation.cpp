#include "Rendering/Backend/NativeShadowTextureSource.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Resource:NativeShadowTextureResource{};
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();history.ActiveTexture(0);
 NativeShadowTextureSource source;int budget=-1;bool captureFails=false;
 source.Attach(registry,history,{[]{return true;},[&](const auto&){if(budget==0)throw std::bad_alloc();if(budget>0)--budget;return std::make_shared<Resource>();},
 [](const auto&,const auto&,bool){return true;},[]{return 7;},[]{return 24;},[&](auto){if(captureFails)throw std::runtime_error("capture failure");}});
 uint32_t depth=0,color=0;assert(source.Initialize(depth,color,4,0x81A6,true));
 for(unsigned i=0;i<100;++i){
  auto oldDepth=source.Image(depth),oldColor=source.Image(color);
  auto frame=source.BeginFrame(depth,color,true);source.Cancel(*frame);
  assert(source.Image(depth)==oldDepth&&source.Image(color)==oldColor);
  captureFails=true;try{source.Capture(depth,color,true);assert(false);}catch(const std::runtime_error&){}captureFails=false;
  assert(source.Image(depth)==oldDepth&&source.Image(color)==oldColor);
  // Color allocation fails after depth publication: rollback exact partial.
  budget=1;try{source.BeginFrame(depth,color,true);assert(false);}catch(const std::bad_alloc&){}budget=-1;
  assert(source.Image(depth)==oldDepth&&source.Image(color)==oldColor);
 }
 for(unsigned state=0;state<4;++state){
  auto frame=source.BeginFrame(depth,color,true);
  auto backing=state<2?frame->depth.backing:frame->color.backing;
  if(state%2)backing->completed=true;else backing->encodedSubmission=9;
  source.Cancel(*frame);source.Cancel(*frame);
  assert(source.Image(depth)==frame->depth.backing&&source.Image(color)==frame->color.backing);
 }
 auto first=source.BeginFrame(depth,color,true),newer=source.BeginFrame(depth,color,true);source.Cancel(*first);
 assert(source.Image(depth)==newer->depth.backing&&source.Image(color)==newer->color.backing);
 auto sampling=registry.Find(depth)->sampling;sampling.wrapS=0x2901;registry.SetSampling(depth,sampling);source.Cancel(*newer);
 assert(source.Image(depth)==first->depth.backing&&registry.Find(depth)->sampling.wrapS==0x2901);
 auto unlit=source.BeginFrame(depth,color,false);auto priorColor=source.Image(color);source.Cancel(*unlit);assert(source.Image(color)==priorColor);
 source.Detach();source.Cancel(*first);
 std::cout<<"Shadow cancellation:100 capture/partial rollback cycles;depth/color encoded/completed pair preservation;newer/sampler/unlit/detach passed\n";
}
