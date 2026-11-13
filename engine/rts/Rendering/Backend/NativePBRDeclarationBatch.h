/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePBRSourceData.h"
#include <algorithm>
#include <memory>
#include <new>
#include <vector>

namespace rendering {
using NativePBRDeclarationBatch = std::vector<std::shared_ptr<const NativePBRImage>>;
using NativePBRDeclarationReceipt = std::shared_ptr<const NativePBRDeclarationBatch>;

inline bool HasPendingNativePBR(const NativePBRDeclarationReceipt& batch,NativePBRKind kind)
{
 return batch&&std::any_of(batch->begin(),batch->end(),[&](const auto& image){
  return image->program->kind==kind&&!image->completed.load();
 });
}
// Only the source/main owner mutates this store. A render request receives an
// immutable batch, never the store. GPU completion may only update image atomics.
class NativePBRDeclarations {
 NativePBRDeclarationReceipt current;
public:
 void Enqueue(std::shared_ptr<const NativePBRImage> image){
  auto next=current?std::make_shared<NativePBRDeclarationBatch>(*current):std::make_shared<NativePBRDeclarationBatch>();
  next->push_back(std::move(image));
  current=std::move(next); // Publish only after all potentially failing allocation.
 }
 NativePBRDeclarationReceipt Capture()noexcept{
  // Best-effort compaction at the source boundary. Failure retains completed
  // entries; consumer predicates still exclude them, without a new GPU failure.
  if(current&&std::any_of(current->begin(),current->end(),[](const auto& image){return image->completed.load(std::memory_order_acquire);})){
   try{
    auto next=std::make_shared<NativePBRDeclarationBatch>();next->reserve(current->size());
    for(const auto& image:*current)if(!image->completed.load(std::memory_order_acquire))next->push_back(image);
    current=std::move(next);
   }catch(const std::bad_alloc&){}
  }
  return current;
 }
 void Reset()noexcept{current.reset();}
};
}
