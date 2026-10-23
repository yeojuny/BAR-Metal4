/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeGenericShadowSource.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Game/Camera.h"
#include "Game/Game.h"
#include <stdexcept>
namespace rendering {
namespace {
void Check(bool value,const char* message){if(!value)throw std::logic_error(message);}
}
NativeGenericShadowSource::NativeGenericShadowSource(NativeTextureContext& t,ui::NativeUIRecorder& r,NativeEngineModelClipState& c):textures(t),recorder(r),clips(c){}
std::shared_ptr<const NativeGenericShadowInput> NativeGenericShadowSource::Snapshot(std::span<const NativeGenericShadowVertex> vertices,const std::array<float,16>& modelView,const std::array<float,16>& projection,bool zeroToOneDepth)const
{
	Check(globalRendering&&game&&game->GetDrawMode()==CGame::gameShadowDraw&&camera&&camera->GetCamType()==CCamera::CAMTYPE_SHADOW&&shadowHandler.InShadowPass()&&recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque,"Original generic shadow input lost its actual source camera/pass");
	Check(textures.Initialized()&&textures.UIAttached()&&clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration(),"Original generic shadow input lost its actual context/clip owner");
	Check(modelView==recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView)&&projection==recorder.GetMatrix(ui::NativeUIMatrixMode::Projection),"Original generic shadow input matrices differ from the replaying recorder");
	const auto program=OriginalNativeSourceProgramBinding().Current();
	// A CUS private handle or raw LuaShaders GL name is not this compiler token.
	// Their uniform/attribute snapshots require the actual custom source owner.
	Check(program.kind==NativeSourceProgramBinding::Kind::EngineModel&&program.owner,"Original generic shadow selected a custom/unbound program without its typed source snapshot");
	auto result=std::make_shared<NativeGenericShadowInput>();result->sourceDrawFrame=globalRendering->drawFrame;result->contextGeneration=clips.contextGeneration;result->compiledOriginal=program.owner;
	result->modelView=modelView;result->projection=projection;result->zeroToOneDepth=zeroToOneDepth;
	result->clipEnabled=clips.enabled;result->clipPlanes=clips.eyeEquations;result->vertices.assign(vertices.begin(),vertices.end());return result;
}
}
