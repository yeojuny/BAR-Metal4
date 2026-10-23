/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBlueprintData.h"
#include <string_view>

namespace rendering {

// The actual original helpers expand map-dependent unused water constants.
// Pair only the verified source template after reversing those exact literal
// expansions. This does not accept arbitrary shaders with similar names.
void ValidateNativeBlueprintProgram(NativeBlueprintProgram program,
	std::string_view vertex, std::string_view fragment, std::string_view engineDefinitions,
	std::string_view quaternionDefinitions, std::string_view shaderDefines, bool game31657=false);

} // namespace rendering
