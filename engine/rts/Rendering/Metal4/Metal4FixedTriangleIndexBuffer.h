/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4RuntimeNative.h"
#include "NativeTriangleReuseIndices.h"

#include <cstdlib>
#include <cstring>

namespace rendering::metal4 {

inline bool FixedTriangleIndexReuseEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("BAR_NATIVE_FIXED_TRIANGLE_INDEX_REUSE");
		return value && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

// Each renderer owns one immutable buffer for its index table. A prepared
// draw holds the buffer and registers it with the same lease as its inputs.
template <const auto& Indices>
class FixedTriangleIndexBuffer {
public:
	id<MTLBuffer> Prepare(RuntimeLease& lease)
	{
		if (!FixedTriangleIndexReuseEnabled())
			return nil;
		if (!buffer || context != lease.ContextIdentity() || buffer.device != lease.Device()) {
			constexpr auto bytes = Indices.size() * sizeof(Indices[0]);
			buffer = bytes <= lease.Device().maxBufferLength
				? [lease.Device() newBufferWithBytes:Indices.data() length:bytes options:MTLResourceStorageModeShared]
				: nil;
			context = lease.ContextIdentity();
			if (buffer && (buffer.length != bytes || !buffer.gpuAddress || (buffer.gpuAddress & 1u)))
				buffer = nil;
		}
		if (buffer)
			lease.AddAllocation(buffer);
		return buffer;
	}

	void Clear()
	{
		buffer = nil;
		context = nullptr;
	}

private:
	const Runtime* context = nullptr;
	__strong id<MTLBuffer> buffer = nil;
};

} // namespace rendering::metal4
