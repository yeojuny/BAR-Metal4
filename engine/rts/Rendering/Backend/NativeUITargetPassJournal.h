/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rendering::ui {
// Capture-owner transaction storage. Execution descriptors never acquire mutable
// GPU submission receipts. Both arrays retain the same order; no operation here
// enables multiple in-flight frames or transfers publication ownership to a worker.
class NativeUITargetPassJournal {
public:
 struct Receipt {
  std::shared_ptr<const ScreenTexture> destination, depthDestination;
  std::uint64_t actualSubmission=0;
 };
 const std::vector<ScreenRenderTargetPass>& Passes() const noexcept {return passes;}
 void Append(ScreenRenderTargetPass pass) {
  // Allocate before publishing either row. On failure the two logical arrays
  // remain unchanged, including when a nested target callback is still open.
  Grow(passes);
  Grow(receipts);
  receipts.push_back({pass.destination,pass.depthDestination,0});
  passes.push_back(std::move(pass));
 }
 template<class Predicate> void EraseIf(Predicate remove) noexcept {
  std::size_t kept=0;
  for(std::size_t i=0;i<passes.size();++i) {
   if(remove(passes[i],receipts[i]))continue;
   if(kept!=i){passes[kept]=std::move(passes[i]);receipts[kept]=std::move(receipts[i]);}
   ++kept;
  }
  passes.resize(kept);receipts.resize(kept);
 }
 void Clear() noexcept {passes.clear();receipts.clear();}
 void Record(std::span<const ScreenRenderTargetPass> selected,std::uint64_t actualSubmission) noexcept {
  if(!actualSubmission)std::terminate();
  // Preserve the existing color-version match, including aliases. All storage
  // was reserved during capture: recording after real GPU Begin cannot allocate.
  for(auto& receipt:receipts) {
   if(receipt.actualSubmission==actualSubmission)continue;
   for(const auto& pass:selected)if(receipt.destination==pass.destination) {
    if(receipt.actualSubmission)std::terminate();
    receipt.actualSubmission=actualSubmission;break;
   }
  }
 }
private:
 template<class T> static void Grow(std::vector<T>& rows) {
  if(rows.size()<rows.capacity())return;
  const auto next=rows.size()==0?std::size_t(1):
   (rows.size()<=rows.max_size()/2?rows.size()*2:rows.max_size());
  if(next<=rows.size())throw std::length_error("Native UI target journal exhausted");
  rows.reserve(next);
 }
 static_assert(std::is_nothrow_move_constructible_v<ScreenRenderTargetPass>);
 static_assert(std::is_nothrow_move_assignable_v<ScreenRenderTargetPass>);
 std::vector<ScreenRenderTargetPass> passes;
 std::vector<Receipt> receipts;
};
}
