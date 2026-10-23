/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSSourceData.h"
#include "NativeWorldDrawData.h"

#include <map>
#include <optional>
#include <variant>

namespace rendering {
using NativeOutlineDefines = std::map<std::string, std::variant<float, std::string>, std::less<>>;
struct NativeOutlineProgramSource {
	std::string name, vertex, fragment;
	std::string assembledVertex, assembledFragment, engineDefinitions, quaternionDefinitions, definitions;
	std::string vertexPath, fragmentPath;
	NativeOutlineDefines defines;
	std::map<std::string, std::vector<float>> initialFloat, initialInt;
	std::string vertexSHA512, fragmentSHA512;
};
struct NativeOutlineProgramConsumer {
	std::shared_ptr<const void> pipelineLifetime;
	std::string vertexSHA512, fragmentSHA512, log;
	bool perUnit = false, useQuaternions = false;
	std::vector<NativeCUSUniformDescription> uniforms;
	std::map<std::string, std::vector<float>> defaultFloat, defaultInt;
};
struct NativeOutlineDraw {
	uint64_t sequence = 0;
	std::shared_ptr<const NativeOutlineProgramConsumer> program;
	NativeBufferSnapshotRef vertices, instances;
	unsigned mode = 0x0005;
	uint32_t count = 4, firstVertex = 0, instanceCount = 0, firstInstance = 0;
	std::map<std::string, std::vector<float>> floatUniforms, intUniforms;
	std::array<NativeCUSTextureBinding, 3> textures;
	std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms, modelUniforms;
	NativeWorldDrawState world;
	ScreenDrawState surface;
};
// The source glClear is a distinct ordered event. It uses FRONT's write mask,
// the actual scissor and clear value, and ignores viewport and raster tests.
using NativeOutlineClear = NativeWorldAttachmentClear;
using NativeOutlineOperation = std::variant<NativeOutlineDraw, NativeOutlineClear>;
struct NativeOutlineFrame {
	uint64_t sourceDrawFrame = 0;
	std::vector<NativeOutlineOperation> operations;
	std::optional<NativeWorldDrawState> exitWorldState;
};
float NativeOutlineDefine(const NativeOutlineProgramSource&, const char*);
void ValidateNativeOutlineProgramSource(const NativeOutlineProgramSource&);
void ValidateNativeOutlineDraw(const NativeOutlineDraw&);
void ValidateNativeOutlineClear(const NativeOutlineClear&);
} // namespace rendering
