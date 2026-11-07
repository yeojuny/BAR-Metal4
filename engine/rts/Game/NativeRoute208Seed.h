/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
// Diagnostic allowlist: values decoded from the preserved original GameData
// packets. Never alter feature IDs, material routing, or the later network RNG.
inline std::optional<uint32_t> NativeRoute208InitialSeed(const char* selector) {
 if (!selector || !*selector || std::strcmp(selector,"off")==0) return std::nullopt;
 if (std::strcmp(selector,"slow206")==0) return uint32_t(1863150106u);
 if (std::strcmp(selector,"slow207")==0) return uint32_t(1246140902u);
 if (std::strcmp(selector,"fast207")==0) return uint32_t(154658555u);
 if (std::strcmp(selector,"outside208")==0) return uint32_t(1155403446u);
 throw std::invalid_argument("Unknown Route208 recorded initial-seed selector");
}
