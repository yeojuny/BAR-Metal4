// Shader-only transport contract for the exact game31657 original Preview.
// This is not the old30770 flattened224-byte Sensor block.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace rendering {
struct alignas(16) NativeSensorPreviewUniformBlock {
 std::array<float,4> passRect,mipParams,losParams,coverageFlags,alliedCircle,smoothParams;
 std::array<float,4> radarcenter_range,lookupParams,animParams,circleParams,discParams,inactiveParams,mapParams;
 std::array<float,4> fragmentTransform;
 std::array<std::uint32_t,4> imageOptions;
 // Jammer-only tail; the existing Radar fields and their offsets stay intact.
 std::array<float,4> previewParams,jammerFlags,emitterAllied;
 std::array<float,192> jammerCircles;
 std::array<float,4> distanceFade; // x: actual jammer sheet fadeStrength, GL default zero
};
static_assert(sizeof(NativeSensorPreviewUniformBlock)==1072);
static_assert(offsetof(NativeSensorPreviewUniformBlock,fragmentTransform)==208);
static_assert(offsetof(NativeSensorPreviewUniformBlock,imageOptions)==224);
static_assert(offsetof(NativeSensorPreviewUniformBlock,previewParams)==240);
static_assert(offsetof(NativeSensorPreviewUniformBlock,jammerCircles)==288);
static_assert(offsetof(NativeSensorPreviewUniformBlock,distanceFade)==1056);
} // namespace rendering
