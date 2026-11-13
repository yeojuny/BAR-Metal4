/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace rendering {
// Data-only GPU completion notice. No Session, renderer, live binding, or
// ownership-retirement callback may cross into the producer's mailbox.
struct NativeFrameCompletionNotice {
 bool sourceFrameCompleted=false;
 std::uint64_t sourceDrawFrame=0,completedPostprocessSerial=0;
 std::uint64_t dynamicWaterGeneration=0,dynamicWaterSequence=0,dynamicWaterSourceDrawFrame=0;
 std::array<std::uint64_t,2> completedWorldInfo{};
 std::array<std::uint64_t,3> completedWaterUpdates{};
};
static_assert(std::is_trivially_copyable_v<NativeFrameCompletionNotice>);

// One Session epoch. A callback retains only this independently owned mailbox.
// Close and publication/pop are serialized: a late completion never accesses the
// destroyed Session or applies a retirement. Main remains the sole receipt applier.
class NativeFrameCompletionMailbox {
public:
 class Ticket;
private:
 struct Slot {std::weak_ptr<Ticket> ticket;bool queued=false;};
 std::array<Slot,2> slots{};
 std::array<std::size_t,2> entrySlots{};
 const std::uint64_t epoch;
 std::mutex mutex;
 bool open=true;
 std::array<NativeFrameCompletionNotice,2> entries{};
 std::size_t read=0,count=0;
 static void Reject() {throw std::logic_error("Deferred completion differs from its owned original source frame");}
public:
 class Ticket {
  friend class NativeFrameCompletionMailbox;
  const NativeFrameCompletionMailbox* const owner;
  const std::uint64_t epoch,draw,post;
  const std::size_t slot;
  bool published=false; // Accessed only under this owner's mutex.
  Ticket(const NativeFrameCompletionMailbox* owner,std::uint64_t epoch,std::uint64_t draw,std::uint64_t post,std::size_t slot):
   owner(owner),epoch(epoch),draw(draw),post(post),slot(slot) {}
 public:
  Ticket(const Ticket&)=delete;Ticket& operator=(const Ticket&)=delete;
 };
 explicit NativeFrameCompletionMailbox(std::uint64_t generation):epoch(generation) {if(!generation)Reject();}
 std::uint64_t Epoch() const noexcept {return epoch;}
 bool HasCapacity() {
  const std::lock_guard lock(mutex);
  if(!open)Reject();
  for(const auto& slot:slots)if(!slot.queued && slot.ticket.expired())return true;
  return false;
 }
 // Reserve completion capacity before invoking the renderer/GPU submit.
 // An abandoned unpublished callback releases its weak reservation. Published
 // results keep their slot until Pop, even after every callback copy is gone.
 // Copies of the callback share a ticket, so replay after Pop is also rejected.
 std::shared_ptr<Ticket> Prepare(std::uint64_t expectedEpoch,std::uint64_t draw,std::uint64_t post) {
  const std::lock_guard lock(mutex);
  if(!open || epoch!=expectedEpoch)Reject();
  for(std::size_t i=0;i<slots.size();++i)if(!slots[i].queued && slots[i].ticket.expired()){
   auto ticket=std::shared_ptr<Ticket>(new Ticket(this,epoch,draw,post,i));
   slots[i].ticket=ticket;return ticket;
  }
  Reject();return {};
 }
 void Publish(const std::shared_ptr<Ticket>& ticket,const NativeFrameCompletionNotice& notice) {
  const std::lock_guard lock(mutex);
  if(!open || !ticket || ticket->owner!=this || ticket->epoch!=epoch || ticket->published ||
     count==entries.size() || slots[ticket->slot].ticket.lock()!=ticket || slots[ticket->slot].queued || !notice.sourceFrameCompleted ||
     notice.sourceDrawFrame!=ticket->draw || notice.completedPostprocessSerial!=ticket->post)Reject();
  const auto index=(read+count)%entries.size();
  entries[index]=notice;entrySlots[index]=ticket->slot;
  slots[ticket->slot].queued=true;++count;ticket->published=true;
 }
 std::optional<NativeFrameCompletionNotice> Pop() {
  const std::lock_guard lock(mutex);
  if(!open)Reject();
  if(!count)return std::nullopt;
  auto result=entries[read];slots[entrySlots[read]]={};read=(read+1)%entries.size();--count;return result;
 }
 void Close() {
  const std::lock_guard lock(mutex);open=false;
 }
};
}
