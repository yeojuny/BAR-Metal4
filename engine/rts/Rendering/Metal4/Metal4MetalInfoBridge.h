/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

namespace rendering {
// Engine-thread lifetime hooks. The maintenance callback runs the original
// info handler cadence even without a visible drawable, and retires real GPU work.
void InitializeMetalInfoMaintenance();
void ResetMetalInfoResources();
void ShutdownMetalInfoMaintenance();
}
