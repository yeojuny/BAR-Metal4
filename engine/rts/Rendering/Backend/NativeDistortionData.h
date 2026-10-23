/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include <map>
#include <variant>
namespace rendering {
using NativeDistortionDefines=std::map<std::string,std::variant<float,std::string>,std::less<>>;
enum class NativeDistortionProgram:uint8_t { Volume, Combine };
struct NativeDistortionProgramSource {
 std::string name,vertex,fragment,assembledVertex,assembledFragment,engineDefinitions,quaternionDefinitions,definitions,vertexPath,fragmentPath;
 NativeDistortionDefines defines;
 std::map<std::string,std::vector<float>> initialFloat,initialInt;
 std::string vertexSHA512,fragmentSHA512;
};
struct NativeDistortionProgramConsumer {
 std::shared_ptr<const void> pipelineLifetime;
 NativeDistortionProgram kind=NativeDistortionProgram::Volume;
 bool uniformBufferCopy=false;
 std::string vertexSHA512,fragmentSHA512,log;
 std::vector<NativeCUSUniformDescription> uniforms;
 std::map<std::string,std::vector<float>> defaultFloat,defaultInt;
};
struct NativeDistortionDraw {
 uint64_t sequence=0;
 std::shared_ptr<const NativeDistortionProgramConsumer> program;
 NativeBufferSnapshotRef vertices,indices,instances;
 NativeBufferRange uniformBufferCopy;
 bool indexed=true;
 unsigned mode=0x0004,indexType=0;
 uint32_t count=0,firstIndex=0,firstVertex=0,instanceCount=0,firstInstance=0;
 int32_t baseVertex=0;
 std::map<std::string,std::vector<float>> floatUniforms,intUniforms;
 std::array<NativeCUSTextureBinding,3> textures;
 std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
 std::shared_ptr<const NativeCUSPublishedBuffer> transforms,modelUniforms;
 NativeWorldDrawState world;
 ScreenDrawState surface;
};
enum class NativeDistortionFormat:uint8_t { RGBA8,RGBA16Float };
struct NativeDistortionImage {
 uint64_t identity=0;uint32_t width=0,height=0;
 NativeDistortionFormat format=NativeDistortionFormat::RGBA8;
 NativeTextureSampling sampling;
};
struct NativeDistortionVersion {
 std::shared_ptr<const NativeDistortionImage> image;
 uint64_t generation=0,sourceDrawFrame=0,producerSequence=0;
};
enum class NativeDistortionOperation:uint8_t { Clear,Volume,CopyScreen,Combine };
// One typed source command in the existing common world recorder/ordinal stream.
// Local immutable versions describe actual private GL image writes, not fake GL names.
struct NativeDistortionCommand {
 uint64_t sequence=0,sourceDrawFrame=0;
 NativeDistortionOperation kind=NativeDistortionOperation::Clear;
 std::shared_ptr<const NativeDistortionVersion> output,previous;
 std::array<std::shared_ptr<const NativeDistortionVersion>,2> inputs;
 std::shared_ptr<const NativeDistortionDraw> draw;
 std::shared_ptr<const NativeDistortionProgramConsumer> transferProgram;
 std::array<float,4> clearColor{};
 std::array<int32_t,4> copySource{},copyDestination{};
 ScreenDrawState surface;
 NativeWorldDrawState world;
};
void ValidateNativeDistortionProgramSource(const NativeDistortionProgramSource&);
void ValidateNativeDistortionDraw(const NativeDistortionDraw&);
void ValidateNativeDistortionCommand(const NativeDistortionCommand&);
} // namespace rendering
