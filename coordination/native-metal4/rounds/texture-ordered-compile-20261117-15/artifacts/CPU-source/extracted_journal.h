#include <algorithm>
#include <functional>
#include <vector>
#include "NativeTextureProducerReceipt.h"
using namespace rendering;
struct WorldTexture {};
inline void Require(bool b,const char* s){if(!b)throw std::logic_error(s);}
class WorldTexturePreparationJournal {
public:
 struct UploadState : NativeTextureProducerReceipt {std::shared_ptr<int> uploadSource=nullptr;};
 struct Entry {
  std::shared_ptr<const WorldTexture> owner;
  std::shared_ptr<int> texture=nullptr,sampled=nullptr;
  std::shared_ptr<UploadState> status=std::make_shared<UploadState>();
  std::shared_ptr<const NativeTextureProducerReceipt::OrderedInput> borrow;
 };
 std::vector<Entry> entries;
 bool initialized=false;
 void Register(const Entry& entry) {
  if(entry.status->ready)return;
  if(std::none_of(entries.begin(),entries.end(),[&](const auto& existing){return existing.owner.get()==entry.owner.get();}))entries.push_back(entry);
 }
 void Recover(const WorldTexturePreparationJournal& interrupted) {
  entries.clear();
  // Recovery preserves unfinished producers, never a prior consumer permit.
  for(const auto& entry:interrupted.entries)if(!entry.borrow)entries.push_back(entry);
 }
 std::function<void()> TakeCompletion(uint64_t submission) {
  std::vector<std::shared_ptr<UploadState>> states;
  for(const auto& cached:entries)if(!cached.borrow&&!cached.status->ready)states.push_back(cached.status);
  entries.clear();
  return [states=std::move(states),submission]{
   for(const auto& state:states)Require(submission&&state->encodedSubmission==submission,"Texture mip completion lost its submitting owner");
   for(const auto& state:states){state->Complete(submission);state->uploadSource=nullptr;}
  };
 }
 void Complete(uint64_t submission) {
  for(const auto& cached:entries)if(!cached.borrow)Require(submission&&cached.status->encodedSubmission==submission,"Cannot complete original texture mips before their actual producer retires");
  for(const auto& cached:entries)if(!cached.borrow){cached.status->Complete(submission);cached.status->uploadSource=nullptr;}
  entries.clear();
 }
};
