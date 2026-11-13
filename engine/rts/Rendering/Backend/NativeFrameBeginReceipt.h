/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include <cstdint>
#include <exception>
namespace rendering {
// Owned evidence for one renderer invocation. Contains no renderer/recorder
// pointer. A failed return is not evidence that Begin happened.
class NativeFrameBeginReceipt {
 const std::uint64_t draw,post;
 std::atomic<std::uint64_t> submission{0};
 std::atomic<bool> failed{false};
public:
 enum class Failure {None,BeforeBegin,AfterBegin};
 NativeFrameBeginReceipt(std::uint64_t draw,std::uint64_t post) noexcept:draw(draw),post(post) {}
 bool Matches(std::uint64_t expectedDraw,std::uint64_t expectedPost) const noexcept {
  return draw==expectedDraw && post==expectedPost;
 }
 // Invoke immediately after the actual command Begin succeeds, before any
 // source publication callback. Duplicate/reused invocations are not allowed.
 void MarkBegun(std::uint64_t value) noexcept {
  std::uint64_t expected=0;
  if(!value || !submission.compare_exchange_strong(expected,value,std::memory_order_release,std::memory_order_relaxed))std::terminate();
 }
 void MarkFailed() noexcept {failed.store(true,std::memory_order_release);}
 std::uint64_t Submission() const noexcept {return submission.load(std::memory_order_acquire);}
 Failure FailureStage() const noexcept {
  if(!failed.load(std::memory_order_acquire))return Failure::None;
  return Submission()?Failure::AfterBegin:Failure::BeforeBegin;
 }
};
}
