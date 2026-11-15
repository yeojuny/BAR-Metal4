// Included in the renderer's private namespace. Only the synchronous
// sourceImages callback borrows this local CPU fact scope.
struct ModelTypeFactsScope {
 using Facts=NativePositiveBackingFacts<NativeModelTextureBacking>;
 static bool Enabled()noexcept {
  static const bool enabled=[] {const char* v=std::getenv("BAR_NATIVE_MODEL_TYPE_FACTS");return v&&v[0]=='1'&&!v[1];}();return enabled;
 }
 static bool Diagnostic()noexcept {
  const auto context=CaptureNativeCPUPhaseContext();return context.sampled&&context.buffer;
 }
 static bool Wanted()noexcept{return Enabled()||Diagnostic();}
 static Facts::Scope Identity(RuntimeLease& lease,std::uint64_t submission)noexcept {
  return {&lease,lease.ContextIdentity(),submission==lease.SourceSubmission()?submission:0};
 }
 struct Duration {std::uint64_t wall=0,cpu=0;};
 Facts facts;
 bool enabled,diagnostic;
 std::uint64_t requests=0,hits=0,positiveCasts=0,fallbacks=0;
 std::uint64_t providerWallNs=0,providerCPUNs=0,invalidCPU=0,scopeMismatch=0;
 std::uint64_t completedModel=0,completedOther=0,failed=0,invalidWall=0;
 Duration dispatchTime{},preparationTime{},otherTime{},failureTime{};
 explicit ModelTypeFactsScope(RuntimeLease& lease,std::uint64_t submission)
  :facts(Identity(lease,submission),Diagnostic()),enabled(Enabled()),diagnostic(Diagnostic()){}
 ~ModelTypeFactsScope(){
  if(!diagnostic)return;
  // One parent identifies an exact local provider scope, even if a job has
  // multiple scopes. Records stay in the existing bounded, sampled job buffer.
  // Their emission duration is NOT provider work. count_a/count_b are the data.
  NativeCPUPhaseScope summary(NativeCPUPhaseKind::ModelProviderSummary);
  summary.Counts(requests,completedModel+completedOther);
  const auto record=[](NativeCPUPhaseKind kind,std::uint64_t a,std::uint64_t b){NativeCPUPhaseScope row(kind);row.Counts(a,b);};
  record(NativeCPUPhaseKind::ModelFactsRequests,requests,hits);
  record(NativeCPUPhaseKind::ModelFactsPositive,positiveCasts,fallbacks);
  record(NativeCPUPhaseKind::ModelFactsCasts,hits*10,facts.Counts().probes);
  record(NativeCPUPhaseKind::ModelFactsCapacity,facts.Size(),facts.Counts().full);
  record(NativeCPUPhaseKind::ModelFactsProviderTime,providerWallNs,providerCPUNs);
  record(NativeCPUPhaseKind::ModelFactsCPUValidity,invalidCPU,scopeMismatch);
  record(NativeCPUPhaseKind::ModelProviderCompleted,completedModel,completedOther);
  record(NativeCPUPhaseKind::ModelProviderFailures,failed,invalidWall);
  record(NativeCPUPhaseKind::ModelProviderDispatchTime,dispatchTime.wall,dispatchTime.cpu);
  record(NativeCPUPhaseKind::ModelProviderPreparationTime,preparationTime.wall,preparationTime.cpu);
  record(NativeCPUPhaseKind::ModelProviderOtherTime,otherTime.wall,otherTime.cpu);
  record(NativeCPUPhaseKind::ModelProviderFailureTime,failureTime.wall,failureTime.cpu);
 }
 struct ProviderCall {
  ModelTypeFactsScope* scope;
  Facts::Scope identity;
  bool matches=false,modelBoundary=false;
  unsigned boundaries=0;
  int exceptions=0;
  std::uint64_t wall=0,modelWall=0;
  NativeThreadCPUStamp cpu{},modelCPU{};
  static std::uint64_t Now()noexcept {
   // The renderer's diagnostic Clock is deliberately {} in a live build.
   // This timer is entered only for the existing sampled diagnostic scope.
   return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  ProviderCall(ModelTypeFactsScope* owner,RuntimeLease& lease,std::uint64_t submission)
   :scope(owner),identity(Identity(lease,submission)){
   if(!scope)return;matches=scope->facts.Matches(identity);
   if(scope->diagnostic){
    ++scope->requests;if(!matches)++scope->scopeMismatch;
    exceptions=std::uncaught_exceptions();wall=Now();cpu=ReadNativeThreadCPUTime();
   }
  }
  ~ProviderCall(){
   if(!scope||!scope->diagnostic)return;
   // Same stamp order at all three boundaries. Values include observer cost;
   // they are inclusive provider envelopes, not exclusive CPU/GPU attribution.
   const auto end=Now();const auto endCPU=ReadNativeThreadCPUTime();
   const bool failure=std::uncaught_exceptions()>exceptions;
   if(failure)++scope->failed;
   else if(modelBoundary)++scope->completedModel;
   else ++scope->completedOther;
   const bool split=modelBoundary&&!failure;
   const bool wallValid=end>=wall&&(!split||(boundaries==1&&modelWall>=wall&&modelWall<=end));
   const bool cpuValid=cpu.valid&&endCPU.valid&&endCPU.ns>=cpu.ns&&
    (!split||(boundaries==1&&modelCPU.valid&&modelCPU.ns>=cpu.ns&&modelCPU.ns<=endCPU.ns));
   if(wallValid){
    scope->providerWallNs+=end-wall;
    if(split){scope->dispatchTime.wall+=modelWall-wall;scope->preparationTime.wall+=end-modelWall;}
    else (failure?scope->failureTime:scope->otherTime).wall+=end-wall;
   }else ++scope->invalidWall;
   if(cpuValid){
    scope->providerCPUNs+=endCPU.ns-cpu.ns;
    if(split){scope->dispatchTime.cpu+=modelCPU.ns-cpu.ns;scope->preparationTime.cpu+=endCPU.ns-modelCPU.ns;}
    else (failure?scope->failureTime:scope->otherTime).cpu+=endCPU.ns-cpu.ns;
   }else ++scope->invalidCPU;
  }
  bool ReuseEnabled()const noexcept{return scope&&scope->enabled&&matches;}
  std::shared_ptr<const NativeModelTextureBacking> Find(const std::shared_ptr<const NativeTextureBacking>& backing)noexcept {
   auto model=ReuseEnabled()?scope->facts.Find(identity,backing):std::shared_ptr<const NativeModelTextureBacking>{};
   if(scope&&scope->diagnostic){if(model)++scope->hits;else ++scope->fallbacks;}return model;
  }
  void Positive(const std::optional<Facts::Proof>& proof)noexcept {
   if(!scope)return;
   if(scope->diagnostic){if(!ReuseEnabled()||proof)++scope->positiveCasts;}
   if(ReuseEnabled()&&proof)scope->facts.Remember(identity,*proof);
   // OriginalModelImage calls Positive once, after model payload validation
   // and before Texture/sampler/registration/readability-wrapper preparation.
   // Keep proof insertion in dispatch cost. Do not query readiness, execute
   // callbacks, retain images or change provider decisions for this diagnostic.
   if(scope->diagnostic){
    ++boundaries;modelBoundary=true;modelWall=Now();modelCPU=ReadNativeThreadCPUTime();
   }
  }
  ProviderCall(const ProviderCall&)=delete;
  ProviderCall& operator=(const ProviderCall&)=delete;
 };
};
