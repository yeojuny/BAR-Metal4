/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include <span>
#include <string_view>

namespace rendering {
enum class NativeCUSUniformScalar {Float,Int,UInt};
struct NativeCUSGeneratedField {
 const char* name;NativeCUSUniformScalar scalar;uint32_t offset,components,count,stride;bool active;
};
struct NativeCUSGeneratedDefault {
 const char* name;unsigned glType,components;bool integer;
 std::array<float,4> values;
};
struct NativeCUSGeneratedStage {
 const char* metalSource;
 std::span<const NativeCUSGeneratedField> fields;
 uint32_t uniformBytes,textureMask;
};
struct NativeCUSGeneratedProgram {
 const char* family;unsigned originalPass;
 const char* vertexSHA512;const char* fragmentSHA512;
 NativeCUSGeneratedStage vertex,fragment;
 std::span<const NativeCUSGeneratedDefault> defaults;
};
// Transport state is captured from actual source images/target; it never
// changes the retained original engine matrices or material uniform values.
struct NativeCUSSampleTransport {
 uint32_t emptyMask=0,flipMask=0,depthMask=0,clipFlags=0;
 std::array<uint32_t,12> sampleCodes{};
 uint32_t clampDepthCompareMask=0; // Existing NativeCUSImageMasks.w transport.
};
// Match the original 16-byte block padding without uint32 addition overflow.
inline std::size_t NativeCUSProgramUniformBytes(const NativeCUSGeneratedStage& stage)
{return (std::size_t(stage.uniformBytes)+15u)&~std::size_t(15u);}
void PackNativeCUSProgramUniformsInto(const NativeCUSGeneratedStage&,
 const NativeCUSSourceDraw&,const NativeCUSProgramSource&,const NativeCUSSampleTransport&,std::span<uint8_t>);
uint32_t NativeCUSSampleCode(const NativeTextureSampling&);
std::vector<uint8_t> PackNativeCUSProgramUniforms(const NativeCUSGeneratedStage&,
 const NativeCUSSourceDraw&,const NativeCUSProgramSource&,const NativeCUSSampleTransport&);
// One renderer-owned plan per linked stage. Only immutable scalar content is
// cached; image transport and map values are patched for every draw. Weak owner
// identity does not keep excluded frame/phase uniform content alive. Calls are
// serialized by the existing renderer gate (this is not a concurrent cache).
class NativeCUSProgramUniformPlan {
 std::vector<std::string> names;
 std::vector<NativeCUSGeneratedField> allFields,staticFields,dynamicFields;
 std::size_t byteSize=0;
 bool split=true;
 std::weak_ptr<const NativeCUSProgramUniforms> owner;
 const NativeCUSProgramUniforms* ownerAddress=nullptr;
 std::vector<uint8_t> packed;
public:
 explicit NativeCUSProgramUniformPlan(const NativeCUSGeneratedStage&);
 NativeCUSProgramUniformPlan(const NativeCUSProgramUniformPlan&)=delete;
 NativeCUSProgramUniformPlan& operator=(const NativeCUSProgramUniformPlan&)=delete;
 // Returns true when static packing ran; false means exact-owner reuse.
 bool Pack(const NativeCUSSourceDraw&,const NativeCUSProgramSource&,
  const NativeCUSSampleTransport&,std::span<uint8_t>);
};
void PopulateNativeCUSProgramUniformMetadata(const NativeCUSGeneratedProgram&,NativeCUSProgramConsumer&);
} // namespace rendering
