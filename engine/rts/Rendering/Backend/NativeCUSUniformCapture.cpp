/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSUniformCapture.h"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "Rendering/GlobalRendering.h"
#include "Rendering/UniformConstants.h"
#include "Sim/Misc/TeamHandler.h"

namespace rendering {

static_assert(sizeof(UniformMatricesBuffer) == NativeCUSMatrixBytes);
static_assert(offsetof(UniformParamsBuffer, teamColor) == NativeCUSParamsPrefixBytes);
static_assert(sizeof(UniformParamsBuffer) == NativeCUSParamsBytes);
static_assert(MAX_PALETTE_COLORS == NativeCUSTeamRows);

struct NativeCUSUniformCapture::State {
	NativeCUSUniformStorage storage;
	UniformMatricesBuffer matrices;
	UniformParamsBuffer params{};
	bool initialized = false;
};

NativeCUSUniformCapture::NativeCUSUniformCapture(): state(std::make_unique<State>()) {}
NativeCUSUniformCapture::~NativeCUSUniformCapture() = default;

void NativeCUSUniformCapture::Initialize()
{
	if (state->initialized) throw std::runtime_error("CUS uniform capture initialized twice");
	if (!globalRendering || globalRendering->haveGL4 || UniformConstants::Supported())
		throw std::runtime_error("CUS native uniform capture cannot replace an active original GL uploader");
	state->storage.NewContext();
	state->initialized = true;
}

void NativeCUSUniformCapture::Reset()
{
	state->storage.NewContext();
	state->initialized = false;
}

bool NativeCUSUniformCapture::Initialized() const { return state->initialized; }

void NativeCUSUniformCapture::UpdateMatrices(uint64_t frame, const std::function<void(UniformMatricesBuffer*)>& writer)
{
	if (!state->initialized || !writer) throw std::runtime_error("CUS original matrix writer is not connected");
	writer(&state->matrices);
	state->storage.PublishMatrices(frame, std::span(reinterpret_cast<const uint8_t*>(&state->matrices), sizeof(state->matrices)));
}

void NativeCUSUniformCapture::UpdateParams(uint64_t frame, const std::function<void(UniformParamsBuffer*)>& writer)
{
	if (!state->initialized || !writer) throw std::runtime_error("CUS original parameter writer is not connected");
	// Same original writer predicates. Gather before invoking the callback so
	// allocation failure cannot consume another RNG vector on a retried update.
	std::vector<uint16_t> writtenRows;
	writtenRows.reserve(MAX_PALETTE_COLORS);
	for (int teamID = 0; teamID < teamHandler.ActiveTeams(); ++teamID) {
		if (teamHandler.Team(teamID) == nullptr || !teamHandler.IsActiveTeam(teamID)) continue;
		writtenRows.push_back(static_cast<uint16_t>(teamID));
	}
	for (int i = 0; i < MAX_CUSTOM_COLORS; ++i)
		writtenRows.push_back(static_cast<uint16_t>(CCustomColorPalette::EncodePaletteIndex(i)));
	writer(&state->params);
	state->storage.PublishParams(frame, std::span(reinterpret_cast<const uint8_t*>(&state->params), sizeof(state->params)), writtenRows);
}

std::shared_ptr<const NativeCUSUniformPublication> NativeCUSUniformCapture::Current() const { return state->storage.Current(); }

NativeCUSUniformCapture& OriginalCUSUniformCapture()
{
	static NativeCUSUniformCapture capture;
	return capture;
}

} // namespace rendering
