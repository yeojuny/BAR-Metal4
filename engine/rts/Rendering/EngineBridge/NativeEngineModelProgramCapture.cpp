/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineModelProgramCapture.h"
#include "Rendering/Backend/NativeModelLightCapture.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SkyLight.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Game/Game.h"
#include <algorithm>
#include <stdexcept>
namespace rendering {
NativeEngineModelProgramState CaptureNativeEngineModelEnableValues(bool modern,bool shadows,const NativeEngineModelFogSource& fog){
 if(!game||!globalRendering||(game->GetDrawMode()!=CGame::gameNormalDraw&&game->GetDrawMode()!=CGame::gameDeferredDraw&&game->GetDrawMode()!=CGame::gameReflectionDraw&&game->GetDrawMode()!=CGame::gameRefractionDraw))
  throw std::logic_error("Original opaque model Enable was called outside its actual main or planar-water pass");
 NativeEngineModelProgramState out;out.observed=true;out.modern=modern;out.shadows=shadows;
 if(modern){out.alphaControl={.5f,1,0,0};out.cameraMode=game->GetDrawMode()==CGame::gameReflectionDraw?1:(game->GetDrawMode()==CGame::gameRefractionDraw?2:0);return out;}
 const auto* sky=ISky::GetSky().get();
 if(!sky||!sky->GetLight()||!sunLighting||!fog.observed||fog.sourceDrawFrame!=uint64_t(globalRendering->drawFrame))
  throw std::runtime_error("Original legacy Enable lacks actual sky/lighting/fixed-function fog writers");
 const auto direction=sky->GetLight()->GetLightDir();out.sunDirection={direction.x,direction.y,direction.z,0};
 const auto& ambient=sunLighting->modelAmbientColor;const auto& diffuse=sunLighting->modelDiffuseColor;const auto& specular=sunLighting->modelSpecularColor;
 out.sunAmbient={ambient.x,ambient.y,ambient.z,0};out.sunDiffuse={diffuse.x,diffuse.y,diffuse.z,0};out.sunSpecular={specular.x,specular.y,specular.z,0};
 out.shadowDensity=sunLighting->modelShadowDensity;std::copy_n(shadowHandler.GetShadowMatrixRaw(),16,out.shadowMatrix.begin());
 out.fogColor=fog.color;out.fogParameters=fog.parameters;
 out.lights=CaptureNativeModelLights();return out;
}
} // namespace rendering
