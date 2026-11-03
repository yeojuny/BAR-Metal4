/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCUSProgramData.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rendering::metal4 {
// Slot6 is the shadow comparison resource. It always retains the original
// helper, even if a future reviewed program also uses a color read there.
inline constexpr uint32_t NativeCUSColorChannelMask194(uint32_t textures) {
 return textures & 0xfffu & ~(1u<<6);
}
inline bool NativeCUSIdentityChannels194(uint32_t textures,const NativeCUSSampleTransport& transport) {
 if(textures&~0xfffu)return false;
 const auto colors=NativeCUSColorChannelMask194(textures);
 if(!colors||(transport.depthMask&colors))return false;
 for(unsigned slot=0;slot<12;++slot)
  if((colors&(1u<<slot))&&(transport.sampleCodes[slot]&0xffffu)!=0x3210u)return false;
 return true;
}

// Only the channel helper gains a compile-time-selected identity branch.
// Its complete original body remains for every unselected slot. This operates
// on the already validated generated program and existing156 material source;
// the large generated header and original fragment equations are untouched.
inline std::string NativeCUSIdentityFragment194(std::string source,uint32_t textures) {
 const auto colors=NativeCUSColorChannelMask194(textures);
 if(!colors||(textures&~0xfffu)||source.find("NATIVE_CUS_IDENTITY_CHANNEL_MASK194")!=std::string::npos)
  throw std::runtime_error("CUS194 channel specialization has an invalid source mask/prefix");
 constexpr std::string_view signature="\nfloat4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& ";
 const auto begin=source.find(signature);
 if(begin==std::string::npos||source.find(signature,begin+signature.size())!=std::string::npos)
  throw std::runtime_error("CUS194 generated channel helper is absent or ambiguous");
 const auto name=begin+signature.size();
 const auto body=source.find(")\n{\n",name);
 if(body==std::string::npos||body<=name+1||source[name]!='_'||
    source.find_first_not_of("0123456789",name+1)!=body)
  throw std::runtime_error("CUS194 generated channel helper signature changed");
 source.insert(body+4,
  "    if ((NATIVE_CUS_IDENTITY_CHANNEL_MASK194 & (1u << uint(slot))) != 0u) return value;\n");
 return "#define NATIVE_CUS_IDENTITY_CHANNEL_MASK194 "+std::to_string(colors)+"u\n"+source;
}
} // namespace rendering::metal4
