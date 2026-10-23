/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::distortionProgram {
inline constexpr uint64_t VolumeVertexHash=0x58f2e969d4ddd79bULL;
inline constexpr std::string_view VolumeVertexSHA512="92340b9de6d00e4284ac4d82324b27bf49cd8366ed9b016cad113c60de69e895c9057af4c2e5de5ae88ae934dede8471f6eb2f0218880200db21551e41e0303a";
inline constexpr uint64_t VolumeFragmentHash=0x57374eff9bd58887ULL;
inline constexpr std::string_view VolumeFragmentSHA512="ecd77671e5a9e8b2eae9cd339a47d3bda29ddda1f23ee2a7c64fc62600147e9df53548039882dbacb815ed26f7dcfd2358ab83e087890e368a2e064d6309aa9b";
inline constexpr uint64_t CombineVertexHash=0xf178135a4fd01da6ULL;
inline constexpr std::string_view CombineVertexSHA512="3923f4092a3cabce6f18c2887af45f5c7ce630f89655985dd76edb1ed1d4613eeb341be5d80ee200e5dc58f683eed07895704d6a481fbdeb9e02a1e069f5ee41";
inline constexpr uint64_t CombineFragmentHash=0x99bf0b229bc11fb6ULL;
inline constexpr std::string_view CombineFragmentSHA512="9c294e64623db684a708e2150606ef278ed75ef591d4345c83e51e2222778c06ef89448885ef3a64fddce050a636a41f82174fc835e1f21df844857b02b60b5e";
// Same exact packed attributes as original layout3..10; neutral transport follows
// the existing source-native UBO/model publication ABI, not lighting equations.
struct alignas(16) DrawUniforms {
 float pointbeamcone=0,attachedtounitID=0,windX=0,windZ=0;
 float radiusMultiplier=1,intensityMultiplier=1,overallStrength=1,pad=0;
 std::array<uint32_t,4> transport{};
 std::array<float,4> framebuffer{},alpha{};
 std::array<std::array<uint32_t,4>,10> imageSwizzle{};
 std::array<std::array<uint32_t,4>,3> depthMode{};
 std::array<std::array<uint32_t,4>,10> imageSize{};
 std::array<float,4> inverseScreenResolution{};
};
static_assert(sizeof(DrawUniforms)==464);
} // namespace rendering::distortionProgram
