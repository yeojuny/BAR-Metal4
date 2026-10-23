/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSMaterialControl.h"
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace rendering {
namespace {
std::atomic<bool> ready{false};
std::mutex mutex;
NativeCUSMaterialState state;
uint64_t nextRevision = 0;
uint64_t NextRevision()
{
	if (nextRevision == std::numeric_limits<uint64_t>::max())
		throw std::overflow_error("Native CUS material revision overflow");
	return ++nextRevision;
}
void Initialize(int configuredCUS2)
{
	if (state.revision == 0) state = {configuredCUS2 == 1, NextRevision()};
}
} // namespace
void SetNativeCUSMaterialConsumerReady(bool value) noexcept { ready.store(value); }
bool IsNativeCUSMaterialConsumerAvailable() noexcept { return ready.load(); }
NativeCUSMaterialState CaptureNativeCUSMaterialState(int configuredCUS2)
{
	std::lock_guard lock(mutex);
	Initialize(configuredCUS2);
	return state;
}
std::optional<NativeCUSMaterialState> ApplyNativeCUSMaterialAction(
	std::string_view action, int callerPlayerID, int localPlayerID, int configuredCUS2)
{
	const auto end = action.find_first_of(" \t\r\n");
	const auto name = action.substr(0, end);
	if (name != "reloadcusgl4" && name != "disablecusgl4") return std::nullopt;
	if (callerPlayerID < 0 || callerPlayerID != localPlayerID) return std::nullopt;
	if (!IsNativeCUSMaterialConsumerAvailable())
		throw std::runtime_error("The paired native CUS and original engine-standard material consumers are unavailable");
	std::lock_guard lock(mutex);
	// Failed revision allocation cannot partially initialize/change the state.
	const NativeCUSMaterialState candidate{name == "reloadcusgl4", NextRevision()};
	state = candidate;
	(void)configuredCUS2; // Original actions change effective state, not config.
	return state;
}
void ResetNativeCUSMaterialState()
{
	std::lock_guard lock(mutex);
	state = {};
}
} // namespace rendering
