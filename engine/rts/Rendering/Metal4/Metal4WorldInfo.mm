/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldInfo.h"
#include "Metal4MiniMapNative.h"
#include "Metal4MetalInfo.h"
#include <algorithm>
namespace rendering::metal4 {
struct WorldInfoGPU::Impl {
	MiniMapGPU producers;
	__strong id<MTLTexture> completed=nil;
	std::uint64_t map=0,serial=0,generation=0;
	NativeWorldInfoMode mode=NativeWorldInfoMode::Normal;
	std::weak_ptr<PreparedWorldInfo> planned;
};
struct PreparedWorldInfo::Impl {
	std::shared_ptr<WorldInfoGPU::Impl> state;
	std::shared_ptr<const NativeWorldInfoFrame> frame;
	__strong id<MTLTexture> image=nil,previous=nil;
	std::vector<std::shared_ptr<PreparedMiniMap>> updates;
	std::shared_ptr<PreparedMetalInfo> metal;
	mutable bool encoded=false,completed=false;
};
PreparedWorldInfo::PreparedWorldInfo():impl(std::make_unique<Impl>()){}
PreparedWorldInfo::~PreparedWorldInfo()=default;
id<MTLTexture> PreparedWorldInfo::Info()const{return impl->image;}
uint32_t PreparedWorldInfo::SourceDrawFrame()const{return impl->frame->sourceDrawFrame;}
bool PreparedWorldInfo::MatchesFrame(const NativeWorldInfoFrame& frame)const{return SameNativeWorldInfoVersion(*impl->frame,frame);}
uint64_t PreparedWorldInfo::MapIdentity()const{return impl->frame->mapIdentity;}
uint64_t PreparedWorldInfo::CompletionSerial()const{return impl->frame->serial;}
bool PreparedWorldInfo::HasUpdates()const{return !impl->completed&&(!impl->updates.empty()||(impl->metal&&impl->metal->HasUpdates()));}
void PreparedWorldInfo::RegisterResources(RuntimeLease& lease)const
{
	lease.AddAllocation(impl->image);if(impl->previous!=nil)lease.AddAllocation(impl->previous);
	if(impl->metal)lease.AddAllocation(impl->metal->Info());
	for(const auto& update:impl->updates)update->RegisterInfoResources(lease);
}
void PreparedWorldInfo::EncodeDependencies(id<MTL4CommandBuffer> command)const
{
	if(impl->encoded||impl->completed)return;
	if(impl->metal)impl->metal->EncodeDependencies(command);
	if(impl->previous!=nil){auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode original info copy-on-write history");
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[e copyFromTexture:impl->previous sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(impl->image.width,impl->image.height,1) toTexture:impl->image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	for(const auto& update:impl->updates)update->EncodeDependencies(command);
	impl->encoded=true;
}
void PreparedWorldInfo::DidComplete()const
{
	Require(!HasUpdates()||impl->encoded,"Cannot publish original global info before its actual producer encodes");
	if(impl->completed)return;
	if(impl->metal)impl->metal->DidComplete();for(const auto& update:impl->updates)update->DidComplete();
	if(impl->state->map<MapIdentity()||(impl->state->map==MapIdentity()&&(impl->state->generation<impl->frame->modeGeneration||(impl->state->generation==impl->frame->modeGeneration&&impl->state->serial<=CompletionSerial())))){impl->state->completed=impl->image;impl->state->map=MapIdentity();impl->state->serial=CompletionSerial();impl->state->generation=impl->frame->modeGeneration;impl->state->mode=impl->frame->mode;}impl->completed=true;
}
WorldInfoGPU::WorldInfoGPU():impl(std::make_shared<Impl>()){}
WorldInfoGPU::~WorldInfoGPU()=default;
void WorldInfoGPU::ClearResources(){impl=std::make_shared<Impl>();}
WorldInfoGPU& SharedWorldInfoGPU(){static WorldInfoGPU value;return value;}
std::shared_ptr<PreparedWorldInfo> WorldInfoGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldInfoFrame> frame,bool validation,double& shaderMs)
{
	Require(frame!=nullptr,"Original global info frame unavailable");ValidateNativeWorldInfoFrame(*frame);
	Require(frame->paddedSquaresX<=lease.Capabilities().maxTextureDimension2D&&frame->paddedSquaresZ<=lease.Capabilities().maxTextureDimension2D,"Original global info exceeds actual device extent");
	if(auto planned=impl->planned.lock();planned&&planned->MatchesFrame(*frame)){lease.AddAllocation(planned->Info());if(planned->impl->previous!=nil)lease.AddAllocation(planned->impl->previous);if(planned->HasUpdates())for(const auto& update:planned->impl->updates)update->RegisterInfoResources(lease);return planned;}
	auto result=std::shared_ptr<PreparedWorldInfo>(new PreparedWorldInfo);result->impl->state=impl;result->impl->frame=frame;
	const bool same=impl->completed!=nil&&impl->map==frame->mapIdentity&&impl->generation==frame->modeGeneration&&impl->mode==frame->mode;
	Require(!same||frame->serial>=impl->serial,"Original global info cannot reconstruct an older unretained generation");
	if(same&&frame->serial==impl->serial){result->impl->image=impl->completed;result->impl->completed=true;lease.AddAllocation(result->Info());return result;}
	if(frame->mode==NativeWorldInfoMode::Metal){result->impl->metal=SharedMetalInfoGPU().Prepare(lease,frame->metal,validation,shaderMs);Require(!result->impl->metal->HasUpdates(),"Global info consumers require original metal maintenance to complete its update transaction");result->impl->image=result->impl->metal->Info();impl->planned=result;return result;}
	std::vector<const NativeWorldInfoUpdate*> pending;for(const auto& u:frame->updates)if(!same||u.serial>impl->serial)pending.push_back(&u);
	Require(!pending.empty(),"Original global info target has no native producer history");
	Require(pending.front()->clear||(same&&pending.front()->serial==impl->serial+1),"Original global info lost its clear/previous completed dependency");
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGB10A2Unorm width:frame->paddedSquaresX height:frame->paddedSquaresZ mipmapped:YES];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
	result->impl->image=[lease.Device() newTextureWithDescriptor:d];Require(result->Info()!=nil,"Cannot allocate original global RGB10A2 combiner image");lease.AddAllocation(result->Info());
	if(!pending.front()->clear){result->impl->previous=impl->completed;lease.AddAllocation(impl->completed);}
	for(const auto* u:pending)result->impl->updates.push_back(impl->producers.PrepareInfoImage(lease,u->inputs,u->radarGroundLos,u->radarGroundGlobalLos,result->Info(),frame->squaresX,frame->squaresZ,u->clear,validation,shaderMs));
	impl->planned=result;return result;
}
}
