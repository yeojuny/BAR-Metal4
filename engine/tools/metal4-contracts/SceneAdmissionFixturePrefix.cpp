// Adapters model engine globals; admission transaction, mailbox and Begin receipt
// are production headers. ScenePendingAdmission body is extracted verbatim.
#include "Rendering/Backend/NativeFrameAdmissionTransaction.h"
#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include "Rendering/Backend/NativeFrameBeginReceipt.h"
#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Ledger {
 bool pending=true,held=false,quarantined=false,blocked=false;uint64_t id=0;
 void ValidateSubmittedCapacity(uint64_t)const{if(held)throw std::logic_error("full");}
 void TransferPreparedFrame(uint64_t n)noexcept{pending=false;held=true;id=n;}
 void ValidateRestoreHeldFrame(uint64_t n)const{if(!held||n!=id||pending||blocked)throw std::logic_error("restore");}
 void RestoreHeldFrame(uint64_t)noexcept{pending=true;held=false;}
 void QuarantineHeldFrame(uint64_t)noexcept{quarantined=true;}
 void ReportSubmittedFrame(uint64_t n){assert(n==id&&held);}
};
namespace ui {Ledger recorder;Ledger& GetRecorder(){return recorder;}}
namespace LuaNativePostprocess {Ledger recorder;Ledger& GetRecorder(){return recorder;}}
struct CloudAdmissionOwner:Ledger{};
struct NativeDynamicWaterFrame{uint64_t sourceDrawFrame=0;};
struct DynamicWaterAdmissionOwner{
 Ledger& capture;std::shared_ptr<const NativeDynamicWaterFrame> frame;
 void ValidateSubmittedCapacity(uint64_t n)const{if(frame)capture.ValidateSubmittedCapacity(n);}
 void TransferPreparedFrame(uint64_t n)noexcept{if(frame)capture.TransferPreparedFrame(n);}
 void ValidateRestoreHeldFrame(uint64_t n)const{if(frame)capture.ValidateRestoreHeldFrame(n);}
 void RestoreHeldFrame(uint64_t n)noexcept{if(frame)capture.RestoreHeldFrame(n);}
 void QuarantineHeldFrame(uint64_t n)noexcept{if(frame)capture.QuarantineHeldFrame(n);}
};
struct Registry{uint64_t generation=3;uint64_t ContextGeneration()const{return generation;}};
struct Context{::Registry registry;::Registry& Registry(){return registry;}} context;
Context& OriginalNativeTextureContext(){return context;}
struct NativeMainSourceSettlement{
 bool detached=false;uint64_t generation=0;std::shared_ptr<const NativeDynamicWaterFrame> water;
 void Take(uint64_t c,uint64_t,int& a,int& b,int& d,std::shared_ptr<const NativeDynamicWaterFrame>& w,int& e,int& f){
  if(detached)throw std::logic_error("take");generation=c;detached=true;a=b=d=e=f=0;water=std::move(w);
 }
 void ValidateRestore(uint64_t c,int a,int b,int d,const std::shared_ptr<const NativeDynamicWaterFrame>& w,int e,int f)const{
  if(c!=generation||!detached||a||b||d||w||e||f)throw std::logic_error("source occupied");
 }
 void RestoreValidated(int& a,int& b,int& d,std::shared_ptr<const NativeDynamicWaterFrame>& w,int& e,int& f)noexcept{a=b=d=e=f=1;w=std::move(water);detached=false;}
};
struct Post{uint64_t serial=9;};struct Screen{std::shared_ptr<Post> postprocess=std::make_shared<Post>();};
struct Options{Screen* screenUI;bool allowPrepareReady=false;NativeSourceFrameCredits::Lease sourceCredit;std::shared_ptr<NativeFrameBeginReceipt> beginReceipt=std::make_shared<NativeFrameBeginReceipt>(0,9);};
struct Frame{uint64_t sourceDrawFrame=0;std::shared_ptr<const NativeDynamicWaterFrame> dynamicWater=std::make_shared<NativeDynamicWaterFrame>();};
struct Metal4WorldSubmission{
 ::Frame frame;Screen screen;::Options options{&screen};
 const ::Frame& Frame()const{return frame;}const ::Options& Options()const{return options;}
};
struct Metal4WorldPresentationResult{bool presented=false,sourceFrameCompleted=false,screenTargetsCompleted=false;uint64_t sourceDrawFrame=0;bool queued=false;uint64_t completedPostprocessSerial=0;};
struct ScenePendingAdmission;
struct Session{
 NativeSourceFrameCredits sourceCredits;
 std::shared_ptr<NativeFrameCompletionMailbox> completionMailbox=std::make_shared<NativeFrameCompletionMailbox>(4);
 Ledger dynamicWaterCapture;
 int opaqueSourcePending=1,actualShadowSourcePending=1,mainGroundPending=1,environmentStage=1,shieldCapture=1;
 std::shared_ptr<const NativeDynamicWaterFrame> dynamicCaptureReceipt=std::make_shared<NativeDynamicWaterFrame>();
 std::shared_ptr<NativeMainSourceSettlement> sourceSettlement;
 std::shared_ptr<ScenePendingAdmission> admissionSettlement;
 std::shared_ptr<NativeFrameBeginReceipt> lastFailedBeginReceipt;
};
std::unique_ptr<Session> session;

unsigned reported=0;
void ReportPresentedWorldResult(Session&,const Frame& frame,const Metal4WorldPresentationResult& result,const Screen* screen){assert(frame.sourceDrawFrame==result.sourceDrawFrame&&screen&&screen->postprocess);++reported;}
