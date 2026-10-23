/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
#include <span>
#include <stdexcept>
namespace rendering {
// Actual retained MRT producer identity, not a frame number or a resource name.
// Allocation-only storage remains explicitly undefined and is never promoted
// to cleared/produced contents by this downstream contract.
struct NativeDeferredPostprocessEpoch {
 std::shared_ptr<const void> actualProducer;
 bool allocatedUndefined=false;
};
inline void ValidateNativeDeferredModelEpochs(std::span<const NativeDeferredPostprocessEpoch> inputs) {
 if(inputs.size()<2)throw std::logic_error("Deferred postprocess lacks its model color/depth pair");
 const auto&first=inputs.front();if(!first.actualProducer)throw std::logic_error("Deferred postprocess has no actual MRT producer identity");
 for(const auto&input:inputs){
  if(!input.actualProducer)throw std::logic_error("Deferred postprocess input has no actual producer");
  if(input.allocatedUndefined!=first.allocatedUndefined)throw std::logic_error("Deferred postprocess mixed undefined allocation with produced model contents");
  if(!first.allocatedUndefined&&input.actualProducer!=first.actualProducer)throw std::logic_error("Deferred postprocess model colors/depth are from different MRT source phases");
 }
}
} // namespace rendering
