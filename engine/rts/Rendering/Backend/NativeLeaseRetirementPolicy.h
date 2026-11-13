/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <stdexcept>
namespace rendering {
// Called under the runtime request lock. Control mode must never run a source
// completion callback on the worker, even if a new pending owner appeared.
template<class Drain>
void ApplyNativeLeaseRetirementPolicy(bool requireAlreadyRetired,bool preparationCallback,bool hasPending,Drain&& drain) {
 if(requireAlreadyRetired){
  if(hasPending)throw std::logic_error("Renderer control requires main-retired completion receipts");
  return;
 }
 if(!preparationCallback)drain();
}
}
