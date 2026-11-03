/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeScreenGeometry.h"
#include <map>
namespace rendering::metal4 {
class ScreenModelBuffers {
 struct Entry {std::weak_ptr<const WorldModel> owner;__strong id<MTLBuffer> vertices=nil,indices=nil;size_t cost=0;};
 std::map<std::pair<const WorldModel*,uint32_t>,Entry> entries;
 const void* context=nullptr;__strong id<MTLDevice> device=nil;size_t bytes=0;
public:
 static constexpr size_t EntryLimit=256,ByteLimit=32*1024*1024;
 struct Pair {__strong id<MTLBuffer> vertices=nil,indices=nil;};
 void Clear(){entries.clear();bytes=0;context=nullptr;device=nil;}
 void BeginBatch(RuntimeLease& lease){
  if(context!=lease.ContextIdentity()||device!=lease.Device()){Clear();context=lease.ContextIdentity();device=lease.Device();}
  for(auto i=entries.begin();i!=entries.end();)if(i->second.owner.expired()){bytes-=i->second.cost;i=entries.erase(i);}else ++i;
 }
 Pair Read(RuntimeLease& lease,const std::shared_ptr<const WorldModel>& model,uint32_t piece){
  BeginBatch(lease);Require(model&&piece<model->pieces.size(),"Immutable screen model piece missing");
  const auto key=std::make_pair(model.get(),piece);auto found=entries.find(key);
  if(found!=entries.end()){
   auto owner=found->second.owner.lock();
   if(owner&&!owner.owner_before(model)&&!model.owner_before(owner)){lease.AddAllocation(found->second.vertices);lease.AddAllocation(found->second.indices);return {found->second.vertices,found->second.indices};}
   bytes-=found->second.cost;entries.erase(found);
  }
  const auto& p=model->pieces[piece];static_assert(sizeof(WorldModelVertex)==76&&offsetof(WorldModelVertex,uv)==48);
  const size_t vb=p.vertices.size()*sizeof(WorldModelVertex),ib=p.indices.size()*sizeof(uint32_t);
  Require(vb&&ib&&vb<=lease.Device().maxBufferLength&&ib<=lease.Device().maxBufferLength,"Immutable screen model exceeds device buffer extent");
  Pair result{[lease.Device() newBufferWithBytes:p.vertices.data() length:vb options:MTLResourceStorageModeShared],[lease.Device() newBufferWithBytes:p.indices.data() length:ib options:MTLResourceStorageModeShared]};
  Require(result.vertices&&result.indices,"Cannot retain immutable screen model geometry");lease.AddAllocation(result.vertices);lease.AddAllocation(result.indices);
  const size_t cost=std::max(vb,size_t(result.vertices.allocatedSize))+std::max(ib,size_t(result.indices.allocatedSize));
  if(cost<=ByteLimit){while(!entries.empty()&&(entries.size()>=EntryLimit||bytes>ByteLimit-cost)){auto i=entries.begin();bytes-=i->second.cost;entries.erase(i);}entries.emplace(key,Entry{model,result.vertices,result.indices,cost});bytes+=cost;}
  return result;
 }
};
}
