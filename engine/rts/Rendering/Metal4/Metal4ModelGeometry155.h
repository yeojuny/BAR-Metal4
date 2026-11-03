/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include <functional>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace rendering::metal4 {
// Addresses refer to ONE immutable allocation. Index bytes remain model-local;
// baseVertex relocates only stage_in fetches, never original SSBO/bone indices.
struct ModelGeometrySlice155 {
 __strong id<MTLBuffer> storage=nil;
 MTLGPUAddress vertexAddress=0,indexAddress=0;
 NSUInteger indexLength=0;
 uint32_t count=0;
 int32_t baseVertex=0;
};

// A batch exists only during Prepare. It never appends to a published page.
// Exact allocation lengths avoid reserved GPU slack. Reclamation is page-wide,
// so cap the amount a surviving model can keep with it (large models alone).
class ModelGeometryBatch155 {
 struct Pending {NativeCUSRenderGeometry data;uint32_t preparedIndex;};
 std::vector<Pending> pending;
 size_t vertexCount=0,indexCount=0;
 size_t deviceLimit;
 std::function<id<MTLBuffer>(size_t)> allocate;
 std::function<void(const std::shared_ptr<const WorldModel>&,uint32_t,const ModelGeometrySlice155&)> publish;
 static void Check(bool ok){if(!ok)throw std::runtime_error("CUS geometry page exceeds native address/index bounds");}
public:
 static constexpr size_t PageBytes=4*1024*1024,PageModels=256,Stride=sizeof(NativeCUSRenderVertex);
 size_t uploadedModels=0,uploadedPages=0,uploadedBytes=0;
 ModelGeometryBatch155(size_t limit,decltype(allocate) a,decltype(publish) p):deviceLimit(limit),allocate(std::move(a)),publish(std::move(p)){}
 static size_t Bytes(size_t vertices,size_t indices,size_t limit) {
  Check(vertices>0&&indices>0&&vertices<=INT32_MAX&&indices<=UINT32_MAX);
  Check(vertices<=limit/Stride);const auto v=vertices*Stride;
  Check(indices<=(limit-v)/sizeof(uint32_t));return v+indices*sizeof(uint32_t);
 }
 void Add(uint32_t preparedIndex,NativeCUSRenderGeometry data) {
  Check(bool(data.owner));const size_t v=data.vertices.size(),i=data.indices.size();
  const auto bytes=Bytes(v,i,deviceLimit);
  if(!pending.empty()&&(pending.size()==PageModels||bytes>PageBytes||
     vertexCount*Stride+indexCount*4>PageBytes-bytes||
     v>size_t(INT32_MAX)-vertexCount||i>size_t(UINT32_MAX)-indexCount||
     bytes>deviceLimit-(vertexCount*Stride+indexCount*4)))Flush();
  // BuildNativeCUSRenderGeometry has already checked every local index.
  pending.push_back({std::move(data),preparedIndex});vertexCount+=v;indexCount+=i;
 }
 void Flush() {
  if(pending.empty())return;
  const auto bytes=Bytes(vertexCount,indexCount,deviceLimit);
  auto storage=allocate(bytes);Check(storage!=nil&&storage.length>=bytes&&storage.contents!=nullptr);
  const auto base=storage.gpuAddress;Check(base<=std::numeric_limits<MTLGPUAddress>::max()-bytes);
  auto* dst=static_cast<unsigned char*>(storage.contents);size_t v=0,i=0;
  // Finish all writes before publishing any address. A thrown allocation or
  // copy setup cannot replace an already usable cache entry.
  for(const auto& item:pending){
   const auto& data=item.data;
   std::memcpy(dst+v*Stride,data.vertices.data(),data.vertices.size()*Stride);
   std::memcpy(dst+vertexCount*Stride+i*4,data.indices.data(),data.indices.size()*4);
   v+=data.vertices.size();i+=data.indices.size();
  }
  v=i=0;
  for(const auto& item:pending){
   const auto& data=item.data;const auto length=data.indices.size()*4;
   publish(data.owner,item.preparedIndex,{storage,base,base+vertexCount*Stride+i*4,length,uint32_t(data.indices.size()),int32_t(v)});
   v+=data.vertices.size();i+=data.indices.size();
  }
  uploadedModels+=pending.size();++uploadedPages;uploadedBytes+=bytes;
  pending.clear();vertexCount=indexCount=0;
 }
};
}
