/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeOriginalShadowParticlesCapture.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/GL/VertexArrayTypes.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Game/Camera.h"
#include "Game/Game.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
namespace {
thread_local NativeOriginalShadowParticlesCapture* actualSource=nullptr;
void Check(bool value,const char* message){if(!value)throw std::logic_error(message);}
}
NativeOriginalShadowParticlesCapture* CurrentNativeOriginalShadowParticlesCapture()noexcept{return actualSource;}
NativeOriginalShadowParticlesCapture::NativeOriginalShadowParticlesCapture(NativeTextureContext& t,ui::NativeUIRecorder& r,NativeEngineModelClipState& c):textures(t),recorder(r),clips(c){}
void NativeOriginalShadowParticlesCapture::RequireScope()const
{
	Check(actualSource==this&&frame&&shadowHandler.InShadowPass()&&recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque,"Original shadow particles lost the actual common source scope");
}
std::shared_ptr<const NativeOriginalShadowParticlesFrame> NativeOriginalShadowParticlesCapture::Capture(uint64_t drawFrame,unsigned size,bool colored,const std::function<void()>& draw)
{
	Check(!actualSource&&!frame&&!subState&&draw&&size&&globalRendering&&drawFrame==uint64_t(globalRendering->drawFrame)&&game&&game->GetDrawMode()==CGame::gameShadowDraw&&camera&&camera->GetCamType()==CCamera::CAMTYPE_SHADOW,"Original shadow particle capture lost its actual frame/camera/caller");
	Check(textures.Initialized()&&textures.UIAttached()&&clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration(),"Original shadow particle context/history/clip owner is unconnected");
	auto result=std::make_shared<NativeOriginalShadowParticlesFrame>();result->sourceDrawFrame=drawFrame;result->size=size;result->colored=colored;
	frame=result.get();actualSource=this;
	try{RequireScope();draw();Check(!subState,"Original shadow particle state scope did not close");actualSource=nullptr;frame=nullptr;return result;}
	catch(...){actualSource=nullptr;frame=nullptr;subState=false;throw;}
}
void NativeOriginalShadowParticlesCapture::EnableColorOutput()
{
	RequireScope();recorder.UpdateWorldState([](auto& state){state.colorWrite={true,true,true,false};state.combinedColorWrite=state.colorWrite;},false);
}
void NativeOriginalShadowParticlesCapture::WithOriginalSubState(const std::function<void()>& body)
{
	RequireScope();Check(!subState&&body,"Original shadow particle substate is nested or missing");
	const auto oldWorld=recorder.CurrentWorldState();const auto oldBlend=recorder.CurrentState().draw.blend;
	const auto restore=[&]{
		recorder.UpdateWorldState([&](auto& state){state.depthTest=oldWorld.depthTest;state.depthWrite=oldWorld.depthWrite;},false);
		recorder.UpdateBlend([&](auto& state){state.enabled=oldBlend.enabled;state.sourceColor=state.sourceAlpha=oldBlend.sourceColor;state.destinationColor=state.destinationAlpha=oldBlend.destinationColor;});subState=false;
	};
	// GL::State::BlendFunc saves GL_BLEND_SRC/DST (RGB), then calls the
	// two-factor glBlendFunc again on pop. It also restores alpha factors to
	// those RGB factors; restoring an inherited separate alpha pair is wrong.
	subState=true;
	try{
		recorder.UpdateWorldState([](auto& state){state.depthTest=true;state.depthWrite=false;},false);
		recorder.UpdateBlend([](auto& state){state.enabled=true;state.sourceColor=state.sourceAlpha=ScreenBlendFactor::Zero;state.destinationColor=state.destinationAlpha=ScreenBlendFactor::SourceColor;});
		body();restore();
	}catch(...){restore();throw;}
}
void NativeOriginalShadowParticlesCapture::EnableProgram(){RequireScope();Check(subState,"Original ProjFX Enable has no actual state scope");OriginalNativeShadowParticlesProgramSource().Enable();}
void NativeOriginalShadowParticlesCapture::SetTextureArray(bool value){RequireScope();OriginalNativeShadowParticlesProgramSource().SetTextureArray(value);}
void NativeOriginalShadowParticlesCapture::SetShadowColorMode(float value){RequireScope();OriginalNativeShadowParticlesProgramSource().SetShadowColorMode(value);}
void NativeOriginalShadowParticlesCapture::DisableProgram(){RequireScope();OriginalNativeShadowParticlesProgramSource().Disable();}
void NativeOriginalShadowParticlesCapture::UnbindCurrent2D()
{
	RequireScope();const auto active=textures.History().ActiveUnit();Check(active.has_value(),"Original shadow particle unbind has no observed active unit");
	textures.Registry().Bind(*active,0,NativeCUSTextureTarget::Image2D);if(*active<2)recorder.SyncObservedTextureBinding(*active);
}
void NativeOriginalShadowParticlesCapture::Submit(uint32_t mode,std::span<const VA_TYPE_PROJ> vertices,std::span<const uint32_t> indices)
{
	RequireScope();Check(subState&&mode==GL_TRIANGLES,"Original shadow particle submit has another source primitive/state scope");
	NativeOriginalShadowParticlesDraw draw;OriginalNativeShadowParticlesProgramSource().Freeze(draw);
	// The original DrawElements still verifies its program and consumes upload
	// cursors on a zero-index call, but emits no draw or ordinal in that case.
	if(indices.empty())return;
	Check(indices.size()%3==0,"Original shadow particle pending range is not triangles");
	Check(std::none_of(clips.enabled.begin(),clips.enabled.end(),[](bool enabled){return enabled;}),"Original ProjFX has no defined legacy gl_ClipVertex for an enabled clip plane");
	for(uint32_t index:indices)Check(index<vertices.size(),"Original shadow particle pending index exceeds the actual primary buffer");
	draw.view=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);draw.zeroToOneDepth=globalRendering->supportClipSpaceControl;
	draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;
	draw.texture=textures.History().Binding(0,draw.textureArray?NativeCUSTextureTarget::Image2DArray:NativeCUSTextureTarget::Image2D);
	draw.vertices.reserve(vertices.size());
	for(const auto& v:vertices)draw.vertices.push_back({{v.pos.x,v.pos.y,v.pos.z},{v.uvw.x,v.uvw.y,v.uvw.z},{v.uvInfo.x,v.uvInfo.y,v.uvInfo.z,v.uvInfo.w},{v.aparams.x,v.aparams.y,v.aparams.z},{v.c.r,v.c.g,v.c.b,v.c.a}});
	draw.indices.assign(indices.begin(),indices.end());draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
}
