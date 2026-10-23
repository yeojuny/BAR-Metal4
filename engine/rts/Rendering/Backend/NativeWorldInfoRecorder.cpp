/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldInfoRecorder.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<std::uint64_t> nextMap{1};
void Check(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
std::uint64_t Next(std::uint64_t value){if(value==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native world info serial exhausted");return value+1;}
void Coverage(const std::shared_ptr<const NativeMiniMapCoverage>& v){Check(v&&v->width&&v->height&&v->width<=16384&&v->height<=16384&&v->counts.size()==std::uint64_t(v->width)*v->height,"Original info coverage bounds differ");}
void Inputs(const NativeMiniMapInfo& v){Check(v.mode==NativeMiniMapInfoMode::Los&&std::isfinite(v.time),"Original info inputs require finite LOS time");Coverage(v.groundLos);Coverage(v.airLos);Coverage(v.radar);Coverage(v.jammer);Check(v.radar->width==v.jammer->width&&v.radar->height==v.jammer->height,"Original info radar/jammer extents differ");for(const auto& c:{v.alwaysColor,v.losColor,v.radarColor,v.jammerColor,v.radarColor2})for(float f:c)Check(std::isfinite(f),"Original info color is nonfinite");}
}
void ValidateNativeWorldInfoFrame(const NativeWorldInfoFrame& f){Check(f.mode==NativeWorldInfoMode::Normal||f.mode==NativeWorldInfoMode::Los||f.mode==NativeWorldInfoMode::Metal,"Unknown original global info mode");Check(f.mapIdentity&&f.modeGeneration&&f.squaresX&&f.squaresZ&&f.paddedSquaresX>=f.squaresX&&f.paddedSquaresZ>=f.squaresZ&&f.paddedSquaresX<=16384&&f.paddedSquaresZ<=16384&&std::has_single_bit(f.paddedSquaresX)&&std::has_single_bit(f.paddedSquaresZ),"Original global info map/padded bounds differ");std::uint64_t prior=0;for(const auto& u:f.updates){Check(u.serial>prior&&(!prior||u.serial==prior+1)&&u.serial<=f.serial,"Original global info pending update order differs");if(!u.clear){Check(bool(u.inputs),"Original global info update lacks actual inputs");Inputs(*u.inputs);Coverage(u.radarGroundLos);}prior=u.serial;}Check(f.updates.empty()||prior==f.serial,"Original global info pending history is incomplete");if(f.mode==NativeWorldInfoMode::Metal){Check(bool(f.metal),"Original global metal info token is absent");ValidateNativeMetalInfoFrame(*f.metal);}}
NativeWorldInfoRecorder::NativeWorldInfoRecorder(){Reset();}
void NativeWorldInfoRecorder::Reset(){auto id=nextMap.load();for(;;){if(id==0||id==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native world info map identity exhausted");if(nextMap.compare_exchange_weak(id,id+1))break;}frame={};frame.mapIdentity=id;previous.reset();previousRadarGround.reset();previousRadarGroundGlobal=false;}
void NativeWorldInfoRecorder::SetMode(NativeWorldInfoMode mode,std::uint32_t x,std::uint32_t z,std::uint32_t px,std::uint32_t pz){Check(x&&z&&px>=x&&pz>=z&&px<=16384&&pz<=16384&&std::has_single_bit(px)&&std::has_single_bit(pz),"Original global info selected map bounds differ");auto candidate=frame;candidate.mode=mode;candidate.squaresX=x;candidate.squaresZ=z;candidate.paddedSquaresX=px;candidate.paddedSquaresZ=pz;candidate.modeGeneration=Next(candidate.modeGeneration);candidate.updates.clear();if(mode!=NativeWorldInfoMode::Metal){candidate.serial=Next(candidate.serial);candidate.updates.push_back({candidate.serial,true,{},{}});}frame=std::move(candidate);}
void NativeWorldInfoRecorder::Update(std::shared_ptr<const NativeMiniMapInfo> raw,std::uint32_t drawFrame){Check(bool(raw),"Original global info raw source unavailable");Inputs(*raw);Check(frame.modeGeneration!=0,"Original global info map must initialize before update");auto candidate=frame;candidate.sourceDrawFrame=drawFrame;
	if(candidate.mode==NativeWorldInfoMode::Los){auto draw=std::make_shared<NativeMiniMapInfo>(previous?*previous:*raw);draw->time=raw->time;draw->highQuality=raw->highQuality;draw->alwaysColor=raw->alwaysColor;draw->losColor=raw->losColor;draw->radarColor=raw->radarColor;draw->jammerColor=raw->jammerColor;draw->radarColor2=raw->radarColor2;candidate.serial=Next(candidate.serial);candidate.updates.push_back({candidate.serial,false,std::move(draw),previousRadarGround?previousRadarGround:raw->groundLos,previousRadarGround?previousRadarGroundGlobal:raw->globalLos});}else if(candidate.mode==NativeWorldInfoMode::Metal){candidate.serial=Next(candidate.serial);}
	frame=std::move(candidate);previousRadarGroundGlobal=previous?previous->globalLos:raw->globalLos;previousRadarGround=previous?previous->groundLos:raw->groundLos;previous=std::move(raw);
}
std::shared_ptr<const NativeWorldInfoFrame> NativeWorldInfoRecorder::Capture(std::shared_ptr<const NativeMetalInfoFrame> metal)const{if(!frame.modeGeneration)return {};auto result=std::make_shared<NativeWorldInfoFrame>(frame);result->metal=std::move(metal);ValidateNativeWorldInfoFrame(*result);return result;}
void NativeWorldInfoRecorder::Retire(std::uint64_t identity,std::uint64_t serial){Check(identity==frame.mapIdentity&&serial<=frame.serial,"Original global info retirement refers to another map/future serial");std::erase_if(frame.updates,[&](const auto& u){return u.serial<=serial;});}
}
