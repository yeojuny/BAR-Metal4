/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ShieldNative.h"
#include "Rendering/Backend/NativeMapDepthPlan.h"
namespace rendering::metal4 {
struct MapDepthDraw {
	id<MTLBuffer> heights=nil,tiles=nil,uniforms=nil,scene=nil;
	std::size_t uniformOffset=0,tileCount=0;
};
class MapDepthGPU;
class PreparedMapDepth {
public:
	~PreparedMapDepth();
	const ShieldDepthImage& Image() const;
	bool RequiresEncode() const;
	NativeMapDepthAction Action() const;
	void Encode(id<MTL4CommandBuffer>);
	void DidComplete();
private:
	PreparedMapDepth();struct Impl;std::unique_ptr<Impl> impl;friend class MapDepthGPU;
};
class MapDepthGPU {
public:
	MapDepthGPU();~MapDepthGPU();
	void Initialize(RuntimeLease&,id<MTLLibrary> actualWorldLibrary,bool validation,double&);
	// prior is the registry's actual previous storage version. On logical
	// creation/resize it MUST be null after the original delete/create writers.
	// Retain+null creates actual allocation-only undefined storage only when
	// selection.namedDepthStorageExists records the real logical Create event.
	// No observed object means no image; it never changes a prior binding.
	std::unique_ptr<PreparedMapDepth> Prepare(RuntimeLease&,std::uint64_t sourceDrawFrame,const WorldCamera&,
		std::uint32_t renderWidth,std::uint32_t renderHeight,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		const NativeMapDepthSelection&,const MapDepthDraw*,const ShieldDepthImage* prior,
		id<MTLLibrary> actualWorldLibrary,bool validation,double&,std::uint64_t submissionIdentity=0);
	std::unique_ptr<PreparedMapDepth> PrepareBatches(RuntimeLease&,std::uint64_t sourceDrawFrame,const WorldCamera&,
		std::uint32_t renderWidth,std::uint32_t renderHeight,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		const NativeMapDepthSelection&,std::span<const MapDepthDraw>,const ShieldDepthImage* prior,
		id<MTLLibrary> actualWorldLibrary,bool validation,double&,std::uint64_t submissionIdentity=0);
	void ClearResources(); // After common retirement; real shader PSO retained.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
