/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4Pass160Policy.h"

namespace rendering::metal4 {
enum class ModelPhaseBody165 : uint8_t { Unknown, CPULifecycleOnly };
// An effect description, NOT permission to omit the phase or its callbacks.
// Unlike TryKeepForwardPass160, inspection consumes no CPU dependency state.
// The actual ordered EncodeMRTPhase must still check prior readability, finish
// empty consumers and publish this distinct producer exactly once.
struct ModelPhaseExecution165 {
	SourceDependencyWork160 dependencies=SourceDependencyWork160::Unknown;
	ModelPhaseBody165 body=ModelPhaseBody165::Unknown;
	bool HasOnlyCPULifecycle()const
	{
		return dependencies==SourceDependencyWork160::NoCommands&&body==ModelPhaseBody165::CPULifecycleOnly;
	}
};
inline bool ModelPass165Enabled()
{
	static const bool enabled=[](){const char* value=std::getenv("BAR_NATIVE_PASS165");return !value||!(value[0]=='0'&&value[1]=='\0');}();
	return enabled;
}
// Local main opaque invocation counts only. These do NOT count all nested
// render encoders and cannot establish a net pass reduction by themselves.
struct ModelPassCounters165 {
	uint64_t queries=0,kept=0,disabled=0,noEncoder=0,postDeferred=0,attachment=0,unknown=0;
	uint64_t followingQueries160=0,followingKept160=0,followingBoundary160=0;
};
} // namespace rendering::metal4
