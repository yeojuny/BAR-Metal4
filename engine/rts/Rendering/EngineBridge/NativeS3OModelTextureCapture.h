/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/ModelsLock.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include <stdexcept>
namespace rendering {
// Same original handler/material owner; never reparse/load/rebind the names.
inline std::shared_ptr<const NativeS3OTexturePair> CaptureNativeS3OModelTextures(const S3DModel* model){
 auto& source=OriginalNativeS3OTextures();if(!source.Attached())return {};
 auto lock=CModelsLock::GetScopedLock();
 if(!model||model->type!=MODELTYPE_S3O||model->loadStatus!=S3DModel::LOADED)
  throw std::runtime_error("Original native S3O draw lacks its actual loaded model");
 const auto* material=textureHandlerS3O.GetTexture(model->textureType);
 if(!material)throw std::runtime_error("Original native S3O draw has no actual handler material");
 return source.CapturePair(model->textureType,material->tex1,material->tex2);
}
} // namespace rendering
