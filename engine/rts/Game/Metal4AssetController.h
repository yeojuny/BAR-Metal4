/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <string>

// Engine-owned asset diagnostic entered before window, font or GL startup.
// This is not a gameplay renderer or a product launch mode.
int RunMetal4AssetController(const std::string& assetDirectory,
	const std::string& outputDirectory, const std::string& materialSource,
	uint32_t width, uint32_t height);
