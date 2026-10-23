/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "WorldFrameData.h"
#include "NativeCUSTextureHistory.h"
#include <array>
#include <memory>
namespace rendering {
struct NativeUnitGlassInput {
 std::shared_ptr<const void> compiled;
 std::shared_ptr<const WorldModel> model;
 uint32_t piece=0;
 std::array<float,16> modelView{},projection{},viewInverse{};
 std::array<float,3> sun{},specular{};
 std::array<float,4> team{}; // actual GL default zero, not the unused Lua cache
 std::array<float,8> pbr{};
 std::array<NativeCUSTextureBinding,4> images;
 bool zeroToOne=false;
};
inline std::shared_ptr<const void>& UnitGlassConsumerOwner(){static std::shared_ptr<const void> owner;return owner;}
void ValidateNativeUnitGlass(const NativeUnitGlassInput&);
}
