/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSSourceIdentity.h"
#include "NativeCUSSourceData.h"
#include "System/Sync/SHA512.hpp"
#include <stdexcept>

namespace rendering {
namespace {
std::string Digest(std::string_view bytes)
{
 sha512::raw_digest result;
 sha512::calc_digest(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),result.data());
 return sha512::dump_digest(result);
}
}
NativeCUSProgramIdentity ValidateNativeCUSProgramSource(NativeCUSProgramSource& source)
{
 if(source.vertexSHA512!=Digest(source.definitions+source.vertex)||source.fragmentSHA512!=Digest(source.definitions+source.fragment))
  throw std::runtime_error("CUS actual source program hashes differ from submitted bytes");
 const auto identity=FindNativeCUSProgramIdentity(source.vertexSHA512,source.fragmentSHA512);
 if(!identity)throw std::runtime_error("CUS actual original program/variant differs from the supported selected31657 assembly");
 // selected31657 no longer injects unused map-specific waterBlend definitions
 // into CUS. Retain the inactive old native uniform padding ABI, never accept
 // an arbitrary water literal or expose a fabricated active source uniform.
 source.waterColors={};
 return *identity;
}
} // namespace rendering
