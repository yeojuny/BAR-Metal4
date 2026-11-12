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
 uint64_t context=1;
 NativeTerrainProgramServices svc;
 svc.contextGeneration=[&]{return context;};
 svc.compile=[](const NativeTerrainProgramRequest& r){NativeTerrainProgramLink l;l.consumer=std::make_shared<TestConsumer>(r.identity);l.linkDefaults["value"].values=std::vector<float>{0};return l;};
 ConfigureNativeTerrainProgramSource(svc);
 auto p=CreateNativeTerrainProgramSource(NativeTerrainProgramSelection::ForwardStandard,"fixture","");
 p->Link();p->EnableRaw();int slot=p->Slot("value");
 auto before=CaptureCurrentNativeTerrainProgram();
 TakeNativeTerrainUniformWriteStats();
 auto allocationBaseline=allocations;
 p->SetUniform1f(slot,0.f);
 check(allocations==allocationBaseline);
 auto stats=TakeNativeTerrainUniformWriteStats();
 check(stats.alignedNoOps==1&&stats.mapCopies==0&&stats.publications==0);
 std::cout<<"aligned no-op: allocations=0 mapCopies=0 publications=0\n";
 check(CaptureCurrentNativeTerrainProgram().uniforms==before.uniforms);
 p->SetUniform1f(slot,-0.f);auto neg=CaptureCurrentNativeTerrainProgram();
 check(neg.uniforms!=before.uniforms);check(std::signbit(std::get<std::vector<float>>(neg.uniforms->values.at("value").values)[0]));
 p->SetUniform1f(slot,2.f);auto b=CaptureCurrentNativeTerrainProgram();
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
 auto mutableSnapshot=std::const_pointer_cast<NativeTerrainUniformSnapshot>(CaptureCurrentNativeTerrainProgram().uniforms);auto rev=mutableSnapshot->revision;
 mutableSnapshot->revision=std::numeric_limits<uint64_t>::max();rejects([&]{p->SetUniform4iv(slot,ints);});mutableSnapshot->revision=rev;
 context=2;rejects([&]{p->SetUniform1i(slot,1);});context=1;
 p->DisableRaw();rejects([&]{p->SetUniform1f(-1,0);});p.reset();ConfigureNativeTerrainProgramSource({});
 std::cout<<"PASS "<<checks<<" terrain writer contract checks; GPU/Lua/gameplay not exercised\n";
}
