/* Production host contract: descriptor/journal separation, not Metal execution. */
#include "Rendering/Backend/NativeUITargetPassJournal.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <cstdlib>
#include <new>
static int allocationBudget=-1;
void* operator new(std::size_t n){if(allocationBudget==0)throw std::bad_alloc();if(allocationBudget>0)--allocationBudget;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
using namespace rendering;
using namespace rendering::ui;
static unsigned checks=0;
static void Check(bool value){++checks;if(!value)throw std::runtime_error("journal mismatch");}
struct Legacy {ScreenRenderTargetPass pass;std::uint64_t submission=0;};
static ScreenRenderTargetPass Pass(unsigned id){
 ScreenRenderTargetPass p;p.destination=std::make_shared<ScreenTexture>();
 p.depthDestination=std::make_shared<ScreenTexture>();p.commandSequence=id;
 p.sourceDependent=id%2;p.sourceStage=id%3?NativePostprocessStage::Screen:NativePostprocessStage::World;
 return p;
}
int main(int argc,char** argv){
 NativeUITargetPassJournal journal;auto p=Pass(1);journal.Append(p);
 if(argc>1){
  if(std::string(argv[1])=="zero")journal.Record(journal.Passes(),0);
  journal.Record(journal.Passes(),7);journal.Record(journal.Passes(),8);return 2;
 }
 // Append failure after the execution-vector reservation leaves no orphan
 // receipt; test both first/second allocation failure from an empty journal.
 for(int allowed=0;allowed<2;++allowed){
  NativeUITargetPassJournal empty;allocationBudget=allowed;bool failed=false;
  try{empty.Append(p);}catch(const std::bad_alloc&){failed=true;}
  allocationBudget=-1;Check(failed&&empty.Passes().empty());
  empty.Append(p);Check(empty.Passes().size()==1);
  allocationBudget=0;empty.Record(empty.Passes(),99);
  empty.EraseIf([](const auto&,const auto& r){return r.actualSubmission==99;});
  allocationBudget=-1;Check(empty.Passes().empty());
 }
 // Capture descriptors do not mutate when real submission receipts arrive.
 auto before=journal.Passes();journal.Record(journal.Passes(),7);journal.Record(journal.Passes(),7);
 Check(journal.Passes()[0].destination==before[0].destination);
 Check(journal.Passes()[0].commandSequence==before[0].commandSequence);
 journal.EraseIf([](const auto&,const auto& r){return r.actualSubmission==6;});Check(journal.Passes().size()==1);
 journal.EraseIf([](const auto&,const auto& r){return r.actualSubmission==7;});Check(journal.Passes().empty());
 // Two color aliases preserve legacy matching, regardless of depth identity.
 journal.Append(p);auto alias=p;alias.depthDestination.reset();journal.Append(alias);
 journal.Record(std::span(&p,1),9);
 unsigned aliases=0;journal.EraseIf([&](const auto&,const auto& r){Check(r.actualSubmission==9);++aliases;return true;});Check(aliases==2);
 // Independent unsubmitted passes survive source rollback. Already submitted
 // source work also survives; only never-submitted source work rolls back.
 auto independent=Pass(2);independent.sourceStage=NativePostprocessStage::Screen;
 journal.Append(independent);journal.Append(p);auto held=Pass(3);journal.Append(held);
 journal.Record(std::span(&held,1),11);
 journal.EraseIf([](const auto& pass,const auto& r){return !r.actualSubmission&&(pass.sourceDependent||pass.sourceStage!=NativePostprocessStage::Screen);});
 Check(journal.Passes().size()==2&&journal.Passes()[0].destination==independent.destination&&journal.Passes()[1].destination==held.destination);
 // Frame hold is a move, not a shared mutable receipt transaction.
 auto submitted=std::move(journal);journal.Clear();journal.Append(Pass(4));
 submitted.EraseIf([](const auto&,const auto& r){return r.actualSubmission==11;});
 Check(submitted.Passes().size()==1&&journal.Passes().size()==1&&journal.Passes()[0].commandSequence==4);
 journal.Clear();
 std::vector<Legacy> legacy;std::mt19937 random(0x721531);
 std::uint64_t nextTicket=20;
 for(unsigned step=0;step<20000;++step){
  switch(random()%4){
   case 0:{auto value=Pass(step+10);journal.Append(value);legacy.push_back({std::move(value),0});break;}
   case 1:{std::vector<ScreenRenderTargetPass> selected;for(auto& row:legacy)if(!row.submission&&random()%2){row.submission=nextTicket;selected.push_back(row.pass);}journal.Record(selected,nextTicket++);break;}
   case 2:{auto remove=[](const auto& pass,std::uint64_t ticket){return !ticket&&(pass.sourceDependent||pass.sourceStage!=NativePostprocessStage::Screen);};journal.EraseIf([&](const auto& pass,const auto& r){return remove(pass,r.actualSubmission);});std::erase_if(legacy,[&](const auto& row){return remove(row.pass,row.submission);});break;}
   default:{const auto cutoff=nextTicket>25?nextTicket-5:0;journal.EraseIf([&](const auto&,const auto& r){return r.actualSubmission&&r.actualSubmission<cutoff;});std::erase_if(legacy,[&](const auto& row){return row.submission&&row.submission<cutoff;});break;}
  }
  Check(journal.Passes().size()==legacy.size());unsigned i=0;
  journal.EraseIf([&](const auto& pass,const auto& r){const auto& ref=legacy[i++];Check(pass.destination==ref.pass.destination&&pass.depthDestination==ref.pass.depthDestination&&pass.commandSequence==ref.pass.commandSequence&&r.actualSubmission==ref.submission);return false;});
 }
 std::cout<<checks<<" production journal checks passed\n";
}
