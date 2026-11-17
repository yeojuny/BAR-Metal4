/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <utility>

namespace rendering {
struct NativeGroundDecalRange { size_t first, count; };
// Immutable CPU pages preserve every version without copying the whole row
// vector. 64 rows is a bounded copy granularity, not a performance-tuned value.
template<class Row> struct NativeGroundDecalSnapshot {
 static_assert(std::is_trivially_copyable_v<Row>);
 static constexpr size_t RowsPerPage = 64;
 using Page = std::array<Row, RowsPerPage>;
 uint64_t revision = 0;
 size_t rowCount = 0;
 std::vector<std::shared_ptr<const Page>> pages;
 size_t Size() const { return rowCount; }
 void Validate() const {
  if (!revision || rowCount > 1'000'000 || pages.size() != (rowCount + RowsPerPage - 1) / RowsPerPage)
   throw std::invalid_argument("Ground decal page extent/revision differs");
  for (const auto& page : pages) if (!page) throw std::invalid_argument("Ground decal page missing");
 }
};

// rows is the original byte-compatible GroundDecal stream. Dirty flags are
// cleared by the producer only after this complete immutable result exists.
template<class Row>
std::shared_ptr<const NativeGroundDecalSnapshot<Row>> CaptureNativeGroundDecalPages(
 const std::shared_ptr<const NativeGroundDecalSnapshot<Row>>& previous,
 std::span<const std::byte> rows, std::span<const NativeGroundDecalRange> dirty, uint64_t revision)
{
 using Snapshot = NativeGroundDecalSnapshot<Row>;
 if (!revision || rows.size() % sizeof(Row) || rows.size() / sizeof(Row) > 1'000'000)
  throw std::invalid_argument("Ground decal capture extent/revision differs");
 const size_t count = rows.size() / sizeof(Row), pageSize = Snapshot::RowsPerPage;
 for (const auto& r : dirty) if (r.first > count || r.count > count - r.first)
  throw std::invalid_argument("Ground decal dirty range exceeds rows");
 const bool replace = !previous || previous->Size() != count;
 if (previous) previous->Validate();
 auto output = std::make_shared<Snapshot>();
 output->revision = revision; output->rowCount = count;
 if (replace) output->pages.resize((count + pageSize - 1) / pageSize);
 else output->pages = previous->pages;
 // Mutable owners exist only while building this version. Non-dirty bytes
 // come from the old page, exactly as in the original snapshot contract.
 std::vector<std::shared_ptr<typename Snapshot::Page>> writable(output->pages.size());
 auto write = [&](size_t first, size_t n) {
  while (n) {
   const size_t p = first / pageSize, within = first % pageSize;
   const size_t take = std::min(n, pageSize - within);
   if (!writable[p]) {
    writable[p] = replace ? std::make_shared<typename Snapshot::Page>() :
     std::make_shared<typename Snapshot::Page>(*previous->pages[p]);
    output->pages[p] = writable[p];
   }
   std::memcpy(writable[p]->data() + within, rows.data() + first * sizeof(Row), take * sizeof(Row));
   first += take; n -= take;
  }
 };
 if (replace) write(0, count);
 else for (const auto& r : dirty) write(r.first, r.count);
 return output;
}

// Caller serializes access, as for the owning GroundDecalGPU cache. A packet
// AND its RuntimeLease pin the returned Slot until actual submission retirement.
// Extra simultaneous generations are transient; pinned storage is never written.
template<class Row, class Value> class NativeGroundDecalBufferPool {
public:
 using Snapshot = NativeGroundDecalSnapshot<Row>;
 struct Slot { std::shared_ptr<const Snapshot> source; const void* context; size_t rows; Value value; };
 using Pin = std::shared_ptr<Slot>;
 template<class Allocate, class Copy>
 Pin Acquire(const std::shared_ptr<const Snapshot>& source, const void* context, Allocate&& allocate, Copy&& copy) {
  if (!source) throw std::invalid_argument("Ground decal pool source absent");
  source->Validate();
  for (const auto& slot : slots)
   if (slot && slot->context == context && slot->source == source) return slot;
  size_t reuse = slots.size(), free = slots.size();
  for (size_t i = 0; i < slots.size(); ++i) {
   if (!slots[i]) { if (free == slots.size()) free = i; continue; }
   if (slots[i].use_count() != 1) continue;
   if (free == slots.size()) free = i;
   if (slots[i]->context == context && slots[i]->rows == source->Size()) { reuse = i; break; }
  }
  Pin slot;
  if (reuse != slots.size()) slot = slots[reuse];
  else {
   // Finish allocation before constructing a slot or touching cache state.
   auto value = allocate(std::max(size_t{1}, source->Size()) * sizeof(Row));
   slot = std::make_shared<Slot>(Slot{{}, context, source->Size(), std::move(value)});
  }
  // Keep the old page identities for arbitrary skipped/reordered revisions.
  // Invalidate before writing: a failed copy cannot masquerade as old bytes.
  auto previous = std::move(slot->source);
  for (size_t p = 0; p < source->pages.size(); ++p) {
   if (previous && previous->pages[p] == source->pages[p]) continue;
   const size_t first = p * Snapshot::RowsPerPage;
   copy(slot->value, first * sizeof(Row), source->pages[p]->data(), std::min(Snapshot::RowsPerPage, source->Size() - first) * sizeof(Row));
  }
  slot->source = source;
  if (reuse == slots.size() && free != slots.size()) slots[free] = slot;
  return slot;
 }
 void Clear() { slots = {}; }
private:
 std::array<Pin, 2> slots{};
};
}
