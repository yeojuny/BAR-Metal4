/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeOriginalModelContext.h"
#include "NativeWorldParticleCapture.h"
#include "NativeWorldParticleGeometry.h"
#include "NativeLuaNanoRecorder.h"
#include "WorldFrameData.h"
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "System/Config/ConfigHandler.h"
#include "Game/GlobalUnsynced.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/GroundFlash.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/NanoParticles/NanoParticleConfig.h"
#include "Rendering/Env/NanoParticles/NanoParticleSystem.h"
#include "Rendering/Env/NanoParticles/NanoParticleRenderer.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/3DModelPiece.hpp"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Projectiles/PieceProjectile.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "System/Exceptions.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace rendering {
namespace {
std::array<float,3> Vector(const float3& value) { return {value.x,value.y,value.z}; }
std::array<float,16> Matrix(const CMatrix44f& value)
{
	std::array<float,16> result; std::copy_n(value.m,16,result.begin()); return result;
}
static_assert(sizeof(VA_TYPE_PROJ)==sizeof(NativeWorldParticleVertex));
void Append(NativeWorldParticleFrame& frame,const TypedRenderBuffer<VA_TYPE_PROJ>& geometry,NativeWorldParticleRange range)
{
	std::vector<NativeWorldParticleVertex> vertices(geometry.GetElems().size());
	for (size_t n=0;n<vertices.size();++n) {
		const auto& input=geometry.GetElems()[n];auto& output=vertices[n];
		std::memcpy(output.position.data(),&input.pos.x,sizeof(output.position));
		std::memcpy(output.uvw.data(),&input.uvw.x,sizeof(output.uvw));
		std::memcpy(output.uvInfo.data(),&input.uvInfo.x,sizeof(output.uvInfo));
		std::memcpy(output.animation.data(),&input.aparams.x,sizeof(output.animation));
		std::memcpy(output.color.data(),&input.c[0],sizeof(output.color));
	}
	AppendNativeParticleGeometry(frame,vertices,geometry.GetIndcs(),range);
}
}
struct NativeWorldParticleCapture::Impl {
	// This buffer has CPU arrays only: capture never Uploads/Draws it. Its
	// lifetime is engine-thread scoped, like the original MT scratch registry.
	TypedRenderBuffer<VA_TYPE_PROJ> geometry;
	std::shared_ptr<const NativeWorldParticleFrame> mainFrame;
};
NativeWorldParticleCapture::NativeWorldParticleCapture():impl(std::make_unique<Impl>()){}
NativeWorldParticleCapture::~NativeWorldParticleCapture()=default;
void NativeWorldParticleCapture::Reset() { impl->geometry.Clear(); impl->mainFrame.reset(); }

std::shared_ptr<const NativeWorldParticleFrame> NativeWorldParticleCapture::Capture(const CCamera& viewCamera,const ModelProvider& models,NativeWaterScenePass pass)
{
	if (projectileDrawer==nullptr || gs==nullptr || gu==nullptr || globalRendering==nullptr || readMap==nullptr)
		throw content_error("Native particle capture requires original loaded engine producers");
	// The class Draw methods use the engine's active camera for billboards.
	// Accepting an unrelated DTO camera would change the generated geometry.
	if (&viewCamera!=camera)
		throw content_error("Native particle capture requires the actual active engine camera");
	auto frame=std::make_shared<NativeWorldParticleFrame>();
	frame->drawFrame=globalRendering->drawFrame;
	frame->animationFrame=gs->frameNum+globalRendering->timeOffset;
	frame->cameraPosition=Vector(viewCamera.GetPos());
	frame->cameraRight=Vector(viewCamera.GetRight());frame->cameraUp=Vector(viewCamera.GetUp());
	frame->cameraView=Matrix(viewCamera.GetViewMatrix());
	frame->cameraProjection=Matrix(viewCamera.GetProjectionMatrix());
	frame->cameraViewProjection=Matrix(viewCamera.GetViewProjectionMatrix());
	for (unsigned n=0;n<6;++n) { const auto& p=viewCamera.GetFrustumPlane(n);frame->frustumPlanes[n]={p.x,p.y,p.z,p.w}; }
	const auto& sky=ISky::GetSky();frame->fogColor=Vector(sky->fogColor);
	const auto* fogCamera=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
	frame->fogCameraPosition=Vector(fogCamera->GetPos());
	frame->fogDistances={sky->fogStart*fogCamera->GetFarPlaneDist(),sky->fogEnd*fogCamera->GetFarPlaneDist()};
	frame->drawReflection=pass==NativeWaterScenePass::Reflection;frame->drawRefraction=pass==NativeWaterScenePass::Refraction;
	frame->visibleWater=globalRendering->drawWater&&readMap->HasVisibleWater();
	frame->softenRequested=projectileDrawer->wantSoften>0&&pass==NativeWaterScenePass::Main;frame->softenThreshold=8;
	frame->engineShapeConfigured=NanoParticles::GetConfig().enabled;
	frame->luaShapeConsumerAvailable=IsNativeLuaNanoConsumerAvailable();
	frame->atlas=projectileDrawer->textureAtlas->GetNativeAtlas();
	frame->groundAtlas=projectileDrawer->groundFXAtlas->GetNativeAtlas();
	if (frame->engineShapeConfigured && !NanoParticles::system.Enabled())
		frame->unsupported.emplace_back("Configured engine analytic nano consumer is not available; original legacy fallback remains selected");
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (NanoParticles::renderer) NanoParticles::renderer->CaptureNative(frame->engineNano,frame->engineNanoTemplate);
	const auto& ap=NanoParticles::GetConfig().appearance;
	frame->engineNanoAppearance={ap.drawRadius,ap.sizeVariation,ap.baseAlpha,ap.alphaVariation,ap.glowScale,ap.glowIntensity,ap.glowFalloff,
		ap.colorEqualize,ap.colorTargetLuma,ap.hueJitter,ap.coreBoost,ap.showInside,ap.noiseAmount,ap.noiseScale,ap.whiteHotspot,
		ap.whiteHotspotThreshold,ap.rotationRange,ap.rotationRate/GAME_SPEED,ap.noiseSpeed/GAME_SPEED};
#endif
	if (projectileDrawer->perlinTexObjects>0)
		frame->unsupported.emplace_back("Original animated perlin atlas subimage producer is not yet native");
	for (const auto& pieces:projectileHandler.flyingPieces) if (!pieces.empty())
		frame->unsupported.emplace_back("Original shatter FlyingPiece geometry requires native model index-range capture");

	// Original terrain/groundflash stage, including each class's own geometry,
	// viewer air-LOS, camera culling and per-flash depth parameter changes.
	if (pass==NativeWaterScenePass::Main) for (CGroundFlash* flash:projectileHandler.groundFlashes) {
		if (!(flash->alwaysVisible||gu->spectatingFullView||losHandler->InAirLos(flash,gu->myAllyTeam)) || !viewCamera.InView(flash->pos,flash->size)) continue;
		impl->geometry.Clear();
		{ CExpGenSpawnable::ScopedGeomFillTarget target(&impl->geometry);flash->Draw(); }
		NativeWorldParticleRange range;range.stage=NativeWorldParticleStage::GroundFlashes;range.groundAtlas=true;
		range.depthTest=frame->softenRequested?false:flash->depthTest;range.depthWrite=flash->depthMask;
		range.alphaThreshold=.01f;range.softenThreshold=-350;
		Append(*frame,impl->geometry,range);
	}

	// Actual CProjectileDrawer::DrawOpaque entry, including both water passes.
	// Use the real writer so mode, range, color and enable share its camera lineage.
	sky->SetupFog();
	// Preserve the original model-type/texture bins and their object order.
	for (unsigned type=MODELTYPE_3DO;type<MODELTYPE_CNT;++type) {
		const auto& renderer=projectileDrawer->modelRenderers[type];
		for (uint32_t bin=0;bin<renderer.GetNumObjectBins();++bin) for (const CProjectile* projectile:renderer.GetObjectBin(bin)) {
			const auto mask=pass==NativeWaterScenePass::Reflection?DrawFlags::SO_REFLEC_FLAG:pass==NativeWaterScenePass::Refraction?DrawFlags::SO_REFRAC_FLAG:DrawFlags::SO_OPAQUE_FLAG;
			if (!CProjectileDrawer::ShouldDrawProjectile(projectile,mask)) continue;
			if (projectile->luaDraw) {frame->unsupported.emplace_back("Original DrawProjectile Lua model override requires a native world command consumer");continue;}
			if (!projectile->weapon&&!projectile->piece) continue; // original switch's empty default
			try {
				NativeWorldProjectileModel output;output.projectileId=projectile->id;output.model=models(projectile->model);output.modelTextures=CaptureNativeS3OModelTextures(projectile->model);
				if (!output.model) throw content_error("Original projectile model provider returned no geometry");
				if (!teamHandler.IsValidTeam(projectile->GetTeamID())) throw content_error("Original projectile has an invalid team color");
				std::copy_n(teamHandler.Team(projectile->GetTeamID())->color,4,output.teamColor.begin());
				output.pieces.resize(projectile->model->pieceObjects.size());
				if (projectile->weapon) {
					output.world=Matrix(projectile->GetTransformMatrix(projectile->GetProjectileType()==WEAPON_MISSILE_PROJECTILE));
					for (size_t n=0;n<output.pieces.size();++n) output.pieces[n]={Matrix(projectile->model->pieceObjects[n]->bposeTransform.ToMatrix()),true};
				} else {
					const auto* piece=static_cast<const CPieceProjectile*>(projectile);
					CMatrix44f transform(piece->drawPos);transform.Rotate(piece->GetDrawAngle()*math::DEG_TO_RAD,piece->spinVec);output.world=Matrix(transform);
					const auto it=std::find(projectile->model->pieceObjects.begin(),projectile->model->pieceObjects.end(),piece->omp);
					if (it==projectile->model->pieceObjects.end()) throw content_error("Original flying projectile piece is absent from its model");
					output.modelPiece=std::distance(projectile->model->pieceObjects.begin(),it);output.singlePiece=(piece->explFlags&PF_Recursive)==0;
					const auto include=[&](auto&& self,const S3DModelPiece* p)->void {
						const auto found=std::find(projectile->model->pieceObjects.begin(),projectile->model->pieceObjects.end(),p);
						if (found==projectile->model->pieceObjects.end()) throw content_error("Original recursive flying piece is absent from its model");
						const auto& b=p->bposeTransform;
						const Transform pose(b.r,output.singlePiece?ZeroVector:b.t-piece->omp->bposeTransform.t,b.s);
						output.pieces[std::distance(projectile->model->pieceObjects.begin(),found)]={Matrix(pose.ToMatrix()),true};
						if (!output.singlePiece) for (const auto* child:p->children) self(self,child);
					};include(include,piece->omp);
				}
				frame->models.emplace_back(std::move(output));
			} catch (const content_error& error) {frame->unsupported.emplace_back(error.what());}
		}
	}
	// DrawOpaque has a persistent GL_FOG disable before the alpha stage.
	OriginalNativeModelContext().fogEnabled=false;
	// Original sorted then unsorted class geometry. The water halves share
	// exact same immutable range, as the original reuse-water path does.
	const auto mask=pass==NativeWaterScenePass::Reflection?DrawFlags::SO_REFLEC_FLAG:pass==NativeWaterScenePass::Refraction?DrawFlags::SO_REFRAC_FLAG:DrawFlags::SO_ALPHAF_FLAG;
	const bool reuse=pass==NativeWaterScenePass::Refraction && configHandler->GetBool("ProjectileDrawReuseWaterPasses") &&
		impl->mainFrame && impl->mainFrame->drawFrame==frame->drawFrame;
	if (reuse) {
		// Original reuseRefraction submits the saved main-camera alpha range.
		frame->vertices=impl->mainFrame->vertices;frame->indices=impl->mainFrame->indices;
		for (const auto& range:impl->mainFrame->ranges) if(range.stage==NativeWorldParticleStage::AlphaBelowWater || range.stage==NativeWorldParticleStage::AlphaAll) {
			auto refracted=range;refracted.stage=NativeWorldParticleStage::AlphaBelowWater;frame->ranges.push_back(refracted);
		}
	} else {
		impl->geometry.Clear();projectileDrawer->FillNativeAlphaGeometry(impl->geometry,frame->unsupported,mask);
		NativeWorldParticleRange alpha;
		alpha.stage=pass==NativeWaterScenePass::Reflection?NativeWorldParticleStage::AlphaAboveWater:
			(frame->visibleWater||pass==NativeWaterScenePass::Refraction)?NativeWorldParticleStage::AlphaBelowWater:NativeWorldParticleStage::AlphaAll;
		Append(*frame,impl->geometry,alpha);
		if (pass==NativeWaterScenePass::Main&&frame->visibleWater&&!impl->geometry.GetIndcs().empty()) {
			auto above=frame->ranges.back();above.stage=NativeWorldParticleStage::AlphaAboveWater;frame->ranges.push_back(above);
		}
	}
	if (pass==NativeWaterScenePass::Main) impl->mainFrame=frame;
	return frame;
}
}
