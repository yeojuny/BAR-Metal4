/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"

namespace rendering {
struct NativeSnapshotCopyStats {
 uint64_t full = 0, partial = 0, cached = 0, copiedBytes = 0, baselineBytes = 0;
};

// Lua-owned CPU shadows only. Like NativeBufferSnapshotSource, all accesses,
// including weak GPU-cache ownership checks, are serialized on the owner thread.
// The original source supplies globally unique object IDs and checked revisions.
class NativeDirtyBufferSnapshotSource {
public:
 NativeDirtyBufferSnapshotSource() = default;
 NativeDirtyBufferSnapshotSource(const NativeDirtyBufferSnapshotSource&) = delete;
 NativeDirtyBufferSnapshotSource& operator=(const NativeDirtyBufferSnapshotSource&) = delete;
 NativeDirtyBufferSnapshotSource(NativeDirtyBufferSnapshotSource&&) noexcept = default;
 NativeDirtyBufferSnapshotSource& operator=(NativeDirtyBufferSnapshotSource&&) noexcept = default;
 void Define(const NativeBufferLayout& layout) { identity.Define(layout); }
 void Delete() noexcept { previous.reset(); identity.Delete(); first = SIZE_MAX; end = 0; }
 bool IsDefined() const noexcept { return identity.IsDefined(); }
 uint64_t Revision() const noexcept { return identity.Revision(); }
 const NativeBufferLayout& Definition() const { return identity.Definition(); }
 void InvalidateContents() {
  identity.InvalidateContents(); previous.reset(); first = SIZE_MAX; end = 0;
 }
 void InvalidateRange(size_t offset, size_t size) {
  const size_t capacity = CheckedNativeBufferByteSize(identity.Definition());
  if (offset > capacity || size > capacity - offset)
   throw std::out_of_range("Native dirty range exceeds capacity");
  identity.InvalidateContents();
  first = std::min(first, offset); end = std::max(end, offset + size);
 }
 NativeBufferSnapshotRef Capture(std::span<const uint8_t> bytes, NativeSnapshotCopyStats* stats = nullptr) const {
  if (bytes.size() != CheckedNativeBufferByteSize(identity.Definition()))
   throw std::invalid_argument("Native dirty snapshot capacity differs from producer layout");
  if (previous && previous->revision == identity.Revision()) {
   if (stats) ++stats->cached;
   return previous;
  }
  if (previous && previous.use_count() == 1 && first != SIZE_MAX) {
   // Allocate the NEW identity before releasing the old wrapper: weak caches
   // must expire, never mistake changed bytes for the same captured generation.
   // All potentially throwing allocations precede the ownership transfer.
   auto next = std::make_shared<NativeBufferSnapshot>();
   next->layout = identity.Definition(); next->object = previous->object;
   next->revision = identity.Revision();
   next->bytes = std::move(std::const_pointer_cast<NativeBufferSnapshot>(previous)->bytes);
   if (end > first) std::memmove(next->bytes.data() + first, bytes.data() + first, end - first);
   previous = std::move(next);
   if (stats) { ++stats->partial; stats->copiedBytes += end - first; stats->baselineBytes += bytes.size(); }
  } else {
   // A recorded frame / strong GPU-cache owner still needs the old bytes,
   // or an unbounded writer invalidated them: use the original complete copy.
   previous = identity.Capture(bytes);
   if (stats) { ++stats->full; stats->copiedBytes += bytes.size(); stats->baselineBytes += bytes.size(); }
  }
  first = SIZE_MAX; end = 0;
  return previous;
 }
private:
 NativeBufferSnapshotSource identity;
 mutable NativeBufferSnapshotRef previous;
 mutable size_t first = SIZE_MAX, end = 0;
};
} // namespace rendering
