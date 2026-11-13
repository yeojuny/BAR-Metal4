/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUITextureObserver.h"
#include <stdexcept>
#include <utility>
namespace rendering::ui {
// Request-owned captured descriptor resolver, not a mutable renderer slot.
// Empty captures preserve main-only live fallback and worker rejection.
class NativeUIBindingResolverOwner {
 NativeUICapturedBindingResolver resolver;
 bool bound=false;
public:
 NativeUIBindingResolverOwner()=default;
 NativeUIBindingResolverOwner(const NativeUIBindingResolverOwner&)=delete;
 NativeUIBindingResolverOwner& operator=(const NativeUIBindingResolverOwner&)=delete;
 void Bind(NativeUICapturedBindingResolver value) {
  if(bound)throw std::logic_error("UI binding resolver already belongs to this request");
  resolver=std::move(value);bound=true;
 }
 template<class Fallback> NativeUITextureObserver::ObservedImage Describe(
  const NativeCUSTextureBinding& binding,bool mainThread,Fallback&& fallback) const {
  if(resolver)return resolver(binding);
  if(!mainThread)throw std::runtime_error("Worker UI binding lacks captured descriptor resolver");
  return std::forward<Fallback>(fallback)();
 }
};
}
