template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
std::shared_ptr<ScenePendingAdmission> Make(bool early=false){
 reported=0;ui::recorder={};LuaNativePostprocess::recorder={};context.registry.generation=3;session=std::make_unique<Session>();
 auto packet=std::make_shared<Metal4WorldSubmission>();packet->options.allowPrepareReady=early;packet->options.sourceCredit=session->sourceCredits.Acquire(0);auto mailbox=session->completionMailbox;
 auto result=std::make_shared<ScenePendingAdmission>(*session,packet,mailbox->Prepare(mailbox->Epoch(),0,9));
 session->admissionSettlement=result;result->Transfer();return result;
}
int main(){
 for(unsigned i=0;i<100;++i){
  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);job->MarkQueued();
   auto younger=session->sourceCredits.Acquire(1);session->opaqueSourcePending=99;
   job->CompleteCPU(nullptr,std::make_exception_ptr(std::runtime_error("older encode failed")));
   assert(job->transferred&&job->failureHandled&&session->sourceSettlement==job->source);
   assert(session->opaqueSourcePending==99); // Never restore N over N+1.
   Reject([&]{session->sourceCredits.Validate(younger,1);});
   Reject([&]{session->sourceCredits.Hold(younger);});
   Reject([&]{session->sourceCredits.Acquire(2);});
  }

  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);job->MarkQueued();Metal4WorldPresentationResult wrong{true,false,false,1};Reject([&]{job->CompleteCPU(&wrong,{});});assert(job->failureHandled&&ui::recorder.quarantined&&job->transferred);Reject([&]{session->sourceCredits.ValidateHeld(0);});}

  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);job->MarkQueued();Metal4WorldPresentationResult result{true,true,true,0};Reject([&]{job->CompleteCPU(&result,{});});assert(job->failureHandled&&job->transferred&&ui::recorder.quarantined);Reject([&]{session->sourceCredits.ValidateHeld(0);});assert(!reported);}

  {auto job=Make();job->CompleteCPU(nullptr,std::make_exception_ptr(std::runtime_error("pre")));assert(job->failureHandled&&!job->transferred&&!session->admissionSettlement);job->Fail();}
  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);Metal4WorldPresentationResult result{true,false,false,0};job->CompleteCPU(&result,{});assert(job->cpuAccepted&&!job->transferred&&!session->admissionSettlement&&reported==1);}

  {auto job=Make();assert(ui::recorder.held&&!session->opaqueSourcePending);job->Fail();assert(ui::recorder.pending&&session->opaqueSourcePending);assert(job->submission->Options().beginReceipt->FailureStage()==NativeFrameBeginReceipt::Failure::BeforeBegin);job->ReleaseAnchor();assert(!session->admissionSettlement);}
  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);job->Fail();assert(job->transferred&&ui::recorder.quarantined&&job->cloud.quarantined&&session->dynamicWaterCapture.quarantined);assert(session->sourceSettlement==job->source);Reject([&]{job->ReleaseAnchor();});}
  {auto job=Make();job->submission->Options().beginReceipt->MarkBegun(11);job->mailbox->Publish(job->ticket,{true,0,9});assert(!job->mailbox->Pop());job->Accept({true,false,false,0});assert(job->mailbox->Pop());assert(!session->sourceSettlement);job->ReleaseAnchor();}
  {auto job=Make();Reject([&]{job->Accept({true,false,false,0});});job->submission->Options().beginReceipt->MarkBegun(11);Metal4WorldPresentationResult wrong;wrong.presented=true;wrong.sourceDrawFrame=1;Reject([&]{job->Accept(wrong);});assert(job->transferred);job->Fail();}
  {auto job=Make();session->opaqueSourcePending=99;Reject([&]{job->Restore();});assert(ui::recorder.held&&job->cloud.held);session->opaqueSourcePending=0;job->Restore();job->ReleaseAnchor();}
  {auto job=Make();LuaNativePostprocess::recorder.blocked=true;Reject([&]{job->Restore();});assert(ui::recorder.held&&job->source->detached);LuaNativePostprocess::recorder.blocked=false;job->Restore();job->ReleaseAnchor();}
  {auto job=Make();++context.registry.generation;Reject([&]{job->Accept({true,false,false,0});});--context.registry.generation;job->Restore();job->ReleaseAnchor();}
  {auto job=Make();auto exact=session->sourceSettlement;session->sourceSettlement=std::make_shared<NativeMainSourceSettlement>();Reject([&]{job->Restore();});assert(ui::recorder.held);session->sourceSettlement=exact;job->Restore();job->ReleaseAnchor();}
  {auto job=Make();session->admissionSettlement.reset();Reject([&]{job->Restore();});session->admissionSettlement=job;job->Restore();job->ReleaseAnchor();}
  {auto job=Make();auto exact=session->completionMailbox;session->completionMailbox=std::make_shared<NativeFrameCompletionMailbox>(5);Reject([&]{job->Accept({true,false,false,0});});session->completionMailbox=exact;job->Restore();job->ReleaseAnchor();}
 }
 for(unsigned mode=0;mode<2;++mode){
  auto job=Make(true);job->MarkQueued();auto younger=session->sourceCredits.Acquire(1);
  session->opaqueSourcePending=99;
  if(mode==0)job->CompleteCPU(nullptr,std::make_exception_ptr(std::runtime_error("prepare-before-Begin")));
  else {Metal4WorldPresentationResult skipped{};Reject([&]{job->CompleteCPU(&skipped,{});});}
  assert(job->failureHandled&&job->transferred&&ui::recorder.quarantined);
  assert(session->opaqueSourcePending==99); // No historical Restore over N+1.
  assert(!job->submission->Options().beginReceipt->Submission());
  Reject([&]{session->sourceCredits.Validate(younger,1);});
 }
 std::cout<<"100 extracted production scene records: real transaction/mailbox/Begin, pre-Begin restore, post-Begin quarantine, acceptance ordering, stale owner/context/anchor and occupied restore rejection PASS (engine adapters modeled)\n";
}
