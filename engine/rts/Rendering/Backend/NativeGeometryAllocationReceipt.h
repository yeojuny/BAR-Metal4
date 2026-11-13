/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureContext.h"
#include "NativeTextureBacking.h"
#include <limits>
#include <stdexcept>
namespace rendering {
struct NativeGeometryAllocationReceipt {
 std::array<std::shared_ptr<const NativeTextureBacking>,7> allocations{};
 bool complete=false;
 static void ValidateSources(NativeTextureRegistry& registry,uint64_t generation,const NativeGeometryTextureRequest& request) {
  if(registry.ContextGeneration()!=generation)throw std::logic_error("Geometry allocation source epoch changed");
  const unsigned count=request.combinedColor?7:6;
  for(unsigned i=0;i<count;++i){
   const auto& object=i==6?request.combinedColor:request.attachments[i];
   if(!object||registry.Find(object->name)!=object||object->version>=std::numeric_limits<uint64_t>::max()-1||
    object->target!=request.target||object->allocation.width!=request.width||object->allocation.height!=request.height||object->allocation.samples!=request.samples)
    throw std::logic_error("Geometry allocation source object changed or lacks rollback capacity");
   for(unsigned j=0;j<i;++j){const auto& prior=j==6?request.combinedColor:request.attachments[j];
    if(prior->name==object->name||prior->identity==object->identity)throw std::logic_error("Geometry allocation attachments alias");}
  }
 }
 // Source thread only. A rejected/partial GPU allocation publishes nothing.
 bool Publish(NativeTextureRegistry& registry,uint64_t generation,const NativeGeometryTextureRequest& request) const {
  if(!complete)return false;
  const unsigned count=request.combinedColor?7:6;
  std::array<std::shared_ptr<const NativeCUSTextureObject>,7> prior{},published{};
  ValidateSources(registry,generation,request);
  for(unsigned i=0;i<count;++i){
   prior[i]=i==6?request.combinedColor:request.attachments[i];
   const auto& backing=allocations[i];
   if(!backing||backing->content!=NativeTextureContent::AllocationOnly||backing->storageGeneration!=prior[i]->identity||backing->contentVersion!=prior[i]->version+1)
    throw std::logic_error("Geometry allocation receipt differs from exact source allocation");
  }
  try {
   for(unsigned i=0;i<count;++i){registry.PublishContent(prior[i]->name,{},allocations[i]);published[i]=registry.Find(prior[i]->name);}
  }catch(...){
   for(unsigned i=count;i-->0;)if(published[i])registry.CancelContentPublication(published[i],prior[i]);
   throw;
  }
  return true;
 }
};
}
