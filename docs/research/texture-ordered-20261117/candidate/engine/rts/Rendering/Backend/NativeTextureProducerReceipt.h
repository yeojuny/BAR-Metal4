#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace rendering {
// Serialized by RuntimeLease. A permit authorizes CPU encoding only, never
// submission, retirement, completed-only proofs, or mutable storage reuse.
class NativeTextureProducerReceipt {
public:
 struct OrderedInput {
  const void* context;
  std::uint64_t producer, consumer;
  std::weak_ptr<const void> preparation;
 };
 bool ready=false;
 std::uint64_t encodedSubmission=0;
private:
 const void* producerContext=nullptr;
 std::weak_ptr<const OrderedInput> borrowed;
public:
 void Record(const void* context,std::uint64_t submission) {
  if(!context||!submission||ready||(encodedSubmission&&(encodedSubmission!=submission||producerContext!=context)))
   throw std::logic_error("Texture producer cannot be encoded twice");
  producerContext=context;encodedSubmission=submission;
 }
 std::shared_ptr<const OrderedInput> Borrow(const void* context,std::uint64_t producer,std::uint64_t consumer,std::weak_ptr<const void> preparation) {
  if(!context||context!=producerContext||!producer||producer!=encodedSubmission||!consumer||consumer==producer||ready||preparation.expired())
   throw std::logic_error("Texture borrow lacks exact pending producer identity");
  if(auto previous=borrowed.lock();previous&&!previous->preparation.expired()) {
   if(previous->consumer!=consumer||previous->preparation.owner_before(preparation)||preparation.owner_before(previous->preparation))throw std::logic_error("Texture already borrowed by another preparation");
   return previous;
  }
  auto permit=std::make_shared<const OrderedInput>(OrderedInput{context,producer,consumer,std::move(preparation)});
  borrowed=permit;return permit;
 }
 bool Readable(std::uint64_t consumer) const noexcept {
  if(ready)return true;
  if(!consumer)return false;
  if(encodedSubmission==consumer)return true;
  auto permit=borrowed.lock();
  return permit&&!permit->preparation.expired()&&permit->context==producerContext&&permit->producer==encodedSubmission&&permit->consumer==consumer;
 }
 void RequireCompleted(std::uint64_t producer) const {
  if(!producer||producer!=encodedSubmission||!ready)
   throw std::logic_error("Borrowed texture producer did not retire before submission");
 }
 void Complete(std::uint64_t producer) {
  if(!producer||producer!=encodedSubmission)
   throw std::logic_error("Texture completion lacks its exact unfinished producer");
  ready=true;borrowed.reset();
 }
};
}
