// GPL v2 or later. Included after the verbatim production terrain source TU.
// External validation/compiler/VFS and shader-base services are test doubles.
static size_t allocations=0;
static int failAllocationAfter=-1;
static size_t validationCalls=0;
void* operator new(size_t size){if(failAllocationAfter==0){failAllocationAfter=-1;throw std::bad_alloc();}if(failAllocationAfter>0)--failAllocationAfter;++allocations;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
namespace rendering {
void ValidateNativeTerrainProgram(const NativeTerrainProgramIdentity& i,
 const std::shared_ptr<const NativeTerrainProgramConsumer>& c,uint64_t context){
 ++validationCalls;
 if(!c || i.contextGeneration!=context || !c->Matches(i) || !c->Supports(i))throw std::runtime_error("fixture invalid consumer");
}
struct TestConsumer:NativeTerrainProgramConsumer {
 const NativeTerrainProgramIdentity id;
 explicit TestConsumer(NativeTerrainProgramIdentity i):id(std::move(i)){}
 bool HasImmutableDefinition()const noexcept override{return true;}
 uint64_t ContextGeneration()const noexcept override{return id.contextGeneration;}
 uint64_t LibraryGeneration()const noexcept override{return 1;}
 bool Matches(const NativeTerrainProgramIdentity& i)const noexcept override{return id==i;}
 bool Supports(const NativeTerrainProgramIdentity&)const noexcept override{return true;}
};
}
// Trace source observations only, excluding additional eager-reference hook calls.
// Hash exact float/integer bytes, metadata and logical revision, not owner addresses.
static uint64_t observationFingerprint=14695981039346656037ULL;
static void TraceBytes(const void* bytes,size_t count){const auto* p=static_cast<const unsigned char*>(bytes);for(size_t i=0;i<count;++i)observationFingerprint=(observationFingerprint^p[i])*1099511628211ULL;}
template<class T>static void TraceScalar(const T& value){TraceBytes(&value,sizeof(value));}
static rendering::NativeTerrainProgramSnapshot TraceCapture(){
 auto result=rendering::CaptureCurrentNativeTerrainProgram();
 TraceScalar(result.Identity().sourceRevision);TraceScalar(result.uniforms->revision);
 for(const auto& [name,value]:result.uniforms->values){TraceBytes(name.data(),name.size());TraceScalar(value.rows);TraceScalar(value.columns);TraceScalar(value.count);TraceScalar(value.transpose);auto type=value.values.index();TraceScalar(type);
  std::visit([](const auto& values){auto size=values.size();TraceScalar(size);TraceBytes(values.data(),values.size()*sizeof(values[0]));},value.values);
 }
 return result;
}
#define CaptureCurrentNativeTerrainProgram TraceCapture
static unsigned checks=0;
void check(bool yes){++checks;if(!yes)throw std::runtime_error("check "+std::to_string(checks)+" failed");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}check(failed);}
int main(){using namespace rendering;
 uint64_t context=1;bool failCompile=false;
 NativeTerrainProgramServices svc;
 svc.contextGeneration=[&]{return context;};
 svc.compile=[&](const NativeTerrainProgramRequest& r){if(failCompile)throw std::runtime_error("fixture compiler failure");NativeTerrainProgramLink l;l.consumer=std::make_shared<TestConsumer>(r.identity);l.linkDefaults["value"].values=std::vector<float>{0};return l;};
 ConfigureNativeTerrainProgramSource(svc);
 auto p=CreateNativeTerrainProgramSource(NativeTerrainProgramSelection::ForwardStandard,"fixture","");
 p->Link();p->EnableRaw();int slot=p->Slot("value");
 auto before=CaptureCurrentNativeTerrainProgram();
 auto initialDefinition=OriginalNativeSourceProgramBinding().Current().terrainSource->definition;
 check(before.definition==initialDefinition);
 // Certified source capture reuses static proof; generic definition still validates.
 // Stub callback counts are structural evidence, not real-validator performance.
 auto captureAllocations=allocations;auto validations=validationCalls;
 auto repeatedCapture=CaptureCurrentNativeTerrainProgram();
 check(allocations==captureAllocations);
 check(validationCalls==validations);
 NativeTerrainProgramDefinition unvalidated(before.Identity(),before.Consumer());
 ValidateNativeTerrainDefinition(unvalidated,1);check(validationCalls==validations+1);
 rejects([&]{ValidateNativeTerrainDefinition(*before.definition,2);});
 NativeTerrainProgramDefinition nullConsumer(before.Identity(),{});
 rejects([&]{ValidateNativeTerrainDefinition(nullConsumer,1);});
 check(repeatedCapture.definition==before.definition&&repeatedCapture.uniforms==before.uniforms);
 check(&repeatedCapture.Identity()==&before.Identity());
 check(&repeatedCapture.Consumer()==&before.Consumer());
 NativeTerrainProgramSnapshot missingDefinition;
 rejects([&]{missingDefinition.Identity();});rejects([&]{missingDefinition.Consumer();});
 TakeNativeTerrainUniformWriteStats();
 auto allocationBaseline=allocations;
 p->SetUniform1f(slot,0.f);
 check(allocations==allocationBaseline);
 auto stats=TakeNativeTerrainUniformWriteStats();
 check(stats.alignedNoOps==1&&stats.mapCopies==0&&stats.publications==0);
 std::cout<<"aligned no-op: allocations=0 mapCopies=0 publications=0\n";
 check(CaptureCurrentNativeTerrainProgram().uniforms==before.uniforms);
 // Three accepted writes with no owning observation: inspect/kind must not seal.
 TakeNativeTerrainUniformWriteStats();
 const auto priorRevision=before.uniforms->revision;
 p->SetUniform1f(slot,41.f);p->SetUniform1f(slot,42.f);p->SetUniform1f(slot,43.f);
 OriginalNativeSourceProgramBinding().InspectCurrent();OriginalNativeSourceProgramBinding().CurrentKind();
 stats=TakeNativeTerrainUniformWriteStats();
#ifdef BAR_NATIVE_TERRAIN_EAGER_REFERENCE
 check(stats.mapCopies==3&&stats.publications==3);
#else
 check(stats.mapCopies==1&&stats.publications==0);
#endif
 check(stats.exactChanges==3);
 auto batch=CaptureCurrentNativeTerrainProgram();stats=TakeNativeTerrainUniformWriteStats();
#ifdef BAR_NATIVE_TERRAIN_EAGER_REFERENCE
 check(stats.publications==0);
#else
 check(stats.publications==1);
#endif
 check(batch.uniforms->revision==priorRevision+3);
 check(std::get<std::vector<float>>(batch.uniforms->values.at("value").values)[0]==43.f);
 check(std::get<std::vector<float>>(before.uniforms->values.at("value").values)[0]==0.f);
 p->SetUniform1f(slot,0.f);CaptureCurrentNativeTerrainProgram();
 TakeNativeTerrainUniformWriteStats();
 p->SetUniform1f(slot,-0.f);OriginalNativeSourceProgramBinding().Current();stats=TakeNativeTerrainUniformWriteStats();
 check(stats.exactChanges==1&&stats.mapCopies==1&&stats.publications==1&&stats.definitionCopies==0);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==initialDefinition);
 auto neg=CaptureCurrentNativeTerrainProgram();
 check(neg.definition==before.definition);
 check(neg.uniforms!=before.uniforms);check(std::signbit(std::get<std::vector<float>>(neg.uniforms->values.at("value").values)[0]));
 p->SetUniform1f(slot,2.f);auto b=CaptureCurrentNativeTerrainProgram();
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==initialDefinition);
 check(b.Identity()==initialDefinition->identity&&b.Consumer()==initialDefinition->consumer);
 check(std::get<std::vector<float>>(before.uniforms->values.at("value").values)[0]==0);
 // Inspection is a borrowed immediate view, not a retained SaveCurrent snapshot.
 auto& binding=OriginalNativeSourceProgramBinding();
 const auto& inspection=binding.InspectCurrent();const auto* selectionAddress=&inspection;
 const auto sourceOwners=inspection.terrainSource.use_count();
 const auto consumerOwners=inspection.terrain.use_count();
 allocationBaseline=allocations;
 check(&binding.InspectCurrent()==selectionAddress);
 check(binding.CurrentKind()==NativeSourceProgramBinding::Kind::Terrain);
 check(binding.InspectCurrent().terrainSource.use_count()==sourceOwners);
 check(binding.InspectCurrent().terrain.use_count()==consumerOwners);
 check(allocations==allocationBaseline);
 // Nested save/restore mirrors an ActiveShader callback unwinding an exception.
 auto outer=binding.SaveCurrent();auto outerOwner=outer.terrainSource;
 auto nonterrainOwner=std::make_shared<int>(42);
 binding.BindCUS(nonterrainOwner);auto inner=binding.SaveCurrent();
 check(binding.CurrentKind()==NativeSourceProgramBinding::Kind::CUS);
 try{
  binding.Restore(outer);p->SetUniform1f(slot,17.f);
  check(outer.terrainSource==outerOwner);
  check(std::get<std::vector<float>>(outerOwner->uniforms->values.at("value").values)[0]==2.f);
  throw 17; // Dedicated fixture exception, never swallow check failures.
 }catch(int){binding.Restore(inner);}
 check(binding.CurrentKind()==NativeSourceProgramBinding::Kind::CUS);
 check(binding.InspectCurrent().owner==nonterrainOwner);
 binding.Restore(outer);rejects([&]{CaptureCurrentNativeTerrainProgram();});
 TakeNativeTerrainUniformWriteStats();p->SetUniform1f(slot,17.f);
 OriginalNativeSourceProgramBinding().Current();stats=TakeNativeTerrainUniformWriteStats();check(stats.stalePublicationFallbacks==1&&stats.publications==1);
 check(std::get<std::vector<float>>(CaptureCurrentNativeTerrainProgram().uniforms->values.at("value").values)[0]==17.f);
 check(outer.terrainSource==outerOwner);
 // The borrowed Selection address stays stable but observes subsequent bindings.
 binding.Restore(inner);check(selectionAddress==&binding.InspectCurrent());
 check(inspection.kind==NativeSourceProgramBinding::Kind::CUS);
 binding.Restore(outer);p->SetUniform1f(slot,2.f);
 auto saved=OriginalNativeSourceProgramBinding().Current();
 p->SetUniform1f(slot,3.f);auto c=CaptureCurrentNativeTerrainProgram();
 OriginalNativeSourceProgramBinding().Restore(saved);TakeNativeTerrainUniformWriteStats();
 p->SetUniform1f(slot,3.f);OriginalNativeSourceProgramBinding().Current();stats=TakeNativeTerrainUniformWriteStats();
 check(stats.stalePublicationFallbacks==1&&stats.mapCopies==1&&stats.publications==1);
 check(CaptureCurrentNativeTerrainProgram().uniforms!=c.uniforms);
 check(std::get<std::vector<float>>(b.uniforms->values.at("value").values)[0]==2);
 float values[]{1,2,3,4};p->SetUniform1fv(slot,4,values);auto array=CaptureCurrentNativeTerrainProgram();
 p->SetUniform1fv(slot,4,values);check(CaptureCurrentNativeTerrainProgram().uniforms==array.uniforms);
 values[3]=5;p->SetUniform1fv(slot,4,values);check(CaptureCurrentNativeTerrainProgram().uniforms!=array.uniforms);
 auto changed=CaptureCurrentNativeTerrainProgram();p->SetUniform4fv(slot,values);check(CaptureCurrentNativeTerrainProgram().uniforms!=changed.uniforms);
 p->SetUniformMatrix2fv(slot,false,values);auto matrix=CaptureCurrentNativeTerrainProgram();
 p->SetUniformMatrix2fv(slot,true,values);check(CaptureCurrentNativeTerrainProgram().uniforms!=matrix.uniforms);
 int ints[]{1,2,3,5};auto floating=CaptureCurrentNativeTerrainProgram();p->SetUniform4iv(slot,ints);check(CaptureCurrentNativeTerrainProgram().uniforms!=floating.uniforms);
 auto integer=CaptureCurrentNativeTerrainProgram();p->SetUniform4iv(slot,ints);check(CaptureCurrentNativeTerrainProgram().uniforms==integer.uniforms);
 p->SetUniform1fv(-1,-3,nullptr);p->SetUniform1fv(999,0,nullptr);check(CaptureCurrentNativeTerrainProgram().uniforms==integer.uniforms);
 rejects([&]{p->SetUniform1fv(slot,-1,values);});rejects([&]{p->SetUniform1fv(slot,1,nullptr);});rejects([&]{p->SetUniform1f(999,1);});
 rejects([&]{p->SetUniform1f(slot,std::numeric_limits<float>::infinity());});rejects([&]{p->SetUniform1f(slot,std::numeric_limits<float>::quiet_NaN());});
 saved=OriginalNativeSourceProgramBinding().Current();OriginalNativeSourceProgramBinding().Unbind();rejects([&]{p->SetUniform1i(slot,1);});OriginalNativeSourceProgramBinding().Restore(saved);
 // A malformed restored selection must take the existing repair/publication path.
 saved=OriginalNativeSourceProgramBinding().Current();auto malformed=saved;
 malformed.kind=NativeSourceProgramBinding::Kind::CUS;malformed.terrain.reset();
 OriginalNativeSourceProgramBinding().Restore(malformed);TakeNativeTerrainUniformWriteStats();
 p->SetUniform4iv(slot,ints);stats=TakeNativeTerrainUniformWriteStats();
 check(stats.stalePublicationFallbacks==1);check(bool(CaptureCurrentNativeTerrainProgram().Consumer()));
 // Relink keeps previously written names, but old source-revision bindings fail.
 auto oldBinding=OriginalNativeSourceProgramBinding().Current();
 p->Reload(true,false);OriginalNativeSourceProgramBinding().Restore(oldBinding);
 rejects([&]{p->SetUniform4iv(slot,ints);});
 p->DisableRaw();p->EnableRaw();check(std::get<std::vector<int32_t>>(CaptureCurrentNativeTerrainProgram().uniforms->values.at("value").values)[3]==5);
 auto reloadedDefinition=OriginalNativeSourceProgramBinding().Current().terrainSource->definition;
 auto reloadedSnapshot=CaptureCurrentNativeTerrainProgram();
 check(reloadedSnapshot.definition==reloadedDefinition&&reloadedSnapshot.definition!=before.definition);
 check(before.Identity().sourceRevision<reloadedSnapshot.Identity().sourceRevision);
 check(reloadedDefinition!=oldBinding.terrainSource->definition);
 check(oldBinding.terrainSource->definition==initialDefinition);
 check(initialDefinition->identity.sourceRevision<reloadedDefinition->identity.sourceRevision);
 check(before.Consumer()==initialDefinition->consumer);
 // A throwing compiler invalidates source identity before any new publication.
 auto beforeFailure=OriginalNativeSourceProgramBinding().Current();failCompile=true;
 rejects([&]{p->Reload(true,false);});failCompile=false;
 TakeNativeTerrainUniformWriteStats();rejects([&]{p->SetUniform4iv(slot,ints);});
 check(TakeNativeTerrainUniformWriteStats().alignedNoOps==0);
 rejects([&]{CaptureCurrentNativeTerrainProgram();});
 check(beforeFailure.terrainSource->definition==reloadedDefinition);
 p->Reload(true,false);p->DisableRaw();p->EnableRaw();
 auto mutableSnapshot=std::const_pointer_cast<NativeTerrainUniformSnapshot>(CaptureCurrentNativeTerrainProgram().uniforms);auto rev=mutableSnapshot->revision;
 mutableSnapshot->revision=std::numeric_limits<uint64_t>::max();rejects([&]{p->SetUniform4iv(slot,ints);});mutableSnapshot->revision=rev;
 context=2;rejects([&]{p->SetUniform1i(slot,1);});context=1;
 p->DisableRaw();rejects([&]{p->SetUniform1f(-1,0);});p.reset();ConfigureNativeTerrainProgramSource({});
 // Unsupported, invalid and observed Lua imports retain distinct source states.
 svc.resolveLua=[&](uint32_t handle){NativeTerrainLuaProgramImport i;
  if(handle==7){i.unsupportedReason="unsupported fixture shader";return i;}
  i.status=NativeTerrainLuaProgramImport::Status::Observed;
  i.request.vertexSource="v";i.request.fragmentSource="f";
  auto hash=[](const std::string& x){uint64_t h=14695981039346656037ULL;for(unsigned char c:x)h=(h^c)*1099511628211ULL;return h;};
  i.request.identity.contextGeneration=context;i.request.identity.originalProgramGeneration=99;
  i.request.identity.sourceRevision=1;i.request.identity.originalGLName=handle;
  i.request.identity.vertexSourceHash=hash(i.request.vertexSource);i.request.identity.fragmentSourceHash=hash(i.request.fragmentSource);
  return i;};
 ConfigureNativeTerrainProgramSource(svc);
 auto lua=CreateNativeTerrainProgramSource(NativeTerrainProgramSelection::LuaForward,"lua-fixture","");
 LoadNativeTerrainLuaProgram(lua.get(),7);lua->EnableRaw();int luaSlot=lua->Slot("value");
 auto unsupported=OriginalNativeSourceProgramBinding().Current();
 check(unsupported.kind==NativeSourceProgramBinding::Kind::UnsupportedTerrain);
 check(unsupported.terrainSource->state==NativeTerrainProgramPublication::State::Unsupported);
 rejects([&]{CaptureCurrentNativeTerrainProgram();});
 lua->SetUniform1f(luaSlot,4);auto unsupportedDefinition=OriginalNativeSourceProgramBinding().Current().terrainSource->definition;
 TakeNativeTerrainUniformWriteStats();lua->SetUniform1f(luaSlot,4);check(TakeNativeTerrainUniformWriteStats().alignedNoOps==1);
 check(unsupportedDefinition==unsupported.terrainSource->definition);
 lua->DisableRaw();LoadNativeTerrainLuaProgram(lua.get(),0);lua->EnableRaw();
 auto invalid=OriginalNativeSourceProgramBinding().Current();check(invalid.kind==NativeSourceProgramBinding::Kind::Unbound);
 check(invalid.terrainSource->state==NativeTerrainProgramPublication::State::Invalid);
 check(invalid.terrainSource->definition!=unsupportedDefinition);rejects([&]{CaptureCurrentNativeTerrainProgram();});
 lua->DisableRaw();LoadNativeTerrainLuaProgram(lua.get(),8);lua->EnableRaw();
 auto imported=CaptureCurrentNativeTerrainProgram();auto importedDefinition=OriginalNativeSourceProgramBinding().Current().terrainSource->definition;
 check(imported.Identity().originalGLName==8&&imported.Identity().originalProgramGeneration==99);
 check(importedDefinition!=invalid.terrainSource->definition);
 luaSlot=lua->Slot("value");TakeNativeTerrainUniformWriteStats();lua->SetUniform1f(luaSlot,9);OriginalNativeSourceProgramBinding().Current();stats=TakeNativeTerrainUniformWriteStats();
 check(stats.definitionCopies==0&&stats.publications==1);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==importedDefinition);
 check(std::get<std::vector<float>>(imported.uniforms->values.at("value").values)[0]==0);
 lua->DisableRaw();lua->Release();lua->EnableRaw();
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->state==NativeTerrainProgramPublication::State::Unlinked);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition!=importedDefinition);
 rejects([&]{CaptureCurrentNativeTerrainProgram();});lua->DisableRaw();lua.reset();
 // A saved binding owns publications, not the original writer object.
 auto dying=CreateNativeTerrainProgramSource(NativeTerrainProgramSelection::ForwardStandard,"dying","");
 dying->Link();dying->EnableRaw();auto orphan=binding.SaveCurrent();
 auto retainedCapture=CaptureCurrentNativeTerrainProgram();
 auto retainedGeneration=retainedCapture.Identity().originalProgramGeneration;
 dying.reset();
 check(retainedCapture.Identity().originalProgramGeneration==retainedGeneration);
 check(bool(retainedCapture.Consumer()));
 check(retainedCapture.definition==orphan.terrainSource->definition);
 check(binding.CurrentKind()==NativeSourceProgramBinding::Kind::Unbound);
 binding.Restore(orphan);rejects([&]{CaptureCurrentNativeTerrainProgram();});binding.Unbind();
 auto replacement=CreateNativeTerrainProgramSource(NativeTerrainProgramSelection::ForwardStandard,"replacement","");
 replacement->Link();replacement->EnableRaw();auto replacementBinding=binding.SaveCurrent();
 binding.Restore(orphan);rejects([&]{CaptureCurrentNativeTerrainProgram();});
 binding.Restore(replacementBinding);check(bool(CaptureCurrentNativeTerrainProgram().Consumer()));
 // Current/Save seal without adding capture-time context validation.
 const int replacementSlot=replacement->Slot("value");
 replacement->SetUniform1f(replacementSlot,23.f);context=2;
 auto contextSaved=binding.SaveCurrent();
 check(std::get<std::vector<float>>(contextSaved.terrainSource->uniforms->values.at("value").values)[0]==23.f);
 rejects([&]{CaptureCurrentNativeTerrainProgram();});context=1;
 check(std::get<std::vector<float>>(CaptureCurrentNativeTerrainProgram().uniforms->values.at("value").values)[0]==23.f);
 // Targeted allocation fault probes, not an exhaustive strong-exception proof.
 // Keep these extra observations outside the cross-mode semantic fingerprint.
#undef CaptureCurrentNativeTerrainProgram
 auto faultSnapshot=rendering::CaptureCurrentNativeTerrainProgram();
 for(int failurePoint:{0,1,2}){
  failAllocationAfter=failurePoint;bool threw=false;
  try{replacement->SetUniform1f(replacementSlot,24.f);}catch(const std::bad_alloc&){threw=true;}
  failAllocationAfter=-1;check(threw);
  check(rendering::CaptureCurrentNativeTerrainProgram().uniforms==faultSnapshot.uniforms);
 }
#ifndef BAR_NATIVE_TERRAIN_EAGER_REFERENCE
 replacement->SetUniform1f(replacementSlot,25.f);
 // Failure allocating the next input cannot mutate the existing dirty map.
 failAllocationAfter=0;bool updateFailed=false;
 try{replacement->SetUniform1f(replacementSlot,26.f);}catch(const std::bad_alloc&){updateFailed=true;}
 failAllocationAfter=-1;check(updateFailed);
 // Publication allocation failure leaves pending writer available for retry.
 failAllocationAfter=0;bool sealFailed=false;
 try{binding.SaveCurrent();}catch(const std::bad_alloc&){sealFailed=true;}
 failAllocationAfter=-1;check(sealFailed);
 check(std::get<std::vector<float>>(faultSnapshot.uniforms->values.at("value").values)[0]==23.f);
 auto retried=binding.SaveCurrent();
 check(std::get<std::vector<float>>(retried.terrainSource->uniforms->values.at("value").values)[0]==25.f);
 check(retried.terrainSource->uniforms->revision==faultSnapshot.uniforms->revision+1);
#endif
 // Typed slot index must follow the owning immutable snapshot across writes.
 const int typedSlot=replacement->Slot("borderMinHeight");
 replacement->SetUniform1f(typedSlot,31.f);
 auto typedOld=CaptureCurrentNativeTerrainProgram();
 auto* typedValue=typedOld.uniforms->Find(NativeTerrainUniformSlot::borderMinHeight);
 check(typedValue==&typedOld.uniforms->values.at("borderMinHeight"));
 check(std::get<std::vector<float>>(typedValue->values)[0]==31.f);
 replacement->SetUniform1f(typedSlot,32.f);replacement->SetUniform1f(typedSlot,33.f);
 auto typedNew=CaptureCurrentNativeTerrainProgram();
 check(typedNew.uniforms->Find(NativeTerrainUniformSlot::borderMinHeight)!=typedValue);
 check(std::get<std::vector<float>>(typedNew.uniforms->Find(NativeTerrainUniformSlot::borderMinHeight)->values)[0]==33.f);
 check(std::get<std::vector<float>>(typedValue->values)[0]==31.f);
 replacement->DisableRaw();replacement.reset();binding.Unbind();ConfigureNativeTerrainProgramSource({});
 std::cout<<"observationFingerprint="<<observationFingerprint<<"\n";
 std::cout<<"PASS "<<checks<<" terrain writer contract checks; GPU/Lua/gameplay not exercised\n";
}
