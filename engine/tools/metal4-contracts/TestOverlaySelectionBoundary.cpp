#include "Rendering/Backend/NativeWorldDrawData.h"
#include <span>
#include <array>
#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <stdexcept>
#include <cassert>
#include <cstdint>
struct Buffers{int marker=7;bool completed=false;};
struct Lease{};
using Source=rendering::NativeWorldDrawCommand;
using NativeWorldDrawCommand=Source;
struct Mapped{std::array<double,4> viewport;};
struct Probe{struct Counter{void fetch_add(int,std::memory_order){}} hits;bool Enabled()const{return false;}};
Probe& LineProbe(){static Probe p;return p;}
struct Harness{
 std::vector<std::string> calls;std::string fail;bool exact=false,borrow=false;
 std::shared_ptr<const Buffers> cache;Lease lease;bool receipt=true,hit=false;const Source* expectedSource=nullptr;
 std::array<double,4> expected;uint32_t width,height;unsigned samples;size_t ordinal;
 void Call(const char* name,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){calls.emplace_back(name);assert(vp==expected&&w==width&&h==height&&s==samples);if(fail==name)throw std::runtime_error(name);}
};
static Harness* active;
namespace exactWorldLine {struct Packet{};std::optional<Packet> TryBuild(const Source&,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){active->Call("Try",vp,w,h,s);return active->exact?std::optional<Packet>(Packet{}):std::nullopt;}}
struct NativeWorldRasterMesh{bool fullViewport=false;bool built=false;};
NativeWorldRasterMesh BuildNativeWorldDrawGeometry(const Source&,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){active->Call("Build",vp,w,h,s);return {false,true};}
struct Borrower{bool Borrow(const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){active->Call("Borrow",vp,w,h,s);return active->borrow;}};
struct Cache{std::shared_ptr<const Buffers> Find(Lease& lease,const Source&,size_t ordinal,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned s){assert(&lease==&active->lease&&ordinal==active->ordinal);active->Call("Find",vp,w,h,s);return active->cache;}};
struct Impl{Cache lineGeometry;};
void ValidateNativeWorldGeometryPrefix(const Source& source,const std::array<double,4>& vp,uint32_t w,uint32_t h,unsigned samples){assert(&source==active->expectedSource);active->Call("Validate",vp,w,h,samples);}
NativeWorldRasterMesh BuildValidatedNativeWorldClippedFill(std::span<const rendering::NativeWorldDrawVertex> vertices,std::span<const uint32_t> indices,std::array<float,4> plane){assert(vertices.data()==active->expectedSource->vertices.data()&&indices.data()==active->expectedSource->indices.data()&&plane==*active->expectedSource->world.inheritedClipPlane);active->Call("Kernel",active->expected,active->width,active->height,active->samples);return {false,true};}
// Actual owned helper compiled against the boundary fixture geometry mocks.
#include "owned-world-geometry.inc"
using rendering::NativeOwnedWorldGeometry;
struct Frame{std::shared_ptr<const Source> owner;const auto& Owner()const{return owner;}};
struct Precompute {bool TryTake(const std::shared_ptr<const Source>& owner,const Source* source,NativeWorldRasterMesh& mesh){assert(owner.get()==source&&source==active->expectedSource);assert(!active->calls.empty()&&active->calls.back()=="Validate");active->Call("Take",active->expected,active->width,active->height,active->samples);if(active->hit){mesh.built=true;return true;}return false;}};
struct Result{std::shared_ptr<const Buffers> cache;bool packet,borrowed,built,full;};
Result Select(Harness& h,const Source& input){Frame frame{std::make_shared<const Source>(input)};const auto& source=*frame.owner;active=&h;h.expectedSource=&source;Precompute receipt;auto* clippedFill=h.receipt?&receipt:nullptr;Impl state;auto* impl=&state;auto& lease=h.lease;size_t lineOrdinal92=h.ordinal;Mapped map{h.expected};auto* mapped=&map;auto width=h.width,height=h.height;auto logicalWidth=width+13,logicalHeight=height+17;auto samples=h.samples;Borrower validatedBorrow168;
#include "overlay-selection.inc"
 return {cachedLines,bool(linePacket),borrowed,mesh.built,mesh.fullViewport};
}
int main(){
 for(unsigned samples:{1u,2u,8u})for(unsigned variant=0;variant<5;++variant){Harness h;h.expected={-17.25,41.5,1024.75,768.125};h.width=1537;h.height=901;h.samples=samples;h.ordinal=113;Source s;
  if(variant==0){auto owner=std::make_shared<int>(1);s.sourceLines=std::shared_ptr<const rendering::NativeSourceLineInput133>(owner,reinterpret_cast<const rendering::NativeSourceLineInput133*>(owner.get()));}if(variant==1)h.cache=std::make_shared<Buffers>();if(variant==2)h.exact=true;if(variant==3)h.borrow=true;
  auto r=Select(h,s);
  const std::vector<std::vector<std::string>> expected={{},{"Find"},{"Find","Try"},{"Find","Try","Borrow"},{"Find","Try","Borrow","Build"}};
  assert(h.calls==expected[variant]);assert(r.built==(variant==4));assert(r.borrowed==(variant==3));assert(r.packet==(variant==2));assert(bool(r.cache)==(variant==1));assert(r.full==(variant==0||(variant==2&&samples>1)));
  // Strong cached result outlives replacing/evicting its mock cache entry.
  if(variant==1){std::weak_ptr<const Buffers> weak=h.cache;h.cache.reset();assert(!weak.expired()&&r.cache->marker==7);r.cache.reset();assert(weak.expired());}
 }
 const std::vector<std::string> order={"Find","Try","Borrow","Build"};
 for(size_t failure=0;failure<order.size();++failure){Harness h;h.expected={0,0,32,64};h.width=32;h.height=64;h.samples=1;h.ordinal=0;h.fail=order[failure];bool threw=false;
  try{Select(h,{});}catch(const std::runtime_error& e){threw=true;assert(e.what()==h.fail);}assert(threw&&h.calls==std::vector<std::string>(order.begin(),order.begin()+failure+1));}
 // Positive demand seal always validates the actual mapped target before optional
 // take; a candidate miss uses the same pure kernel and a hit avoids recomputation.
 for(int mode=0;mode<4;++mode){Harness h;h.expected={-4.5,8.25,32,64};h.width=32;h.height=64;h.samples=4;h.ordinal=2;h.hit=mode==0;h.receipt=mode!=2;Source s;s.world.inheritedClipPlane=std::array<float,4>{1,0,0,0};if(mode==3)h.fail="Validate";
  std::vector<std::string> expected={"Find","Try","Borrow","Validate"};if(mode<2)expected.push_back("Take");if(mode==1||mode==2)expected.push_back("Kernel");
  bool failed=false;try{auto result=Select(h,s);assert(result.built);}catch(const std::runtime_error& e){assert(mode==3&&std::string(e.what())=="Validate");failed=true;}assert(failed==(mode==3)&&h.calls==expected);
 }

}
