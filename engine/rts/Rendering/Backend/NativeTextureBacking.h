/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstdint>

namespace rendering {

enum class NativeTextureBackingKind { ColorImage, MapDepth, ModelDepth, Other };
enum class NativeTextureContent { AllocationOnly, ProducerVersion };

// A family-specific derived owner retains the real immutable GPU image and
// its producer/dependency state. Neither a logical texture name nor the
// currently rendered frame is an image-content generation.
struct NativeTextureBacking {
	virtual ~NativeTextureBacking() = default;
	virtual NativeTextureBackingKind Kind() const noexcept = 0;
	uint64_t storageGeneration = 0, contentVersion = 0, sourceDrawFrame = 0;
	// ProducerVersion may be scheduled: its typed owner must enforce encoding
	// before a consumer. AllocationOnly retains actual undefined storage; it
	// does not certify a clear, a rendered image or constant default pixels.
	NativeTextureContent content = NativeTextureContent::AllocationOnly;
};

// Initial texture-object values from the original GL contract. Creation is
// the only implicit observation; later values come from actual source writes.
struct NativeTextureSampling {
	unsigned minFilter = 0x2702, magFilter = 0x2601; // NEAREST_MIPMAP_LINEAR / LINEAR
	unsigned wrapS = 0x2901, wrapT = 0x2901, wrapR = 0x2901; // REPEAT
	unsigned compareMode = 0, compareFunction = 0x0203; // NONE / LEQUAL
	unsigned depthMode = 0x1909; // LUMINANCE, original compatibility state
	std::array<unsigned, 4> swizzle{0x1903, 0x1904, 0x1905, 0x1906}; // RGBA
	std::array<float, 4> borderColor{};
	float minLod = -1000, maxLod = 1000, lodBias = 0, anisotropy = 1;
	int baseLevel = 0, maxLevel = 1000;
	bool operator==(const NativeTextureSampling&) const = default;
};

} // namespace rendering
