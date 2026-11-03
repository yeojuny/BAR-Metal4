/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
inline std::uint64_t NextNativePreparation154(std::uint64_t& value) {
 if(value==std::numeric_limits<std::uint64_t>::max())
  throw std::runtime_error("Model preparation generation exhausted");
 return ++value;
}

// A generation identifies ONE prepared owner in one GPU consumer. This marker
// owns no resources and never allows a later owner to reuse an earlier index.
struct NativePreparedGeometry154 {
 std::uint64_t generation=0;
 std::uint32_t index=0;
 template<class Table,class Prepare>
 std::uint32_t Resolve(std::uint64_t current,Table& table,Prepare&& prepare) {
  if(!current)throw std::runtime_error("Model preparation requires a generation");
  if(generation==current) {
   if(index>=table.size())throw std::runtime_error("Model preparation owner/index differs");
   return index;
  }
  if(table.size()>=std::numeric_limits<std::uint32_t>::max())
   throw std::runtime_error("Prepared model table exceeds command index capacity");
  const auto next=static_cast<std::uint32_t>(table.size());
  table.push_back(std::forward<Prepare>(prepare)());
  generation=current;index=next; // Publish only after preparation/storage succeed.
  return next;
 }
};
}
