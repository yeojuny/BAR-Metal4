/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSPublishedData.h"

namespace rendering {
// Publish retains unchanged immutable page owners, including their observed
// masks. Publication serial/frame are metadata, not GPU bytes. Allocation and
// context boundaries must still produce distinct storage, even for empty data.
inline bool SameNativeCUSPublishedStorage(const NativeCUSPublishedBuffer& a, const NativeCUSPublishedBuffer& b)
{
	if (a.kind != b.kind || a.stride != b.stride || a.contextGeneration != b.contextGeneration ||
		a.allocationSerial != b.allocationSerial || a.capacityElements != b.capacityElements ||
		a.pages.size() != b.pages.size()) return false;
	for (size_t n = 0; n < a.pages.size(); ++n) {
		const auto& x = a.pages[n]; const auto& y = b.pages[n];
		if (x.get() != y.get() || x.owner_before(y) || y.owner_before(x)) return false;
	}
	return true;
}
} // namespace rendering
