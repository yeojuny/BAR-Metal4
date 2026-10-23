/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
namespace rendering {
// Return false only when the native source scope is absent: normal GL then
// executes its original setter. An active unsupported sink throws, never
// silently accepts a headless uniform write.
bool NativeLegacyUniformFloat(unsigned type,int location,size_t count,bool transpose,const float* values);
bool NativeLegacyUniformInt(unsigned type,int location,size_t count,const int* values);
} // namespace rendering
