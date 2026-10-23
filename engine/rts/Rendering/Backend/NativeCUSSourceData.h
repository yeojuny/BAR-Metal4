/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeCUSModelInputs.h"
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rendering {
enum class NativeCUSSourceLocation { Unobserved, Raw, WorkingDirectory, Archive };
struct NativeCUSLoadedSource {
	std::string path, modes, archive, sha512;
	NativeCUSSourceLocation location=NativeCUSSourceLocation::Unobserved;
	int archiveSection=-1;
	std::string resolvedPath;
};
struct NativeCUSProgramSource {
	std::string definitions, vertex, fragment;
	std::map<std::string,std::vector<float>> initialFloat,initialInt;
	// Exact map values inserted by original LuaShader, retained as compilation inputs.
	std::array<std::array<float,3>,3> waterColors{}; // absorb, min, base
	std::string vertexSHA512,fragmentSHA512;
};
struct NativeCUSUniformDescription {
	std::string name;
	unsigned glType=0,size=1;
	bool integer=false;
};
// A real pipeline owner supplies this only after validating/compiling the
// exact program. The Lua adapter never declares a program successful itself.
struct NativeCUSProgramConsumer {
	std::shared_ptr<const void> pipelineLifetime;
	std::string family,vertexSHA512,fragmentSHA512;
	unsigned originalPass=0;
	std::vector<NativeCUSUniformDescription> uniforms;
	// Actual original GLSL link-time scalar defaults, before Lua's supplied
	// uniformFloat/uniformInt tables and subsequent gl.Uniform writes.
	std::map<std::string,std::vector<float>> defaultFloat,defaultInt;
};
struct NativeCUSSourceDraw {
	uint64_t sequence=0;
	std::shared_ptr<const NativeCUSProgramConsumer> program;
	std::map<std::string,std::vector<float>> floatUniforms,intUniforms;
	NativeBufferSnapshotRef instances;
	std::shared_ptr<const NativeCUSDrawTextureSnapshot> textures;
	std::vector<NativeCUSSubmittedObject> objects;
	std::vector<std::shared_ptr<const NativeCUSModelSubmission>> submissions;
	std::shared_ptr<const NativeCUSModelInputs> modelInputs;
	std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
	// Original warm-up draw masks are retained, including all-false color.
	std::array<bool,4> colorMask{true,true,true,true};
	bool depthTest=true,depthWrite=true,cullEnabled=false,clipDistance0=false;
	unsigned depthFunction=0x0203,cullFace=0x0405,sourceBlend=1,destinationBlend=0;
	// Complete inherited state is captured at the original callback. Legacy
	// fields above are the exact private CUS gl.* mutations applied over it.
	NativeWorldDrawState world;
	ScreenDrawState surface;
	bool rasterObserved=false;
};
struct NativeCUSSourceFrame {
	uint64_t sourceDrawFrame=0;
	std::vector<NativeCUSLoadedSource> sourceLoads;
	std::vector<NativeCUSSourceDraw> draws;
};
void ValidateNativeCUSSourceDraw(const NativeCUSSourceDraw&);
// The actual original warm-up keeps every source draw, program/pass, ordinal,
// pose and uniform owner. Only proven absence of attachment writes may omit
// raster execution or permit another compiled pass in an MRT source scope.
bool HasNoNativeCUSAttachmentEffects(const NativeCUSSourceDraw&) noexcept;
// Original gl.Blending(src,dst) enables blending and writes RGB+alpha factors.
void SetNativeCUSBlend(NativeCUSSourceDraw&,unsigned source,unsigned destination);
} // namespace rendering
