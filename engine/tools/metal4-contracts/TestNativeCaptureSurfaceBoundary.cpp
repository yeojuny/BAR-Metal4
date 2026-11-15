#include "Rendering/Metal4/Metal4CaptureSurfaceBoundary.h"
#include <cassert>
#include "Rendering/Backend/NativeDeferredParticleCapture.h"
#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include <limits>
#include <string>
#include <stdexcept>
using namespace rendering;
int main(){
 // BEGIN uses two credits: held N and captured N+1. Stable CPU capture keeps
 // N pending until the existing full predispatch drain, not after Begin.
 {
  NativeSourceFrameCredits credits(2);NativeFrameCompletionMailbox mailbox(9);
  auto old=credits.Acquire(20);credits.Hold(old);old.reset(); // Prior CPU owner settled; GPU-held credit remains.
  auto next=credits.Acquire(21);
  const auto ticket=mailbox.Prepare(9,20,120);mailbox.Accept(ticket);
  bool gpuPending=true;unsigned drains=0;
  const auto drain=[&]{++drains;gpuPending=false;mailbox.Publish(ticket,{true,20,120});
   const auto notice=mailbox.Pop();assert(notice&&notice->sourceDrawFrame==20);credits.ReleaseHeld(20);};
  NativeCaptureRollbackFence fence;
  assert(CaptureAtNativeSurfaceBoundary(true,[]{return true;},drain,[]{assert(false);},[]{},[]{return true;}));
  fence.Arm();assert(fence.Armed()&&gpuPending&&!credits.HasCapacity());
  try{credits.Acquire(22);assert(false);}catch(const std::logic_error&){}
  // The production guard skips early drain only when this exact fence is armed.
  if(!fence.Armed())drain();
  fence.RefreshForDispatch([]{return true;},drain,[]{assert(false);});assert(drains==0&&gpuPending);
  drain(); // Preserved options.allowBeginReady predispatch requirement.
  assert(!gpuPending&&drains==1);credits.Validate(next,21);credits.Hold(next);
  auto future=credits.Acquire(22);assert(!credits.HasCapacity());
  fence.Dismiss();assert(!fence.Armed());
 }

 // A surface can change after stable capture but before dispatch. Do not write
 // CAMetalLayer properties under a retained predecessor, even on that edge.
 {
  NativeCaptureRollbackFence fence;std::string order;int observed=1,applied=0;
  auto refresh=[&]{order+='R';applied=observed;};
  auto drain=[&]{order+='D';observed=2;};
  fence.RefreshForDispatch([&]{order+='O';return true;},drain,refresh);assert(order=="R");
  fence.Arm();order.clear();
  fence.RefreshForDispatch([&]{order+='O';return true;},drain,refresh);assert(order=="O");
  order.clear();fence.RefreshForDispatch([&]{order+='O';return false;},drain,refresh);
  assert(order=="ODR"&&applied==2);
  order.clear();fence.RefreshForDispatch([&]()->bool{order+='O';throw std::runtime_error("unknown");},drain,refresh);assert(order=="ODR");
  order.clear();try{fence.RefreshForDispatch([&]{order+='O';return false;},[&]{order+='D';throw std::runtime_error("drain");},refresh);assert(false);}catch(const std::runtime_error&){}assert(order=="OD");
 }
 // Exceptions after the surface helper returned (alpha/Lua/publication or
 // later DrawWorld preparation) must fence before ANY source rollback.
 {
  NativeCaptureRollbackFence fence;bool gpuPending=true,rolledBack=false;int drains=0;
  auto drain=[&]{++drains;gpuPending=false;};
  fence.BeforeRollback(drain);assert(drains==0); // OFF/fallback unchanged.
  const bool retained=CaptureAtNativeSurfaceBoundary(true,[]{return true;},drain,[]{},[]{},[]{return true;});
  assert(retained&&gpuPending);fence.Arm();
  try{throw std::runtime_error("later alpha callback");}
  catch(...){fence.BeforeRollback(drain);assert(!gpuPending);rolledBack=true;}
  assert(rolledBack&&drains==1);fence.BeforeRollback(drain);assert(drains==1);
  fence.Arm();gpuPending=true;
  try{fence.BeforeRollback([]{throw std::runtime_error("GPU quarantine");});assert(false);}catch(const std::runtime_error&){}
  assert(gpuPending);fence.BeforeRollback(drain);assert(drains==2&&!gpuPending);
  fence.Arm();gpuPending=true;fence.Dismiss();fence.BeforeRollback(drain);
  assert(gpuPending&&drains==2); // Successful source end doesn't claim GPU done.
 }
 // A stable, CPU-settled suffix must retain the exact older GPU source lease.
 // CPU acceptance alone cannot pop a GPU notice or admit unbounded captures.
 {
  NativeSourceFrameCredits credits(3);NativeFrameCompletionMailbox mailbox(7);
  auto old=credits.Acquire(10);credits.Hold(old);
  const auto ticket=mailbox.Prepare(7,10,110);
  auto current=credits.Acquire(11);
  bool cpuPending=true,gpuPending=true;unsigned drains=0;
  const auto drain=[&]{++drains;gpuPending=false;mailbox.Publish(ticket,{true,10,110});};
  const bool deferred=CaptureAtNativeSurfaceBoundary(true,[]{return true;},drain,[]{assert(false);},
   [&]{cpuPending=false;mailbox.Accept(ticket);},[&]{assert(!cpuPending);return true;});
  assert(deferred&&gpuPending&&drains==0&&!mailbox.Pop());credits.ValidateHeld(10);
  auto third=credits.Acquire(12);assert(!credits.HasCapacity());
  try{credits.Acquire(13);assert(false);}catch(const std::logic_error&){}
  old.reset();assert(!credits.HasCapacity()); // GPU-held even without caller alias.
  drain();const auto notice=mailbox.Pop();assert(notice&&notice->sourceDrawFrame==10);
  credits.ReleaseHeld(notice->sourceDrawFrame);assert(credits.HasCapacity());
  credits.Validate(current,11);credits.Validate(third,12); // Never retire N+1.
  try{mailbox.Publish(ticket,{true,10,110});assert(false);}catch(const std::logic_error&){}
  mailbox.Close();NativeFrameCompletionMailbox replacement(8);
  try{replacement.Publish(ticket,{true,10,110});assert(false);}catch(const std::logic_error&){}
  try{mailbox.Publish(ticket,{true,10,110});assert(false);}catch(const std::logic_error&){}
 }
 // Readiness is a post-capture proof. Required GPU consumers, unknown policy,
 // changed/unknown surface, failing join/capture and failed drain all preserve
 // the full conservative boundary. No readiness claim survives failure.
 for(unsigned scenario=0;scenario<8;++scenario){
  std::string order;bool readyCalled=false;bool threw=false;
  const auto observe=[&](){order+='O';if(scenario==3)throw std::runtime_error("observation");return scenario!=2;};
  const auto drain=[&]{order+='D';if(scenario==7)throw std::runtime_error("quarantine");};
  try{
   bool retained=CaptureAtNativeSurfaceBoundary(true,observe,drain,[&]{order+='R';},[&]{
    order+='C';if(scenario==4||scenario==7)throw std::runtime_error("join");
    if(scenario==5)throw std::runtime_error("capture");
   },[&]{readyCalled=true;order+='P';if(scenario==6)throw std::runtime_error("proof");return scenario!=1;});
   assert(retained==(scenario==0));
  }catch(const std::runtime_error& e){threw=true;if(scenario==7)assert(std::string(e.what())=="quarantine");}
  assert(threw==(scenario>=4));
  if(scenario==0)assert(order=="OCP");
  if(scenario==1)assert(order=="OCPD");
  if(scenario==2||scenario==3)assert(order=="ODRCD"&&!readyCalled);
  if(scenario==4||scenario==5||scenario==7)assert(order=="OCD"&&!readyCalled);
  if(scenario==6)assert(order=="OCPD");
 }
 {
  NativeDeferredParticleCapture state;state.RequireComplete();int writes=0;
  try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}assert(writes==0);
  state.Defer();try{state.RequireComplete();assert(false);}catch(const std::logic_error&){}
  state.Complete([&]{++writes;try{state.RequireComplete();assert(false);}catch(const std::logic_error&){}
   try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){} });
  state.RequireComplete();assert(writes==1);
  try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}
  try{state.Defer();assert(false);}catch(const std::logic_error&){}
  NativeDeferredParticleCapture failed;failed.Defer();
  try{failed.Complete([&]{++writes;throw std::runtime_error("writer");});assert(false);}catch(const std::runtime_error&){}
  try{failed.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}
  try{failed.RequireComplete();assert(false);}catch(const std::logic_error&){}assert(writes==2);
 }


 // Production placement: CPU settlement and particle-only tail stay inside
 // capture so its existing exception fence still GPU-drains before rollback.
 {
  std::string order; bool gpuPending=true, cpuPending=true;
  auto gpuDrain=[&]{order+='G';gpuPending=false;};
  auto cpuTail=[&]{order+='C';cpuPending=false;order+='J';
   assert(!cpuPending&&gpuPending);order+='P';};
  CaptureAtNativeSurfaceBoundary(true,[]{return true;},gpuDrain,[]{},cpuTail);
  assert(order=="CJPG"&&!gpuPending);
  order.clear();gpuPending=true;
  try{CaptureAtNativeSurfaceBoundary(true,[]{return true;},gpuDrain,[]{},[&]{
   order+='J';throw std::runtime_error("particle");});assert(false);}
  catch(const std::runtime_error& e){assert(std::string(e.what())=="particle");}
  assert(order=="JG"&&!gpuPending);
  order.clear();gpuPending=true;
  try{CaptureAtNativeSurfaceBoundary(true,[]{return true;},gpuDrain,[]{},[&]{
   order+='J';throw std::runtime_error("cpu settlement");});assert(false);}
  catch(const std::runtime_error& e){assert(std::string(e.what())=="cpu settlement");}
  assert(order=="JG"&&!gpuPending);
 }
 Metal4SurfaceSnapshot p; p.logicalWidth=800;p.logicalHeight=600;
 p.presentationBackingWidth=1600;p.presentationBackingHeight=1200;
 p.renderWidth=1024;p.renderHeight=768;p.visible=true;
 Metal4SurfaceObservation o{800,600,1600,1200,true,false};
 for(int n=0;n<1000;++n){
  assert(NativeCaptureSurfaceUnchanged(p,o,1024,768,true));
  for(int field=0;field<6;++field){auto changed=o;
   switch(field){case 0:++changed.logicalWidth;break;case 1:++changed.logicalHeight;break;
    case 2:++changed.presentationBackingWidth;break;case 3:++changed.presentationBackingHeight;break;
    case 4:changed.visible=false;break;case 5:changed.minimized=true;break;}
   assert(!NativeCaptureSurfaceUnchanged(p,changed,1024,768,true));
  }
  assert(!NativeCaptureSurfaceUnchanged(p,o,1023,768,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,1024,767,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,std::numeric_limits<double>::quiet_NaN(),768,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,1024,768,false));
  auto closed=p;closed.closeRequested=true;assert(!NativeCaptureSurfaceUnchanged(closed,o,1024,768,true));
  closed=p;closed.attached=false;assert(!NativeCaptureSurfaceUnchanged(closed,o,1024,768,true));
  std::string order;
  auto observe=[&]{order+='O';return true;};auto drain=[&]{order+='D';};
  auto refresh=[&]{order+='R';};auto capture=[&]{order+='C';};
  CaptureAtNativeSurfaceBoundary(true,observe,drain,refresh,capture);assert(order=="OCD");
  order.clear();CaptureAtNativeSurfaceBoundary(false,observe,drain,refresh,capture);assert(order=="RC");
  order.clear();CaptureAtNativeSurfaceBoundary(true,[&]{order+='O';return false;},drain,refresh,capture);assert(order=="ODRCD");
  order.clear();CaptureAtNativeSurfaceBoundary(true,[&]()->bool{order+='O';throw std::runtime_error("observe");},drain,refresh,capture);assert(order=="ODRCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,observe,drain,refresh,[&]{order+='C';throw std::runtime_error("capture");});assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="capture");}assert(order=="OCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,observe,[&]{order+='D';throw std::runtime_error("quarantine");},refresh,[&]{order+='C';throw std::runtime_error("capture");});assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="quarantine");}assert(order=="OCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,[&]{order+='O';return false;},[&]{order+='D';throw std::runtime_error("drain");},refresh,capture);assert(false);}catch(const std::runtime_error&){}assert(order=="OD");
  // Observe cannot become a stale write plan: original refresh reobserves after drain.
  int live=1,applied=0;CaptureAtNativeSurfaceBoundary(true,[]{return false;},[&]{live=2;},[&]{applied=live;},[]{});assert(applied==2);
 }
}
