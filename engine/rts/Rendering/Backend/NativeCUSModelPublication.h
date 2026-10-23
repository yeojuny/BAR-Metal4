/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSPublishedData.h"

class UpdateList;

namespace rendering {

// This is the original CPU upload boundary. It uses the original UpdateList
// ranges and consumes them only after the immutable publication succeeds.
std::shared_ptr<const NativeCUSPublishedBuffer> PublishNativeCUSModelRanges(
	NativeCUSPublishedStorage&, NativeCUSPublishedKind, UpdateList&,
	std::span<const uint8_t> actualStorage, uint64_t drawFrame,
	size_t& allocationElements);

class NativeCUSModelPublication {
public:
	void Initialize();
	void Reset();
	bool Initialized() const { return initialized; }
	// Call at the corresponding original ModelsDataUploader::Update, before
	// Draw callbacks can mutate CPU model uniforms for the next publication.
	void UpdateTransforms(uint64_t drawFrame);
	void UpdateModelUniforms(uint64_t drawFrame);
	std::shared_ptr<const NativeCUSPublishedBuffer> Transforms() const { return transforms.Current(); }
	std::shared_ptr<const NativeCUSPublishedBuffer> ModelUniforms() const { return uniforms.Current(); }
private:
	NativeCUSPublishedStorage transforms{NativeCUSPublishedKind::Transforms};
	NativeCUSPublishedStorage uniforms{NativeCUSPublishedKind::ModelUniforms};
	size_t transformCapacity = 0, uniformCapacity = 0;
	bool initialized = false;
};

NativeCUSModelPublication& OriginalCUSModelPublication();

} // namespace rendering
