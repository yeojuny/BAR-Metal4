/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/WorldFrameData.h"
#include <atomic>
#include <span>

namespace rendering {
// Byte-for-byte GroundDecal attribute stream, including original unused user
// attributes. Creation, expiration, ghost visibility and Lua ownership remain
// in CGroundDecalHandler; this is an immutable unsynced rendering snapshot.
struct NativeGroundDecalInstance {
	std::array<float,4> forcedHeight{}, posT{}, posB{}, uvMain{}, uvNorm{};
	std::array<float,4> createParams1{}, createParams2{}, createParams3{}, createParams4{};
	std::array<uint32_t,4> createParams5{};
	std::array<std::array<float,4>,2> userDefined{};
};
static_assert(sizeof(NativeGroundDecalInstance)==192);
struct NativeGroundDecalInstances {
	uint64_t revision=0;
	std::vector<NativeGroundDecalInstance> values;
};
struct NativeGroundDecalAtlasEntry {
	uint32_t source=0, page=0;
	std::array<int32_t,4> rectangle{}; // Original inclusive allocator pixels.
	std::array<float,4> sourceUV{0,0,1,1};
};
struct NativeGroundDecalAtlas {
	uint64_t generation=0;
	uint32_t width=0,height=0,pages=0,levels=0,padding=0;
	std::vector<std::shared_ptr<const WorldTexture>> sources; // Stable insertion IDs.
	// The actual allocator's iteration order, retained across overlapping pads.
	std::vector<NativeGroundDecalAtlasEntry> entries;
};
struct NativeGroundDecalAtlasVertex { std::array<float,2> position{},uv{}; };
struct NativeGroundDecalAtlasDraw {
	uint32_t source=0,page=0,level=0;
	std::array<float,4> sourceClamp{};
	std::array<NativeGroundDecalAtlasVertex,6> vertices{};
};
struct NativeGroundDecalFrame {
	uint64_t sourceDrawFrame=0;
	std::shared_ptr<const NativeGroundDecalAtlas> atlas;
	std::shared_ptr<const NativeGroundDecalInstances> instances;
	WorldMatrix viewProjection{},inverseViewProjection{},shadow{};
	std::array<float,4> mapDims{},mapDimsPO2{},groundAmbient{};
	WorldVector3 groundDiffuse{},sunDirection{},waterMin{},waterBase{},waterAbsorb{};
	std::array<int32_t,4> viewport{}; // Original GL viewPos/viewSize.
	float adjustedFrame=0;
	bool zeroToOneDepth=false, shadows=false, waterAbsorption=false, highQuality=false;
	uint32_t sampleCount=1;
};
struct alignas(16) NativeGroundDecalUniforms {
	WorldMatrix viewProjection{},inverseViewProjection{},shadow{};
	std::array<float,4> mapDims{},mapDimsPO2{},groundAmbient{},groundDiffuse{},sunDirection{};
	std::array<float,4> waterMin{},waterBase{},waterAbsorb{},frameScreen{};
	// Native fragment position -> original GL window coordinates; original
	// texelFetch intentionally uses these unoffset window coordinates.
	std::array<float,4> fragmentToLogical{};
	std::array<uint32_t,4> flags{}; // depth01, shadows, water, image flip mask
};
static_assert(sizeof(NativeGroundDecalUniforms)==368);
void ValidateNativeGroundDecalAtlas(const NativeGroundDecalAtlas&);
void ValidateNativeGroundDecalFrame(const NativeGroundDecalFrame&);
std::vector<NativeGroundDecalAtlasDraw> MakeNativeGroundDecalAtlasDraws(const NativeGroundDecalAtlas&);
NativeGroundDecalUniforms MakeNativeGroundDecalUniforms(const NativeGroundDecalFrame&,
	std::array<float,4> fragmentToLogical,uint32_t imageFlipMask);

inline std::atomic<bool> nativeGroundDecalConsumerAvailable{false};
inline bool IsNativeGroundDecalConsumerAvailable() noexcept {return nativeGroundDecalConsumerAvailable.load(std::memory_order_acquire);}
inline void SetNativeGroundDecalConsumerAvailable(bool available) noexcept {nativeGroundDecalConsumerAvailable.store(available,std::memory_order_release);}
}
