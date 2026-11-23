#include "Rendering/Backend/NativeExactBindingMemo.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
struct Resource {uint64_t id;int contents=0;};
using R=std::shared_ptr<Resource>;
using Memo=rendering::NativeExactBindingMemo<R,R,R,12>;
using Inputs=std::array<R,12>;
struct Model {
 Memo memo;std::map<Memo::Key,R> buffers;size_t reads=0,pins=0,hits=0;bool enabled;
 explicit Model(bool enabled):enabled(enabled){}
 R Get(const Inputs& t,const Inputs& s){
  auto read=[&](const R& p){++reads;return p?p->id:0;};Memo::Key key{};
  if(enabled){key=memo.Resolve(t,s,read,read);if(auto b=memo.Find(key)){++pins;++hits;return b;}memo.ForgetBuffer();}
  else for(size_t i=0;i<12;++i){key[i]=read(t[i]);key[i+12]=read(s[i]);}
  auto [it,inserted]=buffers.try_emplace(key);if(inserted)it->second=std::make_shared<Resource>(Resource{buffers.size()});
  ++pins;if(enabled)memo.Remember(key,it->second);return it->second;
 }
};
int main(){
 Inputs t{},s{};for(size_t i=0;i<12;++i){t[i]=std::make_shared<Resource>(Resource{i+1});s[i]=std::make_shared<Resource>(Resource{i+101});}
 Model off(false),on(true);
 for(int i=0;i<10000;++i)assert(off.Get(t,s)->id==on.Get(t,s)->id);
 assert(off.reads==240000&&on.reads==24&&off.pins==10000&&on.pins==10000);
 assert(on.hits==9999&&off.buffers.size()==1&&on.buffers.size()==1);
 const auto warmReads=on.reads;
 auto alternate=t;alternate[0]=std::make_shared<Resource>(Resource{501});alternate[1]=std::make_shared<Resource>(Resource{502});
 for(int i=0;i<10000;++i){const auto& use=i%2?t:alternate;assert(off.Get(use,s)->id==on.Get(use,s)->id);}
 assert(on.reads-warmReads==20000); // two changed texture handles, twenty-two reused slots
 assert(off.pins==on.pins&&off.buffers.size()==on.buffers.size());
 on.memo.Reset();const auto resetReads=on.reads;on.Get(t,s);assert(on.reads-resetReads==24);
 auto changedSampler=s;changedSampler[11]=std::make_shared<Resource>(Resource{900});assert(off.Get(t,changedSampler)->id==on.Get(t,changedSampler)->id);
 auto view=t;view[11]=std::make_shared<Resource>(Resource{901});assert(off.Get(view,s)->id==on.Get(view,s)->id);
 Inputs nil{};assert(off.Get(nil,nil)->id==on.Get(nil,nil)->id);
 // Same handle can have new contents: no content/readiness verdict is memoized.
 t[0]->contents=1;assert(off.Get(t,s)->id==on.Get(t,s)->id);
 Memo lifetime;std::weak_ptr<Resource> weak;
 {Inputs a{};a[0]=std::make_shared<Resource>(Resource{7});weak=a[0];auto read=[](const R& p){return p?p->id:0;};lifetime.Resolve(a,nil,read,read);}
 assert(!weak.expired());lifetime.Reset();assert(weak.expired());
 // A failed ID reader cannot publish a buffer for a partial/new key.
 Memo exceptional;size_t n=0;bool threw=false;
 try{exceptional.Resolve(t,s,[&](const R& p){if(++n==3)throw std::runtime_error("modeled getter failure");return p->id;},[](const R& p){return p->id;});}catch(const std::runtime_error&){threw=true;}
 assert(threw);const auto key=exceptional.Resolve(t,s,[](const R& p){return p->id;},[](const R& p){return p->id;});assert(!exceptional.Find(key));
 auto b=std::make_shared<Resource>(Resource{55});exceptional.Remember(key,b);assert(exceptional.Find(key)==b);exceptional.ForgetBuffer();assert(!exceptional.Find(key));
 std::cout<<"binding_memo repeated=10000 key_reads_off=240000 key_reads_on=24 pins=10000 alternating_changed_slots=2 PASS\n";
}
