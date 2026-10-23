/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeUnitStencilData.h"
namespace rendering::metal4 {
class PreparedUnitStencil {
public:
	~PreparedUnitStencil();
	size_t PassCount()const;
	uint64_t SequenceAt(size_t)const;
	void EncodePass(size_t,id<MTL4CommandBuffer>)const;
	void DidComplete()const;
private:
	PreparedUnitStencil();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class UnitStencilGPU;
};
class UnitStencilGPU {
public:
	UnitStencilGPU();~UnitStencilGPU();
	std::shared_ptr<const NativeUnitStencilProgramConsumer> LinkOriginalProgram(RuntimeLease&,const NativeUnitStencilProgramSource&,bool validation,double& milliseconds);
	std::unique_ptr<PreparedUnitStencil> Prepare(RuntimeLease&,std::shared_ptr<const NativeUnitStencilFrame>,bool validation,double& milliseconds,uint64_t actualSubmissionIdentity);
	CUSSourceImage Resolve(RuntimeLease&,std::shared_ptr<const NativeUnitStencilVersion>,uint64_t actualSubmissionIdentity);
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
