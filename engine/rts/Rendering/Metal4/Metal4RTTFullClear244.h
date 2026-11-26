/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#import <Metal/Metal.h>
#include "Metal4CoverageRTTPool220.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace rendering::metal4 {
inline bool RTTFullClear244Enabled(){static bool enabled=[]{auto p=std::getenv("BAR_NATIVE_RTT_CLEAR244");return p&&std::strcmp(p,"1")==0;}();return enabled;}
// Only the existing independent R8/D16 coverage RTT class. Keep distinct old
// and new storage, every graph owner/read check, and the original clear draw.
// This proves both old attachment contents dead before their first read.
inline unsigned RTTFullClear244Proof(const ScreenRenderTargetPass& target){
 if(!CoveragePoolEligible220(target))return 0;
 const auto& list=*target.drawList;if(list.Commands().empty())return 0;
 unsigned mask=0;
 const auto indices=list.Indices();const auto vertices=list.Vertices();
 const std::array<std::array<float,2>,6> xy{{{0,0},{float(target.coordinateWidth),0},
  {float(target.coordinateWidth),float(target.coordinateHeight)},{0,0},
  {float(target.coordinateWidth),float(target.coordinateHeight)},{0,float(target.coordinateHeight)}}};
 // A preceding color-only clear cannot read old depth, and a depth-only clear
 // cannot read old color. Stop before the first noncanonical command.
 for(const auto& command:list.Commands()){
  const auto& state=command.state;
  ScreenDrawState canonical;canonical.attachmentClear=true;canonical.blend.enabled=false;
  canonical.colorWrite=state.colorWrite;canonical.depthStencil=state.depthStencil;
  if(state!=canonical||state.scissor||state.depthStencil.stencilTest||
   state.depthStencil.depthTest!=state.depthStencil.depthWrite||
   state.depthStencil.depthCompare!=ScreenAlphaCompare::Always||command.geometry||
   command.pip||command.startBox||command.weaponRange||command.sensorPreview||
   command.sensorLOS||command.unitIcons||command.buildSquare||command.healthBars||
   command.panelShader||command.indexCount!=6||command.firstIndex>indices.size()||
   indices.size()-command.firstIndex<6)break;
  bool full=true;float z=0;
  for(unsigned i=0;i<6;++i){auto index=indices[command.firstIndex+i];if(index>=vertices.size()){full=false;break;}
   const auto& v=vertices[index];if(i==0)z=v.z;
   if(v.x!=xy[i][0]||v.y!=xy[i][1]||!std::isfinite(v.z)||v.z!=z||z< -1||z>1)full=false;
   for(float channel:v.color)if(!std::isfinite(channel))full=false;
  }
  if(!full)break;
  if(std::all_of(state.colorWrite.begin(),state.colorWrite.end(),[](bool v){return v;}))mask|=1;
  if(state.depthStencil.depthWrite)mask|=2;
  if(mask==3)break;
 }
 return mask;
}
} // namespace rendering::metal4
