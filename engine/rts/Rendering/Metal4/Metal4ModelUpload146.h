/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeRetiredResourcePool.h"
#include <cstring>
#include <span>
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif

namespace rendering::metal4 {
struct ModelUploadStorage146 { __strong id<MTLBuffer> buffer=nil; };
using ModelUploadOwners146=std::vector<std::shared_ptr<ModelUploadStorage146>>;

// Reuse depends on the prepared/submitted owner's token, never BeginBatch.
// Eight fixed 64 KiB chunks bound retained payload to 512 KiB per consumer.
class ModelUploadPool146 {
public:
 static constexpr size_t ChunkBytes=64u*1024u,ChunkCount=8;
 template<class Lease> void BeginBatch(Lease& lease) {
  if(device!=lease.Device()||context!=lease.ContextIdentity()) {
   pool.Clear();device=lease.Device();context=lease.ContextIdentity();
  }
  pool.BeginBatch();
 }
 template<class Lease> std::shared_ptr<ModelUploadStorage146> Acquire(Lease& lease,bool& created) {
  Require(device==lease.Device()&&context==lease.ContextIdentity(),"Model uploads require their actual context batch");
  return pool.Acquire(0,[&]{
   created=true;
   auto buffer=[device newBufferWithLength:ChunkBytes options:MTLResourceStorageModeShared];
   Require(buffer&&buffer.contents,"Cannot allocate original model upload chunk");
   return ModelUploadStorage146{buffer};
  },[](ModelUploadStorage146&){});
 }
 size_t CachedCount() const {return pool.CachedCount();}
private:
 NativeRetiredResourcePool<unsigned,ModelUploadStorage146,ChunkCount> pool;
 __strong id<MTLDevice> device=nil;const void* context=nullptr;
};

struct ModelUploadSlice146 {
 __strong id<MTLBuffer> buffer=nil;size_t offset=0,length=0;
 std::span<uint8_t> Bytes() const {return {static_cast<uint8_t*>(buffer.contents)+offset,length};}
 MTLGPUAddress Address() const {return buffer.gpuAddress+offset;}
};

// Local to one Prepare result. Every chunk is registered once and its token
// stays in that result until its existing event+feedback completion graph dies.
class ModelUploadArena146 {
public:
 ModelUploadArena146(ModelUploadPool146& p,ModelUploadOwners146& o):pool(p),owners(o){}
 template<class Lease> ModelUploadSlice146 Allocate(Lease& lease,size_t length) {
  Require(length&&length<=lease.Device().maxBufferLength,"Original model uniform exceeds actual device capacity");
  ++requests;payloadBytes+=length;
  if(length>ModelUploadPool146::ChunkBytes||ModelUploadPool146::ChunkBytes>lease.Device().maxBufferLength) {
   auto buffer=[lease.Device() newBufferWithLength:length options:MTLResourceStorageModeShared];
   Require(buffer&&buffer.contents,"Cannot allocate original large model uniform");
   owners.push_back(std::make_shared<ModelUploadStorage146>(ModelUploadStorage146{buffer}));
   lease.AddAllocation(buffer);++chunks;++newBuffers;
   return {buffer,0,length};
  }
  const size_t aligned=(cursor+255u)&~size_t(255u);
  if(!current||aligned>current.length||length>current.length-aligned) {
   bool created=false;auto owner=pool.Acquire(lease,created);current=owner->buffer;cursor=0;
   owners.push_back(std::move(owner));lease.AddAllocation(current);++chunks;newBuffers+=created;
  } else cursor=aligned;
  ModelUploadSlice146 result{current,cursor,length};cursor+=length;return result;
 }
 template<class Lease> ModelUploadSlice146 Copy(Lease& lease,const void* bytes,size_t length) {
  Require(bytes!=nullptr,"Original model uniform has no source bytes");
  auto result=Allocate(lease,length);std::memcpy(result.Bytes().data(),bytes,length);return result;
 }
 void Report(const char* family,uint64_t frame,bool& reported) const {
#ifdef RECOIL_METAL4_LIVE_SCENE
  if(!reported&&frame>=512&&requests) {
   LOG("[NativeModel146] family=%s requests=%llu chunks=%llu new_buffers=%llu payload_bytes=%llu retained_chunks=%zu source_values_unchanged=1",
    family,(unsigned long long)requests,(unsigned long long)chunks,(unsigned long long)newBuffers,
    (unsigned long long)payloadBytes,pool.CachedCount());reported=true;
  }
#endif
 }
 uint64_t requests=0,chunks=0,newBuffers=0,payloadBytes=0;
private:
 ModelUploadPool146& pool;ModelUploadOwners146& owners;
 __strong id<MTLBuffer> current=nil;size_t cursor=0;
};
}
