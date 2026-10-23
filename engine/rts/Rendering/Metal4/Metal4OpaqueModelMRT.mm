/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4OpaqueModelMRT.h"
#include "Metal4TerrainGroundClear.h"

namespace rendering::metal4 {
std::unique_ptr<PreparedCUSDeferred> PrepareOpaqueModelMRTPhase(RuntimeLease& lease,
	CUSSourceGPU& cus,EngineModelDeferredGPU& engine,const NativeOpaqueModelPass& pass,
	const EngineDeferredGeometryProvider& geometry,const CUSSourceGPU::TextureProvider& images,
	uint64_t frame,uint64_t submission,bool validation,double& milliseconds)
{
	Require(pass.sequence&&pass.deferred&&pass.engine&&pass.lua&&pass.mrt&&submission&&geometry&&images,
		"Actual opaque MRT preparation requires its selected deferred source entry and real providers");
	Require(pass.mrt->lua==pass.lua&&pass.mrt->sourceDrawFrame==frame&&pass.engine->sourceDrawFrame==frame&&
		pass.lua->sourceDrawFrame==frame&&pass.engine->feature==pass.feature&&pass.mrt->feature==pass.feature,
		"Opaque MRT engine, whole original CUS callback and attachments belong to different source entries");
	const auto& sourceCamera=pass.engine->viewCamera;
	const auto& targetCamera=pass.mrt->camera;
	Require(sourceCamera.view==targetCamera.view&&sourceCamera.projection==targetCamera.projection&&
		sourceCamera.position==targetCamera.position&&sourceCamera.viewport==targetCamera.viewport&&
		sourceCamera.clipDepth==targetCamera.clipDepth,
		"Actual engine and original CUS MRT source camera differ");
	Require(pass.mrt->role==NativeGeometryTextureRole::Models&&pass.mrt->clearBefore==pass.FullInitialClear(),
		"Original model MRT clear/load selection differs from its captured clear state");
	if(pass.initialClear){
		const auto& clear=*pass.initialClear;
		Require(clear.stage==NativeWorldDrawStage::ModelDeferred&&clear.sequence>pass.sequence&&
			clear.mask==static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth)&&
			clear.values.color==ScreenColor{0,0,0,0}&&clear.values.depth==1,
			"Original model MRT clear lost its target, source ordinal or GeometryBuffer values");
		const auto afterClear=[&](const auto& draw){Require(clear.sequence<draw.sequence,
			"Original model MRT clear must precede every engine and Lua source draw");};
		for(const auto& draw:pass.engine->beforeLua)afterClear(draw);
		for(const auto& draw:pass.engine->afterLua)afterClear(draw);
		for(const auto& draw:pass.lua->draws)afterClear(draw);
	}
	// This genuine consumer still rejects allocated legacy bins until their
	// original shader/state execution exists. Do not turn that rejection into
	// a successful empty after-Lua callback or a readiness signal.
	const auto& metadata=pass.mrt->depth.backing->Metadata();
	CUSSourceTarget target;target.colors.fill(MTLPixelFormatRGBA8Unorm);target.depth=MTLPixelFormatDepth32Float;
	target.width=target.logicalWidth=metadata.width;target.height=target.logicalHeight=metadata.height;target.camera=targetCamera;
	target.camera.viewport={0,0,int32_t(metadata.width),int32_t(metadata.height)};
	auto actualEngine=engine.Prepare(lease,pass.engine,target,geometry,images,validation,milliseconds);
	Require(bool(actualEngine),"Original engine MRT preparation returned no actual consumer");
	std::shared_ptr<PreparedTerrainGroundClear> partialClear;
	if(pass.initialClear&&!pass.mrt->clearBefore)
		partialClear=PrepareTerrainGroundClears(lease,
			std::span<const NativeWorldAttachmentClear>(&*pass.initialClear,1),target,validation,milliseconds);
	CUSDeferredOtherDraws other;
	other.observed=pass.engine->engineObserved&&pass.engine->legacyObserved;
	other.lifetime=actualEngine;
	other.dependencies=[actualEngine](id<MTL4CommandBuffer> command){actualEngine->EncodeDependencies(command);};
	other.sourceOrder=true;
	// The existing source-order merge consumes the clear on THIS MRT encoder,
	// after its COW load and before any engine/CUS draw, even for a no-op clear.
	if(partialClear)other.drawSequences.push_back(partialClear->SequenceAt(0));
	for(size_t n=0;n<actualEngine->DrawCount();++n)other.drawSequences.push_back(actualEngine->SequenceAt(n));
	other.drawAt=[actualEngine,partialClear](size_t index,id<MTL4RenderCommandEncoder> encoder){
		if(partialClear){
			if(index==0){partialClear->EncodeDraw(0,encoder);return;}
			--index;
		}
		actualEngine->EncodeOrderedDraw(index,encoder);
	};
	other.didEncodeSourceDraws=[actualEngine]{actualEngine->FinishOrderedDraws();};
	other.didComplete=[actualEngine]{actualEngine->DidComplete();};
	// Prepare fulfills all five immutable colors and the SAME actual D32F
	// backing before downstream consumer Prepare. No source is yet encoded.
	return PrepareCUSDeferred(lease,cus,pass.mrt,images,std::move(other),submission,validation,milliseconds);
}
} // namespace rendering::metal4
