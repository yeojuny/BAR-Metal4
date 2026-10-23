/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace rendering {
struct NativeCUSMaterialState {
	bool enabled = false;
	uint64_t revision = 0;
};
// Called only by the actual native paired CUS+engine-standard PSO owner.
void SetNativeCUSMaterialConsumerReady(bool ready) noexcept;
bool IsNativeCUSMaterialConsumerAvailable() noexcept;
// The first query initializes from the original cus2 integer (enabled iff1).
// Later config writes alone do not undo an original nonpersistent disable.
NativeCUSMaterialState CaptureNativeCUSMaterialState(int configuredCUS2);
// Exact original local-player chat actions; unknown/foreign commands are not
// consumed. Reload enables/revises even when cus2 remains0; Disable does not
// write config. Snapshot publication is all-or-nothing, before the next frame.
std::optional<NativeCUSMaterialState> ApplyNativeCUSMaterialAction(
	std::string_view action, int callerPlayerID, int localPlayerID, int configuredCUS2);
// Per-game lifecycle, after old CPU frames/resources have been retired.
void ResetNativeCUSMaterialState();
} // namespace rendering
