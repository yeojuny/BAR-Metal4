/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSRenderInputs.h"
#include "WorldFrameData.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
uint32_t Add(uint32_t a,uint64_t b){Check(b<=UINT32_MAX-a,"CUS original transform address overflows");return a+uint32_t(b);}
void Unique(std::vector<uint32_t>& rows){std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());}
}
NativeCUSRenderGeometry BuildNativeCUSRenderGeometry(std::shared_ptr<const WorldModel> model)
{
 Check(bool(model),"CUS native geometry has no actual original model");
 NativeCUSRenderGeometry out;out.owner=std::move(model);
 size_t count=0,indices=0;
 for(const auto& piece:out.owner->pieces){
  Check(piece.vertices.size()<=UINT32_MAX-count && piece.indices.size()<=UINT32_MAX-indices,"CUS original model geometry exceeds index ABI");
  count+=piece.vertices.size();indices+=piece.indices.size();
 }
 out.vertices.reserve(count);out.indices.reserve(indices);
 for(const auto& piece:out.owner->pieces){
  const auto base=uint32_t(out.vertices.size());
  for(const auto& v:piece.vertices){
   NativeCUSRenderVertex x{};x.position=v.position;x.normal=v.normal;
   x.tangent=v.sTangent;x.bitangent=v.tTangent;
   x.uv={v.uv[0][0],v.uv[0][1],v.uv[1][0],v.uv[1][1]};
   for(unsigned b=0;b<4;++b){x.bones[0]|=uint32_t(v.boneIds[b]&255u)<<(b*8);x.bones[1]|=uint32_t(v.boneWeights[b])<<(b*8);x.bones[2]|=uint32_t(v.boneIds[b]>>8)<<(b*8);}
   out.vertices.push_back(x);
  }
  for(uint32_t i:piece.indices){Check(i<piece.vertices.size(),"CUS original local index exceeds retained model vertices");out.indices.push_back(base+i);}
 }
 return out;
}

NativeCUSRenderReadSet ValidateNativeCUSRenderReads(const NativeCUSSourceDraw& draw)
{
 ValidateNativeCUSSourceDraw(draw);
 const auto& inputs=*draw.modelInputs;NativeCUSRenderReadSet out;
 Check(draw.program->family=="unit"||draw.program->family=="unitskinning"||draw.program->family=="feature"||draw.program->family=="tree","Unknown original CUS model family");
 const bool skin=draw.program->family=="unitskinning";
 float interpolation;std::memcpy(&interpolation,draw.engineUniforms->params->bytes.data()+28,4);
 for(size_t object=0;object<inputs.submissions.size();++object){
  const auto instance=ReadNativeCUSInstance(inputs,object);out.modelUniforms.push_back(instance[1]);
  (void)ReadNativeCUSModelUniform(inputs,object);
  // selected31657 uses literal zero for the no-team sentinel, without reading
  // the engine's unwritten palette slot. Preserve every other actual read.
  const uint32_t team=instance[2]&255u;
  if(team!=255u){out.teamColors.push_back(team);(void)ReadNativeCUSTeamColor(*draw.engineUniforms,team);}
  out.transforms.push_back(instance[0]);out.transforms.push_back(Add(instance[0],1));
  for(const auto& piece:inputs.submissions[object]->geometry->pieces)for(uint32_t index:piece.indices){
   Check(index<piece.vertices.size(),"CUS actual submitted index is outside original geometry");const auto& vertex=piece.vertices[index];
   // The selected original's first bone deliberately reads ONLY its low byte.
   const uint32_t first=vertex.boneIds[0]&255u,offset=Add(instance[0],2ull*(1+first));
   out.transforms.push_back(offset);out.transforms.push_back(Add(offset,1));
   if(!skin)continue;
   const auto a=ReadNativeCUSTransform(inputs,offset),b=ReadNativeCUSTransform(inputs,Add(offset,1));
   float s0,s1;std::memcpy(&s0,a.data()+28,4);std::memcpy(&s1,b.data()+28,4);
   const float scale=s0*(1-interpolation)+s1*interpolation;
   if(vertex.boneWeights[0]==255 && scale>0)continue; // Exact source early return.
   out.transforms.push_back(Add(instance[3],first));
   // Original loops bi=1,2, not a repaired four-weight skinning algorithm.
   for(unsigned bone=1;bone<3;++bone){const auto id=vertex.boneIds[bone];if(id==0xffffu||!vertex.boneWeights[bone])continue;
    out.transforms.push_back(Add(instance[3],id));const auto row=Add(instance[0],2ull*(1+id));out.transforms.push_back(row);out.transforms.push_back(Add(row,1));
   }
  }
 }
 Unique(out.transforms);Unique(out.modelUniforms);Unique(out.teamColors);
 for(auto row:out.transforms)(void)ReadNativeCUSTransform(inputs,row);
 return out;
}

std::vector<NativeCUSPublishedRange> NativeCUSObservedRanges(const NativeCUSPublishedBuffer& source)
{
 Check(source.stride==32||source.stride==128,"CUS observed GPU buffer stride differs");
 Check(source.capacityElements<=SIZE_MAX/source.stride,"CUS observed GPU buffer extent overflows");
 std::vector<NativeCUSPublishedRange> out;
 for(size_t row=0;row<source.capacityElements;++row){
  if(!NativeCUSPublishedElementObserved(source,row))continue;
  const size_t offset=row*source.stride;
  if(!out.empty()&&out.back().offset+out.back().size==offset)out.back().size+=source.stride;
  else out.push_back({offset,source.stride});
 }
 return out;
}
} // namespace rendering
