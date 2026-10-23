/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSSourceData.h"

#include <map>
#include <optional>
#include <variant>

namespace rendering {
using NativeDeferredLightDefines = std::map<std::string, std::variant<float, std::string>, std::less<>>;
struct NativeDeferredLightProgramSource {
	std::string name, vertex, fragment;
	std::string assembledVertex, assembledFragment, engineDefinitions, quaternionDefinitions, definitions;
	std::string vertexPath, fragmentPath;
	NativeDeferredLightDefines defines;
	std::map<std::string, std::vector<float>> initialFloat, initialInt;
	std::string vertexSHA512, fragmentSHA512;
};
// Only the source-paired compiler returns this owner after successful linking.
struct NativeDeferredLightProgramConsumer {
	std::shared_ptr<const void> pipelineLifetime;
	std::string vertexSHA512, fragmentSHA512, log;
	std::vector<NativeCUSUniformDescription> uniforms;
	std::map<std::string, std::vector<float>> defaultFloat, defaultInt;
};
struct NativeDeferredLightDraw {
	uint64_t sequence = 0;
	std::shared_ptr<const NativeDeferredLightProgramConsumer> program;
	NativeBufferSnapshotRef vertices, indices, instances;
	bool indexed = true;
	unsigned mode = 0x0004, indexType = 0;
	uint32_t count = 0, firstIndex = 0, firstVertex = 0, instanceCount = 0, firstInstance = 0;
	int32_t baseVertex = 0;
	std::map<std::string, std::vector<float>> floatUniforms, intUniforms;
	// Target-aware original bindings: slot8 is sampler3D, the other slots are
	// sampler2D. Disabled 2D enables never erase an existing sampler3D binding.
	std::array<NativeCUSTextureBinding, 12> textures;
	std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms, modelUniforms;
	NativeWorldDrawState world;
	ScreenDrawState surface;
};
struct NativeDeferredLightFrame {
	uint64_t sourceDrawFrame = 0;
	std::vector<NativeDeferredLightDraw> draws;
	std::optional<NativeWorldDrawState> exitWorldState;
};
void ValidateNativeDeferredLightProgramSource(const NativeDeferredLightProgramSource&);
void ValidateNativeDeferredLightDraw(const NativeDeferredLightDraw&);
} // namespace rendering
