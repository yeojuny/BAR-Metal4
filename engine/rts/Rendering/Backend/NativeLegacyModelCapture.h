/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

namespace rendering {

// A connected original opaque graph owns its matching alpha graph too; the
// renderer validates that graph before submitting either main or planar draws.
// Legacy main-view models can still be needed by the independent shadow path.
constexpr bool NeedsLegacyWorldModelCapture(bool originalColor, bool originalShadow,
	bool shadowEnabled, bool planarView) noexcept
{
	return !originalColor || (!planarView && shadowEnabled && !originalShadow);
}

} // namespace rendering
