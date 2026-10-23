/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShadowColorCapture.h"
#include "Rendering/EngineBridge/NativeShadowLuaCapture.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "System/Exceptions.h"
#include <algorithm>
#include <cstring>

namespace rendering {
std::shared_ptr<const NativeShadowColorFrame> CaptureNativeShadowColor()
{
	if (shadowHandler.shadowConfig<=0) return {};
	if (!globalRendering || !projectileDrawer || !ISky::GetSky())
		throw content_error("Original shadow color capture requires loaded draw-side producers");
	auto output=std::make_shared<NativeShadowColorFrame>();
	output->sourceDrawFrame=globalRendering->drawFrame;
	output->size=shadowHandler.shadowMapSize;
	output->colored=shadowHandler.shadowColorMode>0;
	output->redraw=ISky::GetSky()->GetLight()->GetLightIntensity()>0;
	std::copy_n(shadowHandler.GetShadowViewMatrix().m,16,output->view.begin());
	std::copy_n(shadowHandler.GetShadowProjMatrix().m,16,output->projection.begin());
	const auto* shadowCamera=CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW);
	const auto& position=shadowCamera->GetPos();output->position={position.x,position.y,position.z};
	std::array<float,16> control{},identity{};std::copy_n(shadowCamera->GetClipControlMatrix().m,16,control.begin());
	identity[0]=identity[5]=identity[10]=identity[15]=1;
	if(control==identity)output->zeroToOneDepth=false;
	else {identity[10]=identity[14]=.5f;if(control!=identity)throw content_error("Original shadow color clip-control matrix is unsupported");output->zeroToOneDepth=true;}
	if (!output->redraw || !(shadowHandler.shadowGenBits&CShadowHandler::SHADOWGEN_BIT_PROJ)) return output;
	// The actual shadow camera already has the original culling matrices,
	// position and billboard axes from shadowHandler.Update. Do not substitute
	// the main camera or the drawing projection for this CPU geometry step.
	struct ActiveCameraScope {
		CCamera* previous=CCameraHandler::GetSetActiveCamera(CCamera::CAMTYPE_SHADOW);
		~ActiveCameraScope(){CCameraHandler::SetActiveCamera(previous->GetCamType());}
	} active;
	TypedRenderBuffer<VA_TYPE_PROJ> geometry;
	projectileDrawer->FillNativeShadowGeometry(geometry,output->unsupported);
	output->atlas=projectileDrawer->textureAtlas->GetNativeAtlas();
	output->vertices.resize(geometry.GetElems().size());
	for (std::size_t n=0;n<output->vertices.size();++n) {
		const auto& source=geometry.GetElems()[n];auto& target=output->vertices[n];
		std::memcpy(target.position.data(),&source.pos.x,sizeof(target.position));
		std::memcpy(target.uvw.data(),&source.uvw.x,sizeof(target.uvw));
		std::memcpy(target.uvInfo.data(),&source.uvInfo.x,sizeof(target.uvInfo));
		std::memcpy(target.animation.data(),&source.aparams.x,sizeof(target.animation));
		std::memcpy(target.color.data(),&source.c[0],sizeof(target.color));
	}
	output->indices.assign(geometry.GetIndcs().begin(),geometry.GetIndcs().end());
	output->luaEffects=CaptureNativeShadowLuaFrame(*output);
	return output;
}
}
