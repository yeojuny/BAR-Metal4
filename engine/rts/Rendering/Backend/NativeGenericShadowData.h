/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>
namespace rendering {
// Original immediate/list replay input BEFORE fixed-function projection or
// texture-matrix conversion. Normal is sampled at each actual Vertex call.
struct NativeGenericShadowVertex {
	std::array<float,4> position{},normal{},color{},uv{};
};
static_assert(sizeof(NativeGenericShadowVertex)==64);
static_assert(std::is_standard_layout_v<NativeGenericShadowVertex>);
static_assert(offsetof(NativeGenericShadowVertex,normal)==16&&offsetof(NativeGenericShadowVertex,color)==32&&offsetof(NativeGenericShadowVertex,uv)==48);
struct NativeGenericShadowInput {
	uint64_t sourceDrawFrame=0,contextGeneration=0;
	std::shared_ptr<const void> compiledOriginal;
	std::array<float,16> modelView{},projection{};
	std::array<bool,3> clipEnabled{};
	std::array<std::array<float,4>,3> clipPlanes{};
	bool zeroToOneDepth=false;
	std::vector<NativeGenericShadowVertex> vertices;
};
}
