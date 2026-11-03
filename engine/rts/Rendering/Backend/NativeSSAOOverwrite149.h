/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSSAOData.h"
#include <cmath>
namespace rendering {
// Only the private SSAO destination is classified, never the screen composite.
// Rectangles are the final mapped viewport/scissor used by the actual encoder.
inline bool NativeSSAOOverwrite149(const NativeSSAODraw& d,
 const std::array<double,4>& viewport,const std::array<std::uint64_t,4>& scissor)
{
 if(!d.output||!d.output->image)return false;
 const auto& image=*d.output->image;
 if(!image.width||!image.height||scissor!=std::array<std::uint64_t,4>{0,0,image.width,image.height})return false;
 for(bool write:d.world.colorWrite)if(!write)return false;
 // Native clear already ignores viewport/culling/alpha/blending and draws a
 // full attachment rectangle. Keep that original shader and clear value.
 if(d.tiles||d.defines.contains("TILE_CULL"))return false;
 if(d.clear)return true;
 if(d.surface.alphaTest.enabled||d.surface.blend.enabled)return false;
 if(d.world.frontPolygonMode!=NativePolygonMode::Fill||d.world.backPolygonMode!=NativePolygonMode::Fill||
  (d.world.cullEnabled&&d.world.cull!=NativeWorldCull::None))return false;
 for(double v:viewport)if(!std::isfinite(v))return false;
 if(viewport[0]>0||viewport[1]>0||viewport[2]<=0||viewport[3]<=0||
  viewport[0]+viewport[2]<image.width||viewport[1]+viewport[3]<image.height)return false;
 constexpr std::array<std::array<float,2>,6> full{{{-1,-1},{1,1},{-1,1},{1,1},{-1,-1},{1,-1}}};
 for(unsigned i=0;i<6;++i)for(unsigned j=0;j<2;++j)if(d.vertices[i][j]!=full[i][j])return false;
 switch(d.program){
  case NativeSSAOProgram::TexRect:return true; // Only AlphaPass can discard.
  case NativeSSAOProgram::Fuse:case NativeSSAOProgram::Sample:case NativeSSAOProgram::Blur:break;
  default:return false;
 }
 // Fixed original shader bodies: with these variants and alpha disabled,
 // every fragment returns a color. Never infer this from program name alone.
 const auto stencil=d.defines.find("USE_STENCIL"),composite=d.defines.find("BLUR_COMPOSITE");
 return stencil!=d.defines.end()&&stencil->second==0&&composite!=d.defines.end()&&composite->second==0;
}
}
