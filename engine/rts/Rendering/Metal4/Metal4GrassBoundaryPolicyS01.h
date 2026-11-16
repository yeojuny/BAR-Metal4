/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace rendering::metal4 {
enum class GrassBoundaryDrawS01 { Unknown, AttachmentOnly, Grass };
inline constexpr size_t NoGrassBoundaryS01=std::numeric_limits<size_t>::max();
// The caller classifies only the fixed, reviewed shader families. No sorting,
// filtering or sequence rewriting is permitted here.
template<class Draws,class Classify>
size_t GrassSuffixS01(const Draws& draws,Classify classify) noexcept {
 size_t first=NoGrassBoundaryS01;
 for(size_t n=0;n<draws.size();++n){
  const auto kind=classify(draws[n]);
  if(kind==GrassBoundaryDrawS01::Unknown)return NoGrassBoundaryS01;
  if(kind==GrassBoundaryDrawS01::Grass){if(first==NoGrassBoundaryS01)first=n;}
  else if(first!=NoGrassBoundaryS01)return NoGrassBoundaryS01;
 }
 return first;
}
// Recorded producer work is NOT completion. Only the scheduler's unchanged
// pre-A checkpoint turns this same-submission receipt into a GPU ordering edge.
inline bool GrassProducerRecordedS01(uint64_t submission,bool completed,
 uint64_t encodedSubmission,bool producerEncodingFinished) noexcept {
 return submission&&(completed||(encodedSubmission==submission&&producerEncodingFinished));
}
// Exact NEXT boundary, not the distinct Shield model-depth producer. The same
// local Pass165 decision controls keepMRTBoundary165(0); nextMRTKeep220 is empty
// at entry and no GPU call intervenes after the Grass suffix.
struct GrassOpaqueEndS01 {
 bool modelDepth=false,opaque=false;
 size_t phases=0;
 bool firstDeferred=false,pass165=false;
};
inline uint32_t GrassOpaqueEndRejectionsS01(const GrassOpaqueEndS01& end) noexcept {
 return (end.modelDepth?1u:0u)|(!end.opaque?2u:0u)|(!end.phases?4u:0u)|
  (!end.firstDeferred?8u:0u)|(end.pass165?16u:0u);
}
// At most one diagnostic per combat band, and never outside the measured
// simulation interval. Prefer the first actual Grass packet in each band; a
// final frame with no Grass may still report zero. Clean mode is off.
inline unsigned GrassDiagnosticSampleS01(uint64_t simulation,bool hasGrass) noexcept {
 if(simulation<540||simulation>1740||(!hasGrass&&simulation!=1740))return 0;
 return simulation<900?1u:simulation<1590?2u:4u;
}
enum GrassReadRejectionS01 : uint32_t {
 GrassReadNotInspectedS01=1,GrassReadLifecycleS01=2,GrassReadAlreadyEncodedS01=4,
 GrassReadTextureS01=8,GrassReadStorageS01=16,GrassReadUnknownProviderS01=32,
 GrassReadProducerS01=64,GrassReadBufferS01=128,GrassReadNoVisibleS01=256,
 GrassReadExceptionS01=512,GrassReadSubmissionS01=1024,GrassReadAttachmentsS01=2048
};
struct GrassReadDiagnosticS01 {uint32_t reasonBits=GrassReadNotInspectedS01;size_t drawIndex=0;unsigned slot=0;};
struct GrassStorageS01 {uintptr_t root=0;bool independentlyOwned=false;};
inline bool GrassReadDisjointS01(const GrassStorageS01& read,
 const std::array<GrassStorageS01,3>& writes) noexcept {
 if(!read.root||!read.independentlyOwned)return false;
 for(const auto& write:writes)
  if(!write.root||!write.independentlyOwned||write.root==read.root)return false;
 return true;
}
} // namespace rendering::metal4
