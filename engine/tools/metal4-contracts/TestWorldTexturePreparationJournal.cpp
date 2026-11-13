#include <algorithm>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <cstdint>
struct WorldTexture {int id;};
using Texture=std::shared_ptr<int>;
constexpr auto nil=nullptr;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
#include "world-texture-journal.inc"
struct RuntimeLease {
 std::shared_ptr<WorldTexturePreparationJournal> journal=std::make_shared<WorldTexturePreparationJournal>();
 auto WorldTexturePreparation(){return journal;}
};
struct Recovery {
 std::shared_ptr<WorldTexturePreparationJournal> interruptedTexturePreparation;
 #include "world-texture-journal-start.inc"
 void TextureLookup(RuntimeLease& lease,const WorldTexturePreparationJournal::Entry& entry){
  #include "world-texture-journal-lookup.inc"
 }
};
auto Entry(int id){WorldTexturePreparationJournal::Entry e;e.owner=std::make_shared<WorldTexture>(id);e.texture=std::make_shared<int>(id);e.sampled=std::make_shared<int>(id+10);e.status->uploadSource=std::make_shared<int>(id+20);return e;}
template<class F> bool Throws(F f){try{f();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 for(int i=0;i<1000;++i){
  auto a=Entry(1),b=Entry(2),c=Entry(3);Recovery recovery;RuntimeLease A,B;
  recovery.StartTexturePreparation(A);recovery.TextureLookup(A,a);recovery.TextureLookup(A,b);recovery.TextureLookup(A,a);
  assert(A.journal->entries.size()==2);
  // Interrupted A must survive a new lease that only samples an already ready texture.
  c.status->ready=true;recovery.TextureLookup(B,c);assert(B.journal->entries.size()==2);
  assert(B.journal->entries[0].owner==a.owner&&B.journal->entries[1].owner==b.owner);
  A.journal->entries.clear();assert(B.journal->entries.size()==2);
  // Explicit reset affects only the new request, not the saved source request.
  RuntimeLease C;recovery.StartTexturePreparation(C);assert(C.journal->entries.empty()&&B.journal->entries.size()==2);
  recovery.TextureLookup(C,c);assert(C.journal->entries.empty());
  // Exact synchronous validation is all-or-nothing, including already-ready entries.
  a.status->encodedSubmission=9;b.status->encodedSubmission=8;
  assert(Throws([&]{B.journal->Complete(9);}));assert(!a.status->ready&&!b.status->ready);
  b.status->encodedSubmission=9;B.journal->Complete(9);assert(a.status->ready&&b.status->ready&&B.journal->entries.empty());
  assert(!a.status->uploadSource&&!b.status->uploadSource);
  auto d=Entry(4);WorldTexturePreparationJournal D,E;D.Register(d);E.Register(d);d.status->encodedSubmission=10;
  auto complete=D.TakeCompletion(10);assert(D.entries.empty()&&!d.status->ready);complete();assert(d.status->ready);
  // Async completion skips versions retired between preparation and encode.
  auto skipped=E.TakeCompletion(11);skipped();assert(E.entries.empty());
  auto f=Entry(5),g=Entry(6);WorldTexturePreparationJournal F;F.Register(f);F.Register(g);f.status->encodedSubmission=12;g.status->encodedSubmission=13;
  auto failed=F.TakeCompletion(12);assert(Throws(failed));assert(!f.status->ready&&!g.status->ready);
 }
 std::weak_ptr<const WorldTexture> source;std::weak_ptr<int> texture,sampled;
 {WorldTexturePreparationJournal journal;{auto entry=Entry(7);source=entry.owner;texture=entry.texture;sampled=entry.sampled;journal.Register(entry);}assert(!source.expired()&&!texture.expired()&&!sampled.expired());}
 assert(source.expired()&&texture.expired()&&sampled.expired());
 std::cout<<"1000 A/B recovery/reset/completion interleavings, ready-only inheritance, ordered dedup, exact failure validation and full entry lifetime PASS\n";
}
