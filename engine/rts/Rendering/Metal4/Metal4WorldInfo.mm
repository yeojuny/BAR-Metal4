/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldInfo.h"
#include "Metal4MiniMapNative.h"
#include "Metal4MetalInfo.h"
#include <algorithm>

#include <atomic>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdio>
namespace rendering::metal4 {struct WorldInfoStorage245 {__strong id<MTLTexture> image=nil;};}
@interface M4WorldInfoRecoveryStorage245 : NSObject { @public std::shared_ptr<rendering::metal4::WorldInfoStorage245> owner; }
@end
@implementation M4WorldInfoRecoveryStorage245
@end
namespace rendering::metal4 {
namespace {
bool WorldInfoPool245Enabled(){static const bool enabled=[](){const char* v=std::getenv("BAR_NATIVE_WORLDINFO_POOL245");return v&&std::strcmp(v,"1")==0;}();return enabled;}
struct WorldInfoCensus245 {
 std::atomic<unsigned long long> requests{0},allocated{0},reused{0},bytes{0},peakPoolBytes{0},peakSlots{0};
};
WorldInfoCensus245& WorldInfoCounts245(){static auto* p=new WorldInfoCensus245;return *p;}
void EmitWorldInfo245(){auto& c=WorldInfoCounts245();std::fprintf(stderr,"[WorldInfoRecovery245] requests=%llu allocated=%llu reused=%llu allocated_bytes=%llu peak_pool_bytes=%llu peak_slots=%llu\n",c.requests.load(),c.allocated.load(),c.reused.load(),c.bytes.load(),c.peakPoolBytes.load(),c.peakSlots.load());}
void RecordWorldInfo245(bool reused,uint64_t allocatedBytes,uint64_t poolBytes,size_t slots){
 static const bool enabled=[](){const char* v=std::getenv("BAR_NATIVE_WORLDINFO_POOL245_WITNESS");return v&&std::strcmp(v,"1")==0;}();if(!enabled)return;
 static const bool installed=[](){std::atexit(EmitWorldInfo245);return true;}();(void)installed;
 auto& c=WorldInfoCounts245();++c.requests;if(reused)++c.reused;else{++c.allocated;c.bytes+=allocatedBytes;}
 auto max=[](auto& dest,unsigned long long value){auto previous=dest.load(std::memory_order_relaxed);while(previous<value&&!dest.compare_exchange_weak(previous,value,std::memory_order_relaxed)){};};
 max(c.peakPoolBytes,poolBytes);max(c.peakSlots,slots);
}
struct WorldInfoPool245 {
 struct Slot {const Runtime*context;std::shared_ptr<WorldInfoStorage245> owner;uint64_t bytes;};
 std::mutex mutex;std::vector<Slot>slots;uint64_t bytes=0;
 static constexpr size_t MaxSlots=4;static constexpr uint64_t MaxBytes=uint64_t{128}<<20;
};
void PinWorldInfo245(RuntimeLease& lease,const std::shared_ptr<WorldInfoStorage245>& owner){
 if(!owner)return;auto pin=[M4WorldInfoRecoveryStorage245 new];Require(pin!=nil,"Cannot pin actual WorldInfo storage");pin->owner=owner;lease.Retain(pin);
}
std::shared_ptr<WorldInfoStorage245> AcquireWorldInfo245(WorldInfoPool245& pool,RuntimeLease& lease,MTLTextureDescriptor* d){
 std::lock_guard<std::mutex> lock(pool.mutex);std::shared_ptr<WorldInfoStorage245> result;
 // Sole pool ownership is required. Each logical current/prior/producer and
 // reader lease pins the same storage token until the actual retirement.
 for(const auto& slot:pool.slots){const auto image=slot.owner->image;
  if(slot.owner.use_count()==1&&slot.context==lease.ContextIdentity()&&image.device==lease.Device()&&
    image.width==d.width&&image.height==d.height&&image.pixelFormat==d.pixelFormat&&
    image.textureType==d.textureType&&image.depth==d.depth&&image.arrayLength==d.arrayLength&&
    image.mipmapLevelCount==d.mipmapLevelCount&&image.sampleCount==d.sampleCount&&
    image.storageMode==d.storageMode&&image.usage==d.usage){result=slot.owner;break;}
 }
 const bool reused=bool(result);
 if(!result){result=std::make_shared<WorldInfoStorage245>();result->image=[lease.Device() newTextureWithDescriptor:d];Require(result->image!=nil,"Cannot allocate original global RGB10A2 combiner image");
  const uint64_t size=result->image.allocatedSize;
  if(size&&size<=WorldInfoPool245::MaxBytes){
   for(auto i=pool.slots.begin();i!=pool.slots.end()&&(pool.slots.size()>=WorldInfoPool245::MaxSlots||pool.bytes>WorldInfoPool245::MaxBytes-size);){
    if(i->owner.use_count()==1){pool.bytes-=i->bytes;i=pool.slots.erase(i);}else ++i;
   }
   if(pool.slots.size()<WorldInfoPool245::MaxSlots&&pool.bytes<=WorldInfoPool245::MaxBytes-size){
    try{pool.slots.push_back({lease.ContextIdentity(),result,size});pool.bytes+=size;}catch(const std::bad_alloc&){/* optional retention only */}
   }
  }
 }
 RecordWorldInfo245(reused,result->image.allocatedSize,pool.bytes,pool.slots.size());return result;
}
}

struct WorldInfoGPU::Impl {
	MiniMapGPU producers;
 WorldInfoPool245 storagePool245;
 std::shared_ptr<WorldInfoStorage245> completedStorage245;
	__strong id<MTLTexture> completed=nil;
	std::uint64_t map=0,serial=0,generation=0;
	NativeWorldInfoMode mode=NativeWorldInfoMode::Normal;
	std::weak_ptr<PreparedWorldInfo> planned;
};
struct PreparedWorldInfo::Impl {
	std::shared_ptr<WorldInfoGPU::Impl> state;
	std::shared_ptr<const NativeWorldInfoFrame> frame;
	__strong id<MTLTexture> image=nil,previous=nil;
 std::shared_ptr<WorldInfoStorage245> imageStorage245,previousStorage245;
	std::vector<std::shared_ptr<PreparedMiniMap>> updates;
	std::shared_ptr<PreparedMetalInfo> metal;
 std::shared_ptr<PreparedWorldInfo> predecessor;
	mutable bool encoded=false,completed=false;
	mutable uint64_t encodedSubmission=0;
 bool predecessorOrdered=false;
};
PreparedWorldInfo::PreparedWorldInfo():impl(std::make_unique<Impl>()){}
PreparedWorldInfo::~PreparedWorldInfo()=default;
id<MTLTexture> PreparedWorldInfo::Info()const{return impl->image;}
uint32_t PreparedWorldInfo::SourceDrawFrame()const{return impl->frame->sourceDrawFrame;}
bool PreparedWorldInfo::MatchesFrame(const NativeWorldInfoFrame& frame)const{return SameNativeWorldInfoVersion(*impl->frame,frame);}
uint64_t PreparedWorldInfo::MapIdentity()const{return impl->frame->mapIdentity;}
uint64_t PreparedWorldInfo::CompletionSerial()const{return impl->frame->serial;}
bool PreparedWorldInfo::HasUpdates()const{return !impl->completed&&(!impl->updates.empty()||(impl->metal&&impl->metal->HasUpdates()));}
bool PreparedWorldInfo::EncodedFullMipProducerOn(uint64_t sourceSubmission)const
{
 // Non-Metal updates store the private RGB10_A2 base and generate its full mip
 // chain. Cached Metal info and wrapper-only encode callbacks cannot claim it.
 return sourceSubmission&&impl->encoded&&impl->encodedSubmission==sourceSubmission&&
  !impl->metal&&!impl->updates.empty();
}
void PreparedWorldInfo::RegisterResources(RuntimeLease& lease)const
{
 PinWorldInfo245(lease,impl->imageStorage245);PinWorldInfo245(lease,impl->previousStorage245);
	lease.AddAllocation(impl->image);if(impl->previous!=nil)lease.AddAllocation(impl->previous);
	if(impl->metal)lease.AddAllocation(impl->metal->Info());
	for(const auto& update:impl->updates)update->RegisterInfoResources(lease);
}
void PreparedWorldInfo::EncodeDependencies(id<MTL4CommandBuffer> command,uint64_t sourceSubmission)const
{
	if(impl->encoded||impl->completed)return;
 if(impl->predecessor)Require(impl->predecessor->impl->completed||
  (impl->predecessorOrdered&&impl->predecessor->impl->encoded),
  "Info predecessor needs successful encoding and ordered actual retirement before submission");
	if(impl->metal)impl->metal->EncodeDependencies(command);
	if(impl->previous!=nil){auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode original info copy-on-write history");
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[e copyFromTexture:impl->previous sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(impl->image.width,impl->image.height,1) toTexture:impl->image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	for(const auto& update:impl->updates)update->EncodeDependencies(command);
	impl->encodedSubmission=sourceSubmission;
	impl->encoded=true;
}
void PreparedWorldInfo::DidComplete()const
{
	Require(!HasUpdates()||impl->encoded,"Cannot publish original global info before its actual producer encodes");
	if(impl->completed)return;
 if(impl->predecessor){Require(impl->predecessor->impl->completed,"Info predecessor did not actually retire before successor completion");impl->predecessor.reset();}
	if(impl->metal)impl->metal->DidComplete();for(const auto& update:impl->updates)update->DidComplete();
	if(impl->state->map<MapIdentity()||(impl->state->map==MapIdentity()&&(impl->state->generation<impl->frame->modeGeneration||(impl->state->generation==impl->frame->modeGeneration&&impl->state->serial<=CompletionSerial())))){impl->state->completed=impl->image;impl->state->completedStorage245=impl->imageStorage245;impl->state->map=MapIdentity();impl->state->serial=CompletionSerial();impl->state->generation=impl->frame->modeGeneration;impl->state->mode=impl->frame->mode;}impl->completed=true;
}
WorldInfoGPU::WorldInfoGPU():impl(std::make_shared<Impl>()){}
WorldInfoGPU::~WorldInfoGPU()=default;
void WorldInfoGPU::ClearResources(){impl=std::make_shared<Impl>();}
WorldInfoGPU& SharedWorldInfoGPU(){static WorldInfoGPU value;return value;}
std::shared_ptr<PreparedWorldInfo> WorldInfoGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldInfoFrame> frame,bool validation,double& shaderMs)
{
	Require(frame!=nullptr,"Original global info frame unavailable");ValidateNativeWorldInfoFrame(*frame);
	Require(frame->paddedSquaresX<=lease.Capabilities().maxTextureDimension2D&&frame->paddedSquaresZ<=lease.Capabilities().maxTextureDimension2D,"Original global info exceeds actual device extent");
	if(auto planned=impl->planned.lock();planned&&planned->MatchesFrame(*frame)){
		if(planned->impl->encoded&&!planned->impl->completed&&lease.PreparingWhilePending())
			lease.OrderAfterPreviousSubmission(planned.get(),[planned]{
				Require(planned->impl->completed,"Reused global info producer did not retire before submission");
			},NativePriorRead::WorldInfo);
  PinWorldInfo245(lease,planned->impl->imageStorage245);PinWorldInfo245(lease,planned->impl->previousStorage245);
		lease.AddAllocation(planned->Info());
		if(planned->impl->previous!=nil)lease.AddAllocation(planned->impl->previous);
		if(planned->HasUpdates())for(const auto& update:planned->impl->updates)update->RegisterInfoResources(lease);
		return planned;
	}
	auto result=std::shared_ptr<PreparedWorldInfo>(new PreparedWorldInfo);result->impl->state=impl;result->impl->frame=frame;
	auto baseImage=impl->completed;auto baseSerial=impl->serial;auto baseStorage245=impl->completedStorage245;
 bool same=impl->completed!=nil&&impl->map==frame->mapIdentity&&impl->generation==frame->modeGeneration&&impl->mode==frame->mode;
 if(auto prior=impl->planned.lock();lease.PreparingWhilePending()&&prior&&prior->impl->encoded&&!prior->impl->completed&&
  prior->impl->frame->mapIdentity==frame->mapIdentity&&prior->impl->frame->modeGeneration==frame->modeGeneration&&
  prior->impl->frame->mode==frame->mode&&prior->CompletionSerial()<frame->serial){
  result->impl->predecessor=prior;
  result->impl->predecessorOrdered=lease.OrderAfterPreviousSubmission(prior.get(),[prior]{
   Require(prior->impl->completed,"Info predecessor did not actually complete before successor submission");
  },NativePriorRead::WorldInfo);
  baseImage=prior->Info();baseStorage245=prior->impl->imageStorage245;baseSerial=prior->CompletionSerial();same=true;
 }
	Require(!same||frame->serial>=baseSerial,"Original global info cannot reconstruct an older unretained generation");
	if(same&&frame->serial==baseSerial){result->impl->image=baseImage;result->impl->imageStorage245=baseStorage245;PinWorldInfo245(lease,baseStorage245);result->impl->completed=true;lease.AddAllocation(result->Info());return result;}
	if(frame->mode==NativeWorldInfoMode::Metal){result->impl->metal=SharedMetalInfoGPU().Prepare(lease,frame->metal,validation,shaderMs);Require(!result->impl->metal->HasUpdates(),"Global info consumers require original metal maintenance to complete its update transaction");result->impl->image=result->impl->metal->Info();impl->planned=result;return result;}
	std::vector<const NativeWorldInfoUpdate*> pending;for(const auto& u:frame->updates)if(!same||u.serial>baseSerial)pending.push_back(&u);
	Require(!pending.empty(),"Original global info target has no native producer history");
	Require(pending.front()->clear||(same&&pending.front()->serial==baseSerial+1),"Original global info lost its clear/previous completed dependency");
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGB10A2Unorm width:frame->paddedSquaresX height:frame->paddedSquaresZ mipmapped:YES];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
 if(WorldInfoPool245Enabled()){
  result->impl->imageStorage245=AcquireWorldInfo245(impl->storagePool245,lease,d);result->impl->image=result->impl->imageStorage245->image;PinWorldInfo245(lease,result->impl->imageStorage245);
 }else{result->impl->image=[lease.Device() newTextureWithDescriptor:d];Require(result->Info()!=nil,"Cannot allocate original global RGB10A2 combiner image");RecordWorldInfo245(false,result->Info().allocatedSize,0,0);}
 Require(result->Info()!=nil,"Cannot allocate original global RGB10A2 combiner image");lease.AddAllocation(result->Info());
 if(!pending.front()->clear){result->impl->previous=baseImage;result->impl->previousStorage245=baseStorage245;PinWorldInfo245(lease,baseStorage245);lease.AddAllocation(baseImage);}
	for(const auto* u:pending)result->impl->updates.push_back(impl->producers.PrepareInfoImage(lease,u->inputs,u->radarGroundLos,u->radarGroundGlobalLos,result->Info(),frame->squaresX,frame->squaresZ,u->clear,validation,shaderMs));
	impl->planned=result;return result;
}
}
