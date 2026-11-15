/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSTextureHistory.h"
#include <atomic>
#include <memory>
#include <limits>
#include <string>
#include <stdexcept>
#include <vector>

// Private two-consumer experiment. This is an admitted interface, not a GL driver.
namespace rendering::sharedpilot {
enum class Scalar : uint8_t { Float, Int, UInt };
enum Stage : uint8_t { Vertex=1, Fragment=2 };
enum class TextureKind : uint8_t { Image2D, Image3D, Cube };
struct UniformField {
 std::string name; Scalar scalar=Scalar::Float;
 uint32_t offset=0,components=1,count=1,stride=4;
 uint8_t stages=Fragment;
 uint32_t blockSlot=0;
 bool operator==(const UniformField&) const = default;
};
struct UniformBlock {uint32_t slot=0,bytes=0;uint8_t stages=Fragment;bool operator==(const UniformBlock&) const = default;};
struct ResourceSlot {
 std::string name; uint32_t logicalSlot=0,textureSlot=0,samplerSlot=0;
 TextureKind kind=TextureKind::Image2D; uint8_t stages=Fragment;
 int32_t containerBufferSlot=-1; // -1: direct stage binding; otherwise argument-buffer member IDs.
 bool operator==(const ResourceSlot&) const = default;
};
struct ProgramInterface {
 uint64_t generation=0;
 std::string sourceIdentity; // Exact stages/defines/adaptation/options, created at link.
 uint32_t uniformBytes=0;
 std::vector<UniformField> fields;
 std::vector<UniformBlock> blocks;
 std::vector<ResourceSlot> resources;
 std::shared_ptr<const void> nativeOwner;
};
namespace detail {
inline uint64_t ReserveProgramGeneration(std::atomic<uint64_t>& next) {
 auto value=next.load(std::memory_order_relaxed);
 for(;;){
  if(!value||value==std::numeric_limits<uint64_t>::max())
   throw std::overflow_error("Shared pilot program generation exhausted");
  if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
 }
}
}
inline uint64_t NextProgramGeneration() {
 static std::atomic<uint64_t> next{1};
 return detail::ReserveProgramGeneration(next);
}

struct LogicalBinding {
 uint32_t logicalSlot=0;
 NativeCUSTextureBinding captured;
 // Sepia may retain an exact postprocess version; CUS retains captured.object.
 std::shared_ptr<const void> versionOwner;
};
struct MaterialContent {
 std::shared_ptr<const ProgramInterface> program;
 std::shared_ptr<const std::vector<uint8_t>> uniformBytes;
 // Existing CUS immutable packing owner remains authoritative; do not repack it here.
 std::shared_ptr<const void> originalUniformOwner;
 std::vector<LogicalBinding> bindings;
};
struct ExecutionIdentity {
 const void* context=nullptr;
 const void* device=nullptr;
 const void* lease=nullptr;
 const void* journal=nullptr;
 uint64_t submission=0;
 std::vector<uint64_t> target; // Exact values, never a collision-prone digest alone.
 bool operator==(const ExecutionIdentity&) const = default;
};
struct PreparedSlice {
 ExecutionIdentity scope;
 std::shared_ptr<const MaterialContent> material;
 // Prepared native adapters own actual resources and callbacks; this retains source versions.
 std::vector<std::shared_ptr<const void>> owners;
};
}
