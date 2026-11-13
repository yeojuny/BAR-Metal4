/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <mutex>
#include <stdexcept>
namespace rendering {
// Serial CPU cache/control ownership. Never waits for another owning thread:
// RuntimeLease may already own its request lock when retirement enters here.
// Same-thread nesting permits that retirement and renderer-owned providers.
// This is not a lifetime guard or permission for concurrent destruction/moves.
class NativeRendererAccessGate {
 std::recursive_mutex mutex;
public:
 struct Try {};
 class Scope {
  NativeRendererAccessGate* gate;
  bool acquired=false;
 public:
  Scope(NativeRendererAccessGate* gate,Try) noexcept:gate(gate) {
   if(!gate){acquired=true;return;}
   try{acquired=gate->mutex.try_lock();}catch(...){acquired=false;}
  }
  explicit Scope(NativeRendererAccessGate* gate):Scope(gate,Try{}) {
   if(!acquired)throw std::logic_error("Concurrent native renderer cache/control access");
  }
  ~Scope() noexcept {if(gate&&acquired)gate->mutex.unlock();}
  bool Acquired() const noexcept {return acquired;}
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 };
};
}
