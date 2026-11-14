/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstring>
#include <span>
#include <type_traits>

namespace rendering {
// Stack-local to one Terrain Prepare invocation. Cache only a successful derived
// matrix, not draw uniforms, resource readiness or target-dependent state.
// Bit identity deliberately distinguishes signed zero and all 16 input scalars.
template<class Matrix>
class NativeTerrainMatrixMemo {
 static_assert(std::is_nothrow_copy_assignable_v<Matrix>);
 std::array<float,16> key{};
 Matrix value{};
 bool valid=false;
public:
 template<class Compute>
 const Matrix& Get(std::span<const float,16> input,Compute&& compute) {
  if(valid&&std::memcmp(key.data(),input.data(),sizeof(key))==0)return value;
  auto candidate=compute(); // Validation/compute failure leaves the old entry intact.
  value=candidate;
  std::memcpy(key.data(),input.data(),sizeof(key));valid=true;
  return value;
 }
};
} // namespace rendering
