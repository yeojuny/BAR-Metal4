/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeMetalInfoData.h"
namespace rendering::metal4 {
class MetalInfoGPU;
class PreparedMetalInfo {
public:
	~PreparedMetalInfo();
	id<MTLTexture> Info() const;
	std::uint64_t CompletionSerial() const;
	bool HasUpdates() const;
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	void DidComplete() const;
private:
	PreparedMetalInfo();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class MetalInfoGPU;
};
class MetalInfoGPU {
public:
	MetalInfoGPU();~MetalInfoGPU();
	void Initialize(RuntimeLease&,bool validation,double& shaderMs);
	std::shared_ptr<PreparedMetalInfo> Prepare(RuntimeLease&,std::shared_ptr<const NativeMetalInfoFrame>,bool validation,double& shaderMs);
	void ClearResources(); // after actual retirement, per-map images only; retain real PSOs
private:
	struct Impl;std::shared_ptr<Impl> impl;
};
// One physical original info image/history for terrain, minimap and Lua $info.
MetalInfoGPU& SharedMetalInfoGPU();
}
