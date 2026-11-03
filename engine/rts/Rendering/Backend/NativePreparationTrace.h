/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering::metal4 {
// Diagnostic only: one in sixteen positive-gameplay draws, default off.
bool BeginPreparationTrace(std::int64_t frame) noexcept;
void EndPreparationTrace() noexcept;
void MarkPreparationTrace(unsigned category) noexcept;
std::uint64_t StartPreparationScope() noexcept;
void EndPreparationScope(unsigned category, std::uint64_t start) noexcept;
void CountPreparationTrace(unsigned counter, std::uint64_t amount) noexcept;
struct PreparationTraceFrame {
 explicit PreparationTraceFrame(std::int64_t frame) noexcept: active(BeginPreparationTrace(frame)) {}
 ~PreparationTraceFrame() { if (active) EndPreparationTrace(); }
 void Mark(unsigned category) noexcept { if (active) MarkPreparationTrace(category); }
 bool active;
};
struct PreparationTraceScope {
 explicit PreparationTraceScope(unsigned category) noexcept: category(category), start(StartPreparationScope()) {}
 ~PreparationTraceScope() { EndPreparationScope(category, start); }
 unsigned category;
 std::uint64_t start;
};
}
