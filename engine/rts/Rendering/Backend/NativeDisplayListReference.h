/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace rendering::ui {
struct NativeUICommandList;

// Source resource names are independent of recyclable Lua table slots.
// A name is never reused. Delete/Clear invalidate future resolutions, while
// a command list already resolved at an actual source call remains immutable.
class NativeDisplayListOwner;
class NativeDisplayListReference {
public:
	std::uint64_t Identity() const noexcept { return identity; }
	std::shared_ptr<const NativeUICommandList> Resolve() const noexcept
	{
		const auto live = owner.lock();
		return live ? live->commands : nullptr;
	}
private:
	struct State {
		std::uint64_t identity;
		std::shared_ptr<const NativeUICommandList> commands;
	};
	std::uint64_t identity = 0;
	std::weak_ptr<const State> owner;
	friend class NativeDisplayListOwner;
};

// The original unsynced source thread owns mutation, just like its list table.
// References carry no permanent strong command-list or Lua-owner lifetime.
class NativeDisplayListOwner {
public:
	NativeDisplayListOwner() = default;
	explicit NativeDisplayListOwner(std::shared_ptr<const NativeUICommandList> commands)
	{
		if (!commands) throw std::invalid_argument("Native display-list source has no compiled commands");
		state = std::make_shared<NativeDisplayListReference::State>();
		state->identity = AllocateName();
		state->commands = std::move(commands);
	}
	NativeDisplayListReference Reference() const noexcept
	{
		NativeDisplayListReference result;
		if (state) { result.identity = state->identity; result.owner = state; }
		return result;
	}
	void Invalidate() noexcept { if (state) state->commands.reset(); }
private:
	static std::uint64_t AllocateName()
	{
		// Functional logical-name allocation, never a diagnostic event counter.
		static std::atomic<std::uint64_t> next{1};
		auto value = next.load(std::memory_order_relaxed);
		for (;;) {
			if (value == std::numeric_limits<std::uint64_t>::max())
				throw std::overflow_error("Native display-list source names exhausted");
			if (next.compare_exchange_weak(value,value+1,std::memory_order_relaxed)) return value;
		}
	}
	std::shared_ptr<NativeDisplayListReference::State> state;
};
} // namespace rendering::ui
