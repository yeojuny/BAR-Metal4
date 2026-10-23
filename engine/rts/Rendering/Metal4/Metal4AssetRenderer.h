/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "../Backend/AssetDiagnosticTypes.h"

namespace rendering {

// The query creates a native device but submits no work. Lack of a device throws;
// lack of Metal4 family support is returned explicitly without selecting a fallback.
DeviceCapabilities QueryMetal4Capabilities();

// Synchronous, direct MTL4 offscreen asset diagnostic. Input views must remain
// alive through the call. Render dimensions are explicit pixels, independent of
// presentation/backing scale. GPU objects and simd types stay private to .mm.
// Errors (capability, input, shader, capture, execution or timeout) throw runtime_error.
AssetDiagnosticResult RenderMetal4Asset(const AssetDiagnosticRequest& request);

} // namespace rendering
