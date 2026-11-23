/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
namespace rendering::metal4::cubedelta220 {
// Stamps identify exact produced bytes, not frame age or allocation identity.
// Zero is unknown and must never authorize a copy omission.
inline constexpr unsigned MaxLevels=32;
struct Contents { std::array<uint64_t,6*MaxLevels> stamps{}; };
inline constexpr unsigned Index(unsigned face,unsigned mip) noexcept {return face*MaxLevels+mip;}
inline bool CanSkipCopy(const Contents& source,const Contents& committedDestination,
 unsigned face,unsigned mip) noexcept {
 if(face>=6||mip>=MaxLevels)return false;
 const auto stamp=source.stamps[Index(face,mip)];
 return stamp&&stamp==committedDestination.stamps[Index(face,mip)];
}
inline void SetCopied(Contents& target,const Contents& source,unsigned face,unsigned mip) noexcept {
 if(face<6&&mip<MaxLevels)target.stamps[Index(face,mip)]=source.stamps[Index(face,mip)];
}
inline void SetFaceWrite(Contents& target,unsigned face,uint64_t stamp) noexcept {
 if(face<6)target.stamps[Index(face,0)]=stamp;
}
inline void SetGeneratedMips(Contents& target,unsigned levels,uint64_t stamp) noexcept {
 if(levels>MaxLevels)return;
 for(unsigned face=0;face<6;++face)for(unsigned mip=1;mip<levels;++mip)
  target.stamps[Index(face,mip)]=stamp;
}
} // namespace rendering::metal4::cubedelta220
