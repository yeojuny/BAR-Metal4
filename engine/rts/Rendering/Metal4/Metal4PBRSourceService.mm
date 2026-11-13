/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PBRSourceService.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <algorithm>

namespace rendering::metal4 {
void PBRSourceService::Initialize(NativeTextureContext& owner,NativeRendererAccessGate& access,std::unique_ptr<NativeSerialRenderWorker>& worker,std::function<void()> finishCPU,Parser parser,std::function<bool()> ready,
	std::function<uint64_t()> drawFrame,std::function<void()> validateRaster,bool validate)
{
	Require(!context&&owner.Initialized()&&owner.UIAttached()&&finishCPU&&parser&&ready&&drawFrame&&validateRaster,"PBR source context lacks its actual original resource/raster owners");
	context=&owner;resourcesReady=std::move(ready);validation=validate;
	LuaNativePBRSource::Services services;
	services.ready=[this]{return Ready();};
 // Persistent source callback retains the renderer's stable worker slot, not
 // Initialize's stack. Runtime completion callbacks run on main before handoff.
 services.compile=[this,gate=&access,renderWorker=&worker,finishCPU=std::move(finishCPU)](const NativePBRProgramSource& source){
  Require([NSThread isMainThread],"PBR compile controls require the source thread");
  finishCPU(); // Before gate/runtime locks; stable renderer owner callback.
  auto& runtime=SharedRuntime();
  {NativeRendererAccessGate::Scope owner(gate);Require(runtime.CanSafelyDetach(),"PBR compiler cannot pass unsafe pending GPU work");}
  auto request=std::make_shared<const NativePBRProgramSource>(source);
  return InvokeNativeRendererControl(*renderWorker,*gate,std::move(request),
   [this,&runtime](const auto& captured,const NativeRendererAccessGate::Scope&)->std::shared_ptr<const NativePBRProgram>{@autoreleasepool {
    RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime);
    return gpu.Compile(lease,*captured,validation,compileMilliseconds);
   }});
 };
	services.allocate=[](const NativePBRAllocation& allocation){return ReserveNativePBRImageForCapture(allocation);};
	services.parseTexture=std::move(parser);services.sourceDrawFrame=std::move(drawFrame);
	services.enqueue=[this](std::shared_ptr<const NativePBRImage> image){Require(Ready()&&image&&image->program,"Original PBR enqueue has no source owner");pending.Enqueue(std::move(image));};
	services.syncObservedBinding=[](unsigned unit){ui::GetRecorder().SyncObservedTextureBinding(unit);};
	services.validateFullTargetRaster=std::move(validateRaster);
	services.targetRaster=[](uint32_t width,uint32_t height,bool identities,const std::function<void()>& callback){ui::GetRecorder().RunFramebufferState(width,height,identities,callback);};
	try{LuaNativePBRSource::Configure(std::move(services),&owner.Registry(),&owner.History());}catch(...){context=nullptr;resourcesReady={};throw;}
}
void PBRSourceService::Shutdown(){LuaNativePBRSource::Configure({},nullptr,nullptr);pending.Reset();resourcesReady={};context=nullptr;}
bool PBRSourceService::Ready()const{return context&&context->Initialized()&&context->UIAttached()&&resourcesReady&&resourcesReady();}
NativePBRDeclarationReceipt PBRSourceService::CapturePending(){Require([NSThread isMainThread],"PBR source capture requires engine main thread");return pending.Capture();}
std::unique_ptr<PreparedPBRSource> PBRSourceService::Prepare(RuntimeLease& lease,NativePBRKind kind,const CUSSourceImageProvider& provider,const NativePBRDeclarationReceipt& batch)
{
 if(!batch)return nullptr;
	std::vector<std::shared_ptr<const NativePBRImage>> selected;
	for(const auto& p:*batch)if(p->program->kind==kind&&!p->completed.load())selected.push_back(p);
	return selected.empty()?nullptr:gpu.Prepare(lease,selected,provider);
}
void PBRSourceService::ResetWorld()
{
	// ClearStaticResources holds RuntimeCleanupLease, which independently
	// verifies that the common submission has retired. Encoded is not the
	// same as submitted/completed: a failed recording or retired GPU failure
	// must release safely without marking any failed image as completed.
	pending.Reset();
}
CUSSourceImage PBRSourceService::Image(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission)const{return gpu.Image(lease,binding,submission);}
}
