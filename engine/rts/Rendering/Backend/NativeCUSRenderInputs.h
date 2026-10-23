/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include <array>
#include <vector>

namespace rendering {
// Original SVertexData's three packed u32 bone attributes. Native storage
// changes only the model-global index base; shader-addressed SSBO indices stay
// exactly as the original InstanceDataFrom* producer uploaded them.
struct NativeCUSRenderVertex {
 std::array<float,3> position,normal,tangent,bitangent;
 std::array<float,4> uv;
 std::array<uint32_t,3> bones;
};
static_assert(sizeof(NativeCUSRenderVertex)==76);
struct NativeCUSRenderGeometry {
 std::shared_ptr<const WorldModel> owner;
 std::vector<NativeCUSRenderVertex> vertices;
 std::vector<uint32_t> indices;
};
struct NativeCUSPublishedRange {size_t offset=0,size=0;};
struct NativeCUSRenderReadSet {
 std::vector<uint32_t> transforms,modelUniforms,teamColors;
};
NativeCUSRenderGeometry BuildNativeCUSRenderGeometry(std::shared_ptr<const WorldModel>);
// Validate all actual indexed vertices' addressable original storage before
// issuing a draw. Unobserved bytes are not replaced with identity/zero rows.
NativeCUSRenderReadSet ValidateNativeCUSRenderReads(const NativeCUSSourceDraw&);
// Coalesced observed ranges only. A native allocation may leave every other
// byte undefined because no accepted draw is allowed to read those bytes.
std::vector<NativeCUSPublishedRange> NativeCUSObservedRanges(const NativeCUSPublishedBuffer&);
} // namespace rendering
