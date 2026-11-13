#include <memory>
#include <cassert>
#include <stdexcept>
#include <vector>
using Device=std::shared_ptr<int>;
using Sampler=std::shared_ptr<int>;
constexpr auto nil=nullptr;
void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
#include "pbr-sampler-cache.inc"
int main(){
 PBRSamplerCache cache;auto a=std::make_shared<int>(1),b=std::make_shared<int>(2);
 std::weak_ptr<int> old=a;int creates=0;
 auto create=[&]{return std::make_shared<int>(++creates);};
 auto nearest=cache.Get(a,false,create),linear=cache.Get(a,true,create);
 for(int n=0;n<1000;++n){assert(cache.Get(a,false,create)==nearest);assert(cache.Get(a,true,create)==linear);}assert(creates==2);
 // Throws and nil results never publish or discard existing device/slots.
 try{cache.Get(b,false,[]{throw std::runtime_error("create");return Sampler{};});assert(false);}catch(const std::runtime_error&){}
 try{cache.Get(b,false,[]{return Sampler{};});assert(false);}catch(const std::runtime_error&){}
 assert(cache.device==a&&cache.nearest==nearest&&cache.linear==linear);
 a.reset();assert(!old.expired()); // exact device identity stays pinned
 std::vector<Sampler> lease{nearest,linear}; // old leases retain their states after cache rollover
 auto newer=cache.Get(b,false,create);assert(newer!=nearest&&!cache.linear&&old.expired());
 auto newerLinear=cache.Get(b,true,create);assert(newerLinear!=linear&&creates==4);
 assert(*lease[0]==1&&*lease[1]==2);
 // Failed missing slot on same device remains absent, and next call retries.
 PBRSamplerCache empty;try{empty.Get(b,true,[]{return Sampler{};});assert(false);}catch(const std::runtime_error&){}
 assert(!empty.device&&!empty.linear&&!empty.nearest);
 assert(empty.Get(b,true,create)&&creates==5);
 try{empty.Get(b,false,[]{return Sampler{};});assert(false);}catch(const std::runtime_error&){}
 assert(empty.device==b&&empty.linear&&!empty.nearest);
 assert(empty.Get(b,false,create)&&creates==6);
}
