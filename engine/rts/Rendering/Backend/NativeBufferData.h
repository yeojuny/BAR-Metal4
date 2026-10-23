/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rendering {

enum class NativeBufferKind { Vertex, Index, Uniform, Storage };
enum class NativeScalarType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float32 };
enum class NativeValueShape { Scalar, Vector4, Matrix4 };

struct NativeBufferAttribute {
	uint32_t id = 0;
	std::string name;
	NativeScalarType scalarType = NativeScalarType::Float32;
	NativeValueShape valueShape = NativeValueShape::Scalar;
	uint32_t declaredCount = 1;
	size_t offset = 0;
	size_t byteSize = 0;
	bool normalized = false;

	bool operator==(const NativeBufferAttribute&) const = default;
};

struct NativeBufferLayout {
	NativeBufferKind kind = NativeBufferKind::Vertex;
	size_t elements = 0;
	size_t stride = 0;
	std::vector<NativeBufferAttribute> attributes;

	bool operator==(const NativeBufferLayout&) const = default;
};

inline size_t NativeScalarByteSize(NativeScalarType type)
{
	switch (type) {
		case NativeScalarType::Int8: case NativeScalarType::UInt8: return 1;
		case NativeScalarType::Int16: case NativeScalarType::UInt16: return 2;
		case NativeScalarType::Int32: case NativeScalarType::UInt32: case NativeScalarType::Float32: return 4;
	}
	throw std::invalid_argument("Unknown native buffer scalar type");
}

inline size_t CheckedNativeBufferByteSize(const NativeBufferLayout& layout)
{
	if (layout.elements == 0 || layout.stride == 0 || layout.attributes.empty())
		throw std::invalid_argument("Native buffer layout is empty");
	if (layout.elements > std::numeric_limits<size_t>::max() / layout.stride)
		throw std::length_error("Native buffer byte capacity overflows");
	for (size_t i = 0; i < layout.attributes.size(); ++i) {
		const auto& attr = layout.attributes[i];
		const size_t scalarSize = NativeScalarByteSize(attr.scalarType);
		if (attr.declaredCount == 0 || attr.byteSize == 0 || attr.byteSize % scalarSize != 0 ||
			attr.offset > layout.stride || attr.byteSize > layout.stride - attr.offset)
			throw std::invalid_argument("Native buffer attribute exceeds its declared record");
		if (attr.valueShape != NativeValueShape::Scalar && attr.valueShape != NativeValueShape::Vector4 && attr.valueShape != NativeValueShape::Matrix4)
			throw std::invalid_argument("Unknown native buffer value shape");
		for (size_t j = 0; j < i; ++j) {
			const auto& other = layout.attributes[j];
			if (attr.id == other.id || (attr.offset < other.offset + other.byteSize && other.offset < attr.offset + attr.byteSize))
				throw std::invalid_argument("Native buffer attributes duplicate or overlap");
		}
	}
	if (layout.kind == NativeBufferKind::Index) {
		const auto& attr = layout.attributes.front();
		if (layout.attributes.size() != 1 || attr.offset != 0 || attr.byteSize != layout.stride ||
			attr.valueShape != NativeValueShape::Scalar || attr.declaredCount != 1 || attr.normalized ||
			(attr.scalarType != NativeScalarType::UInt8 && attr.scalarType != NativeScalarType::UInt16 && attr.scalarType != NativeScalarType::UInt32) ||
			attr.byteSize != NativeScalarByteSize(attr.scalarType))
			throw std::invalid_argument("Native index buffer must contain one unsigned index per record");
	}
	if (layout.kind != NativeBufferKind::Vertex && layout.kind != NativeBufferKind::Index && layout.kind != NativeBufferKind::Uniform && layout.kind != NativeBufferKind::Storage)
		throw std::invalid_argument("Unknown native buffer kind");
	return layout.elements * layout.stride;
}

inline size_t CheckedNativeElementOffset(const NativeBufferLayout& layout, size_t first, size_t count)
{
	CheckedNativeBufferByteSize(layout);
	if (count == 0 || first > layout.elements || count > layout.elements - first)
		throw std::out_of_range("Native buffer element range exceeds capacity");
	return first * layout.stride;
}

struct NativeBufferAttributeWrite {
	uint32_t attribute = 0;
	std::span<const uint8_t> bytes;
};

// Raw typed bytes only: Lua numeric conversion and normalization policy stay
// with the original producer. Validate every field before changing any byte.
inline void WriteNativeBufferAttributes(std::span<uint8_t> destination, const NativeBufferLayout& layout,
	size_t first, size_t count, std::span<const NativeBufferAttributeWrite> writes)
{
	const size_t capacity = CheckedNativeBufferByteSize(layout);
	const size_t recordStart = CheckedNativeElementOffset(layout, first, count);
	if (destination.size() != capacity || writes.empty())
		throw std::invalid_argument("Native buffer write has no fields or mismatched storage");
	std::vector<const NativeBufferAttribute*> fields;
	std::vector<std::vector<uint8_t>> staging;
	fields.reserve(writes.size()); staging.reserve(writes.size());
	for (const auto& write: writes) {
		const auto field = std::find_if(layout.attributes.begin(), layout.attributes.end(), [&write](const auto& attr) { return attr.id == write.attribute; });
		if (field == layout.attributes.end() || std::find(fields.begin(), fields.end(), &*field) != fields.end())
			throw std::invalid_argument("Native buffer write contains unknown or duplicate attribute");
		if (count > std::numeric_limits<size_t>::max() / field->byteSize || write.bytes.size() != count * field->byteSize)
			throw std::invalid_argument("Native buffer write contains an incomplete or excess attribute");
		fields.push_back(&*field);
		// Stage aliased input before any mutation and preserve raw float/NaN bits.
		staging.emplace_back(write.bytes.begin(), write.bytes.end());
	}
	for (size_t i = 0; i < fields.size(); ++i) {
		const auto& attr = *fields[i];
		for (size_t record = 0; record < count; ++record)
			std::memcpy(destination.data() + recordStart + record * layout.stride + attr.offset,
				staging[i].data() + record * attr.byteSize, attr.byteSize);
	}
}

struct NativeBufferSnapshot {
	uint64_t object = 0;
	uint64_t revision = 0;
	NativeBufferLayout layout;
	std::vector<uint8_t> bytes;
};

using NativeBufferSnapshotRef = std::shared_ptr<const NativeBufferSnapshot>;

// The producer retains its mutable storage. A captured generation owns its
// bytes; later uploads or Delete cannot invalidate a recorded frame's data.
// Calls are serialized by the owning Lua/renderer thread, like the CPU shadow.
class NativeBufferSnapshotSource {
public:
	NativeBufferSnapshotSource() = default;
	NativeBufferSnapshotSource(const NativeBufferSnapshotSource&) = delete;
	NativeBufferSnapshotSource& operator=(const NativeBufferSnapshotSource&) = delete;
	NativeBufferSnapshotSource(NativeBufferSnapshotSource&& other) noexcept { *this = std::move(other); }
	NativeBufferSnapshotSource& operator=(NativeBufferSnapshotSource&& other) noexcept
	{
		if (this == &other) return *this;
		layout = std::move(other.layout);
		object = std::exchange(other.object, 0);
		revision = std::exchange(other.revision, 0);
		defined = std::exchange(other.defined, false);
		cached = std::move(other.cached);
		return *this;
	}

	void Define(const NativeBufferLayout& definition)
	{
		if (defined)
			throw std::logic_error("Native buffer definition is immutable");
		CheckedNativeBufferByteSize(definition);
		auto next = definition; // allocation precedes commit
		const uint64_t nextObject = NextObject();
		layout = std::move(next);
		object = nextObject; revision = 1; defined = true;
		cached.reset();
	}

	void InvalidateContents()
	{
		if (!defined)
			throw std::logic_error("Native buffer has no live definition");
		if (revision == std::numeric_limits<uint64_t>::max())
			throw std::overflow_error("Native buffer revision exhausted");
		++revision;
		cached.reset();
	}

	NativeBufferSnapshotRef Capture(std::span<const uint8_t> currentBytes) const
	{
		if (!defined)
			throw std::logic_error("Cannot capture a deleted or undefined native buffer");
		if (currentBytes.size() != CheckedNativeBufferByteSize(layout))
			throw std::invalid_argument("Native buffer snapshot capacity differs from producer layout");
		if (cached)
			return cached;
		auto next = std::make_shared<NativeBufferSnapshot>();
		next->object = object; next->revision = revision; next->layout = layout;
		next->bytes.assign(currentBytes.begin(), currentBytes.end());
		cached = std::move(next);
		return cached;
	}

	void Delete() noexcept
	{
		cached.reset(); layout = {}; defined = false; object = 0; revision = 0;
	}

	const NativeBufferLayout& Definition() const
	{
		if (!defined) throw std::logic_error("Native buffer has no live definition");
		return layout;
	}
	bool IsDefined() const noexcept { return defined; }
	uint64_t Revision() const noexcept { return revision; }

private:
	static uint64_t NextObject()
	{
		static std::atomic<uint64_t> next{1};
		uint64_t value = next.load(std::memory_order_relaxed);
		for (;;) {
			if (value == std::numeric_limits<uint64_t>::max())
				throw std::overflow_error("Native buffer object identity exhausted");
			if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
				return value;
		}
	}

	NativeBufferLayout layout;
	uint64_t object = 0, revision = 0;
	bool defined = false;
	mutable NativeBufferSnapshotRef cached;
};

struct NativeBufferRange {
	NativeBufferSnapshotRef snapshot;
	uint32_t binding = 0;
	size_t offset = 0;
	size_t size = 0;
};

inline std::pair<size_t, size_t> CheckedNativeBindingRange(const NativeBufferLayout& layout, uint32_t binding,
	size_t first, size_t count, uint32_t maxBindings, size_t offsetAlignment = 1)
{
	const uint32_t minimum = layout.kind == NativeBufferKind::Storage ? 4 : 6;
	if ((layout.kind != NativeBufferKind::Storage && layout.kind != NativeBufferKind::Uniform) || binding < minimum || binding >= maxBindings)
		throw std::out_of_range("Native storage/uniform binding index is outside its supported range");
	const size_t offset = CheckedNativeElementOffset(layout, first, count);
	if (offsetAlignment == 0 || offset % offsetAlignment != 0)
		throw std::invalid_argument("Native buffer binding offset alignment is invalid");
	return {offset, count * layout.stride};
}

inline NativeBufferRange MakeNativeBufferRange(NativeBufferSnapshotRef snapshot, uint32_t binding,
	size_t first, size_t count, uint32_t maxBindings, size_t offsetAlignment = 1)
{
	if (!snapshot || snapshot->object == 0 || snapshot->revision == 0)
		throw std::invalid_argument("Native buffer range has no captured generation");
	const auto [offset, size] = CheckedNativeBindingRange(snapshot->layout, binding, first, count, maxBindings, offsetAlignment);
	if (snapshot->bytes.size() != CheckedNativeBufferByteSize(snapshot->layout))
		throw std::invalid_argument("Native buffer snapshot capacity is invalid");
	return {std::move(snapshot), binding, offset, size};
}

} // namespace rendering
