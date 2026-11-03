/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rendering {

// Local to one immutable PrepareList upload. Equality is byte-for-byte, so
// signed zero and distinct floating-point representations remain distinct.
// A full lookup table falls back to appending; it never drops a command.
template<std::size_t Size, std::size_t Capacity = 256>
class NativeScreenUniforms151 {
public:
	static_assert(Size > 0 && Size <= 256);
	using Key = std::array<std::uint8_t, Size>;

	std::uint32_t Append(const Key& key, std::vector<std::uint8_t>& bytes) {
		if (recent && recent->first == key) return recent->second;
		if (const auto found = offsets.find(key); found != offsets.end()) {
			recent = std::pair{key, found->second};
			return found->second;
		}
		if (bytes.size() % 256 || bytes.size() > std::numeric_limits<std::uint32_t>::max() - 256)
			throw std::overflow_error("Screen uniform upload requires bounded aligned offsets");
		const auto offset = static_cast<std::uint32_t>(bytes.size());
		bytes.resize(bytes.size() + 256, 0);
		std::memcpy(bytes.data() + offset, key.data(), key.size());
		if (offsets.size() < Capacity) offsets.emplace(key, offset);
		recent = std::pair{key, offset};
		return offset;
	}

private:
	std::map<Key, std::uint32_t> offsets;
	std::optional<std::pair<Key, std::uint32_t>> recent;
};

// All bindings used by the generic screen shader: vertex/UV/uniform/optional
// transform addresses, followed by both texture IDs and both sampler IDs.
// A zero transform selects the three-buffer layout; otherwise the layout has
// five slots with unused slot3 still zero. This index owns no resources: its
// caller must retain immutable tables AND their retired-pool ownership tokens.
using NativeScreenBindingKey151 = std::array<std::uint64_t, 8>;
template<std::size_t Capacity = 1024>
class NativeScreenBindings151 {
public:
	std::optional<std::size_t> Find(const NativeScreenBindingKey151& key) const {
		const auto found = entries.find(key);
		if (found == entries.end()) return std::nullopt;
		return found->second;
	}
	bool Remember(const NativeScreenBindingKey151& key, std::size_t index) {
		if (entries.size() >= Capacity) return false;
		return entries.emplace(key, index).second;
	}

private:
	std::map<NativeScreenBindingKey151, std::size_t> entries;
};

} // namespace rendering
