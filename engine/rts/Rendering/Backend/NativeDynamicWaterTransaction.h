/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <bit>

namespace rendering {
// The original dynamic-water queue is not a replayable presentation effect.
inline void ValidateDynamicWaterSequences(uint64_t committed, uint64_t through,
	std::span<const uint64_t> sequences)
{
	if (through < committed) throw std::invalid_argument("Dynamic water frame predates committed recurrence");
	uint64_t previous = 0, next = committed;
	for (const auto sequence: sequences) {
		if (sequence == 0 || sequence <= previous || sequence > through)
			throw std::invalid_argument("Dynamic water update sequence is invalid or reordered");
		previous = sequence;
		if (sequence <= committed) continue;
		if (next == UINT64_MAX || sequence != next + 1)
			throw std::invalid_argument("Dynamic water recurrence update gap");
		next = sequence;
	}
	if (next != through) throw std::invalid_argument("Dynamic water throughUpdate has missing operations");
}
class DynamicWaterTransaction {
public:
	enum class Phase { Prepared, Updating, UpdatesDone, Deriving, DerivedDone, Drawing, Drawn, Retired, Failed };
	Phase State() const { return phase; }
	void BeginUpdates() { Transition(Phase::Prepared, Phase::Updating); }
	void EndUpdates() { Transition(Phase::Updating, Phase::UpdatesDone); }
	void BeginDerived() { Transition(Phase::UpdatesDone, Phase::Deriving); }
	void EndDerived() { Transition(Phase::Deriving, Phase::DerivedDone); }
	void BeginSurface() { Transition(Phase::DerivedDone, Phase::Drawing); }
	void EndSurface() { Transition(Phase::Drawing, Phase::Drawn); }
	void Retire() { Transition(Phase::Drawn, Phase::Retired); }
	void Fail() noexcept { phase = Phase::Failed; }
private:
	Phase phase = Phase::Prepared;
	void Transition(Phase expected, Phase next) {
		if (phase != expected) throw std::logic_error("Dynamic water submission phase is repeated or out of order");
		phase = next;
	}
};
// Exact IEEE binary32 -> binary16 round-to-nearest, ties-to-even. Used only
// for original glTexImage FLOAT payloads; GPU writes still round at each pass.
inline uint16_t DynamicWaterHalf(float value)
{
	const uint32_t bits = std::bit_cast<uint32_t>(value);
	const uint32_t sign = (bits >> 16) & 0x8000;
	const uint32_t exponent = (bits >> 23) & 0xff, mantissa = bits & 0x7fffff;
	if (exponent == 0xff) return uint16_t(sign | 0x7c00 | (mantissa ? 0x200 : 0));
	const int e = int(exponent) - 127 + 15;
	if (e >= 31) return uint16_t(sign | 0x7c00);
	if (e < -10) return uint16_t(sign);
	if (e <= 0) {
		const uint32_t m = mantissa | 0x800000;
		const unsigned shift = unsigned(14 - e);
		const uint32_t base = m >> shift, rest = m & ((1u << shift) - 1), halfway = 1u << (shift - 1);
		return uint16_t(sign | (base + (rest > halfway || (rest == halfway && (base & 1)))));
	}
	uint32_t result = (uint32_t(e) << 10) | (mantissa >> 13);
	const uint32_t rest = mantissa & 0x1fff;
	result += rest > 0x1000 || (rest == 0x1000 && (result & 1));
	return uint16_t(sign | result);
}
}
