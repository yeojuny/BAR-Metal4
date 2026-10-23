/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"
#include <optional>
#include <string_view>
namespace rendering {
struct NativeSensorLOSDefines {
 std::string originalText,originalEngineDefinitions;
 float viewWidth=0,viewHeight=0;
 bool stencilPass=false;
};
struct NativeSensorLOSProgramSnapshot {
 std::shared_ptr<const NativeSensorLOSDefines> defines;
 std::array<float,4> rangeColor{};
 float teamColorMix=1,lineWidthPx=1;
 int32_t heightUnit=0,stencilUnit=0;
};
class NativeSensorLOSProgramState {
public:
 NativeSensorLOSProgramState(std::string_view vertex,std::string_view fragment);
 int Location(std::string_view)const;
 bool Floats(int,std::span<const float>);
 bool Integers(int,std::span<const int32_t>);
 NativeSensorLOSProgramSnapshot Capture()const{return state;}
 std::vector<std::string_view> ActiveUniforms()const;
private:NativeSensorLOSProgramSnapshot state;
};
struct NativeSensorLOSDraw {
 uint64_t sequence=0,sourceDrawFrame=0;
 NativeSensorLOSProgramSnapshot program;
 std::shared_ptr<const void> consumerLifetime;
 NativeBufferSnapshotRef vertices,instances;
 uint32_t first=0,count=0,firstInstance=0,instanceCount=1;
 std::shared_ptr<const NativeCUSUniformPublication> original;
 std::shared_ptr<const NativeCUSPublishedBuffer> models;
 NativeCUSTextureBinding heightImage,stencilImage;
 std::shared_ptr<const ScreenTexture> heightDependency,stencilDependency;
 ScreenDrawState surface;
 NativeWorldDrawState raster;
};
struct NativeSensorLOSInstanceValues {std::vector<std::array<float,4>> positions,colors;};
struct NativeSensorLOSOriginalUniforms {
 std::array<float,16> cameraViewProjection{},cameraViewInverse{},cameraViewProjectionInverse{};
 std::array<float,4> timeInfo{},viewGeometry{},mapSize{};
 uint32_t renderCaps=0;
};
void ValidateNativeSensorLOSDraw(const NativeSensorLOSDraw&);
NativeSensorLOSOriginalUniforms ReadNativeSensorLOSUniforms(const NativeSensorLOSDraw&);
NativeSensorLOSInstanceValues ReadNativeSensorLOSInstances(const NativeSensorLOSDraw&);
void SetNativeSensorLOSConsumerLifetime(std::shared_ptr<const void>);
std::shared_ptr<const void> NativeSensorLOSConsumerLifetime();
bool IsNativeSensorLOSConsumerAvailable();
}
