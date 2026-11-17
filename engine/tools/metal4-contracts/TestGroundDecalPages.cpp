/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeGroundDecalPages.h"
#include <cassert>
#include <iostream>
#include <random>
#include <functional>
using namespace rendering;
struct Row { std::array<unsigned char,192> bytes{}; };
using Snapshot=NativeGroundDecalSnapshot<Row>;
using Source=std::shared_ptr<const Snapshot>;
using Buffer=std::vector<std::byte>;
using Pool=NativeGroundDecalBufferPool<Row,Buffer>;
size_t checks=0, allocations=0, copied=0;
int contextA,contextB;
auto allocate=[](size_t bytes){ ++allocations; return Buffer(bytes,std::byte{0xCD}); };
auto copy=[](Buffer& target,size_t offset,const Row* rows,size_t bytes){ copied+=bytes; std::memcpy(target.data()+offset,rows,bytes); };
Source capture(Source old,const std::vector<Row>& rows,std::vector<NativeGroundDecalRange> ranges,uint64_t revision) {
 return CaptureNativeGroundDecalPages<Row>(old,std::as_bytes(std::span(rows)),ranges,revision);
}
void verify(const Pool::Pin& pin,const std::vector<Row>& rows) {
 assert(pin->source->Size()==rows.size());
 if(!rows.empty()) assert(std::memcmp(pin->value.data(),rows.data(),rows.size()*sizeof(Row))==0);
 ++checks;
}
void throws(const std::function<void()>& fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}assert(caught);++checks;}
int main() {
 Pool pool; std::vector<Row> rows(193); for(size_t i=0;i<rows.size();++i) rows[i].bytes.fill(i%251);
 auto r1=capture({},rows,{},7); assert(r1->pages.size()==4); auto a=pool.Acquire(r1,&contextA,allocate,copy);verify(a,rows);
 auto baseline=rows;auto saved=a->value;
 auto exact=pool.Acquire(r1,&contextA,allocate,copy);assert(exact==a);exact.reset();
 // R2 changes page zero. R3 changes page one. R1 remains pinned and immutable.
 rows[0].bytes.fill(33);auto r2=capture(r1,rows,{{0,1}},11);auto rows2=rows;
 rows[64].bytes.fill(77);auto r3=capture(r2,rows,{{64,1}},19);
 assert(r1->pages[0]!=r2->pages[0]&&r1->pages[1]==r2->pages[1]);
 assert(r2->pages[0]==r3->pages[0]&&r2->pages[1]!=r3->pages[1]);
 auto b=pool.Acquire(r3,&contextA,allocate,copy);verify(b,rows);assert(a->value==saved);
 // Both retained slots pinned: older R2 receives transient storage, not overwrite.
 auto transient=pool.Acquire(r2,&contextA,allocate,copy);verify(transient,rows2);assert(transient!=a&&transient!=b);assert(a->value==saved);
 transient.reset();a.reset();b.reset();
 // Slot containing R1 is now sole-owned: arbitrary R1 -> R3 comparison copies
 // BOTH changed pages (24,576 bytes), not merely R3's latest dirty page.
 copied=0;auto c=pool.Acquire(r3,&contextA,allocate,copy); // exact R3 retained
 verify(c,rows);assert(copied==0);c.reset();
 rows[130].bytes.fill(99);auto r4=capture(r3,rows,{{130,1}},31);
 copied=0;auto d=pool.Acquire(r4,&contextA,allocate,copy);verify(d,rows);
 assert(copied==3*64*sizeof(Row));d.reset();
 // Reordered old revision restores original rows, including all later changes.
 auto old=pool.Acquire(r1,&contextA,allocate,copy);verify(old,baseline);old.reset();
 // Single changed page reuses compatible free slot; no buffer allocation.
 Pool single;auto p=single.Acquire(r1,&contextA,allocate,copy);p.reset();copied=0;auto n=allocations;
 p=single.Acquire(r2,&contextA,allocate,copy);verify(p,rows2);assert(allocations==n&&copied==64*sizeof(Row));p.reset();
 // Preserve unmarked rows even inside a dirty page, matching old capture.
 auto unmarked=baseline;unmarked[0].bytes.fill(1);unmarked[1].bytes.fill(2);
 auto markedOnly=capture(r1,unmarked,{{0,1}},37);auto expected=baseline;expected[0]=unmarked[0];
 Pool marked;auto mp=marked.Acquire(markedOnly,&contextA,allocate,copy);verify(mp,expected);mp.reset();
 // Range crossing a page boundary and final partial-page row.
 unmarked=baseline;unmarked[63].bytes.fill(4);unmarked[64].bytes.fill(5);unmarked[192].bytes.fill(6);
 auto crossing=capture(r1,unmarked,{{63,2},{192,1}},39);
 mp=marked.Acquire(crossing,&contextA,allocate,copy);verify(mp,unmarked);mp.reset();
 // A lease's opaque pin must prevent reuse after the prepared packet dies.
 Pool leasePool;auto packet=leasePool.Acquire(r1,&contextA,allocate,copy);
 std::shared_ptr<const void> leasePin=packet;auto* pinnedAddress=packet.get();packet.reset();
 auto nextPacket=leasePool.Acquire(r2,&contextA,allocate,copy);assert(nextPacket.get()!=pinnedAddress);
 auto restored=std::static_pointer_cast<const Pool::Slot>(leasePin);
 assert(std::memcmp(restored->value.data(),baseline.data(),baseline.size()*sizeof(Row))==0);++checks;
 leasePool.Clear();assert(restored->source==r1);restored.reset();leasePin.reset();nextPacket.reset();
 // Context changes never mutate the old context's storage.
 auto oldContext=single.Acquire(r2,&contextA,allocate,copy);auto other=single.Acquire(r2,&contextB,allocate,copy);
 assert(oldContext!=other);verify(oldContext,rows2);verify(other,rows2);single.Clear();verify(oldContext,rows2);other.reset();oldContext.reset();
 // Failed partial copy invalidates source; subsequent acquire must fully repair.
 Pool failed;auto f=failed.Acquire(r1,&contextA,allocate,copy);f.reset();
 throws([&]{failed.Acquire(r3,&contextA,allocate,[](Buffer& v,size_t off,const Row* src,size_t bytes){std::memcpy(v.data()+off,src,bytes);throw std::runtime_error("copy injection");});});
 copied=0;f=failed.Acquire(r1,&contextA,allocate,copy);verify(f,baseline);assert(copied==baseline.size()*sizeof(Row));f.reset();
 // Allocation failure changes neither cache nor its pinned bytes.
 f=failed.Acquire(r1,&contextA,allocate,copy);
 throws([&]{failed.Acquire(r2,&contextA,[](size_t)->Buffer{throw std::bad_alloc();},copy);});verify(f,baseline);f.reset();
 // Bad ranges/revision fail before modifying original snapshot.
 throws([&]{capture(r1,baseline,{{baseline.size(),1}},41);});throws([&]{capture(r1,baseline,{{0,1}},0);});
 auto malformed=std::make_shared<Snapshot>(*r1);malformed->pages[0].reset();throws([&]{pool.Acquire(malformed,&contextA,allocate,copy);});
 // Growth, compaction/reordering, shrink and same-count replacement.
 auto changed=baseline;changed.push_back(Row{});auto grown=capture(r1,changed,{{193,1}},51);
 auto growPin=pool.Acquire(grown,&contextA,allocate,copy);verify(growPin,changed);growPin.reset();
 changed.erase(changed.begin()+3);std::reverse(changed.begin(),changed.end());auto compact=capture(grown,changed,{{0,changed.size()}},57);
 auto compactPin=pool.Acquire(compact,&contextA,allocate,copy);verify(compactPin,changed);compactPin.reset();
 auto reset=capture({},baseline,{},63);auto resetPin=pool.Acquire(reset,&contextA,allocate,copy);verify(resetPin,baseline);resetPin.reset();
 auto empty=capture(reset,{}, {},71);auto emptyPin=pool.Acquire(empty,&contextA,allocate,copy);verify(emptyPin,{});assert(emptyPin->value.size()==sizeof(Row));emptyPin.reset();
 auto afterEmpty=capture(empty,baseline,{},73);auto afterPin=pool.Acquire(afterEmpty,&contextA,allocate,copy);verify(afterPin,baseline);afterPin.reset();
 // Random capture/skip/reordered submissions with pinned historical snapshots.
 struct Saved {Source source;std::vector<Row> rows;};std::vector<Saved> history;std::vector<std::pair<Pool::Pin,std::vector<Row>>> pending;
 std::mt19937 rng(0xDECA);Source current;std::vector<Row> currentRows(257);Pool random;
 for(uint64_t i=1;i<=2000;++i) {
  std::vector<NativeGroundDecalRange> dirty;
  if(i%17==0) {currentRows.resize(rng()%350);for(auto& row:currentRows)row.bytes.fill(rng());dirty.push_back({0,currentRows.size()});}
  else if(!currentRows.empty()) {size_t at=rng()%currentRows.size();currentRows[at].bytes.fill(rng());dirty.push_back({at,1});}
  current=capture(current,currentRows,dirty,i*3);history.push_back({current,currentRows});
  if(i%3) {size_t pick=(i%5==0)?rng()%history.size():history.size()-1;const auto& src=history[pick];auto pin=random.Acquire(src.source,&contextA,allocate,copy);verify(pin,src.rows);pending.push_back({std::move(pin),src.rows});}
  for(const auto& [pin,expected]:pending)verify(pin,expected);
  if(pending.size()>5)pending.erase(pending.begin(),pending.begin()+3);
 }
 std::cout<<"PASS GroundDecalPages checks="<<checks<<" randomized_revisions=2000\n";
}
