/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
namespace rendering {
// Same 18 full-width fields and lexicographic ordering as the original vector.
using DeferredLightPipelineKey = std::array<uint64_t, 18>;
constexpr DeferredLightPipelineKey MakeDeferredLightPipelineKey(
    const std::array<uint64_t,12>& state, const std::array<bool,4>& mask,
    uint64_t sampleShadingKey, bool coverage) {
    DeferredLightPipelineKey key{};
    for(unsigned i=0;i<state.size();++i)key[i]=state[i];
    for(unsigned i=0;i<mask.size();++i)key[12+i]=mask[i];
    key[16]=sampleShadingKey;key[17]=coverage;return key;
}
}
