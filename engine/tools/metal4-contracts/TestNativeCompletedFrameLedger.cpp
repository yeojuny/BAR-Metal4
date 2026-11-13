#include "Rendering/Backend/NativeCompletedFrameLedger.h"
#include "Rendering/Backend/NativeUITargetPassJournal.h"
#include <iostream>
#include <random>
#include <map>
#include <cstdlib>
#include <new>
#include <stdexcept>
static bool forbidAllocation=false;
void* operator new(std::size_t n){if(forbidAllocation)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
using namespace rendering;
using namespace rendering::ui;
struct Payload {
 NativeUITargetPassJournal journal;std::vector<std::shared_ptr<const ScreenTexture>> publications;
 Payload(NativeUITargetPassJournal&& j,std::vector<std::shared_ptr<const ScreenTexture>>&& p) noexcept:journal(std::move(j)),publications(std::move(p)){}
};
static unsigned checks=0;
static void Check(bool ok){++checks;if(!ok)throw std::runtime_error("completion ledger mismatch");}
static void Populate(NativeUITargetPassJournal& j,std::vector<std::shared_ptr<const ScreenTexture>>& p){
 ScreenRenderTargetPass pass;pass.destination=std::make_shared<ScreenTexture>();pass.commandSequence=99;
 p.push_back(pass.destination);j.Append(std::move(pass));j.Record(j.Passes(),17);
}
int main(){
 NativeCompletedFrameLedger<Payload,2> ledger;
 NativeUITargetPassJournal j;std::vector<std::shared_ptr<const ScreenTexture>> p;
 Populate(j,p);std::weak_ptr<const ScreenTexture> first=p[0];
 forbidAllocation=true;
 Check(!ledger.TryHold(0,std::move(j),std::move(p)));Check(j.Passes().size()==1&&p.size()==1);
 Check(ledger.TryHold(10,std::move(j),std::move(p)));Check(!first.expired());
 forbidAllocation=false;Populate(j,p);std::weak_ptr<const ScreenTexture> second=p[0];
 forbidAllocation=true;
 Check(!ledger.TryHold(10,std::move(j),std::move(p)));Check(p.size()==1&&j.Passes().size()==1);
 Check(ledger.TryHold(20,std::move(j),std::move(p)));Check(ledger.Size()==2);
 forbidAllocation=false;Populate(j,p);std::weak_ptr<const ScreenTexture> current=p[0];
 forbidAllocation=true;
 Check(!ledger.TryHold(30,std::move(j),std::move(p)));Check(p.size()==1&&j.Passes().size()==1);
 Check(!ledger.Retire(0)&&!ledger.Retire(99));Check(ledger.Size()==2);
 Check(ledger.Retire(20));Check(second.expired()&&!first.expired()&&!current.expired());
 Check(!ledger.Retire(20));Check(ledger.Size()==1);
 Check(ledger.TryHold(30,std::move(j),std::move(p)));Check(ledger.Retire(10));Check(first.expired()&&!current.expired());
 Check(ledger.Retire(30));Check(current.expired());Check(ledger.Size()==0);
 forbidAllocation=false;
 // Deterministic reference model: random exact completion, invalid IDs,
 // repeated IDs and bounded overflow. Every surviving owner is checked.
 std::mt19937 random(0x11727);std::map<std::uint64_t,std::weak_ptr<const ScreenTexture>> model;
 for(unsigned step=0;step<10000;++step){
  const std::uint64_t id=random()%7;
  if(random()%2){
   j.Clear();p.clear();Populate(j,p);auto owner=std::weak_ptr<const ScreenTexture>(p[0]);
   const bool expected=id&&model.size()<2&&!model.contains(id);
   forbidAllocation=true;const bool actual=ledger.TryHold(id,std::move(j),std::move(p));forbidAllocation=false;
   Check(actual==expected);
   if(actual)model[id]=owner;else Check(j.Passes().size()==1&&p.size()==1);
  }else{
   const auto found=model.find(id);const bool expected=found!=model.end();
   std::weak_ptr<const ScreenTexture> owner;if(expected)owner=found->second;
   forbidAllocation=true;const bool actual=ledger.Retire(id);forbidAllocation=false;
   Check(actual==expected);if(expected){model.erase(id);Check(owner.expired());}
  }
  Check(ledger.Size()==model.size());for(const auto& [key,owner]:model){Check(!owner.expired());Check(!ledger.CanHold(key));}
 }
 for(const auto& [id,owner]:model){Check(ledger.Retire(id));Check(owner.expired());}
 Check(ledger.Size()==0);
 // Destruction releases retained owners without touching an independent capture.
 std::weak_ptr<const ScreenTexture> teardown;
 {NativeCompletedFrameLedger<Payload,2> temporary;j.Clear();p.clear();Populate(j,p);teardown=p[0];Check(temporary.TryHold(1,std::move(j),std::move(p)));}
 Check(teardown.expired());
 std::cout<<checks<<" production completion-ledger checks passed\n";
}
