#include <array>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <vector>
inline std::vector<std::array<float,3>> CopyShadowPositions(const void* source,std::size_t count,std::size_t stride) {
 const auto* bytes=static_cast<const std::byte*>(source);
 auto positions=std::views::iota(std::size_t{0},count)|std::views::transform([=](std::size_t n){
  std::array<float,3> p;std::memcpy(p.data(),bytes+n*stride,sizeof(p));return p;
 });
 // Sized range constructs each element from its original bytes. It never
 // value-initializes a second zero payload before copying that same element.
 return std::vector<std::array<float,3>>(std::from_range,positions);
}
