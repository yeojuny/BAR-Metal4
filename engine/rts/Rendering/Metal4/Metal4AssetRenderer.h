/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "../Backend/AssetDiagnosticTypes.h"
#include "../Backend/VideoMemoryReporting.h"

namespace rendering {

// The query creates a native device but submits no work. Lack of a device throws;
// lack of Metal4 family support is returned explicitly without selecting a fallback.
DeviceCapabilities QueryMetal4Capabilities();

// Diagnostic only. Query on the already initialized scene device; no lease,
// command submission, residency change or GPU wait. Callers enforce scope.
bool Metal4MemoryReportingEnabled();
video_memory::MetalSample QueryMetal4MemorySample();

// Synchronous, direct MTL4 offscreen asset diagnostic. Input views must remain
// alive through the call. Render dimensions are explicit pixels, independent of
// presentation/backing scale. GPU objects and simd types stay private to .mm.
// Errors (capability, input, shader, capture, execution or timeout) throw runtime_error.
AssetDiagnosticResult RenderMetal4Asset(const AssetDiagnosticRequest& request);

} // namespace rendering
