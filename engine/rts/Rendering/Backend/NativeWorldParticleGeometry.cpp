/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldParticleGeometry.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rendering {
void AppendNativeParticleGeometry(NativeWorldParticleFrame& frame,
	std::span<const NativeWorldParticleVertex> vertices,
	std::span<const std::uint32_t> indices, NativeWorldParticleRange range)
{
	if (indices.empty()) return;
	constexpr std::size_t maximum = std::numeric_limits<std::uint32_t>::max();
	if (frame.vertices.size() > maximum || frame.indices.size() > maximum || indices.size()%3 != 0 || vertices.size() > maximum-frame.vertices.size() || indices.size() > maximum-frame.indices.size())
		throw std::runtime_error("Original native particle range exceeds indexed triangle bounds");
	for (const auto index : indices) if (index >= vertices.size())
		throw std::runtime_error("Original native particle index exceeds its vertex range");
	for (const auto& vertex : vertices) {
		for (float value : vertex.position) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite original particle position");
		for (float value : vertex.uvw) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite original particle UV/page");
		for (float value : vertex.uvInfo) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite original particle UV extent");
		for (float value : vertex.animation) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite original particle animation");
	}
	if (!std::isfinite(range.alphaThreshold) || !std::isfinite(range.softenThreshold))
		throw std::runtime_error("Nonfinite original particle pass parameters");
	// A caller may reuse an existing owned range. Stage aliased input before
	// reserve can relocate either vector; never read an invalidated span.
	std::vector<NativeWorldParticleVertex> aliasedVertices;
	std::vector<std::uint32_t> aliasedIndices;
	const auto vertexAddress=reinterpret_cast<std::uintptr_t>(vertices.data());
	const auto frameVertexAddress=reinterpret_cast<std::uintptr_t>(frame.vertices.data());
	if (!frame.vertices.empty() && vertexAddress>=frameVertexAddress && vertexAddress-frameVertexAddress<frame.vertices.size()*sizeof(NativeWorldParticleVertex)) {
		aliasedVertices.assign(vertices.begin(),vertices.end());vertices=aliasedVertices;
	}
	const auto indexAddress=reinterpret_cast<std::uintptr_t>(indices.data());
	const auto frameIndexAddress=reinterpret_cast<std::uintptr_t>(frame.indices.data());
	if (!frame.indices.empty() && indexAddress>=frameIndexAddress && indexAddress-frameIndexAddress<frame.indices.size()*sizeof(std::uint32_t)) {
		aliasedIndices.assign(indices.begin(),indices.end());indices=aliasedIndices;
	}
	// Reserve every vector before publication. Allocation failure may change
	// capacity, but cannot partially append logical geometry/pass contents.
	frame.vertices.reserve(frame.vertices.size()+vertices.size());
	frame.indices.reserve(frame.indices.size()+indices.size());
	frame.ranges.reserve(frame.ranges.size()+1);
	const auto base = static_cast<std::uint32_t>(frame.vertices.size());
	range.firstIndex = static_cast<std::uint32_t>(frame.indices.size());
	range.indexCount = static_cast<std::uint32_t>(indices.size());
	frame.vertices.insert(frame.vertices.end(), vertices.begin(), vertices.end());
	for (const auto index : indices) frame.indices.push_back(base+index);
	frame.ranges.push_back(range);
}
}
