/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <stdexcept>
namespace rendering::metal4 {
// Capacity is not the captured index stream extent. Preserve the exact
// original fresh-buffer API length even when a retired slot is larger.
inline size_t WorldParticleIndexLength220(size_t logical,size_t capacity,size_t first,size_t count,size_t stride) {
 if(!stride||logical>capacity||logical%stride||first>logical/stride||count>logical/stride-first)
  throw std::runtime_error("World particle indexed draw exceeds its captured logical bytes");
 return logical-first*stride;
}
}
