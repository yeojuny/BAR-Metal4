/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rendering::metal4 {
// Diagnostic policy only. No render state or resource lifetime is changed.
struct ModelTraceBudget {
	static constexpr uint64_t DurationMS = 5 * 60 * 1000;
	static constexpr size_t MaxRecords = 32768, MaxBytes = 16 * 1024 * 1024;
	uint64_t start = 0, next = 0, frame = std::numeric_limits<uint64_t>::max();
	size_t samples = 0, draws = 0, records = 0, bytes = 0;
	bool stopped = false;
	bool Select(uint64_t currentFrame, uint64_t now, bool eligible) {
		if (stopped) return false;
		if (samples && now - start >= DurationMS) { stopped = true; return false; }
		if (samples && frame == currentFrame) return true;
		if (!eligible || (samples && now < next)) return false;
		if (!samples) start = now;
		frame = currentFrame; next = now + 1000; draws = 0; ++samples;
		return true;
	}
	bool TakeDraw() { return !stopped && draws++ < 16; }
	bool TakeRecord(size_t length) {
		if (stopped || records >= MaxRecords || length > MaxBytes - bytes) {
			stopped = true; return false;
		}
		++records; bytes += length; return true;
	}
};
}
