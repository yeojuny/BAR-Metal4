/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSModelInputs.h"

namespace rendering {
class WorldFrameCapture;

// AddToSubmission captures geometry/command identity; Submit retains the
// already published original SSBO versions. No live pose reconstruction.
class NativeCUSModelCapture {
public:
	explicit NativeCUSModelCapture(WorldFrameCapture& sharedModels): sharedModels(sharedModels) {}
	std::shared_ptr<const NativeCUSModelSubmission> CaptureSubmission(const NativeCUSSubmittedObject&);
	std::shared_ptr<const NativeCUSModelInputs> CaptureDraw(uint64_t frame, uint64_t sequence,
		std::span<const std::shared_ptr<const NativeCUSModelSubmission>> submissions,
		NativeBufferSnapshotRef instances,
		std::shared_ptr<const NativeCUSPublishedBuffer> transforms,
		std::shared_ptr<const NativeCUSPublishedBuffer> modelUniforms);
private:
	WorldFrameCapture& sharedModels;
};

} // namespace rendering
