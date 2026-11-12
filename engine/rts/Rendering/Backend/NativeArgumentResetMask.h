/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <cstdint>
namespace rendering {
// Bits denote unconditional writes before a draw can be encoded, never merely
// bindings that happened to be active on the previous draw. Large layouts fall
// back to resetting all slots outside the representable mask.
constexpr bool NeedsArgumentBufferReset(std::size_t index,uint64_t overwritten) {
 return index>=64 || !(overwritten&(uint64_t(1)<<index));
}
}
