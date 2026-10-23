/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CUSDeferred.h"

namespace rendering::metal4 {
namespace {
struct Storage {
	std::array<id<MTLTexture>,5> colors{};
	__strong id<MTLTexture> depth=nil,combinedColor=nil;
};
void Readable(const ShieldDepthImage& image,uint64_t submission)
{
	Require(image.producer&&IsNativeShieldDepthReadable(image.producer->encoded,image.producer->completed,
		image.producer->operation==ShieldDepthOperation::AllocationOnly,image.producer->allocationVerified,
		image.producer->submissionIdentity,submission),"Original deferred phase attempted to load unencoded prior depth");
}
}
struct PreparedCUSDeferred::Impl {
	std::shared_ptr<const NativeCUSDeferredPhase> source;
	std::shared_ptr<Storage> storage;
	std::shared_ptr<ShieldDepthProducerState> producer;
	std::array<DeferredColorImage,5> colors,priorColors;
	DeferredColorImage combinedColor,priorCombinedColor;
	ShieldDepthImage depth,priorDepth;
	std::unique_ptr<PreparedCUSSource> lua;
	CUSDeferredOtherDraws other;
	struct SourceDraw {bool engine;size_t index;};
	std::vector<SourceDraw> sourceOrder;
	uint64_t submission=0;
	bool encoded=false,completed=false;
};
PreparedCUSDeferred::PreparedCUSDeferred():impl(std::make_unique<Impl>()){}
PreparedCUSDeferred::~PreparedCUSDeferred()=default;
const std::array<DeferredColorImage,5>& PreparedCUSDeferred::Colors()const{return impl->colors;}
const ShieldDepthImage& PreparedCUSDeferred::Depth()const{return impl->depth;}

std::unique_ptr<PreparedCUSDeferred> PrepareCUSDeferred(RuntimeLease& lease,CUSSourceGPU& programs,
	std::shared_ptr<const NativeCUSDeferredPhase> source,const CUSSourceGPU::TextureProvider& images,
	CUSDeferredOtherDraws other,uint64_t submission,bool validation,double& milliseconds)
{
	Require(source&&submission&&other.observed,"Original model MRT requires actual selected non-CUS source ownership");
	Require((!other.beforeLua&&!other.afterLua)||bool(other.lifetime),"Original engine MRT callbacks lack their retained geometry owner");
	Require(other.sourceOrder?(!other.beforeLua&&!other.afterLua&&other.drawAt&&other.didEncodeSourceDraws&&other.lifetime):
		(other.drawSequences.empty()&&!other.drawAt&&!other.didEncodeSourceDraws),
		"Original MRT source order and grouped callbacks cannot be absent or mixed");
	ValidateNativeCUSDeferredPhase(*source);
	const auto& m=source->depth.backing->Metadata();
	Require(m.width&&m.height&&m.width<=lease.Capabilities().maxTextureDimension2D&&m.height<=lease.Capabilities().maxTextureDimension2D,
		"Original model MRT extent exceeds its actual native storage limit");
	auto out=std::unique_ptr<PreparedCUSDeferred>(new PreparedCUSDeferred);auto& p=*out->impl;
	p.source=std::move(source);p.other=std::move(other);p.submission=submission;p.storage=std::make_shared<Storage>();
	p.producer=CreateShieldDepthProducer(p.source->role==NativeGeometryTextureRole::Map?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly,p.source->sourceDrawFrame);
	p.producer->submissionIdentity=submission;
 if(p.source->clearOnly)p.producer->operation=ShieldDepthOperation::ClearOne;
	for(unsigned attachment=0;attachment<5;++attachment){
		auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:m.width height:m.height mipmapped:NO];
		descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
		auto texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(texture,"Cannot allocate original five-MRT color output");
		p.storage->colors[attachment]=texture;lease.AddAllocation(texture);
		p.colors[attachment]={texture,p.storage,p.producer,p.source->colors[attachment].backing->Metadata()};
	}
	if(p.source->combinedColor.Changed()){
		auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:m.width height:m.height mipmapped:NO];
		descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
		auto texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(texture,"Cannot allocate original combined ground color output");
		p.storage->combinedColor=texture;lease.AddAllocation(texture);
		p.combinedColor={texture,p.storage,p.producer,p.source->combinedColor.backing->Metadata()};
	}
	auto depthDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:m.width height:m.height mipmapped:NO];
	depthDescriptor.storageMode=MTLStorageModePrivate;depthDescriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
	p.storage->depth=[lease.Device()newTextureWithDescriptor:depthDescriptor];Require(p.storage->depth,"Cannot allocate same-pass original model MRT depth");lease.AddAllocation(p.storage->depth);
	p.producer->allocationVerified=true;
	p.depth={p.storage->depth,p.source->role==NativeGeometryTextureRole::Map?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly,p.source->sourceDrawFrame,p.producer->identity,m.view,m.projection,m.viewport,p.storage,p.producer};
	if(!p.source->clearBefore){
		for(unsigned n=0;n<5;++n){
			const auto previous=p.source->colors[n].previous;
			auto backing=previous?std::dynamic_pointer_cast<const NativeDeferredColorBacking>(previous->backing):nullptr;
			Require(bool(backing),"Original load-phase MRT color has no actual previous image");
			p.priorColors[n]=GetDeferredColorBackingImage(*backing);auto texture=p.priorColors[n].texture;
			Require(texture&&texture.width==m.width&&texture.height==m.height&&texture.pixelFormat==MTLPixelFormatRGBA8Unorm,
				"Original MRT prior color storage differs");lease.AddAllocation(texture);
		}
		if(p.source->combinedColor.Changed()){
			const auto previous=p.source->combinedColor.previous;auto backing=previous?std::dynamic_pointer_cast<const NativeDeferredColorBacking>(previous->backing):nullptr;
			Require(bool(backing),"Original combined color lacks its real prior allocation/content");p.priorCombinedColor=GetDeferredColorBackingImage(*backing);
			auto texture=p.priorCombinedColor.texture;Require(texture&&texture.width==m.width&&texture.height==m.height&&texture.pixelFormat==MTLPixelFormatRGBA8Unorm,"Combined color prior storage differs");lease.AddAllocation(texture);
		}
		const auto previous=p.source->depth.previous;
		auto backing=previous?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(previous->backing):nullptr;
		Require(bool(backing),"Original load-phase MRT depth has no actual previous image");
		p.priorDepth=GetShieldDepthBackingImage(*backing);
		Require(p.priorDepth.texture&&p.priorDepth.texture.width==m.width&&p.priorDepth.texture.height==m.height&&p.priorDepth.texture.pixelFormat==MTLPixelFormatDepth32Float,
			"Original MRT prior depth storage differs");lease.AddAllocation(p.priorDepth.texture);
	}
	CUSSourceTarget target;target.colors.fill(MTLPixelFormatRGBA8Unorm);target.depth=MTLPixelFormatDepth32Float;
	if(p.combinedColor.texture)target.combinedColor=MTLPixelFormatRGBA8Unorm;
	target.width=target.logicalWidth=m.width;target.height=target.logicalHeight=m.height;target.camera=p.source->camera;
	// GeometryBuffer::LoadViewport uses its actual full named allocation extent.
	target.camera.viewport={0,0,int32_t(m.width),int32_t(m.height)};
	p.lua=programs.Prepare(lease,p.source->lua,target,images,validation,milliseconds);
	if(p.other.sourceOrder){
		const size_t luaCount=p.lua?p.lua->DrawCount():0;
		for(size_t n=0;n<p.other.drawSequences.size();++n)Require(p.other.drawSequences[n]&&(!n||p.other.drawSequences[n-1]<p.other.drawSequences[n]),"Engine MRT source draw sequence is absent or out of order");
		for(size_t n=0;n<luaCount;++n)Require(p.lua->SequenceAt(n)&&(!n||p.lua->SequenceAt(n-1)<p.lua->SequenceAt(n)),"CUS MRT source draw sequence is absent or out of order");
		size_t engine=0,lua=0;
		while(engine<p.other.drawSequences.size()||lua<luaCount){
			if(engine==p.other.drawSequences.size()){p.sourceOrder.push_back({false,lua++});continue;}
			if(lua==luaCount){p.sourceOrder.push_back({true,engine++});continue;}
			const auto engineSequence=p.other.drawSequences[engine],luaSequence=p.lua->SequenceAt(lua);
			Require(engineSequence!=luaSequence,"Engine and CUS draws cannot own the same actual source event");
			if(engineSequence<luaSequence)p.sourceOrder.push_back({true,engine++});else p.sourceOrder.push_back({false,lua++});
		}
	}
	// Publish actual storage only after every stage resource prepared. These
	// tokens still report encoded=false until their actual five-output pass.
	for(unsigned n=0;n<5;++n)FulfillDeferredColorBacking(*p.source->colors[n].backing,p.colors[n]);
	if(p.combinedColor.texture)FulfillDeferredColorBacking(*p.source->combinedColor.backing,p.combinedColor);
	FulfillShieldDepthBacking(*p.source->depth.backing,p.depth);
	return out;
}

void PreparedCUSDeferred::Encode(id<MTL4CommandBuffer> command)
{
	auto& p=*impl;Require(command&&!p.encoded&&!p.completed,"Original model MRT phase is missing or encoded twice");
	if(!p.source->clearBefore){
		for(const auto& image:p.priorColors)Require(IsDeferredColorReadable(image,p.submission),"Original MRT phase attempted to load unencoded prior color");
		if(p.priorCombinedColor.texture)Require(IsDeferredColorReadable(p.priorCombinedColor,p.submission),"Combined color prior contents are not readable in this submission");
		Readable(p.priorDepth,p.submission);
		auto copy=[command computeCommandEncoder];Require(copy,"Cannot encode original model MRT prior-content load");
		[copy barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		const auto extent=MTLSizeMake(p.depth.texture.width,p.depth.texture.height,1);
		for(unsigned n=0;n<5;++n)[copy copyFromTexture:p.priorColors[n].texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.colors[n].texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		if(p.priorCombinedColor.texture)[copy copyFromTexture:p.priorCombinedColor.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.combinedColor.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[copy copyFromTexture:p.priorDepth.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:extent toTexture:p.depth.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	if(p.other.dependencies)p.other.dependencies(command);
	if(p.lua)p.lua->EncodeDependencies(command);
	auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=p.depth.texture.width;pass.renderTargetHeight=p.depth.texture.height;
	for(unsigned n=0;n<5;++n){auto color=pass.colorAttachments[n];color.texture=p.colors[n].texture;color.loadAction=p.source->clearBefore?MTLLoadActionClear:MTLLoadActionLoad;color.storeAction=MTLStoreActionStore;color.clearColor=MTLClearColorMake(0,0,0,0);}
	if(p.combinedColor.texture){auto color=pass.colorAttachments[5];color.texture=p.combinedColor.texture;color.loadAction=p.source->clearBefore?MTLLoadActionClear:MTLLoadActionLoad;color.storeAction=MTLStoreActionStore;color.clearColor=MTLClearColorMake(0,0,0,0);}
	pass.depthAttachment.texture=p.depth.texture;pass.depthAttachment.loadAction=p.source->clearBefore?MTLLoadActionClear:MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;
	auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot encode original five-MRT model phase");
	try{
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder setViewport:MTLViewport{0,0,double(pass.renderTargetWidth),double(pass.renderTargetHeight),0,1}];[encoder setScissorRect:MTLScissorRect{0,0,pass.renderTargetWidth,pass.renderTargetHeight}];
	if(p.other.sourceOrder){
		for(const auto& draw:p.sourceOrder){if(draw.engine)p.other.drawAt(draw.index,encoder);else p.lua->EncodeDraw(draw.index,encoder);}
		p.other.didEncodeSourceDraws();
	}else{
		if(p.other.beforeLua)p.other.beforeLua(encoder);
		if(p.lua)for(size_t n=0;n<p.lua->DrawCount();++n)p.lua->EncodeDraw(n,encoder);
		if(p.other.afterLua)p.other.afterLua(encoder);
	}
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	}catch(...){[encoder endEncoding];throw;}
	[encoder endEncoding];
	p.encoded=true;p.producer->encoded=true;
}
void PreparedCUSDeferred::DidComplete()
{
	auto& p=*impl;Require(p.encoded&&!p.completed,"Original model MRT cannot retire before its actual phase or twice");
	// The common caller already observed real submission retirement. Preserve
	// that fact even if a later child retirement callback reports an error.
	p.completed=true;p.producer->completed=true;
	if(p.lua)p.lua->DidComplete();if(p.other.didComplete)p.other.didComplete();
}
} // namespace rendering::metal4
