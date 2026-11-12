/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCoverageRetiredPool220.h"
#include "Rendering/Backend/ScreenDrawList.h"
#include "Rendering/Backend/NativeWeaponRangeData.h"
namespace rendering::metal4 {
struct CoverageNativeImage220 {__strong id<MTLTexture> image=nil;};
inline bool CoveragePoolEligible220(const ScreenRenderTargetPass& target){
 if(!target.destination||!target.depthDestination||!target.drawList||target.editorTarget||
  target.sourceStage!=NativePostprocessStage::World||!target.sourceDrawFrame||!target.commandSequence||
  target.store!=ScreenTargetStore::Store||
  target.destination->format!=ScreenTextureFormat::R8Unorm||target.depthDestination->format!=ScreenTextureFormat::Depth16Unorm||
  !target.coordinateWidth||!target.coordinateHeight||
  target.destination->width!=target.coordinateWidth||target.destination->height!=target.coordinateHeight||
  target.depthDestination->width!=target.coordinateWidth||target.depthDestination->height!=target.coordinateHeight)return false;
 for(const auto& image:{target.destination,target.depthDestination})
  if(!image->renderTarget||!image->generation||image->incomplete||image->generateMipmaps||!image->mips.empty()||
   !image->bytes.empty()||image->sourceTexture||image->postprocessVersion)return false;
 bool mask=false;
 for(const auto& c:target.drawList->Commands()){
  if(c.sensorLOS)return false;
  if(c.state.attachmentClear)continue;
  if(!c.weaponRange||!c.weaponRange->program.defines||!c.weaponRange->program.defines->maskPass||
   c.weaponRange->sourceDrawFrame!=target.sourceDrawFrame||c.geometry||c.pip||c.startBox||c.sensorPreview||
   c.unitIcons||c.buildSquare||c.healthBars||c.panelShader)return false;
  mask=true;
 }
 return mask;
}
}
