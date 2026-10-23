/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSPublishedData.h"
#include <functional>
#include <map>
#include <stdexcept>
#include <string_view>

namespace rendering {
// O01 original inline GLSL compute source, pinned independently of its widget.
inline constexpr uint64_t NativeUnitCopyShaderHash = 7541281065763036796ull; // Lua long-bracket initial newline is discarded.
inline uint64_t HashNativeUnitCopyShader(std::string_view source) {
 uint64_t h=14695981039346656037ull;for(unsigned char c:source){h^=c;h*=1099511628211ull;}return h;
}
class NativeUnitUniformCopyProgram {
public:
 virtual ~NativeUnitUniformCopyProgram()=default;
 // Returns only after actual Metal compute completion and feedback. No CPU
 // kernel substitutes for the original dispatch; padding/unwritten bytes come
 // from the real previous destination generation, including its initial Clear.
 virtual std::vector<uint8_t> Dispatch(const NativeCUSPublishedBuffer& source,
  const NativeBufferSnapshot& previous,uint32_t entries,uint64_t frame) const=0;
};
std::shared_ptr<const NativeUnitUniformCopyProgram> CompileNativeUnitUniformCopy();
// Original SSBO binding4 is shared between the API widget and its consumers in
// the same Lua handle. Resolve current bytes at draw, not the older bind time.
struct NativeUnitCopyBinding {
 const void* owner=nullptr;
 std::function<NativeBufferRange()> capture;
 std::function<void(const NativeBufferSnapshotRef&,std::span<const uint8_t>)> publish;
};
inline auto& NativeUnitCopyBindings(){static std::map<const void*,NativeUnitCopyBinding> values;return values;}
inline NativeBufferRange CaptureNativeUnitCopyBinding(const void* handle) {
 const auto it=NativeUnitCopyBindings().find(handle);
 if(it==NativeUnitCopyBindings().end()||!it->second.capture)throw std::runtime_error("Unit uniform copy binding4 is missing");
 return it->second.capture();
}
} // namespace rendering
