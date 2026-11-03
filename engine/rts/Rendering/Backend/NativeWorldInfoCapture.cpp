/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldInfoCapture.h"
#include "NativeWorldInfoRecorder.h"
#include "NativeCoverageRevision.h"
#include "NativeCoverageSnapshotReuse.h"
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
namespace {NativeCoverageMutationTracker coverageChanges;}
void InvalidateNativeCoverage(const void* owner) noexcept{coverageChanges.Invalidate(owner);}
std::uint64_t WatchNativeCoverage(unsigned slot,const void* owner) noexcept{
#ifdef RECOIL_METAL4_LIVE_SCENE
 return coverageChanges.Watch(slot,owner);
#else
 return 0;
#endif
}
void ResetNativeCoverageWatches() noexcept{coverageChanges.Reset();}
namespace {
struct State {NativeWorldInfoRecorder recorder;std::string mode;std::array<NativeCoverageSnapshotCache,4> coverage;};
State& Current(){static State value;return value;}
NativeWorldInfoRecorder& Recorder(){return Current().recorder;}
std::shared_ptr<const NativeMiniMapCoverage> CaptureCoverage(const ILosType& type,int ally,unsigned slot){if(type.size.x<=0||type.size.y<=0||ally<0||std::size_t(ally)>=type.losMaps.size())throw content_error("Original global info viewer source unavailable");const auto& map=type.losMaps[ally];const auto& values=map.GetLosMap();if(values.size()!=std::size_t(type.size.x)*type.size.y)throw content_error("Original global info coverage extent mismatch");return CaptureNativeWorldInfoCoverage(slot,&map,values,type.size.x,type.size.y);}
void VerifyLOSShader(){CFileHandler file("shaders/GLSL/infoLOS.lua",SPRING_VFS_RAW_FIRST);std::string text;if(!file.FileExists()||!file.LoadStringData(text))throw content_error("Original selected game LOS shader is missing");sha512::raw_digest raw;sha512::hex_digest hex{};sha512::calc_digest({text.data(),text.data()+text.size()},raw);sha512::dump_digest(raw,hex);if(std::string_view(hex.data())!="b91fcd6c263463c8ff3f36dd62f99ab1d904b0f25669aa04c826beae5ebf273d77da0f7145e11fc2915b666094cea117c0cda0eaaadec6448363995b2649ebd8")throw content_error("Native global LOS shader requires exact source review; selected test31657 is supported");}
NativeWorldInfoMode Mode(const std::string& mode){if(mode.empty())return NativeWorldInfoMode::Normal;if(mode=="los")return NativeWorldInfoMode::Los;if(mode=="metal")return NativeWorldInfoMode::Metal;throw content_error("Original global info mode has no native producer: "+mode);}
}
std::shared_ptr<const NativeMiniMapCoverage> CaptureNativeWorldInfoCoverage(unsigned slot,const void* owner,
	std::span<const std::uint16_t> values,std::uint32_t width,std::uint32_t height)
{
	// The fixed role slots and mutation watches are shared by WorldInfo and PiP.
	// A changed/unknown source gets a new copy; retained older snapshots stay immutable.
	auto& cache=Current().coverage.at(slot);
	return cache.Capture(owner,values,width,height,WatchNativeCoverage(slot,owner));
}
void ResetNativeWorldInfo(){Current()=State{};ResetNativeCoverageWatches();
#ifdef RECOIL_METAL4_LIVE_SCENE
	OriginalNativeTerrainImagePublication().InvalidateInfoLineage();
#endif
}
void SelectNativeWorldInfoMode(const std::string& mode){if(mode=="los")VerifyLOSShader();auto candidate=Current();candidate.mode=mode;candidate.recorder.SetMode(Mode(mode),mapDims.mapx,mapDims.mapy,mapDims.pwr2mapx,mapDims.pwr2mapy);SelectNativeMetalInfoMode(mode=="metal");Current()=std::move(candidate);}
void UpdateNativeWorldInfo(){if(!gu||!globalRendering||!readMap||!losHandler)throw content_error("Original global info update requires loaded engine sources");if(!Recorder().IsInitialized())Recorder().SetMode(Mode(Current().mode),mapDims.mapx,mapDims.mapy,mapDims.pwr2mapx,mapDims.pwr2mapy);auto raw=std::make_shared<NativeMiniMapInfo>();raw->mode=NativeMiniMapInfoMode::Los;raw->time=gu->gameTime;raw->globalLos=losHandler->GetGlobalLOS(gu->myAllyTeam);raw->highQuality=configHandler->GetBool("HighResInfoTexture");const auto* d=readMap->GetGroundDrawer();if(!d)throw content_error("Original global info colors unavailable");const auto color=[](const int* v){return std::array<float,4>{v[0]/float(CBaseGroundDrawer::losColorScale),v[1]/float(CBaseGroundDrawer::losColorScale),v[2]/float(CBaseGroundDrawer::losColorScale),0};};raw->alwaysColor=color(d->alwaysColor);raw->losColor=color(d->losColor);raw->radarColor=color(d->radarColor);raw->jammerColor=color(d->jamColor);raw->radarColor2=color(d->radarColor2);raw->groundLos=CaptureCoverage(losHandler->los,gu->myAllyTeam,0);raw->airLos=CaptureCoverage(losHandler->airLos,gu->myAllyTeam,1);raw->radar=CaptureCoverage(losHandler->radar,gu->myAllyTeam,2);raw->jammer=CaptureCoverage(losHandler->jammer,modInfo.separateJammers?gu->myAllyTeam:0,3);Recorder().Update(std::move(raw),globalRendering->drawFrame);}
std::shared_ptr<const NativeWorldInfoFrame> CaptureNativeWorldInfo(){return Recorder().Capture(CaptureNativeMetalInfo());}
void RetireNativeWorldInfo(std::uint64_t map,std::uint64_t serial){Recorder().Retire(map,serial);}
}
