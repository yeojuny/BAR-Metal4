/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Actual backend .cpp implementations linked by runner. No Metal readiness,
// encoded resource ownership, pixel equivalence or performance claim.
#include "Rendering/Backend/NativePostprocessData.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rendering;
namespace {
unsigned checks=0;
void Require(bool b){++checks;if(!b)throw std::runtime_error("plan check "+std::to_string(checks)+" failed");}
template<class F>void Reject(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}Require(failed);}
std::shared_ptr<const NativePostprocessVersion> Version(uint64_t object,uint64_t generation=1,bool rtt=false){
 auto image=std::make_shared<NativePostprocessImage>();image->object=object;image->width=image->height=16;image->renderToTexture=rtt;
 return std::make_shared<NativePostprocessVersion>(NativePostprocessVersion{image,generation,{}});
}
NativePostprocessOperation Copy(std::shared_ptr<const NativePostprocessVersion> out,NativePostprocessStage stage,uint64_t sequence,uint32_t offset=0,uint64_t command=0){
 NativePostprocessOperation op;op.kind=NativePostprocessOperationKind::CopyColor;op.stage=stage;op.sequence=sequence;
 op.screenCommandOffset=offset;op.commandSequence=command;op.outputs={out};op.previous={nullptr};op.source=op.destination={0,0,16,16};return op;
}
std::shared_ptr<NativePostprocessFrame> Frame(){auto f=std::make_shared<NativePostprocessFrame>();f->serial=11;f->sourceDrawFrame=123;f->logicalWidth=f->logicalHeight=16;return f;}
struct Retirement{bool retired=false;unsigned calls=0;};
bool Retired(const NativePostprocessVersion&,void* p){auto& s=*static_cast<Retirement*>(p);++s.calls;return s.retired;}
void ConsumerParity(const NativePostprocessFrame& f,const NativePostprocessPlan& plan,const NativePostprocessVersion& v,NativePostprocessConsumer c,bool expected){
 bool old=true,indexed=true;try{ValidateNativePostprocessConsumer(f,v,c,nullptr,nullptr);}catch(const std::runtime_error&){old=false;}
 try{ValidateNativePostprocessConsumer(plan,v,c,nullptr,nullptr);}catch(const std::runtime_error&){indexed=false;}
 Require(old==expected&&indexed==old);
}
}
int main(){
 auto world=Version(1),screen=Version(2),external=Version(3,1,true);
 auto f=Frame();f->operations={Copy(world,NativePostprocessStage::World,10),Copy(screen,NativePostprocessStage::Screen,0,4,30)};
 NativeExternalImageProducer e;e.sequence=5;e.outputs={external};e.previous={nullptr};f->externalOutputs={e};
 const auto worldRefs=world.use_count(),imageRefs=world->image.use_count(),screenRefs=screen.use_count();
 auto plan=BuildNativePostprocessPlan(f);
 Require(world.use_count()==worldRefs&&world->image.use_count()==imageRefs&&screen.use_count()==screenRefs);
 Require(plan->MatchesRoot(f));Require(&plan->Frame()==f.get());Require(plan->Root().get()==f.get());
 const auto order=OrderNativePostprocessCommands(*f);Require(order.size()==plan->Order().size());
 for(size_t i=0;i<order.size();++i)Require(order[i].external==plan->Order()[i].external&&order[i].index==plan->Order()[i].index);
 ValidateNativePostprocessFrame(*f,nullptr,nullptr);ValidateNativePostprocessFrame(*plan,nullptr,nullptr);
 ConsumerParity(*f,*plan,*world,{NativePostprocessStage::World,11,0,0},true);
 ConsumerParity(*f,*plan,*world,{NativePostprocessStage::World,10,0,0},false);
 ConsumerParity(*f,*plan,*world,{NativePostprocessStage::World,9,0,0},false);
 ConsumerParity(*f,*plan,*world,{NativePostprocessStage::PreUnit,20,0,0},false);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,5,0},true);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,3,99},false);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,4,31},true);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,4,30},false);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,4,29},false);
 ConsumerParity(*f,*plan,*screen,{NativePostprocessStage::Screen,0,4,0},true);
 auto missing=Version(99);Retirement retired;
 Reject([&]{ValidateNativePostprocessConsumer(*plan,*missing,std::nullopt,Retired,&retired);});
 retired.retired=true;ValidateNativePostprocessConsumer(*plan,*missing,std::nullopt,Retired,&retired);
 retired.retired=false;Reject([&]{ValidateNativePostprocessConsumer(*plan,*missing,std::nullopt,Retired,&retired);});Require(retired.calls==3);
 auto suffix=Frame();suffix->operations={Copy(screen,NativePostprocessStage::Screen,0,4,30)};
 auto suffixPlan=BuildNativePostprocessPlan(suffix);Require(suffix->serial==f->serial&&!plan->MatchesRoot(suffix));
 Reject([&]{ValidateNativePostprocessConsumer(*suffixPlan,*world,NativePostprocessConsumer{NativePostprocessStage::Screen,0,5,0},nullptr,nullptr);});
 // Even identical raw addresses with another control block are not exact roots.
 std::shared_ptr<const NativePostprocessFrame> alien(f.get(),[](const NativePostprocessFrame*){});Require(!plan->MatchesRoot(alien));
 auto duplicate=Frame();duplicate->operations={Copy(world,NativePostprocessStage::World,1),Copy(world,NativePostprocessStage::World,2)};
 auto duplicatePlan=BuildNativePostprocessPlan(duplicate);
 ConsumerParity(*duplicate,*duplicatePlan,*world,{NativePostprocessStage::World,2,0,0},true);
 Reject([&]{ValidateNativePostprocessFrame(*duplicate,nullptr,nullptr);});Reject([&]{ValidateNativePostprocessFrame(*duplicatePlan,nullptr,nullptr);});
 auto reverse=Frame();reverse->operations={Copy(world,NativePostprocessStage::Screen,0,5,1),Copy(screen,NativePostprocessStage::Screen,0,4,2)};
 Reject([&]{BuildNativePostprocessPlan(reverse);});
 reverse->operations={Copy(world,NativePostprocessStage::World,10),Copy(screen,NativePostprocessStage::PreUnit,11)};Reject([&]{BuildNativePostprocessPlan(reverse);});
 reverse->operations={Copy(world,NativePostprocessStage::World,10),Copy(screen,NativePostprocessStage::World,10)};Reject([&]{BuildNativePostprocessPlan(reverse);});
 auto partial=Frame();auto next=std::make_shared<NativePostprocessVersion>(NativePostprocessVersion{world->image,2,{}});
 partial->operations={Copy(next,NativePostprocessStage::World,1)};partial->operations[0].previous[0]=world;
 auto partialPlan=BuildNativePostprocessPlan(partial);retired={false,0};
 Reject([&]{ValidateNativePostprocessFrame(*partialPlan,Retired,&retired);});retired.retired=true;ValidateNativePostprocessFrame(*partialPlan,Retired,&retired);
 retired.retired=false;Reject([&]{ValidateNativePostprocessFrame(*partialPlan,Retired,&retired);});Require(retired.calls==3);
 // Missing previous-generation readiness must fail before duplicate producer detection.
 auto duplicateWithPrior=Frame();auto duplicateOut=std::make_shared<NativePostprocessVersion>(NativePostprocessVersion{world->image,2,{}});
 duplicateWithPrior->operations={Copy(duplicateOut,NativePostprocessStage::World,1),Copy(duplicateOut,NativePostprocessStage::World,2)};
 duplicateWithPrior->operations[1].previous[0]=world;
 auto duplicatePriorPlan=BuildNativePostprocessPlan(duplicateWithPrior);
 Retirement duplicateOld{false,0},duplicateNew{false,0};std::string oldError,newError;
 try{ValidateNativePostprocessFrame(*duplicateWithPrior,Retired,&duplicateOld);}catch(const std::runtime_error& error){oldError=error.what();}
 try{ValidateNativePostprocessFrame(*duplicatePriorPlan,Retired,&duplicateNew);}catch(const std::runtime_error& error){newError=error.what();}
 Require(oldError==newError&&oldError=="Postprocess references missing/unretired image contents");Require(duplicateOld.calls==1&&duplicateNew.calls==1);
 // Repeated indexed consumers still re-evaluate runtime retirement every call.
 retired={false,0};for(unsigned i=0;i<100;++i)ValidateNativePostprocessConsumer(*plan,*world,NativePostprocessConsumer{NativePostprocessStage::World,11,0,0},Retired,&retired);
 Require(retired.calls==100);
 Retirement legacyRetired{true,0},planRetired{true,0};
 ValidateNativePostprocessFrame(*partial,Retired,&legacyRetired);ValidateNativePostprocessFrame(*partialPlan,Retired,&planRetired);
 Require(legacyRetired.calls==planRetired.calls&&planRetired.calls==1);
 Reject([&]{BuildNativePostprocessPlan({});});
 std::cout<<"PASS "<<checks<<" production postprocess-plan checks; Metal/GPU not exercised\n";
}
