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
 Facts facts;
 bool enabled,diagnostic;
 std::uint64_t requests=0,hits=0,positiveCasts=0,fallbacks=0;
 std::uint64_t providerWallNs=0,providerCPUNs=0,invalidCPU=0,scopeMismatch=0;
 explicit ModelTypeFactsScope(RuntimeLease& lease,std::uint64_t submission)
  :facts(Identity(lease,submission),Diagnostic()),enabled(Enabled()),diagnostic(Diagnostic()){}
 ~ModelTypeFactsScope(){
  if(!diagnostic)return;
  // These count-only records use the existing sampled job buffer and are
  // printed after Join. Their tiny emission duration is NOT provider time.
  const auto record=[](NativeCPUPhaseKind kind,std::uint64_t a,std::uint64_t b){NativeCPUPhaseScope row(kind);row.Counts(a,b);};
  record(NativeCPUPhaseKind::ModelFactsRequests,requests,hits);
  record(NativeCPUPhaseKind::ModelFactsPositive,positiveCasts,fallbacks);
  record(NativeCPUPhaseKind::ModelFactsCasts,hits*10,facts.Counts().probes);
  record(NativeCPUPhaseKind::ModelFactsCapacity,facts.Size(),facts.Counts().full);
  record(NativeCPUPhaseKind::ModelFactsProviderTime,providerWallNs,providerCPUNs);
  record(NativeCPUPhaseKind::ModelFactsCPUValidity,invalidCPU,scopeMismatch);
 }
 struct ProviderCall {
  ModelTypeFactsScope* scope;
  Facts::Scope identity;
  bool matches=false;
  std::uint64_t wall=0;NativeThreadCPUStamp cpu{};
  static std::uint64_t Now()noexcept{return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();}
  ProviderCall(ModelTypeFactsScope* owner,RuntimeLease& lease,std::uint64_t submission)
   :scope(owner),identity(Identity(lease,submission)){
   if(!scope)return;matches=scope->facts.Matches(identity);
   if(scope->diagnostic){++scope->requests;if(!matches)++scope->scopeMismatch;wall=Now();cpu=ReadNativeThreadCPUTime();}
  }
  ~ProviderCall(){
   if(!scope||!scope->diagnostic)return;
   const auto endCPU=ReadNativeThreadCPUTime();const auto end=Now();scope->providerWallNs+=end-wall;
   if(cpu.valid&&endCPU.valid&&endCPU.ns>=cpu.ns)scope->providerCPUNs+=endCPU.ns-cpu.ns;else ++scope->invalidCPU;
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
  }
 };
};
