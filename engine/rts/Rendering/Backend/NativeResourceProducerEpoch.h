/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include <memory>

namespace rendering {
// Serial preparation selects an epoch on one immutable physical resource.
// Readers retain that exact epoch, never a renderer's latest readiness flag.
// Encoded is command ordering evidence, NOT GPU completion or overlap permission.
class NativeResourceProducerEpoch {
public:
 explicit NativeResourceProducerEpoch(bool completed=false):encoded(completed) {}
 bool Encoded() const noexcept {return encoded.load(std::memory_order_acquire);}
 void DidEncode() noexcept {encoded.store(true,std::memory_order_release);}
private:
 std::atomic<bool> encoded;
};
class NativeResourceProducerEpochs {
public:
 NativeResourceProducerEpochs():current(std::make_shared<NativeResourceProducerEpoch>()) {}
 std::shared_ptr<NativeResourceProducerEpoch> Begin(bool completed) {
  auto next=std::make_shared<NativeResourceProducerEpoch>(completed);
  current=next;return next;
 }
 std::shared_ptr<NativeResourceProducerEpoch> Current() const noexcept {return current;}
private:
 std::shared_ptr<NativeResourceProducerEpoch> current;
};
}
