/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include <atomic>
#include <map>

namespace rendering {
enum class NativePBRKind { BRDF, Environment };
struct NativePBRProgramSource {
	std::string vertex,fragment;
	std::map<std::string,std::vector<float>> initialInt;
};
struct NativePBRLoadedSource {
	std::string path,modes,sha512,owner,archive,resolvedPath;
	int archiveSection=-1;
};
struct NativePBRProgram {
	NativePBRKind kind=NativePBRKind::BRDF;
	int32_t samples=1024;
	std::array<float,2> texSize{512,512};
	std::shared_ptr<const void> pipelineOwner;
};
struct NativePBRAllocation {
	uint32_t width=0,height=0,originalFormat=0;
	NativeTextureSampling sampling;
};

// An allocated resource is not executed contents. The actual GPU subclass
// retains physical storage; command ownership is separate from logical names.
struct NativePBRImage : NativeTextureBacking {
	NativePBRAllocation allocation;
	std::shared_ptr<const NativePBRProgram> program;
	NativeCUSTextureBinding reflection;
	uint64_t sourceSequence=0;
	mutable std::atomic<uint64_t> encodedSubmission{0};
	mutable std::atomic<bool> completed{false};
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
// Validates the complete original shader text and original Initialize uniforms.
NativePBRProgram ValidateNativePBRProgram(const NativePBRProgramSource&);
void ValidateNativePBRAllocation(const NativePBRAllocation&);
std::shared_ptr<const NativePBRImage> GetNativePBRImage(const NativeCUSTextureBinding&);
} // namespace rendering
