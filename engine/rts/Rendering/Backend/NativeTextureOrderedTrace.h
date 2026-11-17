#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace rendering {
enum class NativeTextureOrderedTraceEvent : unsigned { Armed, Admitted, EncodePermitCheck, ProducerRetiredCheck, Count };
// Diagnostic-only bounded sampling. Counts are not performance measurements
// or unique resource counts. Saturation of one event does not hide another.
class NativeTextureOrderedTraceBudget {
 std::array<std::atomic<unsigned>,unsigned(NativeTextureOrderedTraceEvent::Count)> counts{};
public:
 bool Take(NativeTextureOrderedTraceEvent event) noexcept {
  const auto index=unsigned(event);
  if(index>=counts.size())return false;
  const unsigned cap=event==NativeTextureOrderedTraceEvent::Armed?1u:64u;
  auto& count=counts[index];auto value=count.load(std::memory_order_relaxed);
  while(value<cap)if(count.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return true;
  return false;
 }
};
}
