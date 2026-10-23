/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeWorldInfoData.h"
namespace rendering::metal4 {
class WorldInfoGPU;
class PreparedWorldInfo {
public:
	~PreparedWorldInfo();
	id<MTLTexture> Info()const; // real RGB10A2, full original chain, map-Z rows
	// Historical last CPU update; not necessarily the consuming render frame.
	std::uint32_t SourceDrawFrame()const;
	bool MatchesFrame(const NativeWorldInfoFrame&)const;
	std::uint64_t MapIdentity()const;
	std::uint64_t CompletionSerial()const;
	bool HasUpdates()const;
	void RegisterResources(RuntimeLease&)const; // Reuse the retained pre-unit image across runtime leases.
	void EncodeDependencies(id<MTL4CommandBuffer>)const; // before any info consumer; idempotent same plan
	void DidComplete()const; // publish only after feedback+event retirement
private:
	PreparedWorldInfo();struct Impl;std::unique_ptr<Impl> impl;friend class WorldInfoGPU;
};
class WorldInfoGPU {
public:
	WorldInfoGPU();~WorldInfoGPU();
	std::shared_ptr<PreparedWorldInfo> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldInfoFrame>,bool validation,double& shaderMs);
	void ClearResources(); // original map lifetime, only after retirement
private:
	struct Impl;std::shared_ptr<Impl> impl;friend class PreparedWorldInfo;
};
// Same physical completed info image/history for water, minimap and Lua $info.
WorldInfoGPU& SharedWorldInfoGPU();
}
