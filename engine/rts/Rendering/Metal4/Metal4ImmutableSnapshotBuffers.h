/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeUploadLayout.h"
#include "Rendering/Backend/NativeImmutableBufferIdentity170.h"
#include <span>
#include <cstring>
#include <type_traits>
namespace rendering {class NativeRetainedFont170;}
#include <algorithm>
#include <unordered_map>
namespace rendering::metal4 {
// Values are immutable GPU allocations, never mutable pool slots. Source
// generations are weakly held; the submission independently owns every hit.
class ImmutableSnapshotBuffers {
public:
 static constexpr size_t EntryLimit=256,ByteLimit=32u*1024u*1024u;
 void Clear(){entries.clear();bytes=0;context=nullptr;device=nil;}
 void BeginBatch(RuntimeLease& lease){
  Context(lease);
  for(auto i=entries.begin();i!=entries.end();)if(i->second.owner.expired()){bytes-=i->second.cost;i=entries.erase(i);}else ++i;
 }
 id<MTLBuffer> Read(RuntimeLease& lease,const NativeBufferSnapshotRef& source,size_t length){
  Require(source&&length&&length<=source->bytes.size()&&length<=lease.Device().maxBufferLength,
   "Immutable effect buffer read exceeds original snapshot/device bounds");
  return ReadOwned(lease,source,source->object,source->revision,length,[&]{
   return [lease.Device() newBufferWithBytes:source->bytes.data() length:length options:MTLResourceStorageModeShared];
  }).buffer;
 }
 struct Streams170 {__strong id<MTLBuffer> buffer=nil;NativeUploadLayout<3> layout;bool hit=false,cached=false;};
 template<class Owner>
 Streams170 ReadStreams170(RuntimeLease& lease,const std::shared_ptr<const Owner>& source){
  static_assert(std::is_same_v<Owner,NativeRetainedFont170>);
  Require(source&&source->ScreenValidated(),"Immutable font stream owner missing its screen certificate");
  const std::array<std::span<const std::byte>,3> streams{
   std::as_bytes(std::span(source->Vertices())),std::as_bytes(std::span(source->UV())),std::as_bytes(std::span(source->Indices()))};
  const auto layout=PlanNativeUploadLayout<3>({streams[0].size(),streams[1].size(),streams[2].size()},lease.Device().maxBufferLength);
  Require(layout.has_value(),"Immutable font streams exceed aligned device bounds");
  // One allocation/entry, three direct source-to-GPU copies on MISS only.
  // No vector<uint8_t> snapshot or concatenation storage is created.
  auto read=ReadOwned(lease,source,0,0,layout->length,[&]{
   auto value=[lease.Device() newBufferWithLength:layout->length options:MTLResourceStorageModeShared];
   Require(value!=nil&&value.contents,"Cannot retain immutable font streams");
   for(size_t n=0;n<3;++n)std::memcpy(static_cast<std::byte*>(value.contents)+layout->offsets[n],streams[n].data(),streams[n].size());
   return value;
  });
  return {read.buffer,*layout,read.hit,read.cached};
 }
 size_t Count()const{return entries.size();}
 size_t Bytes()const{return bytes;}
private:
 struct Entry {std::weak_ptr<const void> owner;uint64_t object=0,revision=0;__strong id<MTLBuffer> buffer=nil;size_t cost=0;};
 struct ReadResult {__strong id<MTLBuffer> buffer=nil;bool hit=false,cached=false;};
 template<class Owner,class Create>
 ReadResult ReadOwned(RuntimeLease& lease,const std::shared_ptr<const Owner>& source,uint64_t object,uint64_t revision,size_t length,Create&& create){
  Context(lease);
  auto found=entries.find(source.get());
  if(found!=entries.end()){
   const auto& e=found->second;
   if(SameImmutableBufferOwner170(e.owner,source)&&e.object==object&&e.revision==revision&&e.buffer.length>=length){
    lease.AddAllocation(e.buffer);return {e.buffer,true,true};
   }
   bytes-=e.cost;entries.erase(found);
  }
  auto value=create();Require(value!=nil,"Cannot retain original immutable effect buffer");lease.AddAllocation(value);
  const size_t cost=std::max(size_t(value.length),size_t(value.allocatedSize));
  const bool cached=ImmutableBufferAdmission170(cost,ByteLimit);
  if(cached){
   while(!entries.empty()&&ImmutableBufferNeedsEviction170(entries.size(),bytes,cost,EntryLimit,ByteLimit)){
    auto i=entries.begin();bytes-=i->second.cost;entries.erase(i);
   }
   entries.emplace(source.get(),Entry{source,object,revision,value,cost});bytes+=cost;
  }
  return {value,false,cached};
 }
 void Context(RuntimeLease& lease){if(context==lease.ContextIdentity()&&device==lease.Device())return;Clear();context=lease.ContextIdentity();device=lease.Device();}
 const void* context=nullptr;__strong id<MTLDevice> device=nil;
 size_t bytes=0;std::unordered_map<const void*,Entry> entries;
};
}
