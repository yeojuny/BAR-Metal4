#pragma once
#include <cstdint>
#include <memory>
namespace rendering {
enum class NativeCUSBindingKnowledge { Unobserved, BoundObject };
struct NativeCUSTextureObject {uint64_t contextGeneration=1;};
struct NativeCUSTextureBinding {
 NativeCUSBindingKnowledge knowledge=NativeCUSBindingKnowledge::BoundObject;
 std::shared_ptr<const NativeCUSTextureObject> object;
};
}
