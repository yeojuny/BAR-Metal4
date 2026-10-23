/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include <array>
#include <string_view>
namespace rendering {
struct NativeReclaimFieldUniforms {
	std::array<float,2> toggles{},fadeDist{};
	std::array<float,4> fillMetal{},fillEnergy{},edgeMetal{},edgeEnergy{};
	float yOffset=0,gradientAlpha=0,energyOpacity=0;
	uint32_t vertsPerChunk=0;
};
// Immutable versions of the ORIGINAL chunk VBO and SSBO9, at each real draw.
struct NativeReclaimFieldInput {
	uint64_t sourceDrawFrame=0,vertexHash=0,fragmentHash=0;
	NativeBufferSnapshotRef vertices;
	NativeBufferRange params;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	NativeReclaimFieldUniforms uniforms;
	uint32_t first=0,count=0;
	bool edges=false;
};
uint64_t NativeReclaimFieldSourceHash(std::string_view);
void ValidateNativeReclaimFieldInput(const NativeReclaimFieldInput&);
// Only the outline uses the common homogeneous line/width/clipping service.
// Fill triangles keep the original buffers and run the paired Metal vertex stage.
std::vector<NativeWorldDrawVertex> EvaluateNativeReclaimFieldOutline(const NativeReclaimFieldInput&);
void SetNativeReclaimFieldConsumerAvailable(bool);
bool IsNativeReclaimFieldConsumerAvailable();
namespace reclaimfield {
// Original01ef UniformMatricesBuffer: cameraViewProj follows cameraProj;
// cameraViewInv follows cameraBillboardView (not cameraProj at row4).
inline constexpr size_t ViewProjectionByte=5*64,ViewInverseByte=7*64;
inline constexpr uint64_t VertexHash=0x6c1293441a890057ULL;
inline constexpr uint64_t FragmentHash=0x8a0081ded233cf24ULL;
inline constexpr uint64_t WidgetHash=0xfc2ffcdff7014e2bULL;
inline constexpr uint64_t HelperHash=0x5e36e0a925d07b33ULL;
}
}
