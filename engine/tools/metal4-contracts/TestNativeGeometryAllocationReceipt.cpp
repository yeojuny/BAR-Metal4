#include "Rendering/Backend/NativeGeometryAllocationReceipt.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
static thread_local long failAfter=-1;
void* operator new(std::size_t n){if(failAfter>=0&&failAfter--==0){failAfter=-1;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace rendering;
struct Backing:NativeTextureBacking{NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}};
NativeGeometryTextureRequest Make(NativeTextureRegistry& r,bool combined){
 NativeGeometryTextureRequest q{NativeGeometryTextureRole::Map,8,8,1,NativeCUSTextureTarget::Image2D,{}};
 for(unsigned i=0;i<(combined?7:6);++i){auto o=r.Create(q.target,std::string(100,'g'));r.Allocate(o->name,{8,8,1,1,1,0x8058});auto value=r.Find(o->name);if(i==6)q.combinedColor=value;else q.attachments[i]=value;}return q;
}
NativeGeometryAllocationReceipt Receipt(const NativeGeometryTextureRequest& q){NativeGeometryAllocationReceipt out;out.complete=true;for(unsigned i=0;i<(q.combinedColor?7:6);++i){auto o=i==6?q.combinedColor:q.attachments[i];auto b=std::make_shared<Backing>();b->storageGeneration=o->identity;b->contentVersion=o->version+1;out.allocations[i]=b;}return out;}
void Unchanged(NativeTextureRegistry& r,const NativeGeometryTextureRequest& q){for(unsigned i=0;i<(q.combinedColor?7:6);++i){auto o=i==6?q.combinedColor:q.attachments[i];auto current=r.Find(o->name);assert(current->backing==o->backing&&current->storage==o->storage&&current->identity==o->identity);}}
void Refresh(NativeTextureRegistry& r,NativeGeometryTextureRequest& q){for(auto& o:q.attachments)o=r.Find(o->name);if(q.combinedColor)q.combinedColor=r.Find(q.combinedColor->name);}
template<class F>void Reject(F f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
int main(){unsigned faults=0,success=0;
 for(bool combined:{false,true})for(int position=-1;position<35;++position){NativeCUSTextureHistory h;NativeTextureRegistry r(h);r.NewContext();h.ActiveTexture(0);auto q=Make(r,combined);auto receipt=Receipt(q);auto generation=r.ContextGeneration();
 receipt.complete=false;assert(!receipt.Publish(r,generation,q));Unchanged(r,q);receipt.complete=true;
 bool failed=false;failAfter=position;try{assert(receipt.Publish(r,generation,q));}catch(const std::bad_alloc&){failed=true;}failAfter=-1;
 if(failed){++faults;Unchanged(r,q);Refresh(r,q);receipt=Receipt(q);assert(receipt.Publish(r,generation,q));}else ++success;
 for(unsigned i=0;i<(combined?7:6);++i){auto o=i==6?q.combinedColor:q.attachments[i];assert(r.Find(o->name)->backing==receipt.allocations[i]);}
 }
 assert(faults>=26&&success>0);
 NativeCUSTextureHistory h;NativeTextureRegistry r(h);r.NewContext();h.ActiveTexture(0);auto q=Make(r,true);auto receipt=Receipt(q);auto generation=r.ContextGeneration();
 auto alias=q;alias.attachments[1]=alias.attachments[0];Reject([&]{receipt.Publish(r,generation,alias);});Unchanged(r,q);
 auto shape=q;shape.width=9;Reject([&]{receipt.Publish(r,generation,shape);});Unchanged(r,q);
 auto bad=receipt;bad.allocations[6].reset();Reject([&]{bad.Publish(r,generation,q);});Unchanged(r,q);
 auto sampling=q.attachments[2]->sampling;sampling.minFilter=0x2600;r.SetSampling(q.attachments[2]->name,sampling);auto exact=r.Find(q.attachments[2]->name);Reject([&]{receipt.Publish(r,generation,q);});assert(r.Find(exact->name)==exact);Unchanged(r,q);
 Refresh(r,q);receipt=Receipt(q);r.NewContext();Reject([&]{receipt.Publish(r,generation,q);});
 std::cout<<"Actual geometry receipt/registry: "<<faults<<" allocator failures rolled back + retry, "<<success<<" successes; incomplete/stale/alias/shape/epoch rejection PASS\n";
}
