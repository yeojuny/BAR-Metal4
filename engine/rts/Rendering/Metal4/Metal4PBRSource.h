/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativePBRSourceData.h"
namespace rendering::metal4 {
class PreparedPBRSource {
public:
	~PreparedPBRSource();
	void Encode(id<MTL4CommandBuffer>,uint64_t actualSubmission);
	void DidComplete(uint64_t actualSubmission);
private:
	struct Impl;std::unique_ptr<Impl> impl;
	PreparedPBRSource();friend class PBRSourceGPU;
};
class PBRSourceGPU {
public:
	PBRSourceGPU();~PBRSourceGPU();
	std::shared_ptr<const NativePBRProgram> Compile(RuntimeLease&,const NativePBRProgramSource&,bool validation,double& compileMs);
	std::shared_ptr<NativePBRImage> Allocate(RuntimeLease&,const NativePBRAllocation&);
	std::unique_ptr<PreparedPBRSource> Prepare(RuntimeLease&,std::span<const std::shared_ptr<const NativePBRImage>>,
		const CUSSourceImageProvider&);
	// Source binding sampling applies GL swizzle/depth in the shared CUS
	// sampler transport. This view contains original RG/RGB storage channels.
	CUSSourceImage Image(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission)const;
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
