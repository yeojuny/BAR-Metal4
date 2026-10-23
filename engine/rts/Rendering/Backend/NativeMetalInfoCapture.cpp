/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMetalInfoCapture.h"
#include "NativeMetalInfoRecorder.h"
#include "Game/GlobalUnsynced.h"
#include "Map/MetalMap.h"
#include "Map/ReadMap.h"
#include "Sim/Misc/LosHandler.h"
#include "System/Config/ConfigHandler.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
static_assert(GAME_SPEED==30,"Native metal extraction cadence follows this pinned engine GAME_SPEED");
namespace rendering {
namespace {
struct State {
	NativeMetalInfoRecorder recorder;
	std::shared_ptr<const NativeMetalDensity> density;
	std::shared_ptr<const NativeMetalExtraction> extraction;
};
State& Current(){static State state;return state;}
NativeMetalInfoConsumer consumer;
void Check(bool b,const char* text){if(!b)throw content_error(text);}
// The original info handler is engine-thread serialized. A consumer may inspect
// the committed frame, but recursive update/reset/registration cannot replace a
// transaction that has not retired yet.
bool mutating=false;
struct Mutation {
	Mutation(){Check(!mutating,"Recursive native metal info mutation is unsupported");mutating=true;}
	~Mutation(){mutating=false;}
};
void Consume(State& candidate){auto frame=candidate.recorder.Capture();if(!frame->active||frame->updates.empty())return;Check(IsNativeMetalInfoConsumerAvailable()&&bool(consumer),"Native metal info mode has no connected completed GPU producer");const auto completed=consumer(frame);Check(completed==frame->serial,"Native metal info producer did not complete its original update transaction");candidate.recorder.Retire(completed);}
void VerifyShader(){CFileHandler file("shaders/GLSL/infoMetal.lua",SPRING_VFS_RAW_FIRST);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original metal info shader is missing");sha512::raw_digest raw;sha512::hex_digest hex{};sha512::calc_digest({source.data(),source.data()+source.size()},raw);sha512::dump_digest(raw,hex);Check(std::string_view(hex.data())=="14fc3c153fc61ac24eabab82739a315eb0429fdb74f886b1e4924f992a832a2183d2884d77f2b50d9a3e41b6da8079e2b47bbf4cbd68fdd7e81e4ab8101a3157","Native metal info shader source requires review; exact original test31657 is supported");}
std::shared_ptr<const NativeMetalDensity> Density(const State& state){const int width=metalMap.GetSizeX(),height=metalMap.GetSizeZ();Check(width>0&&height>0&&width==mapDims.hmapx&&height==mapDims.hmapy,"Native metal density source map bounds differ");const size_t count=size_t(width)*height;const auto* values=metalMap.GetDistributionMap();Check(values!=nullptr,"Native original metal distribution unavailable");if(state.density&&state.density->width==width&&state.density->height==height&&state.density->values.size()==count&&std::memcmp(state.density->values.data(),values,count)==0)return state.density;auto next=std::make_shared<NativeMetalDensity>();next->width=width;next->height=height;next->values.assign(values,values+count);return next;}
std::shared_ptr<const NativeMetalExtraction> Extraction(const State& state){Check(gu&&losHandler&&gu->myAllyTeam>=0&&size_t(gu->myAllyTeam)<losHandler->los.losMaps.size(),"Native metal extraction requires actual viewer ground LOS");const auto& type=losHandler->los;const auto& los=type.losMaps[gu->myAllyTeam].GetLosMap();const int width=metalMap.GetSizeX(),height=metalMap.GetSizeZ();Check(width>0&&height>0&&type.size.x>0&&type.size.y>0&&los.size()==size_t(type.size.x)*type.size.y,"Native metal extraction/ground LOS bounds differ");const size_t count=size_t(width)*height;const auto* values=metalMap.GetExtractionMap();Check(values!=nullptr,"Native original metal extraction unavailable");const bool global=losHandler->GetGlobalLOS(gu->myAllyTeam);if(state.extraction&&state.extraction->width==width&&state.extraction->height==height&&state.extraction->losWidth==type.size.x&&state.extraction->losHeight==type.size.y&&state.extraction->globalLos==global&&state.extraction->depth.size()==count&&std::memcmp(state.extraction->depth.data(),values,count*sizeof(float))==0&&state.extraction->los==los)return state.extraction;auto next=std::make_shared<NativeMetalExtraction>();next->width=width;next->height=height;next->losWidth=type.size.x;next->losHeight=type.size.y;next->depth.assign(values,values+count);next->los.assign(los.begin(),los.end());next->globalLos=global;ValidateNativeMetalExtraction(*next);return next;}
}
void SetNativeMetalInfoConsumer(NativeMetalInfoConsumer value){Mutation guard;SetNativeMetalInfoMaintenanceReady(false);consumer=std::move(value);SetNativeMetalInfoMaintenanceReady(bool(consumer));}
void ResetNativeMetalInfo(){Mutation guard;Current()=State{};}
void SelectNativeMetalInfoMode(bool active){Mutation guard;if(active){Check(IsNativeMetalInfoConsumerAvailable(),"Native metal info mode requires real connected GPU production");VerifyShader();}const float brightness=active?configHandler->GetFloatSafe("MetalViewBrightness",1.f):1.f;Check(std::isfinite(brightness),"Native metal view brightness is nonfinite");State candidate=Current();candidate.recorder.SetMode(active,brightness,mapDims.mapx,mapDims.mapy,mapDims.pwr2mapx,mapDims.pwr2mapy);Consume(candidate);Current()=std::move(candidate);}
void UpdateNativeMetalInfo(){Mutation guard;State candidate=Current();candidate.density=Density(candidate);if(candidate.recorder.NeedsExtractionUpdate())candidate.extraction=Extraction(candidate);candidate.recorder.Update(candidate.density,candidate.extraction);Consume(candidate);Current()=std::move(candidate);}
std::shared_ptr<const NativeMetalInfoFrame> CaptureNativeMetalInfo(){return Current().recorder.Capture();}
}
