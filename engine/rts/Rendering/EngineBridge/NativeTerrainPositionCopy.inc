#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <vector>
namespace rendering {
namespace terrain_position_detail {
// Some supported C++23 libraries expose from_range_t before vector's range
// constructor. Keep the native constructor where available; reserve/append
// otherwise avoids a zero-fill pass and repeated vector growth.
template<class Range>
inline std::vector<std::array<float,3>> Materialize(Range&& range,[[maybe_unused]] std::size_t count) {
#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
 return std::vector<std::array<float,3>>(std::from_range,range);
#else
 std::vector<std::array<float,3>> result;result.reserve(count);
 for(auto position:range)result.push_back(position);
 return result;
#endif
}
}

// Caller retains extent/stride and context checks. Nothing is published before
// every copied coordinate passes the same finite-value requirement.
inline std::vector<std::array<float,3>> CopyValidatedTerrainPositions(const void* source,std::size_t count,std::size_t stride) {
 const auto* bytes=static_cast<const std::byte*>(source);
 if(stride!=sizeof(std::array<float,3>)){
 auto positions=std::views::iota(std::size_t{0},count)|std::views::transform([=](std::size_t n){
  std::array<float,3> p;std::memcpy(p.data(),bytes+n*stride,sizeof(p));
  for(float value:p)if(!std::isfinite(value))throw std::runtime_error("Original terrain vertex is nonfinite");
  return p;
 });
 // Construct from source bytes once, without a preceding zero-fill pass.
 return terrain_position_detail::Materialize(positions,count);
 }
 // The contiguous common case allows a bulk copy without zero initialization.
 auto sourcePositions=std::views::iota(std::size_t{0},count)|std::views::transform([=](std::size_t n){
  std::array<float,3> p;std::memcpy(p.data(),bytes+n*sizeof(p),sizeof(p));return p;
 });
 auto positions=terrain_position_detail::Materialize(sourcePositions,count);
 // Reduce all finite tests before throwing, so the compiler can check multiple
 // coordinates together. The snapshot remains local on every failure path.
 static_assert(sizeof(float)==sizeof(uint32_t)&&std::numeric_limits<float>::is_iec559);
 uint32_t invalid=0;
 for(const auto& p:positions)for(float value:p)invalid|=(std::bit_cast<uint32_t>(value)&0x7f800000u)==0x7f800000u;
 if(invalid)throw std::runtime_error("Original terrain vertex is nonfinite");
 return positions;
}
}
