/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// OpenAI Codex adaptation of the complete pinned Terraform Brush Capture.
#include "ScreenDrawList.h"
#include "WorldFrameData.h"
#include <functional>
#include <string_view>
#include <stdexcept>
#include <cmath>
#include <cstddef>
struct S3DModel;
namespace rendering {
enum class NativeTerraformCaptureProgram : unsigned { Reproject,Sprite,Model };
inline std::weak_ptr<const void>& NativeTerraformCaptureOwner(){static std::weak_ptr<const void> owner;return owner;}
using NativeTerraformModelProvider=std::function<std::shared_ptr<const WorldModel>(const S3DModel*)>;
inline NativeTerraformModelProvider& NativeTerraformCaptureModels(){static NativeTerraformModelProvider provider;return provider;}
struct NativeTerraformCaptureUniforms {
 std::array<float,4> worldOrigin{};
 std::array<float,4> worldExtent{};
 std::array<float,4> mapSize{};
 std::array<float,4> clampWater{};
 std::array<float,4> teamCol{};
 std::array<float,4> sizePx{};
 std::array<float,4> borderPx{};
 std::array<float,4> radiusPx{};
 std::array<float,4> inset{};
 std::array<float,16> viewProj{};
};
static_assert(sizeof(NativeTerraformCaptureUniforms)==208);
static_assert(offsetof(NativeTerraformCaptureUniforms,worldOrigin)==0);
static_assert(offsetof(NativeTerraformCaptureUniforms,worldExtent)==16);
static_assert(offsetof(NativeTerraformCaptureUniforms,mapSize)==32);
static_assert(offsetof(NativeTerraformCaptureUniforms,clampWater)==48);
static_assert(offsetof(NativeTerraformCaptureUniforms,teamCol)==64);
static_assert(offsetof(NativeTerraformCaptureUniforms,sizePx)==80);
static_assert(offsetof(NativeTerraformCaptureUniforms,borderPx)==96);
static_assert(offsetof(NativeTerraformCaptureUniforms,radiusPx)==112);
static_assert(offsetof(NativeTerraformCaptureUniforms,inset)==128);
static_assert(offsetof(NativeTerraformCaptureUniforms,viewProj)==144);
struct NativeTerraformCaptureParameters {NativeTerraformCaptureUniforms uniforms;std::array<std::array<std::uint32_t,4>,2> sampling{};};
static_assert(sizeof(NativeTerraformCaptureParameters)==240);
static_assert(offsetof(NativeTerraformCaptureParameters,sampling)==208);
struct NativeTerraformCaptureField {std::string_view name;unsigned components,programs;};
inline constexpr std::array<NativeTerraformCaptureField,10> NativeTerraformCaptureFields{{
 {"worldOrigin",2,1},
 {"worldExtent",2,1},
 {"mapSize",2,1},
 {"clampWater",1,1},
 {"teamCol",3,6},
 {"sizePx",1,2},
 {"borderPx",1,2},
 {"radiusPx",1,2},
 {"inset",1,2},
 {"viewProj",16,1}
}};
inline constexpr std::array<std::uint64_t,3> NativeTerraformCaptureVertexHashes{17707545575107498927ull,17707545575107498927ull,5450329082789770569ull};
inline constexpr std::array<std::uint64_t,3> NativeTerraformCaptureFragmentHashes{2931134429788224934ull,5248322792494479076ull,1177466442089295225ull};
struct NativeTerraformCaptureInput {
 NativeTerraformCaptureProgram program=NativeTerraformCaptureProgram::Reproject;
 NativeTerraformCaptureUniforms uniforms;
 std::shared_ptr<const void> compiled;
 std::array<ScreenSecondaryTexture,2> images;
};
inline void ValidateNativeTerraformCapture(const NativeTerraformCaptureInput& input){
 if(!input.compiled||input.program>NativeTerraformCaptureProgram::Model)throw std::runtime_error("Terraform Capture actual compiled owner missing");
 for(unsigned i=0;i<(input.program==NativeTerraformCaptureProgram::Reproject?2u:1u);++i){const auto& image=input.images[i];
  if(!image.texture||image.texture->incomplete||image.uvMode!=ScreenUVMode::Normalized||image.texture->format==ScreenTextureFormat::Depth16Unorm||image.texture->format==ScreenTextureFormat::Depth24Unorm||image.texture->format==ScreenTextureFormat::Depth32Float)throw std::runtime_error("Terraform Capture actual complete color/height input missing");
  ValidateScreenSampler(image.sampler);
 }
 for(float v:input.uniforms.worldOrigin)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite worldOrigin");
 for(float v:input.uniforms.worldExtent)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite worldExtent");
 for(float v:input.uniforms.mapSize)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite mapSize");
 for(float v:input.uniforms.clampWater)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite clampWater");
 for(float v:input.uniforms.teamCol)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite teamCol");
 for(float v:input.uniforms.sizePx)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite sizePx");
 for(float v:input.uniforms.borderPx)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite borderPx");
 for(float v:input.uniforms.radiusPx)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite radiusPx");
 for(float v:input.uniforms.inset)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite inset");
 for(float v:input.uniforms.viewProj)if(!std::isfinite(v))throw std::runtime_error("Terraform Capture nonfinite viewProj");
}
} // namespace rendering
