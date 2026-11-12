/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Actual production include, not a copied algorithm. Caller remains responsible
// for valid source extent, stride, context, and count/stride arithmetic bounds.
// Does not exercise engine capture callers, publication or Metal rendering.
#include "Rendering/EngineBridge/NativeTerrainPositionCopy.inc"
#include <iostream>
#include <string>

namespace {
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("position-copy check "+std::to_string(checks)+" failed");}
using Position=std::array<float,3>;
std::vector<std::byte> Bytes(const std::vector<Position>& input,std::size_t stride,std::size_t offset){
 std::vector<std::byte> bytes(offset+input.size()*stride,std::byte{0x5a});
 for(std::size_t n=0;n<input.size();++n)std::memcpy(bytes.data()+offset+n*stride,input[n].data(),sizeof(Position));
 return bytes;
}
void SameBits(const std::vector<Position>& expected,const std::vector<Position>& actual){
 Check(expected.size()==actual.size());
 for(std::size_t n=0;n<expected.size();++n)
  for(unsigned coordinate=0;coordinate<3;++coordinate)
   Check(std::bit_cast<uint32_t>(expected[n][coordinate])==std::bit_cast<uint32_t>(actual[n][coordinate]));
}
void Valid(const std::vector<Position>& input,std::size_t stride,std::size_t offset){
 auto bytes=Bytes(input,stride,offset);const auto original=bytes;
 const auto output=rendering::CopyValidatedTerrainPositions(bytes.data()+offset,input.size(),stride);
 SameBits(input,output);Check(bytes==original);
}
void Invalid(std::vector<Position> input,std::size_t stride,std::size_t offset,float value,std::size_t vertex,unsigned coordinate){
 input[vertex][coordinate]=value;
 auto bytes=Bytes(input,stride,offset);const auto original=bytes;bool rejected=false;
 try{(void)rendering::CopyValidatedTerrainPositions(bytes.data()+offset,input.size(),stride);}
 catch(const std::runtime_error& error){rejected=std::string(error.what())=="Original terrain vertex is nonfinite";}
 Check(rejected);Check(bytes==original);
}
}
int main(){
 const std::vector<Position> input{
  {std::bit_cast<float>(uint32_t{0x80000000}),std::bit_cast<float>(uint32_t{0x00000001}),std::bit_cast<float>(uint32_t{0x80000001})},
  {0.f,-1.25f,std::numeric_limits<float>::max()},
  {std::numeric_limits<float>::min(),12.5f,-1024.f}
 };
 // Both contiguous and strided production branches accept unaligned bytes.
 for(const auto stride:{sizeof(Position),std::size_t{15},std::size_t{32}}){
  for(const auto offset:{std::size_t{0},std::size_t{1}}){
   Valid(input,stride,offset);
   for(const auto invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
    Invalid(input,stride,offset,invalid,0,0);
    Invalid(input,stride,offset,invalid,input.size()-1,2);
   }
  }
  Check(rendering::CopyValidatedTerrainPositions(nullptr,0,stride).empty());
 }
 std::cout<<"PASS "<<checks<<" production terrain-position checks; "
#ifdef __cpp_lib_containers_ranges
 <<"containers_ranges="<<__cpp_lib_containers_ranges
#else
 <<"reserve/push_back fallback (no containers_ranges)"
#endif
 <<"; no caller/GPU validation\n";
}
