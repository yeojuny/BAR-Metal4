/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSSAOData.h"
#include <optional>
#include <bit>
namespace rendering {
// Exists only for one Prepare call; the immutable source frame owns every key.
// Never infer content from a texture name,generation number or retired cache.
class NativeSSAOClearPlan150 {
public:
 bool Remember(const NativeSSAODraw& d,bool completeOverwrite) {
  if(!completeOverwrite||!d.clear||!d.output||!d.output->image)return false;
  const auto format=d.output->image->format;
  if(format!=NativeSSAOFormat::RGBA8&&format!=NativeSSAOFormat::RGB16Float)return false;
  // Original magenta/black clears use exact0/1. Keep all other values on the
  // old shader path rather than assume hardware clear quantization is identical.
  for(float v:d.clearColor){const auto bits=std::bit_cast<uint32_t>(v);if(bits!=0&&bits!=0x3f800000u)return false;}
  values.emplace(d.output.get(),d.clearColor);return true;
 }
 std::optional<std::array<float,4>> Previous(const NativeSSAODraw& d) const {
  if(!d.output||!d.previous||d.output==d.previous||!d.output->image||d.output->image!=d.previous->image)return {};
  const auto i=values.find(d.previous.get());
  return i==values.end()?std::nullopt:std::optional(i->second);
 }
private:
 std::map<const NativeSSAOImageVersion*,std::array<float,4>> values;
};
}
