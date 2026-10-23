/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeSensorPreviewData.h"
struct lua_State;
namespace rendering::ui {class NativeUIRecorder;}
namespace LuaNativeSensorPreview {
enum class ImageFormat: std::uint8_t { R16Float, R32Float, R8Unorm };
// Exact typed hooks into existing shared owners. Until the real PBR bridge
// installs these, this source family remains unavailable; there is no fallback.
struct Services {
 rendering::NativeSensorPreviewCompileFunction compile;
 std::function<std::shared_ptr<rendering::NativePostprocessBinding>(std::uint32_t,std::uint32_t,ImageFormat,const rendering::NativePostprocessSampler&)> createImage;
 // Execute the original callback once, with original RTT viewport/scissor
 // save/restore semantics. This is not NativeUIRecorder::BeginTarget(world).
 using TargetCaptureFunction=std::function<void(const std::shared_ptr<rendering::NativePostprocessBinding>&,const std::function<void()>&)>;
 TargetCaptureFunction captureTarget;
 // Bridge calls real NativePostprocessRecorder::PublishExternalOutput with
 // explicit Sensor family, actual World stage/ordinal and this commit callback.
 using TargetPublishFunction=std::function<std::shared_ptr<const rendering::NativePostprocessVersion>(const std::shared_ptr<rendering::NativePostprocessBinding>&,std::uint64_t,std::span<const std::shared_ptr<const rendering::NativePostprocessVersion>>,std::function<void(std::shared_ptr<const rendering::NativePostprocessVersion>)>)>;
 TargetPublishFunction publishTarget;
};
using MinimapSink=std::function<void(rendering::ui::NativeUIRecorder&,std::shared_ptr<const rendering::NativeSensorPreviewDraw>)>;
// Called immediately in the original active cached minimap/PiP target ordinal.
void SetMinimapSink(MinimapSink actual);
void SetServices(Services actual);
bool Available();
void BeginCapture(rendering::NativeSensorPreviewCallin actualCallin);
std::shared_ptr<const rendering::NativeSensorPreviewFrame> EndCapture();
void AbortCapture() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
