/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include <array>
#include <string_view>
namespace rendering {
namespace grass {
inline constexpr uint64_t WidgetHash=0x0c5e8d8de57fb752ULL,VertexHash=0x3e752e45e9b82ef9ULL,FragmentHash=0x147b2601c241d12aULL;
inline constexpr uint64_t MeshHash=0x804ef32d65d50774ULL,HelperHash=0xd291eb46066d7b4aULL;
inline constexpr std::array<std::string_view,18> DefineNames={"MAPCOLORFACTOR","MAPCOLORBASE","ALPHATHRESHOLD","WINDSTRENGTH","WINDSCALE","WINDSAMPLESCALE","FADESTART","FADEEND","SHADOWFACTOR","HASSHADOWS","SHADOWTAPS","GRASSBRIGHTNESS","COMPACTVBO","UNITBENDENABLED","UNITBENDSTRENGTH","UNITBENDFALLOFF","UNITBENDSHRINK","DEBUG"};
inline constexpr size_t ViewProjectionByte=5*64,ViewInverseByte=7*64,ShadowViewByte=10*64,MapSizeByte=3*16,FogColorByte=5*16;
}
struct NativeGrassUniforms {
	std::array<float,4> wind{1,1,1,1},night{1,1,1,1};
	float distanceMult=.45f;
	int32_t unitBendCount=0;
};
// Each original indexed cull run retains the same immutable buffer generations.
// No expanded per-frame blade geometry or substitute instance population.
struct NativeGrassInput {
	uint64_t sourceDrawFrame=0,vertexHash=0,fragmentHash=0;
	NativeBufferSnapshotRef vertices,indices,instances;
	NativeBufferRange bend;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::array<NativeCUSTextureBinding,6> images;
	std::array<float,18> defines{};
	NativeGrassUniforms uniforms;
	uint32_t count=0,indexOffset=0,instanceCount=0,baseInstance=0;
	int32_t baseVertex=0;
};
uint64_t NativeGrassSourceHash(std::string_view);
void ValidateNativeGrassInput(const NativeGrassInput&);
void SetNativeGrassConsumerAvailable(bool);
bool IsNativeGrassConsumerAvailable();
}
