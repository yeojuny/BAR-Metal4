/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeModelLightData.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace rendering {
namespace {
std::array<float,3> Normalize(std::array<float,3> p)
{
	const float lengthSquared = p[0]*p[0]+p[1]*p[1]+p[2]*p[2];
	if (lengthSquared != 0) {
		// Preserve original Mesa NORMALIZE_3FV evaluation order. Component
		// division changes the actual halfVector by one ULP for real inputs.
		const float inverseLength = 1.f / std::sqrt(lengthSquared);
		for (float& value: p) value *= inverseLength;
	}
	return p;
}
}
void SetNativeModelLightPosition(NativeModelLight& light, const float* value)
{
	std::copy_n(value,4,light.position.begin());
	// GLSL's original infinite-viewer halfVector is derived twice-normalized,
	// even for positional lights. It is not normalize(position + eyeZ).
	auto p=Normalize({value[0],value[1],value[2]});p[2]+=1;
	p=Normalize(p);light.halfVector={p[0],p[1],p[2],1};
}
void SetNativeModelLightDirection(NativeModelLight& light, const float* value)
{
	// The GLSL built-in exposes the stored direction, not the FFP-only
	// normalized spotlight direction. Original shader does not normalize it.
	std::copy_n(value,3,light.directionCutoff.begin());
}
void SetNativeModelLightCutoff(NativeModelLight& light, float value)
{
	if (value == 180 || (value >= 0 && value <= 90)) {
		// The pinned original Mesa GLSL built-in exposes its stored nonnegative
		// cosine (including 180 -> 0). Verified against the actual selected6085
		// shader state; CGL's documented -1 differs and remains recorded.
		const float radians = static_cast<float>(static_cast<double>(value) * std::numbers::pi_v<double> / 180.0);
		light.directionCutoff[3] = std::max(0.f, std::cos(radians));
	}
}
void ValidateNativeModelLightFrame(const NativeModelLightFrame& frame)
{
	if (frame.lights.size() != frame.configuredLights)
		throw std::invalid_argument("Original model-light slot count changed after capture");
	for (const auto& light: frame.lights) {
		for (const auto* values: {&light.position,&light.halfVector,&light.directionCutoff,&light.ambient,&light.diffuse,&light.specular,&light.attenuation})
			for (float value: *values) if (!std::isfinite(value))
				throw std::invalid_argument("Non-finite original model light uniform");
	}
}
std::vector<uint8_t> PackNativeModelLightFrame(const NativeModelLightFrame& frame)
{
	ValidateNativeModelLightFrame(frame);
	if (frame.lights.size() > (std::numeric_limits<size_t>::max()-16)/sizeof(NativeModelLight))
		throw std::overflow_error("Original model-light buffer size overflow");
	std::vector<uint8_t> bytes(16+frame.lights.size()*sizeof(NativeModelLight));
	const uint32_t count=frame.configuredLights;
	std::memcpy(bytes.data(),&count,sizeof(count));
	if (!frame.lights.empty()) std::memcpy(bytes.data()+16,frame.lights.data(),frame.lights.size()*sizeof(NativeModelLight));
	return bytes;
}
}
