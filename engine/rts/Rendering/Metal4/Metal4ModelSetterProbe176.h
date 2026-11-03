/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "System/Log/ILog.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace rendering::metal4::model176 {
// The five reissue sites after effectstate::RasterState. These never count
// indexed draws, argument-table binds, material packets or the common setup.
enum : unsigned {Cull=1,Fill=2,Bias=4,Winding=8,Clip=16};
struct Probe {
 std::uint64_t frame=0,draws=0,empty=0,skipped=0,emptyLists=0,indexed=0;
 const char* family=nullptr;
 std::array<std::uint64_t,5> prior{},sent{};
 static std::unique_ptr<Probe> Create(std::uint64_t sourceFrame,const char* name) {
  if(sourceFrame<2048||sourceFrame>=2112)return {};
  static const bool enabled=[] {const char* p=std::getenv("BAR_NATIVE_WORLD_ENCODING176");return p&&p[0]=='1'&&p[1]=='\0';}();
  if(!enabled)return {};
  auto probe=std::make_unique<Probe>();probe->frame=sourceFrame;probe->family=name;return probe;
 }
 void Draw(unsigned before,unsigned actual,std::size_t count) {
  ++draws;empty+=count==0;indexed+=count;
  for(unsigned i=0;i<prior.size();++i){prior[i]+=(before>>i)&1u;sent[i]+=(actual>>i)&1u;}
 }
 void Report()const {
  LOG("[NativeModelSetters176] frame=%llu family=%s raster_draws=%llu no_raster=%llu zero_indexed=%llu empty_lists=%llu indexed_calls=%llu prior_cull=%llu sent_cull=%llu prior_fill=%llu sent_fill=%llu prior_bias=%llu sent_bias=%llu prior_winding=%llu sent_winding=%llu prior_clip=%llu sent_clip=%llu",
   (unsigned long long)frame,family,(unsigned long long)draws,(unsigned long long)skipped,(unsigned long long)empty,
   (unsigned long long)emptyLists,(unsigned long long)indexed,(unsigned long long)prior[0],(unsigned long long)sent[0],(unsigned long long)prior[1],(unsigned long long)sent[1],
   (unsigned long long)prior[2],(unsigned long long)sent[2],(unsigned long long)prior[3],(unsigned long long)sent[3],(unsigned long long)prior[4],(unsigned long long)sent[4]);
 }
};
}
