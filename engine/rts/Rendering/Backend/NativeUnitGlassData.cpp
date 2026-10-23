/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeUnitGlassData.h"
#include <cmath>
#include <stdexcept>
namespace rendering {
void ValidateNativeUnitGlass(const NativeUnitGlassInput& d){
 const auto require=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
 require(d.compiled&&d.model&&d.piece<d.model->pieces.size(),"Glass draw lost actual compiled program/model piece");
 const auto& p=d.model->pieces[d.piece];require(p.indices.size()%3==0,"Glass original triangulation differs");
 for(auto i:p.indices)require(i<p.vertices.size(),"Glass original local index is out of bounds");
 for(auto* matrix:{&d.modelView,&d.projection,&d.viewInverse})for(float x:*matrix)require(std::isfinite(x),"Glass source matrix is nonfinite");
 for(float x:d.sun)require(std::isfinite(x),"Glass sun is nonfinite");
 for(float x:d.specular)require(std::isfinite(x),"Glass specular is nonfinite");
 for(float x:d.team)require(std::isfinite(x),"Glass team is nonfinite");
 for(float x:d.pbr)require(std::isfinite(x),"Glass pbr is nonfinite");
 uint64_t context=0;
 for(unsigned n=0;n<4;++n){const auto& b=d.images[n];require(b.knowledge==NativeCUSBindingKnowledge::BoundObject&&b.object,"Glass sampler has no actual immutable object");
  require(b.object->target==(n==3?NativeCUSTextureTarget::Cube:NativeCUSTextureTarget::Image2D),"Glass sampler target differs");
  require(b.object->storage==NativeTextureStorageKnowledge::Published&&(b.object->backing||b.object->image),"Glass sampled storage is unpublished");
  if(!context)context=b.object->contextGeneration;
  require(context&&context==b.object->contextGeneration,"Glass textures cross context lifetime");
 }
}
}
