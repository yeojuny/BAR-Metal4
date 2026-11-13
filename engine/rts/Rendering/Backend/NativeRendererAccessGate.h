/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <mutex>
#include <stdexcept>
#include <thread>
namespace rendering {
// Fail-fast CPU ownership; never wait while holding RuntimeLease's lock.
// Lifetime/move/destruction remain externally serialized.
class NativeRendererAccessGate {
 std::recursive_mutex mutex;
 const void* reservation=nullptr;
 std::thread::id admitted;
public:
 struct Try {};
 class Scope;
 // Stack-owned across synchronous Invoke. No mutex crosses threads. A failed
 // enqueue cancels the reservation; successful Invoke joins before destruction.
 class Handoff {
  friend class Scope;
  NativeRendererAccessGate* gate;
 public:
  explicit Handoff(NativeRendererAccessGate& owner):gate(&owner) {
   std::unique_lock lock(gate->mutex,std::try_to_lock);
   if(!lock.owns_lock()||gate->reservation)throw std::logic_error("Renderer handoff already owned");
   gate->reservation=this;
  }
  ~Handoff() noexcept {
   // After Invoke has joined, only brief rejected probes can own this
   // mutex. Wait for those probes instead of terminating on harmless races.
   // This cleanup is never called inside RuntimeLease/worker ownership.
   std::lock_guard lock(gate->mutex);
   if(gate->reservation!=this||gate->admitted!=std::thread::id{})std::terminate();
   gate->reservation=nullptr;
  }
  Handoff(const Handoff&)=delete;
  Handoff& operator=(const Handoff&)=delete;
 };
 class Scope {
  NativeRendererAccessGate* gate;
  bool acquired=false;
  bool handoff=false;
 public:
  Scope(NativeRendererAccessGate* gate,Try) noexcept:gate(gate) {
   if(!gate){acquired=true;return;}
   try{
    acquired=gate->mutex.try_lock();
    if(acquired&&gate->reservation&&gate->admitted!=std::this_thread::get_id()){
     gate->mutex.unlock();acquired=false;
    }
   }catch(...){acquired=false;}
  }
  explicit Scope(NativeRendererAccessGate* gate):Scope(gate,Try{}) {
   if(!acquired)throw std::logic_error("Concurrent native renderer cache/control access");
  }
  explicit Scope(Handoff& token):gate(token.gate) {
   acquired=gate->mutex.try_lock();
   if(!acquired)throw std::logic_error("Renderer handoff access conflict");
   if(gate->reservation!=&token||gate->admitted!=std::thread::id{}){
    gate->mutex.unlock();acquired=false;throw std::logic_error("Renderer handoff already admitted");
   }
   gate->admitted=std::this_thread::get_id();handoff=true;
  }
  ~Scope() noexcept {if(gate&&acquired){if(handoff)gate->admitted={};gate->mutex.unlock();}}
  bool Acquired() const noexcept {return acquired;}
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 };
};
}
