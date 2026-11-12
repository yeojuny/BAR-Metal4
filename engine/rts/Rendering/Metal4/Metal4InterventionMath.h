/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdlib>
#include <string_view>

namespace rendering::metal4 {
// Separate intervention 2026-11-12. Process-latched: changing the environment
// after compilation must not mix shader policies in an existing PSO cache.
inline bool InterventionFastMathEnabled() {
    static const bool enabled=[] {
        const char* p=std::getenv("BAR_NATIVE_INTERVENTION_FAST_MATH");
        return p && std::string_view(p)=="1";
    }();
    return enabled;
}
inline void ConfigureInterventionColorMath(MTLCompileOptions* options) {
    if (!InterventionFastMathEnabled()) return;
    options.mathMode=MTLMathModeFast;
    options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsFast;
}
}
