/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rendering {
struct NativeMiniMapInfo;
struct NativeMiniMapBackground;
// Actual original game30770 NoGS stream; no engine-mode particle substitution.
struct NativeLuaNanoDraw {
	std::uint64_t sequence=0; // Actual shared later-world submission order.
	NativeBufferSnapshotRef vertexSlots; // Original attribute0 float1, 28 records.
	NativeBufferSnapshotRef indices; // Original 42/30 indices; default UInt16, explicit UInt32 allowed.
	NativeBufferSnapshotRef instances; // Original attrs1..4 float4, stride64.
	std::uint32_t instanceCount=0;
	std::map<std::string,float> uniforms;
	std::int32_t shape=0;
	// Both original submissions remain separate: NOTEQUAL0x40+LEQUAL, then
	// EQUAL0x40+depth-off. Requires the real world stencil producer/resource.
	bool ghostStencilPass=false;
	bool rejectFutureSpawns=false; // Exact31657 staggered batches wait until spawnFrame.
};
struct NativeLuaNanoFrame {
	std::uint64_t serial=0;
	std::vector<NativeLuaNanoDraw> draws;
	std::shared_ptr<const NativeMiniMapInfo> info;
	std::shared_ptr<const NativeMiniMapBackground> infoBackground;
	bool infoIsNormal=true; // Original empty mode's cleared RGB10A2 combiner.
	std::array<float,16> cameraViewInverse{}; // Actual engine UBO source, not a reinversion.
	std::array<float,16> cameraViewProjection{}; // Actual camera UBO product, not a new multiply.
	std::array<std::uint32_t,4> mapSquares{}; // actual mapX/Z and padded X/Z
	std::vector<std::string> unsupported;
};
}
