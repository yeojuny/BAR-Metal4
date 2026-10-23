/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4OpaqueModelSource.h"
#include <algorithm>
#include <exception>
#include <stdexcept>

namespace rendering::metal4 {
namespace {
enum class ForwardOwner {Engine,Lua,Callback,Command};
struct OrderedDraw {uint64_t sequence;ForwardOwner owner;size_t index;};
template<class Prepared> void ValidateSequences(const Prepared& value)
{
	for(size_t n=0;n<value.DrawCount();++n)
		Require(value.SequenceAt(n)&&(!n||value.SequenceAt(n-1)<value.SequenceAt(n)),
			"Original forward source stream is absent or not strictly ordered");
}
std::vector<OrderedDraw> MergeForward(const PreparedEngineModelForward& engine,const PreparedCUSSource* lua,
	const OpaqueModelForwardCallbacks* callbacks=nullptr)
{
	ValidateSequences(engine);if(lua)ValidateSequences(*lua);
	std::vector<OrderedDraw> order;
	for(size_t n=0;n<engine.DrawCount();++n)order.push_back({engine.SequenceAt(n),ForwardOwner::Engine,n});
	if(lua)for(size_t n=0;n<lua->DrawCount();++n)order.push_back({lua->SequenceAt(n),ForwardOwner::Lua,n});
	if(callbacks)for(size_t n=0;n<callbacks->draws.size();++n)order.push_back({callbacks->draws[n].first,ForwardOwner::Callback,n});
	if(callbacks)for(size_t n=0;n<callbacks->commands.size();++n)order.push_back({callbacks->commands[n].first,ForwardOwner::Command,n});
	std::sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
	for(size_t n=0;n<order.size();++n)Require(order[n].sequence&&(!n||order[n-1].sequence<order[n].sequence),
		"Original forward engine/CUS/callback event has duplicate or missing source ownership");
	return order;
}
}

struct PreparedOpaqueModelSource::Impl {
	struct Phase {
		std::unique_ptr<PreparedCUSDeferred> mrt;
		std::shared_ptr<PreparedEngineModelForward> engine;
		std::unique_ptr<PreparedCUSSource> lua;
		std::vector<OrderedDraw> order;
		std::optional<OpaqueModelForwardCallbacks> callbacks;
		bool forwardDependencies=false;
	};
	std::shared_ptr<const NativeOpaqueModelFrame> source;
	std::vector<Phase> phases;
	size_t nextPhase=0;
	bool completed=false;
	Phase& Current(size_t index,bool deferred)
	{
		Require(!completed&&index==nextPhase&&index<phases.size(),"Original opaque phase was repeated, skipped or already retired");
		Require(source->passes[index].deferred==deferred,"Original opaque phase target differs from its recorded source");
		return phases[index];
	}
};
PreparedOpaqueModelSource::PreparedOpaqueModelSource():impl(std::make_unique<Impl>()){}
PreparedOpaqueModelSource::~PreparedOpaqueModelSource()=default;
size_t PreparedOpaqueModelSource::PhaseCount()const{return impl->phases.size();}
uint64_t PreparedOpaqueModelSource::SequenceAt(size_t n)const{return impl->source->passes.at(n).sequence;}
uint64_t PreparedOpaqueModelSource::PostDeferredSequenceAt(size_t n)const{return impl->source->passes.at(n).postDeferredSequence;}
bool PreparedOpaqueModelSource::IsDeferredPhase(size_t n)const{return impl->source->passes.at(n).deferred;}

void PreparedOpaqueModelSource::EncodeMRTPhase(size_t n,id<MTL4CommandBuffer> command)
{
	auto& phase=impl->Current(n,true);
	Require(command&&phase.mrt,"Original opaque MRT phase has no actual prepared producer");
	phase.mrt->Encode(command);++impl->nextPhase;
}
void PreparedOpaqueModelSource::AttachForwardCallbacks(size_t n,OpaqueModelForwardCallbacks callbacks)
{
	Require(!impl->completed&&!impl->nextPhase&&n<impl->phases.size()&&impl->source->alphaStage,
		"Original alpha callbacks must attach before the actual source submission");
	auto& phase=impl->phases[n];const auto& source=impl->source->passes[n];
	const auto end=n+1<impl->phases.size()?impl->source->passes[n+1].sequence:impl->source->endSequence;
	Require(!source.deferred&&!phase.callbacks&&callbacks.lifetime&&callbacks.dependencies&&callbacks.didComplete,
		"Original alpha callback owner/dependencies are missing or repeated");
	for(const auto& draw:callbacks.draws)Require(draw.second&&source.sequence<draw.first&&draw.first<end,
		"Original alpha callback draw escaped its actual unit/feature source phase");
	for(const auto& command:callbacks.commands)Require(command.second&&source.sequence<command.first&&command.first<end,
		"Original alpha callback command escaped its actual unit/feature source phase");
	phase.callbacks=std::move(callbacks);phase.order=MergeForward(*phase.engine,phase.lua.get(),&*phase.callbacks);
}
void PreparedOpaqueModelSource::EncodeForwardDependencies(size_t n,id<MTL4CommandBuffer> command)
{
	auto& phase=impl->Current(n,false);
	Require(command&&phase.engine&&!phase.forwardDependencies,"Original forward dependencies are missing or repeated");
	phase.engine->EncodeDependencies(command);if(phase.lua)phase.lua->EncodeDependencies(command);if(phase.callbacks)phase.callbacks->dependencies(command);
	phase.forwardDependencies=true;
}
void PreparedOpaqueModelSource::EncodeForwardPhase(size_t n,id<MTL4RenderCommandEncoder> encoder)
{
	auto& phase=impl->Current(n,false);
	Require(encoder&&phase.engine&&phase.forwardDependencies&&(!phase.callbacks||phase.callbacks->commands.empty()),"Original forward phase lacks its same-submission dependency stage or command scheduler");
	for(const auto& draw:phase.order){
		switch(draw.owner){
			case ForwardOwner::Engine:phase.engine->EncodeOrderedDraw(draw.index,encoder);break;
			case ForwardOwner::Lua:phase.lua->EncodeDraw(draw.index,encoder);break;
			case ForwardOwner::Callback:phase.callbacks->draws[draw.index].second(encoder);break;
			case ForwardOwner::Command:throw std::logic_error("Original alpha command has no selected command-buffer scheduler");
		}
	}
	phase.engine->FinishOrderedDraws();++impl->nextPhase;
}
void PreparedOpaqueModelSource::EncodeForwardPhase(size_t n,id<MTL4CommandBuffer> command,
	id<MTL4RenderCommandEncoder> __strong& encoder,MTL4RenderPassDescriptor* loadedScene)
{
	auto& phase=impl->Current(n,false);
	Require(command&&loadedScene&&phase.engine&&phase.forwardDependencies&&impl->source->alphaStage,
		"Original alpha source command scheduler lacks its actual common attachments/dependencies");
	for(const auto& draw:phase.order){
		if(draw.owner==ForwardOwner::Command){
			if(encoder){[encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
			phase.callbacks->commands[draw.index].second(command);continue;
		}
		if(!encoder){encoder=[command renderCommandEncoderWithDescriptor:loadedScene];Require(encoder,"Cannot resume original alpha attachments after callback command");[encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
		switch(draw.owner){
			case ForwardOwner::Engine:phase.engine->EncodeOrderedDraw(draw.index,encoder);break;
			case ForwardOwner::Lua:phase.lua->EncodeDraw(draw.index,encoder);break;
			case ForwardOwner::Callback:phase.callbacks->draws[draw.index].second(encoder);break;
			case ForwardOwner::Command:break;
		}
	}
	phase.engine->FinishOrderedDraws();++impl->nextPhase;
	if(!encoder){encoder=[command renderCommandEncoderWithDescriptor:loadedScene];Require(encoder,"Cannot resume original alpha target after its actual source phase");[encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
}
void PreparedOpaqueModelSource::DidComplete()
{
	Require(!impl->completed&&impl->nextPhase==impl->phases.size(),"Original opaque source cannot retire unencoded phases or retire twice");
	// The shared caller has already observed real retirement. Do not make it
	// appear unretired if a later child completion reports a source error.
	impl->completed=true;
	std::vector<std::exception_ptr> failures;
	const auto complete=[&](auto& child){
		if(!child)return;
		try{child->DidComplete();}catch(...){failures.push_back(std::current_exception());}
	};
	for(auto& phase:impl->phases){
		if(phase.mrt)complete(phase.mrt);
		else{complete(phase.engine);complete(phase.lua);if(phase.callbacks)try{phase.callbacks->didComplete();}catch(...){failures.push_back(std::current_exception());}}
	}
	if(failures.size()==1)std::rethrow_exception(failures.front());
	if(!failures.empty()){
		std::string message="Original opaque source child completion failures";
		for(const auto& failure:failures){
			try{std::rethrow_exception(failure);}
			catch(const std::exception& error){message+="; ";message+=error.what();}
			catch(...){message+="; non-standard source completion exception";}
		}
		throw std::runtime_error(message);
	}
}

std::unique_ptr<PreparedOpaqueModelSource> PrepareOpaqueModelSource(RuntimeLease& lease,
	CUSSourceGPU& cus,EngineModelDeferredGPU& deferred,EngineModelForwardGPU& forward,
	std::shared_ptr<const NativeOpaqueModelFrame> source,const CUSSourceTarget& forwardTarget,
	const EngineDeferredGeometryProvider& geometry,const CUSSourceImageProvider& images,
	uint64_t frame,uint64_t submission,bool validation,double& ms)
{
	Require(source&&source->sourceDrawFrame==frame&&source->endSequence&&submission&&geometry&&images,
		"Original opaque source differs from its actual common frame/submission/providers");
	Require(forwardTarget.width&&forwardTarget.height&&forwardTarget.logicalWidth&&forwardTarget.logicalHeight&&
		(forwardTarget.samples>=1&&forwardTarget.samples<=32&&[lease.Device() supportsTextureSampleCount:forwardTarget.samples])&&forwardTarget.colors[0]!=MTLPixelFormatInvalid&&
		std::all_of(forwardTarget.colors.begin()+1,forwardTarget.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;})&&
		forwardTarget.depth==MTLPixelFormatDepth32Float,
		"Original forward source requires the actual scene color and D32 attachments");
	if(source->alphaStage)Require(source->passes.size()==2&&
		(*source->alphaStage==NativeWorldDrawStage::AlphaBelowWater||*source->alphaStage==NativeWorldDrawStage::AlphaAboveWater||*source->alphaStage==NativeWorldDrawStage::AlphaAll||*source->alphaStage==NativeWorldDrawStage::WaterReflection||*source->alphaStage==NativeWorldDrawStage::WaterRefraction),
		"Original alpha source must retain its actual route and both unit/feature callbacks");
	unsigned previousRank=0;bool first=true;
	for(size_t n=0;n<source->passes.size();++n){
		const auto& pass=source->passes[n];const unsigned rank=unsigned(pass.feature)*2+unsigned(!pass.deferred);
		Require(pass.sequence&&(!n||source->passes[n-1].sequence<pass.sequence)&&pass.sequence<source->endSequence&&
			(first||previousRank<rank)&&pass.engine&&pass.engine->engineObserved&&pass.engine->legacyObserved&&
			pass.engine->sourceDrawFrame==frame&&pass.engine->feature==pass.feature&&pass.engine->alpha==bool(source->alphaStage)&&
			(!source->alphaStage||(!pass.deferred&&!pass.mrt&&!pass.postDeferredSequence&&pass.luaEffects))&&
			pass.lua&&pass.lua->sourceDrawFrame==frame&&bool(pass.mrt)==pass.deferred,
			"Original unit/feature deferred/forward source vector is incomplete or reordered");
		Require(!pass.postDeferredSequence||(pass.deferred&&pass.sequence<pass.postDeferredSequence&&
			(n+1==source->passes.size()||pass.postDeferredSequence<source->passes[n+1].sequence)),
			"Original PostDeferred boundary differs from its actual source vector");
		const auto bodyEnd=pass.postDeferredSequence?pass.postDeferredSequence:
			(n+1<source->passes.size()?source->passes[n+1].sequence:source->endSequence);
		const auto checkDraw=[&](const auto& draw){Require(pass.sequence<draw.sequence&&draw.sequence<bodyEnd,
			"Original model draw escaped its actual source phase boundary");};
		for(const auto& draw:pass.engine->beforeLua)checkDraw(draw);
		for(const auto& draw:pass.engine->afterLua)checkDraw(draw);
		for(const auto& draw:pass.lua->draws)checkDraw(draw);
		previousRank=rank;first=false;
	}
	auto out=std::unique_ptr<PreparedOpaqueModelSource>(new PreparedOpaqueModelSource);
	auto& p=*out->impl;p.source=std::move(source);p.phases.resize(p.source->passes.size());
	// Resource fulfillment order is independent of source encode order. Every
	// selected MRT version must exist before screen/Shield/SSAO/Bloom Prepare.
	for(size_t n=0;n<p.phases.size();++n)if(p.source->passes[n].deferred)
		p.phases[n].mrt=PrepareOpaqueModelMRTPhase(lease,cus,deferred,p.source->passes[n],geometry,images,frame,submission,validation,ms);
	for(size_t n=0;n<p.phases.size();++n)if(!p.source->passes[n].deferred){
		auto& phase=p.phases[n];const auto& actual=p.source->passes[n];
		phase.engine=forward.Prepare(lease,actual.engine,forwardTarget,geometry,images,validation,ms);
		Require(phase.engine!=nullptr,"Original forward engine source has no real prepared consumer");
		phase.lua=cus.Prepare(lease,actual.lua,forwardTarget,images,validation,ms);
		phase.order=MergeForward(*phase.engine,phase.lua.get());
	}
	return out;
}
} // namespace rendering::metal4
