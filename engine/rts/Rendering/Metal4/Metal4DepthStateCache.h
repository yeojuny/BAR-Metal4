/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include "Metal4RuntimeNative.h"
#include <map>
namespace rendering::metal4 {
// Immutable descriptor states can outlive cache eviction through the lease's
// ordinary retained objects. Dynamic stencil references are encoder state.
class DepthStateCache {
public:
 using Key=std::array<NSUInteger,16>;
 static constexpr size_t EntryLimit=256;
 static Key DescriptorKey(MTLDepthStencilDescriptor* descriptor){
  Key key{};key[0]=descriptor.depthCompareFunction;key[1]=descriptor.depthWriteEnabled;
  for(unsigned n=0;n<2;++n){auto face=n?descriptor.backFaceStencil:descriptor.frontFaceStencil;const unsigned i=2+n*7;
   if(!face)continue;
   key[i]=1;key[i+1]=face.stencilCompareFunction;key[i+2]=face.stencilFailureOperation;
   key[i+3]=face.depthFailureOperation;key[i+4]=face.depthStencilPassOperation;key[i+5]=face.readMask;key[i+6]=face.writeMask;
  }
  return key;
 }
 id<MTLDepthStencilState> Acquire(id<MTLDevice> device,MTLDepthStencilDescriptor* descriptor){
  Require(device&&descriptor,"Depth-state cache requires its actual device and descriptor");
  if(owner!=device){entries.clear();owner=device;}
  const auto key=DescriptorKey(descriptor);if(auto found=entries.find(key);found!=entries.end())return found->second;
  auto state=[device newDepthStencilStateWithDescriptor:descriptor];Require(state!=nil,"Cannot initialize original cached depth/stencil state");
  if(entries.size()>=EntryLimit)entries.erase(entries.begin());entries.emplace(key,state);return state;
 }
 size_t Count()const{return entries.size();}
private:
 __strong id<MTLDevice> owner=nil;std::map<Key,id<MTLDepthStencilState>> entries;
};
}
