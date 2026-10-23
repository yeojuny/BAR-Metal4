/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4MainAttachments.h"
#include <vector>

namespace rendering::metal4 {

struct MainSamplingSelection {
	NativeMainSampling actual;
	std::vector<unsigned> unsupported;
	std::vector<unsigned> allocationFailures;
};

// Call during renderer construction, before the Session or source consumers are
// published. Resize uses Prepare with actual.samples; it must not renegotiate
// an active context's sample count in the middle of a frame or after producers.
inline MainSamplingSelection SelectMainSampling(RuntimeLease& lease, MainAttachmentStorage& storage,
	int requested, unsigned width, unsigned height, MTLPixelFormat format)
{
	const auto candidates = NativeMainSampleCandidates(requested);
	MainSamplingSelection selected;
	selected.actual.requested = unsigned(requested);
	std::uint64_t tried = 0;
	for (auto candidate : candidates) {
		const auto samples = std::max(1u, candidate);
		const auto bit = std::uint64_t{1} << samples;
		if (tried & bit) continue;
		tried |= bit;
		if (![lease.Device() supportsTextureSampleCount:samples]) {
			selected.unsupported.push_back(samples);
			continue;
		}
		try {
			// N=1 normally aliases a future drawable. Verify the same full-size
			// color format here without retaining an unused extra frame image.
			// No command is recorded and no uninitialized contents are published.
			__strong id<MTLTexture> colorProbe = nil;
			if (samples == 1)
				colorProbe = MainAttachmentStorage::Allocate(lease, width, height, format, 1,
					@"Startup single-sample color allocation verification");
			const auto prepared = storage.Prepare(lease, width, height, format, samples);
			selected.actual.effective = samples > 1 ? samples : 0;
			selected.actual.samples = samples;
			selected.actual.allocationGeneration = prepared->Generation();
			return selected;
		} catch (const MainAttachmentAllocationError&) {
			selected.allocationFailures.push_back(samples);
		}
	}
	throw MainAttachmentAllocationError();
}

} // namespace rendering::metal4
