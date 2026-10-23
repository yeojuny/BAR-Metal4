/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMiniMapCapture.h"
#include "NativeMetalInfoCapture.h"
#include "NativeWorldInfoCapture.h"
#include "Game/GlobalUnsynced.h"
#include "Map/BaseGroundDrawer.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Map/SMF/SMFReadMap.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Map/InfoTexture/IInfoTextureHandler.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/ModInfo.h"
#include "System/Config/ConfigHandler.h"
#include "System/Exceptions.h"
#include <cmath>
#include <cstring>

namespace rendering {
namespace {
std::array<float,4> Copy(const float4& value) { return {value.x,value.y,value.z,value.w}; }
std::array<float,3> Copy(const float3& value) { return {value.x,value.y,value.z}; }
std::shared_ptr<const NativeMiniMapCoverage> Coverage(const ILosType& type, int allyTeam)
{
	if (type.size.x<=0 || type.size.y<=0 || allyTeam<0 || size_t(allyTeam)>=type.losMaps.size())
		throw content_error("Native minimap coverage source is unavailable");
	const auto& source=type.losMaps[allyTeam].GetLosMap();
	if (source.size()!=size_t(type.size.x)*type.size.y)
		throw content_error("Native minimap coverage source extent mismatch");
	auto result=std::make_shared<NativeMiniMapCoverage>();
	result->width=type.size.x;result->height=type.size.y;
	result->counts.assign(source.begin(),source.end());return result;
}
}
void NativeMiniMapCapture::Reset() { cached.reset(); cachedIndividualImages = false; }
std::shared_ptr<const NativeMiniMapBackground> NativeMiniMapCapture::Capture(const NativeMiniMapState& state)
{ return Capture(state, false); }
std::shared_ptr<const NativeMiniMapBackground> NativeMiniMapCapture::CapturePiP(const NativeMiniMapState& state)
{ return Capture(state, true); }
std::shared_ptr<const NativeMiniMapBackground> NativeMiniMapCapture::Capture(const NativeMiniMapState& state, bool individualImages)
{
	auto* smf=dynamic_cast<CSMFReadMap*>(readMap);
	if (!smf || !gu || !losHandler || mapDims.mapx<=0 || mapDims.mapy<=0)
		throw content_error("Native minimap requires loaded original SMF/visibility data");
	const auto base=smf->GetNativeMiniMapImage();
	const std::string mode=individualImages?"los":(infoTextureHandler?infoTextureHandler->GetMode():"");
	const auto metalFrame=CaptureNativeMetalInfo();
	const auto worldInfo=individualImages?std::shared_ptr<const NativeWorldInfoFrame>{}:CaptureNativeWorldInfo();
	const bool sameInfo=cached && cachedIndividualImages==individualImages && (mode.empty()?!cached->info:cached->info && (mode=="los"?(cached->info->mode==NativeMiniMapInfoMode::Los&&cached->info->modeGeneration==metalFrame->modeGeneration&&cached->info->worldInfo&&worldInfo&&cached->info->worldInfo->mapIdentity==worldInfo->mapIdentity&&cached->info->worldInfo->serial==worldInfo->serial):mode=="metal" && cached->info->metal && cached->info->metal->mapIdentity==metalFrame->mapIdentity && cached->info->metal->modeGeneration==metalFrame->modeGeneration && cached->info->metal->serial==metalFrame->serial));
	if (sameInfo && cached->base==base && cached->updateSerial==state.updateSerial && cached->sourceRevision==smf->GetNativeMapEdgeResourceRevision()) return cached;
	cachedIndividualImages=individualImages;
	auto next=std::make_shared<NativeMiniMapBackground>();
	next->updateSerial=state.updateSerial;next->sourceRevision=smf->GetNativeMapEdgeResourceRevision();
	next->squaresX=mapDims.mapx;next->squaresZ=mapDims.mapy;
	next->paddedSquaresX=mapDims.pwr2mapx;next->paddedSquaresZ=mapDims.pwr2mapy;
	next->base=base;
	const uint32_t width=mapDims.mapxp1,height=mapDims.mapyp1;
	const size_t count=size_t(width)*height;
	const float* values=readMap->GetCornerHeightMapUnsynced();
	if (!values || width>32769 || height>32769) throw content_error("Native minimap height source extent is invalid");
	if (cached && cached->heights->width==width && cached->heights->height==height &&
		std::memcmp(cached->heights->corners.data(),values,count*sizeof(float))==0) next->heights=cached->heights;
	else {
		auto heights=std::make_shared<WorldTerrainHeights>();heights->width=width;heights->height=height;
		heights->corners.assign(values,values+count);
		for (float value:heights->corners) if (!std::isfinite(value)) throw content_error("Native minimap height is nonfinite");
		next->heights=std::move(heights);
	}
	if (!ISky::GetSky()) throw content_error("Native minimap requires actual sky light");
	next->lighting.groundAmbient=Copy(sunLighting->groundAmbientColor);
	next->lighting.groundDiffuse=Copy(sunLighting->groundDiffuseColor);
	next->lighting.lightDirection=Copy(ISky::GetSky()->GetLight()->GetLightDir());
	next->lighting.waterBase=Copy(waterRendering->baseColor);next->lighting.waterAbsorb=Copy(waterRendering->absorb);
	next->lighting.waterMinimum=Copy(waterRendering->minColor);next->lighting.waterLevel=CGround::GetWaterPlaneLevel();
	if (!mode.empty()) {
		if (mode!="los" && mode!="metal") throw content_error("Native minimap info mode has no native producer: "+mode);
		auto info=std::make_shared<NativeMiniMapInfo>();info->modeGeneration=metalFrame->modeGeneration;
		if(mode=="metal"){info->mode=NativeMiniMapInfoMode::Metal;info->metal=metalFrame;next->info=std::move(info);return cached=std::move(next);}
		info->mode=NativeMiniMapInfoMode::Los;
		if(!individualImages){if(!worldInfo)throw content_error("Original global LOS update has not initialized before minimap consumption");info->worldInfo=worldInfo;}
		info->globalLos=losHandler->GetGlobalLOS(gu->myAllyTeam);info->time=gu->gameTime;
		info->highQuality=configHandler->GetBool("HighResInfoTexture");
		const auto* drawer=readMap->GetGroundDrawer();
		if (!drawer) throw content_error("Native minimap LOS view colors are unavailable");
		const auto color=[](const int* v){return std::array<float,4>{v[0]/float(CBaseGroundDrawer::losColorScale),v[1]/float(CBaseGroundDrawer::losColorScale),v[2]/float(CBaseGroundDrawer::losColorScale),0};};
		info->alwaysColor=color(drawer->alwaysColor);info->losColor=color(drawer->losColor);info->radarColor=color(drawer->radarColor);
		info->jammerColor=color(drawer->jamColor);info->radarColor2=color(drawer->radarColor2);
		info->groundLos=Coverage(losHandler->los,gu->myAllyTeam);
		if (!individualImages) info->airLos=Coverage(losHandler->airLos,gu->myAllyTeam);
		info->radar=Coverage(losHandler->radar,gu->myAllyTeam);info->jammer=Coverage(losHandler->jammer,modInfo.separateJammers?gu->myAllyTeam:0);
		next->info=std::move(info);
	}
	return cached=std::move(next);
}
}
