/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
namespace rendering {
// Monotonic process-exit marker, not game reload or ordinary GPU drain.
// Main marks it before destroying Lua and renderer owners. Never reset.
inline std::atomic<bool> nativeRenderProcessShutdown{false};
inline void BeginNativeRenderProcessShutdown() noexcept {
 nativeRenderProcessShutdown.store(true,std::memory_order_release);
}
inline bool NativeRenderProcessShutdownRequested() noexcept {
 return nativeRenderProcessShutdown.load(std::memory_order_acquire);
}
}
