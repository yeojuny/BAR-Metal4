/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering::metal4 {
// Snapshot of the actual producer submission, not a logical frame/generation.
struct NativePostprocessProducerIdentity {
 const void* context=nullptr;
 const void* queue=nullptr;
 std::uint64_t submission=0;
 bool Valid() const noexcept {return context && queue && submission;}
 bool operator==(const NativePostprocessProducerIdentity&) const = default;
};
inline bool NativePostprocessReadAllowed(const NativePostprocessProducerIdentity& producer,
 const NativePostprocessProducerIdentity& consumer,bool encoded,bool completed,bool ordered) noexcept {
 if(!producer.Valid() || !consumer.Valid())return false;
 // Completed immutable storage is safe to read even after queue migration.
 if(completed)return true;
 if(!encoded || producer.context!=consumer.context || producer.queue!=consumer.queue)return false;
 return producer.submission==consumer.submission || ordered;
}
} // namespace rendering::metal4
