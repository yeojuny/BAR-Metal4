/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldInfoCapture.h"
#include "NativeWorldInfoRecorder.h"
#include "Rendering/Backend/NativeMetalInfoCapture.h"
#include "Game/GlobalUnsynced.h"
#include "Map/BaseGroundDrawer.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/ModInfo.h"
#include "System/Config/ConfigHandler.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#endif
namespace rendering {
namespace {
struct State {NativeWorldInfoRecorder recorder;std::string mode;};
State& Current(){static State value;return value;}
NativeWorldInfoRecorder& Recorder(){return Current().recorder;}
std::shared_ptr<const NativeMiniMapCoverage> CaptureCoverage(const ILosType& type,int ally){if(type.size.x<=0||type.size.y<=0||ally<0||std::size_t(ally)>=type.losMaps.size())throw content_error("Original global info viewer source unavailable");const auto& values=type.losMaps[ally].GetLosMap();if(values.size()!=std::size_t(type.size.x)*type.size.y)throw content_error("Original global info coverage extent mismatch");auto result=std::make_shared<NativeMiniMapCoverage>();result->width=type.size.x;result->height=type.size.y;result->counts.assign(values.begin(),values.end());return result;}
void VerifyLOSShader(){CFileHandler file("shaders/GLSL/infoLOS.lua",SPRING_VFS_RAW_FIRST);std::string text;if(!file.FileExists()||!file.LoadStringData(text))throw content_error("Original selected game LOS shader is missing");sha512::raw_digest raw;sha512::hex_digest hex{};sha512::calc_digest({text.data(),text.data()+text.size()},raw);sha512::dump_digest(raw,hex);if(std::string_view(hex.data())!="b91fcd6c263463c8ff3f36dd62f99ab1d904b0f25669aa04c826beae5ebf273d77da0f7145e11fc2915b666094cea117c0cda0eaaadec6448363995b2649ebd8")throw content_error("Native global LOS shader requires exact source review; selected test31657 is supported");}
NativeWorldInfoMode Mode(const std::string& mode){if(mode.empty())return NativeWorldInfoMode::Normal;if(mode=="los")return NativeWorldInfoMode::Los;if(mode=="metal")return NativeWorldInfoMode::Metal;throw content_error("Original global info mode has no native producer: "+mode);}
}
void ResetNativeWorldInfo(){Current()=State{};
#ifdef RECOIL_METAL4_LIVE_SCENE
	OriginalNativeTerrainImagePublication().InvalidateInfoLineage();
#endif
}
void SelectNativeWorldInfoMode(const std::string& mode){if(mode=="los")VerifyLOSShader();auto candidate=Current();candidate.mode=mode;candidate.recorder.SetMode(Mode(mode),mapDims.mapx,mapDims.mapy,mapDims.pwr2mapx,mapDims.pwr2mapy);SelectNativeMetalInfoMode(mode=="metal");Current()=std::move(candidate);}
void UpdateNativeWorldInfo(){if(!gu||!globalRendering||!readMap||!losHandler)throw content_error("Original global info update requires loaded engine sources");if(!Recorder().IsInitialized())Recorder().SetMode(Mode(Current().mode),mapDims.mapx,mapDims.mapy,mapDims.pwr2mapx,mapDims.pwr2mapy);auto raw=std::make_shared<NativeMiniMapInfo>();raw->mode=NativeMiniMapInfoMode::Los;raw->time=gu->gameTime;raw->globalLos=losHandler->GetGlobalLOS(gu->myAllyTeam);raw->highQuality=configHandler->GetBool("HighResInfoTexture");const auto* d=readMap->GetGroundDrawer();if(!d)throw content_error("Original global info colors unavailable");const auto color=[](const int* v){return std::array<float,4>{v[0]/float(CBaseGroundDrawer::losColorScale),v[1]/float(CBaseGroundDrawer::losColorScale),v[2]/float(CBaseGroundDrawer::losColorScale),0};};raw->alwaysColor=color(d->alwaysColor);raw->losColor=color(d->losColor);raw->radarColor=color(d->radarColor);raw->jammerColor=color(d->jamColor);raw->radarColor2=color(d->radarColor2);raw->groundLos=CaptureCoverage(losHandler->los,gu->myAllyTeam);raw->airLos=CaptureCoverage(losHandler->airLos,gu->myAllyTeam);raw->radar=CaptureCoverage(losHandler->radar,gu->myAllyTeam);raw->jammer=CaptureCoverage(losHandler->jammer,modInfo.separateJammers?gu->myAllyTeam:0);Recorder().Update(std::move(raw),globalRendering->drawFrame);}
std::shared_ptr<const NativeWorldInfoFrame> CaptureNativeWorldInfo(){return Recorder().Capture(CaptureNativeMetalInfo());}
void RetireNativeWorldInfo(std::uint64_t map,std::uint64_t serial){Recorder().Retire(map,serial);}
}
