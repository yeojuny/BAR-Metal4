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
enum class NativeBloomProgram : uint8_t { Bright, Downsample, Upsample, FinalUpsample, Combine };
enum class NativeBloomFormat : uint8_t { RG11B10Float, RGBA16Float };
using NativeBloomDefines = std::map<std::string,float,std::less<>>;
struct NativeBloomImage {
 uint64_t identity=0;uint32_t width=0,height=0;
 NativeBloomFormat format=NativeBloomFormat::RG11B10Float;
 NativeTextureSampling sampling;
};
struct NativeBloomProducerState {uint64_t submissionIdentity=0;bool encoded=false,completed=false;};
struct NativeBloomImageResource {virtual ~NativeBloomImageResource()=default;};
struct NativeBloomImageVersion {
 std::shared_ptr<const NativeBloomImage> image;
 uint64_t generation=0,sourceDrawFrame=0,producerSequence=0;
 std::shared_ptr<NativeBloomProducerState>producer=std::make_shared<NativeBloomProducerState>();
 const std::shared_ptr<const NativeBloomImageResource>&Resource()const{return resource;}
 void Fulfill(std::shared_ptr<const NativeBloomImageResource>)const;
private:
 mutable std::shared_ptr<const NativeBloomImageResource>resource;
};
struct NativeBloomImageCell {
 std::shared_ptr<const NativeBloomImage> image;
 std::shared_ptr<const NativeBloomImageVersion> current;
 uint64_t nextGeneration=1;
 bool deleted=false;
};
// Actual shared-name contents for the private FBO family. GL object identity
// stays in the common registry; this token never masquerades as a GL name.
struct NativeBloomImageBacking final:NativeTextureBacking {
 std::shared_ptr<const NativeBloomImageVersion> version;
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
struct NativeBloomInput {
 NativeCUSTextureBinding actual;
 std::shared_ptr<const NativeBloomImageVersion> local;
};
struct NativeBloomUniforms {
 std::array<float,2> sourceTexelSize{1,1};
 float illuminationThreshold=0,kneeWidth=.5f,glowAmplifier=1,maxBrightContribution=1.5f;
 float filterRadius=1,historyMix=0,bloomNorm=1;
 int debugDraw=0;
};
struct NativeBloomDraw {
 uint64_t sequence=0;
 NativeBloomProgram program=NativeBloomProgram::Bright;
 NativeBloomDefines defines;
 NativeBloomUniforms uniforms;
 std::array<NativeBloomInput,8> inputs;
 std::shared_ptr<const NativeBloomImageVersion> output,previous;
 std::array<std::array<float,4>,6> vertices{}; // actual MakeTexRectVAO order
 ScreenDrawState surface;
 NativeWorldDrawState world;
 bool clear=false;
 std::array<float,4> clearColor{};
};
struct NativeBloomFrame {
 uint64_t sourceDrawFrame=0;
 mutable bool completed=false;
 std::vector<std::function<void()>> rollback;
 ~NativeBloomFrame();
 std::shared_ptr<const NativeCUSUniformPublication> publication;
 std::vector<NativeBloomDraw> draws;
};
void ValidateNativeBloomDefines(const NativeBloomDefines&);
void ValidateNativeBloomDraw(const NativeBloomDraw&);
std::array<std::array<float,4>,6> MakeNativeBloomTexRect(float,float,float,float,float,float,float,float);
class NativeBloomRecorder {
public:
 void Begin(uint64_t,std::shared_ptr<const NativeCUSUniformPublication>);
 void Record(NativeBloomDraw);
 void RecordTransactional(NativeBloomDraw,std::function<void()> publish,std::function<void()> rollback);
 std::shared_ptr<const NativeBloomFrame> End();
 void Abort() noexcept;
 bool Active()const noexcept{return bool(frame);}
private:std::shared_ptr<NativeBloomFrame>frame;
};
inline std::atomic<bool> nativeBloomConsumerAvailable{false};
inline bool IsNativeBloomConsumerAvailable()noexcept{return nativeBloomConsumerAvailable.load(std::memory_order_acquire);}
// Only the completed paired map/model normal+depth producer, exact stage
// consumer and shader initialization may publish this family-specific gate.
inline void SetNativeBloomConsumerAvailable(bool x)noexcept{nativeBloomConsumerAvailable.store(x,std::memory_order_release);}
} // namespace rendering
