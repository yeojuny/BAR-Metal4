/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace rendering {
// Immutable handle IDs only; never caches image contents, producer state or
// permission to omit a resource registration. Texture/Sampler/Buffer must be
// owning handles (ARC-strong id in production). Pins prevent address reuse.
// Reset at each material batch and context boundary. Storage is fixed-size.
template<class Texture,class Sampler,class Buffer,std::size_t Slots>
class NativeExactBindingMemo {
 std::array<Texture,Slots> textures{};
 std::array<Sampler,Slots> samplers{};
 std::array<std::uint64_t,Slots*2> ids{},bufferKey{};
 Buffer buffer{};
 bool initialized=false;
public:
 using Key=std::array<std::uint64_t,Slots*2>;
 template<class TextureID,class SamplerID>
 const Key& Resolve(const std::array<Texture,Slots>& actualTextures,
                    const std::array<Sampler,Slots>& actualSamplers,
                    TextureID textureID,SamplerID samplerID) {
  for(std::size_t i=0;i<Slots;++i){
   if(!initialized||textures[i]!=actualTextures[i]){
    auto owner=actualTextures[i];const auto value=textureID(owner);
    textures[i]=std::move(owner);ids[i]=value;
   }
   if(!initialized||samplers[i]!=actualSamplers[i]){
    auto owner=actualSamplers[i];const auto value=samplerID(owner);
    samplers[i]=std::move(owner);ids[i+Slots]=value;
   }
  }
  initialized=true;return ids;
 }
 Buffer Find(const Key& key) const {return buffer&&key==bufferKey?buffer:Buffer{};}
 void Remember(const Key& key,const Buffer& actualBuffer) {bufferKey=key;buffer=actualBuffer;}
 void ForgetBuffer() {buffer=Buffer{};}
 void Reset() {ForgetBuffer();textures={};samplers={};ids={};bufferKey={};initialized=false;}
};
}
