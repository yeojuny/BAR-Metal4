template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
std::shared_ptr<ScenePendingAdmission> Make(){
 ui::recorder={};LuaNativePostprocess::recorder={};context.registry.generation=3;session=std::make_unique<Session>();
 auto packet=std::make_shared<const Metal4WorldSubmission>();auto mailbox=session->completionMailbox;
 auto result=std::make_shared<ScenePendingAdmission>(*session,packet,mailbox->Prepare(mailbox->Epoch(),0,9));
 session->admissionSettlement=result;result->Transfer();return result;
}
int main(){
 for(unsigned i=0;i<100;++i){
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
 std::cout<<"100 extracted production scene records: real transaction/mailbox/Begin, pre-Begin restore, post-Begin quarantine, acceptance ordering, stale owner/context/anchor and occupied restore rejection PASS (engine adapters modeled)\n";
}
