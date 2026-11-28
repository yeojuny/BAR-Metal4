/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeGroundDecalData.h"
#include "Metal4GroundDecalIndexPolicyG01.h"
#include <cstddef>
#include <limits>
#include <type_traits>

namespace rendering::metal4 {
// One immutable per-Prepare upload. The trailing host-structure padding is
// deliberately not uploaded or included in the indexed draw's byte range.
struct GroundDecalUploadG02 {
	NativeGroundDecalUniforms uniforms;
	std::array<std::uint16_t,36> indices=GroundDecalIndicesG01;
};
inline constexpr std::size_t GroundDecalIndexOffsetG02=offsetof(GroundDecalUploadG02,indices);
inline constexpr std::size_t GroundDecalUploadBytesG02=GroundDecalIndexOffsetG02+GroundDecalIndexBytesG01;
static_assert(std::is_standard_layout_v<GroundDecalUploadG02>);
static_assert(std::is_trivially_copyable_v<GroundDecalUploadG02>);
static_assert(offsetof(GroundDecalUploadG02,uniforms)==0);
static_assert(sizeof(NativeGroundDecalUniforms)==368);
static_assert(GroundDecalIndexOffsetG02==sizeof(NativeGroundDecalUniforms));
static_assert(GroundDecalIndexOffsetG02%alignof(std::uint16_t)==0);
static_assert(GroundDecalIndexBytesG01==72 && GroundDecalUploadBytesG02==440);
static_assert(sizeof(GroundDecalUploadG02)>=GroundDecalUploadBytesG02);

constexpr bool GroundDecalUploadValidG02(std::uint64_t address,std::uint64_t length)
{
	// Check the entire logical range before adding the tail offset. Uniform
	// address/ABI stays at the allocation base; only indices use the offset.
	return address!=0 && address%alignof(NativeGroundDecalUniforms)==0 &&
		length>=GroundDecalUploadBytesG02 &&
		address<=std::numeric_limits<std::uint64_t>::max()-GroundDecalUploadBytesG02 &&
		GroundDecalIndexBufferValidG01(address+GroundDecalIndexOffsetG02,GroundDecalIndexBytesG01);
}
}
