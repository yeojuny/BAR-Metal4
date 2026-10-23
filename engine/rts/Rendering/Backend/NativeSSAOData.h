/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeCUSUniformData.h"
#include "Rendering/Backend/NativeCUSTextureHistory.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include <atomic>
#include <map>
#include <string>
#include <variant>
namespace rendering {
enum class NativeSSAOProgram : uint8_t { Fuse, Sample, Blur, TexRect, Composite, BlurComposite };
enum class NativeSSAOFormat : uint8_t { RGBA8, RGB16Float };
using NativeSSAODefines = std::map<std::string,float,std::less<>>;
struct NativeSSAOImage {
 uint64_t identity=0;uint32_t width=0,height=0;
 NativeSSAOFormat format=NativeSSAOFormat::RGBA8;
 NativeTextureSampling sampling;
};
struct NativeSSAOProducerState {uint64_t submissionIdentity=0;bool encoded=false,completed=false;};
struct NativeSSAOImageResource {virtual ~NativeSSAOImageResource()=default;};
struct NativeSSAOImageVersion {
 std::shared_ptr<const NativeSSAOImage> image;
 uint64_t generation=0,sourceDrawFrame=0,producerSequence=0;
 std::shared_ptr<NativeSSAOProducerState>producer=std::make_shared<NativeSSAOProducerState>();
 const std::shared_ptr<const NativeSSAOImageResource>&Resource()const{return resource;}
 void Fulfill(std::shared_ptr<const NativeSSAOImageResource>)const;
private:
 mutable std::shared_ptr<const NativeSSAOImageResource>resource;
};
struct NativeSSAOImageCell {
 std::shared_ptr<const NativeSSAOImage> image;
 std::shared_ptr<const NativeSSAOImageVersion> current;
 uint64_t nextGeneration=1;
 bool deleted=false;
};
// Actual shared-name contents for the private FBO family. GL object identity
// stays in the common registry; this token never masquerades as a GL name.
struct NativeSSAOImageBacking final:NativeTextureBacking {
 std::shared_ptr<const NativeSSAOImageVersion> version;
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
struct NativeSSAOInput {
 NativeCUSTextureBinding actual;
 std::shared_ptr<const NativeSSAOImageVersion> local;
};
struct NativeSSAOUniforms {
 std::array<std::array<float,3>,64> samplingKernel{};
 std::array<float,12> weights{},offsets{};
 std::array<float,16> inverseProjection{};
 std::array<float,2> direction{0,1};
 float strength=1,testUniform=.5f;
};
// The original31657 VAO's immutable uploads, retained by recorded draws after
// its Lua buffers are deleted. Vertex coverage is evaluated from the actual
// stencil texture on the GPU; no CPU substitute or extra image cache.
struct NativeSSAOTileGeometry {
 std::array<std::array<float,2>,4> corners{};
 std::array<uint32_t,6> indices{};
 std::vector<std::array<float,2>> instances;
};
struct NativeSSAODraw {
 uint64_t sequence=0;
 NativeSSAOProgram program=NativeSSAOProgram::Sample;
 NativeSSAODefines defines;
 NativeSSAOUniforms uniforms;
 std::array<NativeSSAOInput,8> inputs;
 std::shared_ptr<const NativeSSAOImageVersion> output,previous;
 std::array<std::array<float,4>,6> vertices{}; // actual MakeTexRectVAO order
 std::shared_ptr<const NativeSSAOTileGeometry> tiles;
 ScreenDrawState surface;
 NativeWorldDrawState world;
 bool clear=false;
 std::array<float,4> clearColor{};
};
struct NativeSSAOFrame {
 uint64_t sourceDrawFrame=0;
 mutable bool completed=false;
 uint8_t exitClipDisableMask=0;
 std::optional<NativeWorldDrawState> exitWorldState; // Actual state after the complete original callback.
 std::vector<std::function<void()>> rollback;
 ~NativeSSAOFrame();
 std::shared_ptr<const NativeCUSUniformPublication> publication;
 std::vector<NativeSSAODraw> draws;
};
void ValidateNativeSSAODefines(const NativeSSAODefines&);
void ValidateNativeSSAODraw(const NativeSSAODraw&);
std::array<std::array<float,4>,6> MakeNativeSSAOTexRect(float,float,float,float,float,float,float,float);
class NativeSSAORecorder {
public:
 void Begin(uint64_t,std::shared_ptr<const NativeCUSUniformPublication>);
 void Record(NativeSSAODraw);
 void RecordTransactional(NativeSSAODraw,std::function<void()> publish,std::function<void()> rollback);
 std::shared_ptr<const NativeSSAOFrame> End(std::optional<NativeWorldDrawState> exit = {});
 void Abort() noexcept;
 void DisableClipDistance(unsigned);
 bool Active()const noexcept{return bool(frame);}
private:std::shared_ptr<NativeSSAOFrame>frame;
};
inline std::atomic<bool> nativeSSAOConsumerAvailable{false};
inline bool IsNativeSSAOConsumerAvailable()noexcept{return nativeSSAOConsumerAvailable.load(std::memory_order_acquire);}
// Only the completed paired map/model normal+depth producer, exact stage
// consumer and shader initialization may publish this family-specific gate.
inline void SetNativeSSAOConsumerAvailable(bool x)noexcept{nativeSSAOConsumerAvailable.store(x,std::memory_order_release);}
} // namespace rendering
