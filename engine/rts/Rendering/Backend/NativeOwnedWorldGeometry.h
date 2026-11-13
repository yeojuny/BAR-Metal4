/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawGeometry.h"
#include <exception>
#include <stdexcept>
#include <utility>
namespace rendering {
// Exact invocation input, not an eligibility proof for concurrent typed reads.
// Construct only after the renderer's cache/packet/borrow decisions. The alias
// owns its immutable source frame without copying a command or touching selection
// metrics. No lease, provider, iterator or mutable cache crosses this boundary.
class NativeOwnedWorldGeometry final {
public:
 struct Target {
  std::array<double,4> viewport;
  uint32_t width,height,logicalWidth,logicalHeight;
  unsigned samples;
 };
 // Precondition: source aliases a valid command in its retained frame owner.
 NativeOwnedWorldGeometry(std::shared_ptr<const NativeWorldDrawCommand> source,Target target) noexcept
  :source(std::move(source)),target(target){}
 NativeOwnedWorldGeometry(const NativeOwnedWorldGeometry&)=delete;
 NativeOwnedWorldGeometry& operator=(const NativeOwnedWorldGeometry&)=delete;
 NativeOwnedWorldGeometry(NativeOwnedWorldGeometry&&)=delete;
 NativeOwnedWorldGeometry& operator=(NativeOwnedWorldGeometry&&)=delete;
 // Called at the original fallback site, never during speculative capture.
 // A successful seal permits only the closed pure kernel; false retains the
 // original generic synchronous builder. No validation error is suppressed.
 bool SealIndependentClippedFillOnCaller(){
  if(state!=State::Pending||sealAttempted)throw std::logic_error("World geometry seal is not pending");
  sealAttempted=true;
  const auto& d=*source;
  if(!IsIndependentNativeWorldClippedFill(d))return false;
  // Exact Triangles cannot enter the generic wrapper's GPU-matrix rejection;
  // Fill/Fill makes its later polygon-mode sprite condition identically true.
  ValidateNativeWorldGeometryPrefix(d,target.viewport,target.width,target.height,target.samples);
  clippedPlane=*d.world.inheritedClipPlane;independentSealed=true;return true;
 }
 bool HasIndependentClippedFillSeal()const noexcept{return independentSealed;}
 // Control calls must be serialized. An external executor must join before
 // Take/destruction; this payload neither launches a worker nor supplies a join.
 void Execute(){
  if(state!=State::Pending)throw std::logic_error("World geometry invocation executed twice");
  try{mesh=independentSealed?BuildValidatedNativeWorldClippedFill(source->vertices,source->indices,clippedPlane):BuildNativeWorldDrawGeometry(*source,target.viewport,target.width,target.height,target.samples);}
  catch(...){failure=std::current_exception();}
  state=State::Ready;
 }
 NativeWorldRasterMesh Take(){
  if(state!=State::Ready)throw std::logic_error("World geometry invocation result not available");
  state=State::Consumed;
  if(failure)std::rethrow_exception(failure);
  return std::move(mesh);
 }
private:
 enum class State { Pending,Ready,Consumed };
 const std::shared_ptr<const NativeWorldDrawCommand> source;
 const Target target;
 NativeWorldRasterMesh mesh;
 std::exception_ptr failure;
 std::array<float,4> clippedPlane{};
 bool sealAttempted=false,independentSealed=false;
 State state=State::Pending;
};
}
