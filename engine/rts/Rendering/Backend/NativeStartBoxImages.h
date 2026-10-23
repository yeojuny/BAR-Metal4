/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeStartBoxData.h"
namespace rendering {
enum class NativeStartBoxImageKind : std::uint8_t { Empty, Color, Depth };
// Classification is a property of the retained source allocation, never of a
// $name, current frame, enable bit, or caller's desired shader type.
NativeStartBoxImageKind ClassifyNativeStartBoxImage(const NativeCUSTextureBinding&);
}
