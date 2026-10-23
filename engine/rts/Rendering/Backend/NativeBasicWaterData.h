/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/WorldFrameData.h"

namespace rendering {
struct NativeBasicWaterVertex {
	WorldVector3 position{};
	std::array<float,2> uv{};
};
static_assert(sizeof(NativeBasicWaterVertex)==20);
static_assert(offsetof(NativeBasicWaterVertex,uv)==12);
struct NativeBasicWaterMesh {
	std::vector<NativeBasicWaterVertex> vertices;
	std::vector<uint32_t> indices;
};
struct NativeBasicWaterMaterial {
	uint64_t generation=0;
	std::shared_ptr<const WorldTexture> texture;
	std::shared_ptr<const NativeBasicWaterMesh> mesh;
	bool originalMissingTexture=false;
};
struct NativeBasicWaterFrame {
	uint64_t sourceDrawFrame=0;
	std::shared_ptr<const NativeBasicWaterMaterial> material;
	WorldCamera camera;
	bool drawWater=false,hasVisibleWater=false,forceRendering=false,wireframe=false;
};
struct alignas(16) NativeBasicWaterUniforms {
	WorldMatrix viewProjection{};
	std::array<uint32_t,4> flags{}; // GL minus-one depth, original image flipV.
};
static_assert(sizeof(NativeBasicWaterUniforms)==80);
NativeBasicWaterMesh MakeNativeBasicWaterMesh(uint32_t mapSquaresX,uint32_t mapSquaresZ,
	uint32_t textureWidth,uint32_t textureHeight,const std::string& texturePath,float repeatX,float repeatZ);
bool NativeBasicWaterVisible(const NativeBasicWaterFrame&);
void ValidateNativeBasicWaterMaterial(const NativeBasicWaterMaterial&);
void ValidateNativeBasicWater(const NativeBasicWaterFrame&);
NativeBasicWaterUniforms MakeNativeBasicWaterUniforms(const NativeBasicWaterFrame&);
} // namespace rendering
