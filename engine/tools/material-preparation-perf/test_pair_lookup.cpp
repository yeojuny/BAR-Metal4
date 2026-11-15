// Actual CUSSharedPreparationScope, modeled Metal/runtime and texture objects.
#include "Rendering/Metal4/Metal4SharedProgramPilot.h"
#include <cassert>
#include <iostream>
#include <new>
static bool countNew=false;
static size_t allocations=0;
void* operator new(std::size_t n){if(countNew)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace rendering;
using namespace rendering::metal4;
struct Proof final:CUSCompletedModelImageProof {
 bool ready=true;mutable size_t calls=0;
 bool ValidFor(RuntimeLease&,const NativeCUSTextureBinding&,const CUSSourceImage&)const override{++calls;return ready;}
};
std::shared_ptr<sharedpilot::MaterialContent> Material(std::shared_ptr<const sharedpilot::ProgramInterface> p){
 auto m=std::make_shared<sharedpilot::MaterialContent>();m->program=p;
 for(unsigned slot=0;slot<2;++slot){NativeCUSTextureBinding b;b.object=std::make_shared<NativeCUSTextureObject>();m->bindings.push_back({slot,b,b.object});}
 return m;
}
template<class F>void Throws(F f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}assert(threw);}
int main(int argc,char** argv){
 assert(argc==2);const bool optimized=std::string(argv[1])=="1";
 MTLDevice dev,otherDev;RuntimeLease lease;lease.device=&dev;
 auto program=std::make_shared<sharedpilot::ProgramInterface>();program->generation=1;
 auto material=Material(program);auto proof=std::make_shared<Proof>();
 std::array<CUSSourceImage,2> input{},output{};
 for(unsigned i=0;i<2;++i)input[i]={material->bindings[i].captured.object,proof,int(i+10)};
 CUSSharedPreparationScope cache(lease,1);std::vector<uint64_t> target(29,17);
 assert(!cache.Find(lease,material,target,output));
 {auto transient=target;cache.Admit(lease,material,transient,input);} // no borrowed storage escapes
 auto calls=proof->calls;allocations=0;countNew=true;
 for(int i=0;i<1000;++i)assert(cache.Find(lease,material,target,output));
 countNew=false;assert(allocations==(optimized?0:2000));assert(proof->calls-calls==2000);
 assert(output[0].token==10&&output[1].token==11);
 const auto hitAlloc=allocations;
 auto changed=target;changed.back()=18;assert(!cache.Find(lease,material,changed,output));
 changed=target;changed.push_back(1);assert(!cache.Find(lease,material,changed,output));
 auto sameValues=std::make_shared<sharedpilot::ProgramInterface>(*program);
 auto replaced=std::make_shared<sharedpilot::MaterialContent>(*material);replaced->program=sameValues;
 assert(!cache.Find(lease,replaced,target,output));
 auto alias=std::make_shared<sharedpilot::MaterialContent>(*material);
 alias->bindings[0].captured.object=std::shared_ptr<const NativeCUSTextureObject>(material->bindings[0].captured.object.get(),[](auto*){});
 assert(!cache.Find(lease,alias,target,output)); // same address, different control block
 proof->ready=false;assert(!cache.Find(lease,material,target,output));proof->ready=true;
 auto journal=lease.journal;lease.journal=std::make_shared<int>(1);
 assert(!cache.Find(lease,material,target,output));lease.journal=journal;
 RuntimeLease other;other.device=&dev;Throws([&]{cache.Find(other,material,target,output);});
 ++lease.submission;Throws([&]{cache.Find(lease,material,target,output);});--lease.submission;
 lease.device=&otherDev;Throws([&]{cache.Find(lease,material,target,output);});lease.device=&dev;
 const auto ctx=lease.context;lease.context=&dev;Throws([&]{cache.Find(lease,material,target,output);});lease.context=ctx;
 assert(cache.Find(lease,material,target,output));
 CUSSharedPreparationScope bounded(lease,1);
 for(size_t n=0;n<129;++n){auto m=Material(program);auto images=input;for(unsigned i=0;i<2;++i)images[i].owner=m->bindings[i].captured.object;
  bounded.Admit(lease,m,target,images);assert(bounded.Find(lease,m,target,output)==(n<128));}
 assert(bounded.counts.admissions==128);
 std::cout<<"pair_lookup mode="<<optimized<<" hit_lookups=1000 target_allocations="<<hitAlloc<<" proof_checks=2000 boundaries=PASS\n";
}
