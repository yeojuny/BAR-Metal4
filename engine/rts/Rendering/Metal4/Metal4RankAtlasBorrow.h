/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace rendering { struct NativeCUSTextureObject; }

namespace rendering::metal4::rankAtlasBorrow {

inline bool Enabled()
{
 static const bool enabled = [] {
  const auto* value = std::getenv("BAR_NATIVE_RANK_ATLAS_GPU_BORROW");
  return value && std::string_view(value) == "1";
 }();
 return enabled;
}

inline thread_local bool screenPresentation = false;
inline thread_local const NativeCUSTextureObject* typedScreenReader = nullptr;

// The scope expresses the ordinary presentation route. A texture name or
// external image descriptor alone cannot authorize this use.
class ScreenPresentation {
 const bool previous = screenPresentation;
public:
 explicit ScreenPresentation(bool eligible) noexcept { screenPresentation = eligible && Enabled(); }
 ~ScreenPresentation() { screenPresentation = previous; }
 ScreenPresentation(const ScreenPresentation&) = delete;
};

class ScreenReader {
 const NativeCUSTextureObject* const previous = typedScreenReader;
public:
 explicit ScreenReader(const NativeCUSTextureObject* object) noexcept
 { typedScreenReader = screenPresentation ? object : nullptr; }
 ~ScreenReader() { typedScreenReader = previous; }
 ScreenReader(const ScreenReader&) = delete;
};

inline bool IsScreenReader(const NativeCUSTextureObject& object) noexcept
{
 return screenPresentation && typedScreenReader == &object;
}

inline bool CanRegister(std::uint64_t producer, std::uint64_t consumer,
                        std::uint64_t uniqueEvent, std::uint64_t predecessorEvent) noexcept
{
 return producer && consumer && producer != consumer && uniqueEvent && uniqueEvent == predecessorEvent;
}

inline bool ProducerCompleted(std::uint64_t expected, bool initialized,
                              std::uint64_t completed) noexcept
{
 return expected && initialized && completed == expected;
}

inline bool CanEncodeRead(std::uint64_t expected, bool initialized, std::uint64_t completed,
                          std::uint64_t encoded, bool dependencyRegistered) noexcept
{
 return ProducerCompleted(expected, initialized, completed) ||
        (expected && !initialized && dependencyRegistered && encoded == expected);
}

} // namespace rendering::metal4::rankAtlasBorrow
