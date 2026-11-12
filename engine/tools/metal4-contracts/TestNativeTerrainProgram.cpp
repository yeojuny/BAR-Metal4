// GPL v2 or later. Included after the verbatim production terrain source TU.
// External validation/compiler/VFS and shader-base services are test doubles.
static size_t allocations=0;
void* operator new(size_t size){++allocations;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
namespace rendering {
void ValidateNativeTerrainProgram(const NativeTerrainProgramIdentity& i,
 const std::shared_ptr<const NativeTerrainProgramConsumer>& c,uint64_t context){
 if(!c || i.contextGeneration!=context || !c->Matches(i) || !c->Supports(i))throw std::runtime_error("fixture invalid consumer");
}
struct TestConsumer:NativeTerrainProgramConsumer {
 NativeTerrainProgramIdentity id;
 explicit TestConsumer(NativeTerrainProgramIdentity i):id(std::move(i)){}
 uint64_t ContextGeneration()const noexcept override{return id.contextGeneration;}
 uint64_t LibraryGeneration()const noexcept override{return 1;}
 bool Matches(const NativeTerrainProgramIdentity& i)const noexcept override{return id==i;}
 bool Supports(const NativeTerrainProgramIdentity&)const noexcept override{return true;}
};
}
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
 TakeNativeTerrainUniformWriteStats();
 auto allocationBaseline=allocations;
 p->SetUniform1f(slot,0.f);
 check(allocations==allocationBaseline);
 auto stats=TakeNativeTerrainUniformWriteStats();
 check(stats.alignedNoOps==1&&stats.mapCopies==0&&stats.publications==0);
 std::cout<<"aligned no-op: allocations=0 mapCopies=0 publications=0\n";
 check(CaptureCurrentNativeTerrainProgram().uniforms==before.uniforms);
 TakeNativeTerrainUniformWriteStats();
 p->SetUniform1f(slot,-0.f);stats=TakeNativeTerrainUniformWriteStats();
 check(stats.exactChanges==1&&stats.mapCopies==1&&stats.publications==1&&stats.definitionCopies==0);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==initialDefinition);
 auto neg=CaptureCurrentNativeTerrainProgram();
 check(neg.uniforms!=before.uniforms);check(std::signbit(std::get<std::vector<float>>(neg.uniforms->values.at("value").values)[0]));
 p->SetUniform1f(slot,2.f);auto b=CaptureCurrentNativeTerrainProgram();
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==initialDefinition);
 check(b.identity==initialDefinition->identity&&b.consumer==initialDefinition->consumer);
 check(std::get<std::vector<float>>(before.uniforms->values.at("value").values)[0]==0);
 auto saved=OriginalNativeSourceProgramBinding().Current();
 p->SetUniform1f(slot,3.f);auto c=CaptureCurrentNativeTerrainProgram();
 OriginalNativeSourceProgramBinding().Restore(saved);TakeNativeTerrainUniformWriteStats();
 p->SetUniform1f(slot,3.f);stats=TakeNativeTerrainUniformWriteStats();
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
 check(stats.stalePublicationFallbacks==1);check(bool(CaptureCurrentNativeTerrainProgram().consumer));
 // Relink keeps previously written names, but old source-revision bindings fail.
 auto oldBinding=OriginalNativeSourceProgramBinding().Current();
 p->Reload(true,false);OriginalNativeSourceProgramBinding().Restore(oldBinding);
 rejects([&]{p->SetUniform4iv(slot,ints);});
 p->DisableRaw();p->EnableRaw();check(std::get<std::vector<int32_t>>(CaptureCurrentNativeTerrainProgram().uniforms->values.at("value").values)[3]==5);
 auto reloadedDefinition=OriginalNativeSourceProgramBinding().Current().terrainSource->definition;
 check(reloadedDefinition!=oldBinding.terrainSource->definition);
 check(oldBinding.terrainSource->definition==initialDefinition);
 check(initialDefinition->identity.sourceRevision<reloadedDefinition->identity.sourceRevision);
 check(before.consumer==initialDefinition->consumer);
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
 check(imported.identity.originalGLName==8&&imported.identity.originalProgramGeneration==99);
 check(importedDefinition!=invalid.terrainSource->definition);
 luaSlot=lua->Slot("value");TakeNativeTerrainUniformWriteStats();lua->SetUniform1f(luaSlot,9);stats=TakeNativeTerrainUniformWriteStats();
 check(stats.definitionCopies==0&&stats.publications==1);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition==importedDefinition);
 check(std::get<std::vector<float>>(imported.uniforms->values.at("value").values)[0]==0);
 lua->DisableRaw();lua->Release();lua->EnableRaw();
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->state==NativeTerrainProgramPublication::State::Unlinked);
 check(OriginalNativeSourceProgramBinding().Current().terrainSource->definition!=importedDefinition);
 rejects([&]{CaptureCurrentNativeTerrainProgram();});lua->DisableRaw();lua.reset();ConfigureNativeTerrainProgramSource({});
 std::cout<<"PASS "<<checks<<" terrain writer contract checks; GPU/Lua/gameplay not exercised\n";
}
