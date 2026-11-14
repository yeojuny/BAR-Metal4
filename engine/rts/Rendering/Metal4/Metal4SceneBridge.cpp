/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeLegacyWaterProgramState.h"
#include "Rendering/Backend/NativeLegacyWaterTextureMatrices.h"
#include "Rendering/Backend/NativeLegacyWaterTextureEnvironment.h"
#include "Rendering/Backend/NativeLegacyWaterTexGen.h"
#include "Rendering/Backend/NativeLegacyWaterLighting.h"
#include "Rendering/Backend/NativeTerraformCaptureProgram.h"
#include "Lua/LuaNativeStartBox.h"
#include "Lua/LuaNativeWeaponRange.h"
#include "Lua/LuaNativeBuildSquare.h"
#include "Lua/LuaNativeUI.h"
#include "Lua/LuaNativeSensor31657Circle.h"
#include "Lua/LuaNativeSensorPreview.h"
#include "Rendering/EngineBridge/NativeSensorPreviewTargetBridge.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeDeferredPostprocessSource.h"
#include "Metal4SceneBridge.h"
#include "Metal4DisplayGeometry.h"
#include "Metal4SurfaceEventPolicy.h"
#include "Metal4CaptureSurfaceBoundary.h"
#include "System/Config/ConfigHandler.h"
#include "System/Platform/Threading.h"
#include "Rendering/Backend/NativeLegacyWaterData.h"
#include "Rendering/Backend/NativeDynamicWaterCapture.h"
#include "Rendering/Backend/NativeImageReadback.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include "Rendering/Backend/NativeFrameAdmissionTransaction.h"
#include "Rendering/Backend/NativeMainSourceSettlement.h"
#include "Lua/LuaNativePostprocess.h"
#include "Rendering/EngineBridge/NativeCUSSourceCapture.h"
#include "Rendering/Backend/NativeCUSCPUReadScope.h"
#include "Rendering/Backend/NativeCUSCPUReadWorker220.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
#include "Rendering/EngineBridge/NativeLegacyMaterialSource.h"
#include "Rendering/EngineBridge/NativeEngineModelProgramCapture.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Rendering/EngineBridge/NativeTerrainProgramSource.h"
#include "Rendering/EngineBridge/NativeWaterGroundSourceCapture.h"
#include "Rendering/Backend/NativeOriginalModelContext.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeNamedVolumeSource.h"
#include "Rendering/Backend/NativeShadowTextureSource.h"
#include "Rendering/Backend/NativeOriginalShadowData.h"
#include "Rendering/Backend/NativeOriginalShadowParticlesProgramSource.h"
#include "Rendering/EngineBridge/NativeOriginalShadowCapture.h"
#include "Rendering/EngineBridge/NativeGenericShadowSource.h"
#include "Rendering/Backend/NativeCubeTextureSource.h"
#include "Lua/LuaNativeCUSTextures.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/DebugCubeMapTexture.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Rendering/ShadowHandler.h"
#include "Lua/LuaOpenGLUtils.h"
#include "Metal4WorldRenderer.h"
#include "Metal4ScreenPresenter.h"
#include "Metal4MetalInfoBridge.h"
#include "Rendering/WorldFrameCapture.h"
#include "Rendering/Backend/NativeWorldParticleData.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/Backend/NativeWorldEnvironmentData.h"
#include "Rendering/Backend/NativeMetalInfoData.h"
#include "Rendering/Backend/NativeCUSMaterialControl.h"
#include "Rendering/Textures/Bitmap.h"
#include "Lua/LuaNativeNano.h"
#include "Lua/LuaNativePostprocess.h"
#include "Lua/LuaNativeVolumetricClouds.h"
#include "Lua/LuaNativeProjectileEffects.h"
#include "Lua/LuaNativeEnergyExplosion.h"
#include "Lua/LuaNativeLightning.h"
#include "Lua/LuaNativeFireSmoke.h"
#include "Lua/LuaNativeMissileThruster.h"
#include "Lua/LuaNativeFlamethrower.h"
#include "Lua/LuaNativeFireGL4.h"
#include "Lua/LuaNativeNapalmPreview.h"
#include "Lua/LuaNativeSnow.h"
#include "Lua/LuaNativeCommands.h"
#include "Lua/LuaNativeGameDecal.h"
#include "Lua/LuaNativeUnitPrimitive.h"
#include "Rendering/Backend/NativeGameDecalData.h"
#include "Rendering/Backend/NativeUnitPrimitiveData.h"
#include "Lua/LuaNativeShield.h"
#include "Lua/LuaNativeSSAO.h"
#include "Lua/LuaNativeBloom.h"
#include "Lua/LuaNativeDeferredLights.h"
#include "Lua/LuaNativeUnitStencil.h"
#include "Rendering/Backend/NativeUnitStencilData.h"
#include "Lua/LuaNativeOutline.h"
#include "Lua/LuaNativeDistortion.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeShieldTextureState.h"
#include "Rendering/Backend/NativeShieldDepthPublication.h"
#include "Lua/LuaNativeBlueprint.h"
#include "Lua/LuaNativeBlockout.h"
#include "Lua/LuaNativePlacement.h"
#include "Lua/LuaNativePiP.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/Log/ILog.h"
#include "Rendering/Backend/NativeBumpWaterCapture.h"
#include "Rendering/Backend/NativeBasicWaterCapture.h"
#include "Rendering/Backend/NativeWorldInfoCapture.h"
#include "Rendering/Backend/NativeWaterSceneData.h"
#include "Rendering/Backend/NativeWaterLuaFrame.h"
#include "Rendering/Env/Decals/NativeGroundDecalCapture.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/EngineBridge/NativeWaterLuaCapture.h"
#include "Rendering/Env/IWater.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Map/InfoTexture/IInfoTextureHandler.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Game.h"
#include "Map/ReadMap.h"
#include "Map/SMF/SMFReadMap.h"
#include "System/EventHandler.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Map/BaseGroundDrawer.h"
#include "System/Config/ConfigHandler.h"
#include "System/Matrix44f.h"


#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace rendering::scene {
namespace {

int CPUReadPacketMode220(){
 static const int mode=[] {const char* p=std::getenv("BAR_NATIVE_CPU_READ_PACKET220");return p&&p[0]>='1'&&p[0]<='3'&&!p[1]?p[0]-'0':0;}();
 return mode; // 0 original, 1 early synchronous, 2 worker, 3 worker + exact comparison.
}
bool CPUReadPacketMeasure220(){
 static const bool value=[] {const char* p=std::getenv("BAR_NATIVE_CPU_READ_PACKET220_FILE");return p&&p[0]=='/';}();
 return value;
}
// Immutable optional model-read roster only; never visits Lua/water producers.
struct CPUReadWaterRoster220 {
 uint64_t frame=0;
 std::shared_ptr<const NativeWaterSceneFrame> owner;
 std::array<std::shared_ptr<const NativeWaterSceneView>,2> views;
 std::vector<std::shared_ptr<const NativeOpaqueModelFrame>> models;
 std::vector<std::shared_ptr<const NativeCUSSourceFrame>> sources;
 template<class A,class B>static bool SameOwner(const A& a,const B& b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
 static CPUReadWaterRoster220 Capture(uint64_t draw,std::shared_ptr<const NativeWaterSceneFrame> water){
  CPUReadWaterRoster220 out;out.frame=draw;out.owner=std::move(water);
  if(!out.owner)return out;
  if(out.owner->sourceDrawFrame!=draw)throw std::logic_error("Water CPU read roster draw mismatch");
  out.views={out.owner->refraction,out.owner->reflection};
  const auto append=[&](const auto& model){
   out.models.push_back(model);if(!model)return;
   if(model->sourceDrawFrame!=draw)throw std::logic_error("Water CPU model roster draw mismatch");
   for(const auto& pass:model->passes){out.sources.push_back(pass.lua);if(pass.mrt)out.sources.push_back(pass.mrt->lua);}
  };
  for(const auto& view:out.views)if(view){
   if(view->frame.sourceDrawFrame!=draw)throw std::logic_error("Water CPU view roster draw mismatch");
   append(view->frame.opaqueModelFrame);append(view->frame.alphaModelFrames[0]);
  }
  return out;
 }
 bool Matches(const std::shared_ptr<const NativeWaterSceneFrame>& water)const{
  if(!SameOwner(owner,water))return false;
  if(!water)return views==decltype(views){}&&models.empty()&&sources.empty();
  if(water->sourceDrawFrame!=frame||!SameOwner(views[0],water->refraction)||!SameOwner(views[1],water->reflection))return false;
  size_t modelIndex=0,sourceIndex=0;
  const auto source=[&](const auto& value){return (!value||value->sourceDrawFrame==frame)&&sourceIndex<sources.size()&&SameOwner(sources[sourceIndex++],value);};
  const auto model=[&](const auto& value){
   if(modelIndex>=models.size()||!SameOwner(models[modelIndex++],value))return false;
   if(!value)return true;if(value->sourceDrawFrame!=frame)return false;
   for(const auto& pass:value->passes){if(!source(pass.lua))return false;if(pass.mrt&&!source(pass.mrt->lua))return false;}return true;
  };
  for(const auto& view:views)if(view){if(view->frame.sourceDrawFrame!=frame||!model(view->frame.opaqueModelFrame))return false;if(!model(view->frame.alphaModelFrames[0]))return false;}
  return modelIndex==models.size()&&sourceIndex==sources.size();
 }
};
struct CPUReadReceipt220 {
 uint64_t frame=0,ordinal=0,startupNs=0,mainBeginNs=0,mainEndNs=0;
 std::shared_ptr<const NativeOpaqueModelFrame> models; // Main-only identity pin.
 NativeCUSCPUReadWorker220::Job job;
 bool includesTail=false;
 std::optional<CPUReadWaterRoster220> waterReads;
 std::array<std::shared_ptr<const NativeOpaqueModelFrame>,3> alphaModels;
 std::shared_ptr<const NativeOriginalShadowFrame> shadow;
 NativeCUSCPUReadWorker220::Join pending; // Destroy first, before job/source owners. Main-only.
 template<class A,class B>static bool SameOwner(const A& a,const B& b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
 bool Matches(const WorldFrame& captured)const{
  if(frame!=captured.sourceDrawFrame||!SameOwner(models,captured.opaqueModelFrame))return false;
  if(models&&models->sourceDrawFrame!=frame)return false;
  size_t index=0;
  const auto next=[&](const auto& source){return (!source||source->sourceDrawFrame==frame)&&index<job.sources.size()&&SameOwner(job.sources[index++],source);};
  if(models)for(const auto& pass:models->passes){if(!next(pass.lua))return false;if(pass.mrt&&!next(pass.mrt->lua))return false;}
  if(index!=job.sources.size())return false;
  if(!includesTail)return true;
  if(job.tailState!=NativeCUSCPUReadWorker220::Job::TailState::Published)return false;
  for(size_t i=0;i<alphaModels.size();++i)if(!SameOwner(alphaModels[i],captured.alphaModelFrames[i]))return false;
  if(!SameOwner(shadow,captured.originalShadow))return false;
  size_t tailIndex=0;
  const auto sameTail=[&](const auto& source){return (!source||source->sourceDrawFrame==frame)&&tailIndex<job.tailSources.size()&&SameOwner(job.tailSources[tailIndex++],source);};
  for(const auto& models:alphaModels)if(models){if(models->sourceDrawFrame!=frame)return false;for(const auto& pass:models->passes){if(!sameTail(pass.lua))return false;if(pass.mrt&&!sameTail(pass.mrt->lua))return false;}}
  if(shadow&&(shadow->sourceDrawFrame!=frame||!sameTail(shadow->cus)))return false;
  if(tailIndex!=job.tailSources.size())return false;
  if(job.optionalFailed)return false;
  if(waterReads){
   if(!waterReads->Matches(captured.waterScene)||waterReads->sources.size()!=job.optionalSources.size())return false;
   for(size_t i=0;i<job.optionalSources.size();++i)if(!SameOwner(waterReads->sources[i],job.optionalSources[i]))return false;
  }else if(!job.optionalSources.empty())return false;
  return true;

 }
};
void TraceCPUReadPacket220(uint64_t frame,uint64_t simulationFrame,const CPUReadReceipt220* receipt,bool accepted,bool verified,int fallback,size_t count,size_t bytes,uint64_t consumeNs,uint64_t consumeBeginNs,uint64_t digest){
 if(!CPUReadPacketMeasure220())return;
 struct Output {
  FILE* file=std::fopen(std::getenv("BAR_NATIVE_CPU_READ_PACKET220_FILE"),"w");
  size_t rows=0;
  Output(){if(file)std::fputs("frame,simulation_frame,mode,job,accepted,verified,fallback,sources,entries,bytes,cache_before,cache_after,startup_ns,queue_ns,work_ns,join_ns,consume_ns,row_digest,submit_ns,main_begin_ns,main_end_ns,consume_begin_ns,tail_wait_ns,optional_sources,optional_failed,worker_begin_ns,worker_end_ns,worker_thread_cpu_valid,worker_thread_cpu_ns\n",file);}
  ~Output(){if(file)std::fclose(file);}
 };
 static Output output;
 if(!output.file||output.rows++>=10000)return;
 const auto* job=receipt?&receipt->job:nullptr;
 std::fprintf(output.file,"%llu,%llu,%d,%llu,%d,%d,%d,%zu,%zu,%zu,%zu,%zu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%zu,%d,%llu,%llu,%d,%llu\n",
  (unsigned long long)frame,(unsigned long long)simulationFrame,CPUReadPacketMode220(),(unsigned long long)(receipt?receipt->ordinal:0),int(accepted),int(verified),fallback,
  job?job->sources.size()+job->tailSources.size()+job->optionalSources.size():0,count,bytes,job?job->cacheBefore:0,job?job->cacheAfter:0,
  (unsigned long long)(receipt?receipt->startupNs:0),(unsigned long long)(job?job->queueNs:0),(unsigned long long)(job?job->workNs:0),
  (unsigned long long)(job?job->waitNs:0),(unsigned long long)consumeNs,(unsigned long long)digest,
  (unsigned long long)(job?NativeCUSCPUReadWorker220::Timestamp(job->submitted):0),(unsigned long long)(receipt?receipt->mainBeginNs:0),
  (unsigned long long)(receipt?receipt->mainEndNs:0),(unsigned long long)consumeBeginNs,(unsigned long long)(job?job->tailWaitNs:0),job?job->optionalSources.size():0,int(job&&job->optionalFailed),
  (unsigned long long)(job?job->workerBeginNs:0),(unsigned long long)(job?job->workerEndNs:0),int(job&&job->threadCPUValid),(unsigned long long)(job?job->threadCPUNs:0));
}

bool PrepareReadyPilotRequested() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_PREPARE_READY_PILOT");return value&&std::string_view(value)=="1";}();
 return enabled;
}
struct ScenePendingAdmission;
struct Session {
 // N-1 may remain GPU-held while N prepares and the bounded N+1 prefix captures.
 NativeSourceFrameCredits sourceCredits{PrepareReadyPilotRequested()?3u:2u};
 std::unique_ptr<NativeCUSCPUReadWorker220> cpuReadWorker220;
 std::unique_ptr<CPUReadReceipt220> cpuReadReceipt220;
 uint64_t cpuReadJobs220=0;
 bool cpuReadDisabled220=false;
 int cpuReadFallback220=0; // 1 setup failed before publication; 2 disabled after failure.
 std::shared_ptr<NativeFrameCompletionMailbox> completionMailbox;
 std::shared_ptr<const NativeFrameBeginReceipt> lastFailedBeginReceipt;
	explicit Session(SDL_Window* window, uint32_t width, uint32_t height)
		: requestedMSAA(globalRendering->msaaLevel)
		, surface(AttachMetal4RendererSurface(window, MakeConfig(width, height)))
		, renderer(std::make_unique<Metal4WorldRenderer>(surface,requestedMSAA))
		, loadingScreen(std::make_unique<Metal4ScreenPresenter>(surface,renderer->ActualMainSampling().samples)) {
		RememberLogicalExtent();
		auto& context=OriginalNativeTextureContext();
		NativeGeometryTextureServices services;
		services.constructible=[](NativeGeometryTextureRole){return true;};
		services.multisampleSupported=[](NativeGeometryTextureRole){return false;};
		services.checkStatus=[this](const NativeGeometryTextureRequest& request,NativeTextureRegistry& registry){return renderer->CheckShieldGeometryAllocation(request,registry,globalRendering->drawFrame);};
		context.Initialize([]{return uint64_t(globalRendering->drawFrame);},std::move(services));
		nativeContextOwned=true;
		try {
  completionMailbox=std::make_shared<NativeFrameCompletionMailbox>(context.Registry().ContextGeneration());
		OriginalNativeModelContext().Initialize(context.Registry().ContextGeneration());
  OriginalNativeLegacyWaterProgramState().InitializeContext(context.Registry().ContextGeneration());
	OriginalNativeLegacyWaterTextureMatrices().InitializeContext(context.Registry().ContextGeneration());
	OriginalNativeLegacyWaterTextureEnvironment().InitializeContext(context.Registry().ContextGeneration());
	OriginalNativeLegacyWaterTexGen().InitializeContext(context.Registry().ContextGeneration());
	OriginalNativeLegacyWaterLighting().InitializeContext(context.Registry().ContextGeneration());
		NativeTerrainProgramServices terrainPrograms;
		terrainPrograms.contextGeneration=[]{return OriginalNativeTextureContext().Registry().ContextGeneration();};
		terrainPrograms.compile=[this](const NativeTerrainProgramRequest& request){return renderer->CompileOriginalTerrainProgram(request);};
		ConfigureNativeTerrainProgramSource(std::move(terrainPrograms));
		terrainProgramSourceAttached=true;
		context.AttachUI(ui::GetRecorder());
		OriginalNativeMapTextures().Attach(context.Registry(),context.History());
		mapTextureSourceAttached=true;
		NativeTerrainImagePublicationServices terrainImages;
		terrainImages.resolveActualImage=[this](const std::string& path,bool reload){return capture.CaptureTextureSource(path,reload);};
		terrainImages.captureShading=[this](const void* actualMap,uint64_t revision){
			if(!actualMap)throw std::runtime_error("Original SMF shading writer has no actual read-map owner");
			return capture.CaptureTerrainShadingSource(*static_cast<const CSMFReadMap*>(actualMap),revision);
		};
		terrainImages.resolveBorrowed=[this](const void* owner,NativeTerrainImageRole role,uint32_t provenance){
			return ResolveBorrowedTerrainImage(owner,role,provenance);
		};
		terrainImages.publishBorrowedSquare=[](const NativeTerrainBorrowedSquareWrite& write){
			OriginalNativeTextureContext().UI().PublishBorrowedSquare(write);
		};
		OriginalNativeTerrainImagePublication().Attach(context.Registry(),context.History(),ui::GetRecorder(),std::move(terrainImages));
		terrainImageSourceAttached=true;
		shieldTextures={&context.History(),&context.Registry(),&context.Geometry(NativeGeometryTextureRole::Map),&context.Geometry(NativeGeometryTextureRole::Models),[](unsigned unit){ui::GetRecorder().SyncObservedTextureBinding(unit);}};
		LuaNativeShield::SetTextureContext(&shieldTextures);renderer->ConnectShieldTextureContext(true);
		renderer->ConnectPBRSourceContext(true);
		pbrTextureSourceAttached=true;
  OriginalNativeS3OTextures().Attach(context.Registry(),context.History(),
   [this](const std::string& path,bool reload){return capture.CaptureTextureSource(path,reload);},
   []{return uint64_t(globalRendering->drawFrame);},[](unsigned unit){ui::GetRecorder().SyncObservedTextureBinding(unit);});
  s3oTextureSourceAttached=true;
  OriginalNativeNamedVolumes().Attach(context.Registry(),context.History(),[]{return uint64_t(globalRendering->drawFrame);});
  namedVolumeSourceAttached=true;
		LuaNativeStartBox::SetMiniMapSink([](ui::NativeUIRecorder& recorder,NativeStartBoxDraw draw){recorder.AppendStartBox(std::move(draw));},true);
		cusModelSource=std::make_unique<NativeCUSSourceCapture>(capture,context,ui::GetRecorder());
		waterGroundSource=std::make_unique<NativeWaterGroundSourceCapture>(context.Registry(),context.History(),
			ui::GetRecorder(),OriginalNativeSourceProgramBinding());
		opaqueModelCapture=std::make_unique<NativeOpaqueModelCapture>(capture,*cusModelSource,context,ui::GetRecorder());
  legacyModelSource=std::make_unique<NativeLegacyMaterialSource>(capture,context,ui::GetRecorder(),OriginalNativeModelContext().clips);
  shadowModelSource=std::make_unique<NativeEngineShadowCapture>(capture,context,ui::GetRecorder(),OriginalNativeModelContext().clips,originalShadowPrograms);
  shadowParticlesSource=std::make_unique<NativeOriginalShadowParticlesCapture>(context,ui::GetRecorder(),OriginalNativeModelContext().clips);
  genericShadowSource=std::make_unique<NativeGenericShadowSource>(context,ui::GetRecorder(),OriginalNativeModelContext().clips);
  ui::GetRecorder().SetShadowVertexSource([this](std::span<const NativeGenericShadowVertex> vertices,const ui::NativeUIMatrix& modelView,const ui::NativeUIMatrix& projection,bool zeroToOne){
   return genericShadowSource->Snapshot(vertices,modelView,projection,zeroToOne);
  });
  genericShadowSourceAttached=true;
  originalShadowCapture=std::make_unique<NativeOriginalShadowCapture>(*shadowModelSource,originalShadowPrograms,*cusModelSource,*shadowParticlesSource,context,ui::GetRecorder());
  renderer->SetOriginalShadowCapture([this](std::shared_ptr<const NativeShadowTextureFrame> named){CaptureOriginalShadow(std::move(named));});
  shadowTextureSourceAttached=true;
		// Actual world/screen consumers and the retained height owner are installed in this session.
		LuaNativeWeaponRange::SetMiniMapSink([](ui::NativeUIRecorder& recorder,NativeWeaponRangeDraw draw){recorder.AppendWeaponRange(std::move(draw));},true);
		LuaNativeBuildSquare::SetMiniMapSink([](ui::NativeUIRecorder& recorder,NativeBuildSquareDraw draw){recorder.AppendBuildSquare(std::move(draw));},true);
		LuaNativeUI::SetCoverageTargetAllocator([this](const std::shared_ptr<const ScreenTexture>& color,const std::shared_ptr<const ScreenTexture>& depth){
			return renderer->AllocateCoverageTarget(color,depth);
		});
		LuaNativeWeaponRange::SetCoverageMaskConsumerReady(true);
		LuaNativeSensor31657Circle::SetConsumerLifetime(renderer->Sensor31657CircleConsumerLifetime());
		sensorPreviewTargets=std::make_unique<NativeSensorPreviewTargetBridge>(ui::GetRecorder(),LuaNativePostprocess::GetRecorder(),
			[this](const NativeSensorPreviewProgramSource& source){return renderer->CompileOriginalSensorPreviewProgram(source);},
			renderer->SensorPreviewMaximumDimension());
		LuaNativeSensorPreview::SetMinimapSink([](ui::NativeUIRecorder& recorder,std::shared_ptr<const NativeSensorPreviewDraw> draw){recorder.AppendSensorPreview(std::move(draw));});
		LuaNativeSensorPreview::SetServices(sensorPreviewTargets->MakeServices());
		} catch (...) {
			DisconnectNativeSources();
			throw;
		}
		// Publish only after this Session has established all native owners.
		globalRendering->msaaLevel=int(renderer->ActualMainSampling().effective);
	}
	~Session(){if(completionMailbox)completionMailbox->Close();cpuReadReceipt220.reset();cpuReadWorker220.reset();DisconnectNativeSources();}
	void DisconnectNativeSources()
	{
		// The constructor can fail before the Session unique_ptr is published.
		// Clear callbacks while this renderer/context still exist; normal app
		// teardown reaches here after the original Lua and cube source owners.
		LuaNativeStartBox::SetMiniMapSink({},false);
		LuaNativeWeaponRange::SetCoverageMaskConsumerReady(false);
		LuaNativeUI::SetCoverageTargetAllocator({});
		LuaNativeWeaponRange::SetMiniMapSink({},false);
		LuaNativeBuildSquare::SetMiniMapSink({},false);
		LuaNativeSensor31657Circle::SetConsumerLifetime({});
		LuaNativeSensorPreview::SetServices({});
		LuaNativeSensorPreview::SetMinimapSink({});
		sensorPreviewTargets.reset();
		LuaNativeShield::SetTextureContext(nullptr);
  legacyModelSource.reset();opaqueSourcePending.reset();genesisSourcePending.reset();
  actualShadowSourcePending.reset();
  if(shadowTextureSourceAttached){renderer->SetOriginalShadowCapture({});shadowTextureSourceAttached=false;}
  if(genericShadowSourceAttached){ui::GetRecorder().SetShadowVertexSource({});genericShadowSourceAttached=false;}
  genericShadowSource.reset();
  originalShadowCapture.reset();shadowParticlesSource.reset();shadowModelSource.reset();originalShadowPrograms.Reset();
  originalPrograms.Reset();
		opaqueModelCapture.reset();
		waterGroundSource.reset();
		cusModelSource.reset();
		if(terrainProgramSourceAttached){ConfigureNativeTerrainProgramSource({});terrainProgramSourceAttached=false;}
  // These source owners survive game/Lua teardown. Original handler Kill and
  // CNamedTextures::Kill(true) must have released their names before this site.
  if(namedVolumeSourceAttached){OriginalNativeNamedVolumes().Detach();namedVolumeSourceAttached=false;}
  if(s3oTextureSourceAttached){OriginalNativeS3OTextures().Detach();s3oTextureSourceAttached=false;}
		if(terrainImageSourceAttached){OriginalNativeTerrainImagePublication().Detach();terrainImageSourceAttached=false;}
		if(pbrTextureSourceAttached){renderer->ConnectPBRSourceContext(false);pbrTextureSourceAttached=false;}
		if(mapTextureSourceAttached){OriginalNativeMapTextures().Detach();mapTextureSourceAttached=false;}
		if(nativeContextOwned){auto& context=OriginalNativeTextureContext();context.DetachUI();context.Destroy();OriginalNativeModelContext().Reset();OriginalNativeLegacyWaterProgramState().DestroyContext();
	OriginalNativeLegacyWaterTextureMatrices().DestroyContext();
	OriginalNativeLegacyWaterTextureEnvironment().DestroyContext();
	OriginalNativeLegacyWaterTexGen().DestroyContext();
	OriginalNativeLegacyWaterLighting().DestroyContext();nativeContextOwned=false;}
	}
	bool nativeContextOwned=false,mapTextureSourceAttached=false,pbrTextureSourceAttached=false,s3oTextureSourceAttached=false,namedVolumeSourceAttached=false,terrainImageSourceAttached=false,terrainProgramSourceAttached=false;
	std::shared_ptr<const NativeCUSTextureObject> ResolveBorrowedTerrainImage(const void* owner,NativeTerrainImageRole role,uint32_t provenance)
	{
		LuaMatTexture descriptor;
		switch(role){
			case NativeTerrainImageRole::SkyReflection:
				if(owner!=&cubeMapHandler)throw std::runtime_error("Original terrain sky image has a different source owner");
				descriptor.type=LuaMatTexture::LUATEX_SKY_REFLECTION;break;
			case NativeTerrainImageRole::ShadowDepth:
			case NativeTerrainImageRole::ShadowColor:
				if(owner!=&shadowHandler)throw std::runtime_error("Original terrain shadow image has a different source owner");
				descriptor.type=role==NativeTerrainImageRole::ShadowDepth?LuaMatTexture::LUATEX_SHADOWMAP:LuaMatTexture::LUATEX_SHADOWCOLOR;break;
			case NativeTerrainImageRole::Info: {
				auto& publication=OriginalNativeTerrainImagePublication();
				const auto image=publication.InfoImage(owner);
				(void)publication.InfoSource(owner);
				if(!image||image->name!=provenance)throw std::runtime_error("Original terrain info bind differs from its actual event-published owner");
				return image;
			}
			default:
				// Only the genuine registered Lua/UI/named owner can resolve these
				// names. This method does not reinterpret raw GL provenance as a name.
				return OriginalNativeTextureContext().UI().ResolveObservedName(provenance);
		}
		if(descriptor.GetTextureID()!=provenance)throw std::runtime_error("Original terrain bind differs from its current external publisher name");
		const auto actual=LuaNativeCUSTextures::ResolveMaterialTexture(descriptor);
		if(!actual.parsed)throw std::runtime_error("Original terrain external image owner is unobserved");
		if(!actual.object&&provenance)throw std::runtime_error("Original terrain external nonzero image has no retained source object");
		return actual.object; // Null only when the actual source publisher observed object0.
	}
	std::unique_ptr<NativeCUSSourceCapture> cusModelSource;
	std::unique_ptr<NativeWaterGroundSourceCapture> waterGroundSource;
	std::unique_ptr<NativeSensorPreviewTargetBridge> sensorPreviewTargets;
	std::unique_ptr<NativeOpaqueModelCapture> opaqueModelCapture;
 std::unique_ptr<NativeLegacyMaterialSource> legacyModelSource;
 NativeEngineModelProgramSource originalPrograms;
 std::shared_ptr<const NativeOriginalShadowFrame> actualShadowSourcePending;
 NativeEngineShadowProgramSource originalShadowPrograms;
 std::unique_ptr<NativeEngineShadowCapture> shadowModelSource;
 std::unique_ptr<NativeOriginalShadowCapture> originalShadowCapture;
 std::unique_ptr<NativeOriginalShadowParticlesCapture> shadowParticlesSource;
 std::unique_ptr<NativeGenericShadowSource> genericShadowSource;
 bool shadowTextureSourceAttached=false,genericShadowSourceAttached=false;
 WorldCamera ShadowDrawingCamera()const{
  const auto* active=CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW);
  if(!active||!shadowHandler.HasNativeTextureOwner())throw std::logic_error("Original ShadowGen has no actual initialized shadow camera/target owner");
  WorldCamera out;std::copy_n(shadowHandler.GetShadowViewMatrixRaw(),16,out.view.begin());
  std::copy_n(shadowHandler.GetShadowProjMatrixRaw(),16,out.projection.begin());
  out.position={active->GetPos().x,active->GetPos().y,active->GetPos().z};
  const auto size=OriginalNativeShadowTextures().Image(shadowHandler.GetShadowTextureID())->request.size;
  out.viewport={0,0,int32_t(size),int32_t(size)};
  const auto control=std::to_array(active->GetClipControlMatrix().m);auto zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
  if(control==ui::IdentityMatrix())out.clipDepth=ClipDepthConvention::MinusOneToOne;
  else if(control==zero)out.clipDepth=ClipDepthConvention::ZeroToOne;
  else throw std::runtime_error("Original shadow drawing clip control has no exact native consumer");
  return out;
 }
 void CaptureOriginalShadow(std::shared_ptr<const NativeShadowTextureFrame> named){
  if(actualShadowSourcePending||!originalShadowCapture||!cusModelSource||!cusModelSource->Ready()||!originalShadowPrograms.Constructed())
   throw std::logic_error("Original pre-IBL shadow source is repeated or its actual owners are absent");
  const auto drawing=ShadowDrawingCamera();auto& recorder=ui::GetRecorder();
  const auto* active=CCameraHandler::GetActiveCamera();
  if(!active||active->GetCamType()!=CCamera::CAMTYPE_SHADOW)throw std::logic_error("Original shadow service lost the actual active shadow camera");
  recorder.BeginWorldStage(NativeWorldDrawStage::ShadowOpaque,drawing.view,drawing.projection,std::to_array(active->GetBillBoardMatrix().m),false,drawing.clipDepth==ClipDepthConvention::ZeroToOne);
  recorder.SetViewport({drawing.viewport[0],drawing.viewport[1],uint32_t(drawing.viewport[2]),uint32_t(drawing.viewport[3])});
  NativeEngineShadowCallbacks callbacks;callbacks.drawUnit=[](const CUnit* unit){return eventHandler.DrawUnit(unit);};
  callbacks.drawFeature=[](const CFeature* feature){return eventHandler.DrawFeature(feature);};
  try{actualShadowSourcePending=originalShadowCapture->Capture(std::move(named),drawing,callbacks,[]{shadowHandler.DrawNativeOriginalPasses();});recorder.EndWorldStage();}
  catch(...){recorder.EndWorldStage();throw;}
 }
 NativeLegacyMaterialServices legacyServices;
 std::shared_ptr<const NativeCUSSourceFrame> genesisSourcePending;
 NativeEngineModelTextureNames CommonModelTextures()const{
  return {shadowHandler.ShadowsLoaded(),shadowHandler.GetShadowTextureID(),shadowHandler.GetColorTextureID(),
   cubeMapHandler.GetEnvReflectionTextureID(),cubeMapHandler.GetSpecularTextureID()};
 }
 NativeEngineModelSourceHooks EngineHooks(){
  NativeEngineModelSourceHooks hooks;hooks.clipState=&OriginalNativeModelContext().clips;
  hooks.requireOriginalProgram=[this]{originalPrograms.RequireSelected(OriginalNativeSourceProgramBinding().EngineOwner());};
  hooks.disableOriginalProgram=[this]{originalPrograms.Disable();};
  hooks.captureProgramEnable=[this](bool modern){
   const bool shadows=shadowHandler.ShadowsLoaded(),deferred=game->GetDrawMode()==CGame::gameDeferredDraw;
   return originalPrograms.Enable(modern,shadows,deferred,
    CaptureNativeEngineModelEnableValues(modern,shadows,OriginalNativeModelContext().fog));
  };
  hooks.drawUnitLua=[](const CUnit* unit){return eventHandler.DrawUnit(unit);};
  hooks.drawFeatureLua=[](const CFeature* feature){return eventHandler.DrawFeature(feature);};
  return hooks;
 }
 bool ModelSourceOwnersConnected()const{
  // Shadows < 0 intentionally creates no target. Zero still requests the
  // original fallback target; failed requested initialization is not ready.
  const bool shadowReady=!shadowHandler.NativeShadowInitializationRequested()||
   (shadowHandler.HasNativeTextureOwner()&&originalShadowPrograms.Constructed());
  return originalPrograms.Constructed()&&shadowReady&&originalShadowCapture&&shadowModelSource&&legacyModelSource&&opaqueModelCapture&&OriginalNativeModelContext().clips.observed&&
   OriginalNativeS3OTextures().Attached()&&OriginalNativeNamedVolumes().Attached()&&
   OriginalNativeCubeTextures().Attached()&&cubeMapHandler.HasNativeTextureOwner()&&
   OriginalNativeShadowTextures().Attached()&&
   legacyServices.resolveTexture&&legacyServices.textureTarget&&legacyServices.uniformFloat&&legacyServices.uniformInt;
 }
	NativeShieldTextureContext shieldTextures;
	std::shared_ptr<const NativeMapTextureFrame> mapTextures;
	std::shared_ptr<const NativeOpaqueModelFrame> opaqueSourcePending;
	bool reportedFirstStartBoxWorld=false,reportedFirstStartBoxMiniMap=false;

	static Metal4SurfaceConfig MakeConfig(uint32_t width, uint32_t height)
	{
		Metal4SurfaceConfig config;
		config.renderWidth = width;
		config.renderHeight = height;
		// The original DOF consumer reads the actual color attachment during
		// ordinary play and actual copy-dependent source work.
		config.framebufferOnly = false;
		return config;
	}

	void RememberLogicalExtent()
	{
		const auto observed = surface.Snapshot();
		if (observed.logicalWidth && observed.logicalHeight)
			lastLogicalExtent = {observed.logicalWidth, observed.logicalHeight};
	}

	const int requestedMSAA;
	std::array<uint32_t, 2> lastLogicalExtent{};
	// Reverse destruction order: capture, renderer, then the borrowed-window
	// attachment. Each successful renderer call already retires its GPU work.
	Metal4WindowSurface surface;
	std::unique_ptr<Metal4WorldRenderer> renderer;
	std::unique_ptr<Metal4ScreenPresenter> loadingScreen;
	WorldFrameCapture capture;
 NativeEnvironmentStageReceipt environmentStage;
 NativeShieldCaptureJournal shieldCapture;
 // Main-owned until joined acceptance/rollback; failed-Begun retains it durably.
 std::shared_ptr<NativeMainSourceSettlement> sourceSettlement;
	NativeBumpWaterCapture waterCapture;
 NativeLegacyWaterCapture legacyWaterCapture;
 NativeDynamicWaterCapture dynamicWaterCapture;
 std::shared_ptr<const NativeDynamicWaterFrame> dynamicCaptureReceipt;
 std::shared_ptr<ScenePendingAdmission> admissionSettlement;
	NativeBasicWaterCapture basicWaterCapture;
	std::optional<WorldFrame> preparedFrame;
	std::optional<uint64_t> preparedDecalDraw;
	std::optional<uint64_t> mainInitialClear;
	std::shared_ptr<const NativeTerrainSceneSourceFrame> mainGroundPending,reflectionGroundPending;
	std::shared_ptr<const NativeGroundDecalFrame> preparedGroundDecals;
	std::shared_ptr<const NativeGameDecalFrame> preparedGameDecals;
	std::unordered_set<std::string> reportedUnsupported;
	uint64_t presentedFrames = 0;
	uint64_t lastPresentedSimulationFrame = 0;
	uint64_t reportedCUSRevision = 0;
	std::optional<bool> reportedAdvancedTerrain;
	std::optional<bool> reportedReflectionReady;
	std::optional<std::array<uint64_t,2>> reportedSourcePaths;
	uint64_t lastSSAOImageIdentity = 0;
	uint64_t targetOnlySubmissions = 0;
	uint64_t targetOnlyPasses = 0;
	bool awaitingPresentationAfterTargets = false;
	bool reportedFirstUnits = false;
	bool reportedFirstFeatures = false;
	bool reportedFirstParticles = false;
	bool reportedFirstLuaNano = false;
	bool reportedFirstWorldOverlays = false;
	bool reportedFirstProjectileEffects = false;
	bool reportedFirstEnergyExplosion = false;
	bool reportedFirstLightning = false;
	bool reportedFirstDeferredLights = false;
	bool reportedFirstFireSmoke = false;
	bool reportedFirstMissileThruster = false;
	bool reportedFirstFlamethrower = false;
	bool reportedFirstNapalmPreview = false;
	bool reportedFirstSnow = false;
	bool reportedFirstCommands = false;
	bool reportedFirstGameDecal = false;
	std::array<bool,3> reportedFirstUnitPrimitive{};
	bool reportedFirstShield=false;
	bool reportedFirstBlueprint = false;
	bool reportedFirstBuildingSample = false;
	bool reportedFirstBuildingGrid = false;
	bool reportedFirstPiP = false;
	bool reportedFirstUI = false;
	bool reportedFirstPostprocess = false;
	bool reportedLoadingScreen = false;
};

std::unique_ptr<Session> session;
struct CloudAdmissionOwner {
 void ValidateSubmittedCapacity(std::uint64_t id) const {LuaNativeVolumetricClouds::ValidateSubmittedCapacity(id);}
 void TransferPreparedFrame(std::uint64_t id) noexcept {LuaNativeVolumetricClouds::TransferPreparedFrame(id);}
 void ValidateRestoreHeldFrame(std::uint64_t id) const {LuaNativeVolumetricClouds::ValidateRestoreHeldFrame(id);}
 void RestoreHeldFrame(std::uint64_t id) noexcept {LuaNativeVolumetricClouds::RestoreHeldFrame(id);}
 void QuarantineHeldFrame(std::uint64_t id) noexcept {LuaNativeVolumetricClouds::QuarantineHeldFrame(id);}
};

struct DynamicWaterAdmissionOwner {
 NativeDynamicWaterCapture& capture;std::shared_ptr<const NativeDynamicWaterFrame> frame;
 void ValidateSubmittedCapacity(uint64_t draw) const {if(frame){if(frame->sourceDrawFrame!=draw)throw std::logic_error("Water admission draw mismatch");capture.ValidateSubmittedCapacity(frame);}}
 void TransferPreparedFrame(uint64_t draw) noexcept {if(frame)capture.TransferPreparedFrame(draw);}
 void ValidateRestoreHeldFrame(uint64_t draw) const {if(frame)capture.ValidateRestoreHeldFrame(draw);}
 void RestoreHeldFrame(uint64_t draw) noexcept {if(frame)capture.RestoreHeldFrame(draw);}
 void QuarantineHeldFrame(uint64_t draw) noexcept {if(frame)capture.QuarantineHeldFrame(draw);}
};

DrawResult ReportPresentedWorldResult(Session& owner,const WorldFrame& frame,const Metal4WorldPresentationResult& result,const ScreenFrameView* screenUI)
{
	if (owner.reportedCUSRevision != frame.cusMaterial.revision ||
		!owner.reportedAdvancedTerrain || *owner.reportedAdvancedTerrain != frame.terrain.material->advancedShading) {
		LOG("[Metal4Scene] Presented original shading controls at frame %llu: terrain=%s, models=%s, CUS revision=%llu",
			static_cast<unsigned long long>(frame.simulationFrame),
			frame.terrain.material->advancedShading ? "advanced" : "standard",
			frame.cusMaterial.enabled ? "CUS" : "engine-standard-GL4",
			static_cast<unsigned long long>(frame.cusMaterial.revision));
		owner.reportedCUSRevision = frame.cusMaterial.revision;
		owner.reportedAdvancedTerrain = frame.terrain.material->advancedShading;
	}
	if (!owner.reportedReflectionReady || *owner.reportedReflectionReady != result.reflectionReady) {
		LOG("[Metal4Scene] Original environment cube state at frame %llu: %s; full visual parity pending",
			static_cast<unsigned long long>(frame.simulationFrame), result.reflectionReady ?
			"first complete face/mip cycle available" : "first complete face/mip cycle warming up");
		owner.reportedReflectionReady = result.reflectionReady;
	}
	if (result.sourceFrameCompleted) {
		uint64_t ssaoDrawCommands = 0;
		const auto countSSAO = [&](const std::shared_ptr<const NativeWaterLuaFrame>& bundle) {
			if (!bundle || !bundle->ssao) return;
			for (const auto& draw: bundle->ssao->draws) {
				if (draw.clear) continue; // An FBO clear is not an SSAO draw command.
				++ssaoDrawCommands;
				if (draw.output && draw.output->image)
					owner.lastSSAOImageIdentity = std::max(owner.lastSSAOImageIdentity,draw.output->image->identity);
			}
		};
		countSSAO(frame.alphaAllLua);
		if (frame.waterScene) countSSAO(frame.waterScene->aboveWaterLua);
		for (const auto& models: frame.alphaModelFrames) if (models)
			for (const auto& phase: models->passes) countSSAO(phase.luaEffects);
		const bool mapMRT = frame.mainGround && frame.mainGround->ground && frame.mainGround->ground->mapMRT;
		const bool modelMRT = frame.opaqueModelFrame && frame.opaqueModelFrame->HasWriters();
		// Log only settings transitions or a newly created SSAO image owner.
		// Do not key reporting on camera-dependent draw counts or MRT reuse.
		const uint64_t settings = uint64_t(frame.lighting.requested.deferredMap) |
			(uint64_t(frame.lighting.requested.deferredModel) << 1) |
			(uint64_t(frame.terrain.material->advancedShading) << 2) | (uint64_t(frame.cusMaterial.enabled) << 3);
		const std::array<uint64_t,2> paths{{settings,owner.lastSSAOImageIdentity}};
		if (!owner.reportedSourcePaths || *owner.reportedSourcePaths != paths) {
			LOG("[Metal4Scene] Completed frame with original source paths at frame %llu: mapMRT=%u modelMRT=%u capturedSSAODrawCommands=%llu; this frame only, full visual parity pending",
				static_cast<unsigned long long>(frame.simulationFrame), unsigned(mapMRT), unsigned(modelMRT), static_cast<unsigned long long>(ssaoDrawCommands));
			owner.reportedSourcePaths = paths;
		}
	}
	for (const auto& dependency: result.materialDependenciesPending) {
		if (owner.reportedUnsupported.insert("material: " + dependency).second)
			LOG_L(L_WARNING, "[Metal4Scene] Pending original material dependency: %s", dependency.c_str());
	}
	owner.lastPresentedSimulationFrame = frame.simulationFrame;
	if (owner.awaitingPresentationAfterTargets) {
		LOG("[Metal4Scene] Resumed world presentation at simulation frame %llu; completed offscreen UI totals: %llu submissions, %llu target passes",
			static_cast<unsigned long long>(frame.simulationFrame),
			static_cast<unsigned long long>(owner.targetOnlySubmissions),
			static_cast<unsigned long long>(owner.targetOnlyPasses));
		owner.awaitingPresentationAfterTargets = false;
	}

	if (owner.presentedFrames++ == 0) {
		LOG("[Metal4Scene] First presented simulation frame %llu: map %s, %u original terrain tiles, %u units, %u pieces; logical %ux%u, render %ux%u",
			static_cast<unsigned long long>(frame.simulationFrame), frame.terrain.sourceMap.c_str(),
			result.terrainTiles, result.unitsDrawn, result.piecesDrawn,
			result.surface.logicalWidth, result.surface.logicalHeight, result.surface.renderWidth, result.surface.renderHeight);
		LOG("[Metal4Scene] Native GPU objects: queue=%s buffer=%s encoder=%s",
			result.queueClass.c_str(), result.commandBufferClass.c_str(), result.renderEncoderClass.c_str());
		LOG("[Metal4Scene] First-frame resident allocation bytes=%llu, allocations=%u",
			static_cast<unsigned long long>(result.residentBytes), result.residencyAllocationCount);
		LOG("[Metal4Scene] First-frame original map-edge submission: draws=%u, mirror/seam instances=%u",
			result.mapEdgeDraws, result.mapEdgeInstances);
	}
	if(!owner.reportedFirstStartBoxWorld && result.luaStartBoxDraws){owner.reportedFirstStartBoxWorld=true;LOG("[Metal4Scene] First completed original StartBox world at simulation frame %llu: draws=%u; strict pixel parity pending",static_cast<unsigned long long>(frame.simulationFrame),result.luaStartBoxDraws);}
	if(!owner.reportedFirstStartBoxMiniMap && result.luaStartBoxMiniMapDraws){owner.reportedFirstStartBoxMiniMap=true;LOG("[Metal4Scene] First completed original StartBox minimap/PiP at simulation frame %llu: draws=%u; strict pixel parity pending",static_cast<unsigned long long>(frame.simulationFrame),result.luaStartBoxMiniMapDraws);}
	if (!owner.reportedFirstUnits && result.unitsDrawn > 0) {
		owner.reportedFirstUnits = true;
		LOG("[Metal4Scene] First presented units at simulation frame %llu: units=%u, pieces=%u, selected=%u; actual camera=(%.3f, %.3f, %.3f)",
			static_cast<unsigned long long>(frame.simulationFrame), result.unitsDrawn, result.piecesDrawn, result.selectedUnits,
			frame.viewCamera.position[0], frame.viewCamera.position[1], frame.viewCamera.position[2]);
	}
	if (!owner.reportedFirstFeatures && result.featuresDrawn > 0) {
		owner.reportedFirstFeatures = true;
		LOG("[Metal4Scene] First presented features at simulation frame %llu: features=%u, pieces=%u, opaque draws=%u, alpha draws=%u; shadow terrain/model draws=%u/%u",
			static_cast<unsigned long long>(frame.simulationFrame), result.featuresDrawn, result.featurePiecesDrawn,
			result.opaqueFeatureDraws, result.alphaFeatureDraws, result.shadowTerrainDraws, result.shadowModelDraws);
	}
	if (!owner.reportedFirstParticles && (result.particleDraws > 0 || result.projectileModelsDrawn > 0)) {
		owner.reportedFirstParticles = true;
		LOG("[Metal4Scene] First presented original engine particle stream at simulation frame %llu: draws=%u, projectile models=%u, engine nano instances=%u",
			static_cast<unsigned long long>(frame.simulationFrame), result.particleDraws,
			result.projectileModelsDrawn, result.engineNanoInstances);
	}
	if (!owner.reportedFirstLuaNano && result.luaNanoDraws > 0 && result.luaNanoInstances > 0) {
		owner.reportedFirstLuaNano = true;
		LOG("[Metal4Scene] First presented original Lua nano stream at simulation frame %llu: draws=%u, submitted instances across original passes=%u",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaNanoDraws, result.luaNanoInstances);
	}
	if (!owner.reportedFirstPostprocess && screenUI && screenUI->postprocess && !screenUI->postprocess->operations.empty()) {
		std::size_t colorCopies = 0, depthCopies = 0, dofPasses = 0;
		for (const auto& operation: screenUI->postprocess->operations) {
			colorCopies += operation.kind == NativePostprocessOperationKind::CopyColor;
			depthCopies += operation.kind == NativePostprocessOperationKind::CopyDepth;
			dofPasses += operation.kind == NativePostprocessOperationKind::DOF;
		}
		owner.reportedFirstPostprocess = true;
		LOG("[Metal4Scene] First completed original DOF at simulation frame %llu: color copies=%zu, depth copies=%zu, shader passes=%zu; actual world framebuffer, original screen order, one present",
			static_cast<unsigned long long>(frame.simulationFrame), colorCopies, depthCopies, dofPasses);
	}
	if (!owner.reportedFirstUI && result.screenDraws > 0) {
		LOG("[Metal4Scene] Original environment submission: sky=%u, reflection terrain=%u, face=%u, complete cycle=%d, unit BRDF=%u G_OPTION=%u",
			result.skyDraws, result.reflectionTerrainDraws, result.reflectionFace, result.reflectionReady, result.unitBRDFWidth, result.unitBRDFGOption);
		owner.reportedFirstUI = true;
		LOG("[Metal4Scene] First composited native UI at simulation frame %llu: draws=%u, texture uploads=%u; shared world drawable and one present",
			static_cast<unsigned long long>(frame.simulationFrame), result.screenDraws, result.screenTextureUploads);
	}
	if (!owner.reportedFirstWorldOverlays && (result.luaPreUnitDraws>0 || result.luaWorldDraws>0)) {
		owner.reportedFirstWorldOverlays = true;
		LOG("[Metal4Scene] First original Lua world geometry at simulation frame %llu: PreUnit=%u, World=%u; original call order and shared scene depth/stencil",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaPreUnitDraws, result.luaWorldDraws);
	}
	if (!owner.reportedFirstEnergyExplosion && result.luaEnergyExplosionDraws > 0) {
		owner.reportedFirstEnergyExplosion = true;
		LOG("[Metal4Scene] First completed original energy explosion at simulation frame %llu: draws=%u, instances=%u; original VS motion, GS-equivalent triangles and shared world order",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaEnergyExplosionDraws, result.luaEnergyExplosionInstances);
	}
	if (!owner.reportedFirstGameDecal && result.luaGameDecalDraws>0) {
		owner.reportedFirstGameDecal=true;
		LOG("[Metal4Scene] First completed original Lua game Decals GL4 at simulation frame %llu: draws=%u, instances=%u; strict pixel parity pending",static_cast<unsigned long long>(frame.simulationFrame),result.luaGameDecalDraws,result.luaGameDecalInstances);
	}
	for (size_t i=0;i<3;++i) if (!owner.reportedFirstUnitPrimitive[i] && result.luaUnitPrimitiveDraws[i]>0) {
		owner.reportedFirstUnitPrimitive[i]=true;
		constexpr std::array names{"BuildingAO","FeatureAO","TeamPlatter"};
		LOG("[Metal4Scene] First completed original %s at simulation frame %llu: draws=%u, instances=%u; strict pixel parity pending",names[i],static_cast<unsigned long long>(frame.simulationFrame),result.luaUnitPrimitiveDraws[i],result.luaUnitPrimitiveInstances[i]);
	}
	if (!owner.reportedFirstPiP && result.luaPiPDraws > 0) {
		owner.reportedFirstPiP = true;
		LOG("[Metal4Scene] First completed original PiP programs: draws=%u; original screen/RTT order, shared images and private NoGS family; full visual parity pending", result.luaPiPDraws);
	}
	if (!owner.reportedFirstBuildingSample && result.buildingSampleDraws > 0) {
		owner.reportedFirstBuildingSample = true;
		LOG("[Metal4Scene] First completed original pre-click building sample at simulation frame %llu: samples=%u, pieces=%u; original engine placement producer and legacy material; strict visual parity pending", static_cast<unsigned long long>(frame.simulationFrame), result.buildingSampleDraws, result.buildingSamplePieces);
	}
	if (!owner.reportedFirstBuildingGrid && result.buildingGridDraws > 0) {
		owner.reportedFirstBuildingGrid = true;
		LOG("[Metal4Scene] First completed original optional Building Grid at simulation frame %llu: draws=%u; original height/mouse/water behavior; strict line parity pending", static_cast<unsigned long long>(frame.simulationFrame), result.buildingGridDraws);
	}
	if (!owner.reportedFirstBlueprint && result.luaBlueprintDraws > 0) {
		owner.reportedFirstBlueprint = true;
		LOG("[Metal4Scene] First completed original Blueprint family: draws=%u, models=%u, outlines=%u, stencil clears=%u; strict pixel parity pending",result.luaBlueprintDraws,result.luaBlueprintModels,result.luaBlueprintOutlines,result.luaBlueprintStencilClears);
	}
	if (!owner.reportedFirstFireSmoke && result.luaFireSmokeDraws > 0) {
		owner.reportedFirstFireSmoke = true;
		LOG("[Metal4Scene] First completed original FireSmoke at simulation frame %llu: draws=%u, submitted instances=%u (normal/highlight counted separately); full visual parity pending",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaFireSmokeDraws, result.luaFireSmokeSubmittedInstances);
	}
	if(!owner.reportedFirstShield && result.luaShieldDraws){owner.reportedFirstShield=true;
		LOG("[Metal4Scene] First completed original Shield: draws=%u, actual map/model GBuffer passes=%u/%u model pieces=%u; corrected-reference strict color residual remains pending",result.luaShieldDraws,result.mapGBufferDepthPasses,result.modelGBufferDepthPasses,result.modelGBufferDepthPieces);}
	if (!owner.reportedFirstCommands && result.luaCommandsDraws > 0) {
		owner.reportedFirstCommands=true;
		LOG("[Metal4Scene] First completed original Commands FX at simulation frame %llu: PreUnit line draws=%u; full visual parity pending",static_cast<unsigned long long>(frame.simulationFrame),result.luaCommandsDraws);
	}
	if (!owner.reportedFirstSnow && result.luaSnowDraws > 0) {
		owner.reportedFirstSnow=true;
		LOG("[Metal4Scene] First completed original Snow: World point draws=%u; strict snow-map/command pixel parity pending",result.luaSnowDraws);
	}
	if (!owner.reportedFirstNapalmPreview && result.luaNapalmPreviewDraws > 0) {
		owner.reportedFirstNapalmPreview=true;
		LOG("[Metal4Scene] First completed original AoE Napalm preview: PreUnit draws=%u; full visual parity pending",result.luaNapalmPreviewDraws);
	}
	if (!owner.reportedFirstFlamethrower && result.luaFlamethrowerDraws > 0) {
		owner.reportedFirstFlamethrower = true;
		LOG("[Metal4Scene] First completed original Flamethrower at simulation frame %llu: draws=%u submitted instances=%u; full visual parity pending",
			static_cast<unsigned long long>(frame.simulationFrame),result.luaFlamethrowerDraws,result.luaFlamethrowerSubmittedInstances);
	}
	if (!owner.reportedFirstMissileThruster && result.luaMissileThrusterDraws > 0) {
		owner.reportedFirstMissileThruster = true;
		LOG("[Metal4Scene] First completed original MissileThruster at simulation frame %llu: draws=%u, submitted instances=%u, programs=%u/%u/%u/%u/%u/%u/%u (legacy and31657 variants counted separately); full visual parity pending",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaMissileThrusterDraws, result.luaMissileThrusterSubmittedInstances,
			result.luaMissileThrusterProgramDraws[0], result.luaMissileThrusterProgramDraws[1],
			result.luaMissileThrusterProgramDraws[2], result.luaMissileThrusterProgramDraws[3],
			result.luaMissileThrusterProgramDraws[4], result.luaMissileThrusterProgramDraws[5], result.luaMissileThrusterProgramDraws[6]);
	}
	if (!owner.reportedFirstDeferredLights && result.luaDeferredLightDraws > 0) {
		owner.reportedFirstDeferredLights = true;
		LOG("[Metal4Scene] First completed original Deferred rendering GL4: frame=%llu draws=%u submittedInstances=%u; pixel parity pending",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaDeferredLightDraws, result.luaDeferredLightInstances);
	}
	if (!owner.reportedFirstLightning && result.luaLightningDraws > 0) {
		owner.reportedFirstLightning = true;
		LOG("[Metal4Scene] First completed original Lightning family at simulation frame %llu: draws=%u, instances=%u; strict bolt pixel parity pending",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaLightningDraws, result.luaLightningInstances);
	}
	if (!owner.reportedFirstProjectileEffects && result.luaProjectileEffectDraws>0) {
		owner.reportedFirstProjectileEffects = true;
		LOG("[Metal4Scene] First original Lua Beam/Plasma submission at simulation frame %llu: draws=%u, instances across passes=%u; original order with Nano and world geometry",
			static_cast<unsigned long long>(frame.simulationFrame), result.luaProjectileEffectDraws, result.luaProjectileEffectInstances);
	}
	return {true,result.screenTargetsCompleted,result.sourceFrameCompleted,result.completedPostprocessSerial,result.sourceDrawFrame};
}

// Stable main-owned admission record. Adapters and frame identities outlive
// DrawWorld's stack; no late settlement borrows its local options or helpers.
struct ScenePendingAdmission {
 Session* const owner;
 const uint64_t context,epoch;
 const std::shared_ptr<const Metal4WorldSubmission> submission;
 const std::shared_ptr<NativeFrameCompletionMailbox> mailbox;
 const std::shared_ptr<NativeFrameCompletionMailbox::Ticket> ticket;
 const std::shared_ptr<NativeMainSourceSettlement> source;
 CloudAdmissionOwner cloud;
 DynamicWaterAdmissionOwner water;
 bool transferred=false,failureHandled=false,cpuAccepted=false,queuedReturn=false;
 ScenePendingAdmission(Session& owner,std::shared_ptr<const Metal4WorldSubmission> packet,
  std::shared_ptr<NativeFrameCompletionMailbox::Ticket> ticket):owner(&owner),
  context(OriginalNativeTextureContext().Registry().ContextGeneration()),epoch(owner.completionMailbox->Epoch()),
  submission(std::move(packet)),mailbox(owner.completionMailbox),ticket(std::move(ticket)),
  source(std::make_shared<NativeMainSourceSettlement>()),water{owner.dynamicWaterCapture,submission->Frame().dynamicWater} {}
 ScenePendingAdmission(const ScenePendingAdmission&)=delete;
 ScenePendingAdmission& operator=(const ScenePendingAdmission&)=delete;
 void ValidateOwner()const{
  if(session.get()!=owner||owner->admissionSettlement.get()!=this||owner->completionMailbox!=mailbox||mailbox->Epoch()!=epoch||
   OriginalNativeTextureContext().Registry().ContextGeneration()!=context)
   throw std::logic_error("Scene admission lost its exact context/mailbox owner");
  if(transferred&&owner->sourceSettlement!=source)throw std::logic_error("Scene admission source owner was replaced");
 }
 auto Transaction(){return NativeFrameAdmissionTransaction(ui::GetRecorder(),LuaNativePostprocess::GetRecorder(),cloud,water,
  submission->Frame().sourceDrawFrame,submission->Options().screenUI->postprocess->serial);}
 void Transfer(){
  ValidateOwner();
  const auto& credit=submission->Options().sourceCredit;
  owner->sourceCredits.Validate(credit,submission->Frame().sourceDrawFrame);if(transferred)throw std::logic_error("Scene admission transferred twice");
  auto admission=Transaction();admission.Validate();
  owner->sourceCredits.Hold(credit);
  try{source->Take(context,submission->Frame().sourceDrawFrame,
   owner->opaqueSourcePending,owner->actualShadowSourcePending,owner->mainGroundPending,
   owner->dynamicCaptureReceipt,owner->environmentStage,owner->shieldCapture);}
  catch(...){owner->sourceCredits.ReleaseHeld(submission->Frame().sourceDrawFrame);throw;}
  owner->sourceSettlement=source;
  admission.TransferValidated();transferred=true;
 }
 void Restore(){
  ValidateOwner();if(!transferred)throw std::logic_error("Scene admission has no held frame to restore");
  owner->sourceCredits.ValidateHeld(submission->Frame().sourceDrawFrame);
  source->ValidateRestore(context,owner->opaqueSourcePending,owner->actualShadowSourcePending,owner->mainGroundPending,
   owner->dynamicCaptureReceipt,owner->environmentStage,owner->shieldCapture);
  Transaction().Restore();
  source->RestoreValidated(owner->opaqueSourcePending,owner->actualShadowSourcePending,owner->mainGroundPending,
   owner->dynamicCaptureReceipt,owner->environmentStage,owner->shieldCapture);
  owner->sourceSettlement.reset();owner->sourceCredits.ReleaseHeld(submission->Frame().sourceDrawFrame);transferred=false;
 }
 void Quarantine()noexcept{
  // Invalid owner cannot be repaired by touching the currently active scene.
  try{ValidateOwner();}catch(...){std::terminate();}
  // A failed older early-return frame invalidates dependent younger capture credits.
  // Preserve all source owners; never restore historical state over N+1.
  owner->sourceCredits.QuarantineHeld(submission->Frame().sourceDrawFrame);
  Transaction().Quarantine();
 }
 void Fail(){
  if(failureHandled)return;
  ValidateOwner();const auto& begin=submission->Options().beginReceipt;
  begin->MarkFailed();owner->lastFailedBeginReceipt=begin;
  if(transferred){if(queuedReturn||begin->Submission())Quarantine();else Restore();}
  failureHandled=true;
 }
 void MarkQueued(){
  ValidateOwner();if(!transferred||(!submission->Options().allowPrepareReady&&!submission->Options().beginReceipt->Submission()))throw std::logic_error("Queued source lacks admitted early-return boundary");
  queuedReturn=true;
 }
 void CompleteCPU(const Metal4WorldPresentationResult* result,std::exception_ptr failure){
  if(failure){Fail();if(!transferred)ReleaseAnchor();return;}
  if(!result)throw std::logic_error("CPU settlement has neither result nor failure");
  if(queuedReturn&&(result->queued||!result->presented||result->sourceFrameCompleted||result->screenTargetsCompleted)){
   // A joined fallback may restore current state; a detached result must never
   // restore an older packet over the next capture. Fail closed, keep owners.
   Fail();throw std::logic_error("Early-return pilot returned a nondeferred result; source quarantined");
  }
  if(result->presented&&!result->sourceFrameCompleted){
   try{
    Accept(*result);ReleaseAnchor();cpuAccepted=true;
    // Exact retained frame/UI, never a younger Session pending capture.
    ReportPresentedWorldResult(*owner,submission->Frame(),*result,submission->Options().screenUI);
   }catch(...){
    const auto failure=std::current_exception();
    if(!cpuAccepted){try{Fail();if(!transferred)ReleaseAnchor();}catch(...){}}
    std::rethrow_exception(failure);
   }
  }
  // Synchronous completion/no-Begin fallback still uses the joined source path.
 }
 void ReleaseAnchor(){ValidateOwner();if(transferred)throw std::logic_error("Cannot release a held admission anchor");owner->admissionSettlement.reset();}
 void Accept(const Metal4WorldPresentationResult& result){
  ValidateOwner();
  if(result.queued||!result.presented||result.sourceFrameCompleted||result.screenTargetsCompleted||result.completedPostprocessSerial||
   result.sourceDrawFrame!=submission->Frame().sourceDrawFrame||!submission->Options().beginReceipt->Submission()||
   submission->Options().beginReceipt->FailureStage()!=NativeFrameBeginReceipt::Failure::None)
   throw std::logic_error("Scene admission acceptance lacks its exact deferred renderer result");
  if(!transferred)throw std::logic_error("Cannot accept an unheld scene frame");
  mailbox->Accept(ticket);ui::GetRecorder().ReportSubmittedFrame(submission->Frame().sourceDrawFrame);
  owner->sourceSettlement.reset();transferred=false;
 }
};

void CompleteDynamicCapture(uint64_t generation,uint64_t through,uint64_t draw) {
 session->dynamicWaterCapture.DidComplete(generation,through,draw);
 const auto& receipt=session->dynamicCaptureReceipt;
 if(receipt&&receipt->material&&receipt->material->generation==generation&&receipt->sourceDrawFrame==draw&&receipt->throughUpdate==through)
  session->dynamicCaptureReceipt.reset();
}
bool AbortDynamicCapture() noexcept {
 if(!session||!session->dynamicCaptureReceipt)return true;
 if(!session->dynamicWaterCapture.AbortCapture(session->dynamicCaptureReceipt))return false;
 session->dynamicCaptureReceipt.reset();return true;
}

void ApplyDeferredFrameReceipts()
{
 if(!session)return;
 while(auto notice=session->completionMailbox->Pop()){
 const auto& result=*notice;
 if(!result.sourceFrameCompleted)throw std::logic_error("Deferred scene receipt is not GPU-completed");
 session->sourceCredits.ValidateHeld(result.sourceDrawFrame);
 // Reject missing/mismatched journal owners before any water/world retirement.
 if(result.dynamicWaterGeneration){
  if(result.dynamicWaterSourceDrawFrame!=result.sourceDrawFrame)throw std::logic_error("Deferred water receipt belongs to another scene draw");
  session->dynamicWaterCapture.ValidateHeldFrame(result.dynamicWaterSourceDrawFrame);
  session->dynamicWaterCapture.ValidateCompletion(result.dynamicWaterGeneration,result.dynamicWaterSequence,result.dynamicWaterSourceDrawFrame);
 }
 ui::GetRecorder().ValidateHeldFrame(result.sourceDrawFrame);
 LuaNativePostprocess::GetRecorder().ValidateHeldFrame(result.completedPostprocessSerial);
 LuaNativeVolumetricClouds::ValidateHeldFrame(result.sourceDrawFrame);
 if(result.dynamicWaterGeneration)CompleteDynamicCapture(result.dynamicWaterGeneration,result.dynamicWaterSequence,result.dynamicWaterSourceDrawFrame);
 // Source ownership was transferred at submission; do not clear N+1 capture.
 if(result.completedWorldInfo[0])RetireNativeWorldInfo(result.completedWorldInfo[0],result.completedWorldInfo[1]);
 if(result.completedWaterUpdates[0])session->waterCapture.DidComplete(result.completedWaterUpdates[0],result.completedWaterUpdates[1],result.completedWaterUpdates[2]);
 ui::GetRecorder().RetireSubmittedFrame(result.sourceDrawFrame);
 LuaNativePostprocess::RetireCompletedFrame(result.completedPostprocessSerial);
 LuaNativeVolumetricClouds::RetireCompletedFrame(result.sourceDrawFrame);
 session->sourceCredits.ReleaseHeld(result.sourceDrawFrame);
 }
}
bool BeginReadyPilotRequested() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_BEGIN_READY_PILOT");return value&&std::string_view(value)=="1";}();
 return enabled;
}

void RequireRetiredGPUWork()
{
 if(session&&session->cpuReadReceipt220)session->cpuReadReceipt220->pending.Finish();
 if(!session || !session->renderer)return;
 session->renderer->FinishCPUWork();
 if(Metal4RendererCanSafelyDetachSurface()){ApplyDeferredFrameReceipts();session->completionMailbox->RequireNoQueuedCompletions();return;}
 LOG_L(L_ERROR,"[Metal4Scene] GPU submission has not retired; refusing unsafe renderer/window teardown. Process restart is required");
 std::terminate();
}

} // namespace

void FinishCPUReadPreparation() { if(session&&session->cpuReadReceipt220)session->cpuReadReceipt220->pending.Finish(); }
void FinishPendingFrame() { RequireRetiredGPUWork(); }
void FinishRuntimeControl() { RequireRetiredGPUWork(); }
NativeSourceFrameCredits::Lease AcquireWorldFrameCredit(uint64_t draw){
 if(!session)throw std::logic_error("World capture has no source credit owner");
 if(!session->sourceCredits.HasCapacity())RequireRetiredGPUWork();
 return session->sourceCredits.Acquire(draw);
}

void Initialize(SDL_Window* window, uint32_t renderWidth, uint32_t renderHeight)
{
	if (session)
		throw std::runtime_error("The experimental Metal4 scene is already attached");
	if (configHandler) {
		const auto render = SelectNativeRenderExtent(renderWidth, renderHeight,
			configHandler->GetBool("Fullscreen"), configHandler->GetBool("WindowBorderless"),
			configHandler->GetInt("NativeFullscreenRenderWidth"), configHandler->GetInt("NativeFullscreenRenderHeight"));
		renderWidth = render.width;
		renderHeight = render.height;
	}
	// Publish only after successful construction, so failed initialization leaves
	// Shutdown and the caller's window teardown safe and idempotent.
	auto next = std::make_unique<Session>(window, renderWidth, renderHeight);
	InitializeMetalInfoMaintenance();
	session = std::move(next);
	SetNativeGroundDecalImageProvider([](const std::string& path) {
		if (!session) throw std::runtime_error("Original ground decal image outside scene lifetime");
		return session->capture.CaptureTexture(path);
	});
	// The successfully constructed world renderer owns the paired ScreenGPU.
	// Publish only this private family; no global GLSL capability is changed.
	LuaNativePiP::SetModelProvider([](const S3DModel* model) {
		if (!session) throw std::runtime_error("Original PiP model capture outside scene lifetime");
		return session->capture.CaptureModel(model);
	});
	LuaNativePiP::SetDrawSink([](ui::NativeUIRecorder& recorder, NativePiPDraw draw) {
		recorder.AppendPiP(std::move(draw));
	}, true);
	LuaNativePlacement::SetModelProvider([](const S3DModel* model) {
		if (!session) throw std::runtime_error("Original placement capture outside scene lifetime");
		return session->capture.CaptureModel(model);
	});
	LuaNativeBlueprint::SetModelProvider([](const S3DModel* model) {
		if (!session) throw std::runtime_error("Original Blueprint model capture outside scene lifetime");
		return session->capture.CaptureModel(model);
	});
	LuaNativeBlockout::Configure([](const S3DModel* model) {
		if(!session)throw std::runtime_error("Original Blockout model capture outside scene lifetime");
		return session->capture.CaptureModel(model);
	},[](uint32_t width,uint32_t height) {
		if(!session)throw std::runtime_error("Original Blockout target allocation outside scene lifetime");
		return session->renderer->AllocateBlockoutTarget(width,height);
	});
	NativeTerraformCaptureModels()=[](const S3DModel* model){if(!session)throw std::runtime_error("Terraform Capture model outside scene lifetime");return session->capture.CaptureModel(model);};
	LuaNativeCommands::SetModelProvider([](const S3DModel* model) {
		if (!session) throw std::runtime_error("Original Commands FX model capture outside scene lifetime");
		return session->capture.CaptureModel(model);
	});
	LOG_L(L_WARNING, "[Metal4Scene] Experimental live scene: native world presentation and real SDL input; original graphics coverage and full gameplay acceptance remain incomplete");
	LOG("[Metal4Scene] Render extent %ux%u; original logical window/input geometry retained, Retina backing scale is not applied", renderWidth, renderHeight);
}

void InitializeOriginalShadowPrograms()
{
	if (!session || !session->renderer)
		throw std::logic_error("Original ShadowGen initialization lacks its actual native source Session");
	RequireRetiredGPUWork();
	// Construct against the real new targets before publishing the replacement.
	// Reload may change their color format as well as enable previously absent
	// shadows. Capture owners keep their reference to this stable Session member.
	NativeEngineShadowProgramSource programs;
	session->renderer->ConstructOriginalShadowPrograms(programs, session->ShadowDrawingCamera());
	session->originalShadowPrograms = std::move(programs);
}

void ConstructOriginalShadowParticlePrograms()
{
	if (!session || !session->renderer)
		throw std::logic_error("Original ProjFX initialization lacks its actual native source Session");
	RequireRetiredGPUWork();
	session->renderer->ConstructOriginalShadowParticlePrograms(OriginalNativeShadowParticlesProgramSource());
}

void ResetOriginalShadowParticlePrograms()
{
	// Kill also follows failed Init; an absent Session or an empty source is
	// valid. Retain the common prohibition on releasing unretired GPU owners.
	RequireRetiredGPUWork();
	OriginalNativeShadowParticlesProgramSource().Reset();
}

void InstallOriginalModelSourcesBeforeLua()
{
 if(!session||!session->renderer||!session->cusModelSource)throw std::logic_error("Original Lua initialization lacks its actual native source Session");
 RequireRetiredGPUWork();
 auto& owner=*session;auto& context=OriginalNativeTextureContext();
 if(!owner.originalPrograms.Constructed())owner.renderer->ConstructOriginalModelPrograms(owner.originalPrograms);
 owner.legacyServices.engine=owner.EngineHooks();owner.legacyServices.commonTextures=[&owner]{return owner.CommonModelTextures();};
 owner.legacyServices.resolveTexture=[](const LuaMatTexture& descriptor){
  const auto actual=LuaNativeCUSTextures::ResolveMaterialTexture(descriptor);
  return NativeLegacyMaterialTexture{actual.parsed,actual.object?actual.object->name:0,descriptor.GetTextureTarget()};
 };
 owner.legacyServices.textureTarget=[](const LuaMatTexture& descriptor){return descriptor.GetTextureTarget();};
 owner.legacyServices.uniformFloat=[](unsigned,int location,size_t,bool,std::span<const float>){throw std::runtime_error("Original legacy float uniform has no typed native location owner: "+std::to_string(location));};
 owner.legacyServices.uniformInt=[](unsigned,int location,size_t,std::span<const int>){throw std::runtime_error("Original legacy integer uniform has no typed native location owner: "+std::to_string(location));};
 NativeCUSSourceProviders providers;
 providers.ready=[&owner]{return owner.ModelSourceOwnersConnected();};
 providers.compile=[&owner](const NativeCUSProgramSource& source){return owner.renderer->CompileOriginalCUSProgram(source);};
 providers.parseTexture=LuaNativeCUSTextures::ParseTexture;providers.deleteTexture=LuaNativeCUSTextures::DeleteTexture;
 providers.readClipDistance0=[]{const auto& clips=OriginalNativeModelContext().clips;if(!clips.observed)throw std::logic_error("CUS clip read has no actual context");return clips.enabled[0];};
 providers.writeClipDistance0=[](bool enabled){OriginalNativeModelContext().clips.SetEnabled(0,enabled);};
 if(!owner.ModelSourceOwnersConnected())throw std::runtime_error("Original model Session is missing an actual selected shadow/cube/material/source consumer");
 owner.cusModelSource->Initialize(std::move(providers));
 if(!owner.cusModelSource->Ready())throw std::runtime_error("Original model publications are not initialized before unsynced Lua");
 owner.renderer->ConnectSSAODeferredSources(true);owner.renderer->ConnectBloomDeferredSources(true);
 LuaNativeDeferredLights::Services lightServices;
 lightServices.ready=[&owner]{return owner.ModelSourceOwnersConnected();};
 lightServices.compile=[&owner](const NativeDeferredLightProgramSource& source){return owner.renderer->CompileOriginalDeferredLightProgram(source);};
 LuaNativeDeferredLights::Configure(std::move(lightServices));
 LuaNativeUnitStencil::Services stencilServices;
 stencilServices.ready=[&owner]{return owner.ModelSourceOwnersConnected();};
 stencilServices.compile=[&owner](const NativeUnitStencilProgramSource& source){return owner.renderer->CompileOriginalUnitStencilProgram(source);};
 LuaNativeUnitStencil::Configure(std::move(stencilServices));
 LuaNativeOutline::Services outlineServices;
 outlineServices.ready=[&owner]{return owner.ModelSourceOwnersConnected();};
 outlineServices.compile=[&owner](const NativeOutlineProgramSource& source){return owner.renderer->CompileOriginalOutlineProgram(source);};
 LuaNativeOutline::Configure(std::move(outlineServices));
 LuaNativeDistortion::Services distortionServices;
 distortionServices.ready=[&owner]{return owner.ModelSourceOwnersConnected();};
 distortionServices.compile=[&owner](const NativeDistortionProgramSource& source){return owner.renderer->CompileOriginalDistortionProgram(source);};
 LuaNativeDistortion::Configure(std::move(distortionServices));
}

void CaptureOriginalGenesis(const std::function<void()>& originalCallback)
{
 if(!session||!session->cusModelSource||session->genesisSourcePending)throw std::logic_error("Original Genesis source owner is absent or repeated");
 auto actual=session->cusModelSource->Capture(globalRendering->drawFrame,originalCallback);
 for(const auto& draw:actual->draws)if(!HasNoNativeCUSAttachmentEffects(draw))
  throw std::runtime_error("Original Genesis CUS output requires its actual attachment/pass consumer");
 session->genesisSourcePending=std::move(actual);
}

void PrepareOriginalWorldStateBeforePreUnit()
{
 if(!session||!camera)throw std::logic_error("Original WorldDrawer source state lacks its actual context/camera");
 auto& recorder=ui::GetRecorder();
 if(session->mainInitialClear||session->mainGroundPending)throw std::logic_error("Original WorldDrawer clear/ground boundary was repeated");
 const auto& fog=ISky::GetSky()->fogColor;recorder.SetOriginalClearColor({fog.x,fog.y,fog.z,0});
 session->mainInitialClear=GetWorldDrawRecorder().RecordClear(NativeWorldDrawStage::Ground,ScreenAttachmentAll,
  recorder.CurrentClearValues(),recorder.CurrentWorldState(),recorder.CurrentState().draw.scissor);
 recorder.UpdateWorldState([](auto& state){state.depthWrite=true;state.depthTest=true;},false);
 ScreenBlendState blend;blend.enabled=false;recorder.SetBlend(blend);
 camera->Update();
}
void PrepareOriginalSkyBeforePreUnit()
{
 if(!session||!ISky::GetSky())throw std::logic_error("Original sky source is unavailable before PreUnit");
 ISky::GetSky()->Draw();
}

std::shared_ptr<const NativeOpaqueModelFrame> CaptureOriginalOpaqueModels()
{
 if(!session||!session->cusModelSource||!session->cusModelSource->Ready()||session->opaqueSourcePending)
  throw std::logic_error("Original opaque source has unconnected or unconsumed actual owners");
 auto& recorder=ui::GetRecorder();const auto* active=CCameraHandler::GetActiveCamera();
 if(!active||!game||game->GetDrawMode()!=CGame::gameNormalDraw)throw std::logic_error("Original opaque source lacks its main camera/pass");
 WorldCamera actual;actual.view=std::to_array(active->GetViewMatrix().m);actual.projection=std::to_array(active->GetProjectionMatrix().m);
 actual.position={active->GetPos().x,active->GetPos().y,active->GetPos().z};std::copy_n(active->viewport,4,actual.viewport.begin());
 auto control=std::to_array(active->GetClipControlMatrix().m),zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
 if(control==ui::IdentityMatrix())actual.clipDepth=ClipDepthConvention::MinusOneToOne;
 else if(control==zero)actual.clipDepth=ClipDepthConvention::ZeroToOne;
 else throw std::runtime_error("Original opaque source camera clip control has no actual native consumer");
 recorder.BeginWorldStage(NativeWorldDrawStage::World,actual.view,actual.projection,std::to_array(active->GetBillBoardMatrix().m),false,control==zero);
 recorder.SetViewport({actual.viewport[0],actual.viewport[1],uint32_t(actual.viewport[2]),uint32_t(actual.viewport[3])});
 NativeOpaqueModelCallbacks callbacks;callbacks.ready=[]{return session&&session->ModelSourceOwnersConnected();};callbacks.engine=session->EngineHooks();
 callbacks.commonTextures=[]{return session->CommonModelTextures();};
 callbacks.legacyMaterials=[](NativeEngineModelPhase& phase,bool deferred){session->legacyModelSource->Capture(phase,deferred,session->legacyServices);};
 callbacks.postDeferred=[](bool,const std::function<void()>& original){original();};
 try{
  session->opaqueSourcePending=session->opaqueModelCapture->Capture(globalRendering->drawFrame,actual,callbacks);
  recorder.EndWorldStage();return session->opaqueSourcePending;
 }catch(...){recorder.EndWorldStage();throw;}
}

void SetRenderSize(uint32_t renderWidth, uint32_t renderHeight)
{
	if (!session) return;
	const auto before = session->surface.Snapshot();
	if (before.renderWidth == renderWidth && before.renderHeight == renderHeight) return;
	RequireRetiredGPUWork();
	session->surface.SetRenderSize(renderWidth, renderHeight);
	LOG("[Metal4Scene] Render resize %ux%u -> %ux%u; previous GPU work retired",
		before.renderWidth, before.renderHeight, renderWidth, renderHeight);
}

NativeDisplayGeometry GetNativeDisplayGeometry()
{
	if (!session || !Threading::IsMainThread()) return {};
	const auto observed = session->surface.Snapshot();
	if (!observed.attached) return {};
	return {true, observed.logicalWidth, observed.logicalHeight, observed.renderWidth, observed.renderHeight,
		observed.presentationBackingWidth, observed.presentationBackingHeight};
}

void HandleEvent(const SDL_Event& event)
{
	if (session) {
  ApplyDrainedMetal4SurfaceEvent(event,session->surface.Snapshot().windowID,
   []{RequireRetiredGPUWork();},[&]{session->surface.HandleEvent(event);session->RememberLogicalExtent();});
	}
}

ui::NativeUIFrameRasterMapping GetUIFrameRasterMapping(const ScreenViewport& defaultViewport)
{
 static const bool captureOverlap=[] {const char* value=std::getenv("BAR_NATIVE_CAPTURE_OVERLAP");return !value||std::string_view(value)=="1";}();
 if(!captureOverlap || (session && session->dynamicWaterCapture.Initialized()))RequireRetiredGPUWork();
 else ApplyDeferredFrameReceipts(); // Apply only if an earlier safe boundary already drained N.
	if (!session || !session->lastLogicalExtent[0] || !session->lastLogicalExtent[1])
		throw std::runtime_error("Native UI raster mapping requires an observed window surface");
	const auto observed = session->surface.Snapshot();
	return {session->lastLogicalExtent[0], session->lastLogicalExtent[1],
		observed.renderWidth, observed.renderHeight, defaultViewport, session->renderer->ActualMainSampling().samples};
}

DrawResult DrawLoadingScreen(const ScreenFrameView& screenUI)
{
	if (!session) return {};
	FinishRuntimeControl(); // Also safe if a loading/reload presentation follows a world frame.
	const auto result = session->loadingScreen->Draw(screenUI);
	if (result.presented && !session->reportedLoadingScreen) {
		session->reportedLoadingScreen = true;
		LOG("[Metal4Scene] First original LuaIntro presentation: draws=%u, texture uploads=%u; no world capture",
			result.draws, result.textureUploads);
	}
	return {result.presented, result.targetsCompleted};
}

void EndLoadingScreen()
{
	if (!session) return;
	RequireRetiredGPUWork();
	session->loadingScreen->ClearResources();
	session->reportedLoadingScreen = false;
}

void PrepareMapTextureInputs()
{
	if(session)session->mapTextures=OriginalNativeMapTextures().Publish(session->capture.PrepareMapHeights(),globalRendering->drawFrame);
}
void CompleteStartBoxWorld(std::shared_ptr<const NativeStartBoxFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame || !frame || frame->sourceDrawFrame!=session->preparedFrame->sourceDrawFrame)
		throw std::logic_error("Original StartBox callback completion differs from its world frame");
	session->preparedFrame->luaStartBoxFrame=std::move(frame);
}
void CompleteBlockoutWorld(std::shared_ptr<const NativeBlockoutFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame||!frame||frame->sourceDrawFrame!=session->preparedFrame->sourceDrawFrame||session->preparedFrame->luaBlockoutFrame)
		throw std::logic_error("Original Blockout completion differs from actual world frame");
	session->preparedFrame->luaBlockoutFrame=std::move(frame);
}
void CompleteWeaponRangeWorld(std::shared_ptr<const NativeWeaponRangeFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame || !frame || frame->sourceDrawFrame!=session->preparedFrame->sourceDrawFrame)
		throw std::logic_error("Original range callback completion differs from its world frame");
	session->preparedFrame->luaWeaponRangeFrame=std::move(frame);
}
void CompleteSensor31657CircleWorld(std::shared_ptr<const NativeSensor31657CircleFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame || !frame || !globalRendering ||
		frame->sourceDrawFrame!=session->preparedFrame->sourceDrawFrame ||
		frame->sourceDrawFrame!=uint64_t(globalRendering->drawFrame) || session->preparedFrame->luaSensor31657CircleFrame)
		throw std::logic_error("Original Sensor circle callback differs from its prepared world frame or was repeated");
	session->preparedFrame->luaSensor31657CircleFrame=std::move(frame);
}
void CompleteSensorPreviewWorld(std::shared_ptr<const NativeSensorPreviewFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame || !frame || !globalRendering ||
		frame->sourceDrawFrame!=session->preparedFrame->sourceDrawFrame ||
		frame->sourceDrawFrame!=uint64_t(globalRendering->drawFrame) || session->preparedFrame->luaSensorPreviewFrame)
		throw std::logic_error("Original Sensor Preview callback differs from its prepared world frame or was repeated");
	session->preparedFrame->luaSensorPreviewFrame=std::move(frame);
}
void CompleteUnitPrimitiveWorld(std::shared_ptr<const NativeUnitPrimitiveFrame> frame)
{
	if(!session)return;
	if(!session->preparedFrame || !frame || !globalRendering ||
		frame->uniforms.sourceDrawFrame!=session->preparedFrame->sourceDrawFrame ||
		session->preparedFrame->sourceDrawFrame!=uint64_t(globalRendering->drawFrame) ||
		session->preparedFrame->luaUnitPrimitiveFrame)
		throw std::logic_error("Original unit primitive callback completion differs from its prepared world frame or was repeated");
	// Validate the retained actual publication, never live model/UBO storage.
	(void)ReadNativeUnitPrimitiveUniforms(frame->uniforms);
	session->preparedFrame->luaUnitPrimitiveFrame=std::move(frame);
}
void PrepareEnvironmentBeforePreUnit()
{
 if(!session)return;
 session->environmentStage={}; // A failed new capture must not cancel an older receipt.
 // Original WorldDrawer GenerateIBL: shadow source occurs BEFORE cube/sun.
 // The actual attached capture service must fill actualShadowSourcePending;
 // incomplete selected producers throw rather than setting availability.
 if(shadowHandler.ShadowsLoaded()){
  const auto previous=game->GetDrawMode();game->SetDrawMode(CGame::gameShadowDraw);
  try{shadowHandler.CreateShadows();}catch(...){game->SetDrawMode(previous);throw;}
  game->SetDrawMode(previous);
 }
 session->environmentStage=session->renderer->CaptureEnvironmentInputs(session->capture.PrepareEnvironmentBeforePreUnit());
 // GenerateIBLTextures restores the main default viewport after the actual
 // selected reflection face and sun/specular updates.
 ui::GetRecorder().SetViewport({globalRendering->viewPosX,globalRendering->viewPosY,uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)});
}

bool CanInitializeWater()
{
 return session && session->renderer && session->renderer->CanInitializeWater();
}
void InitializeWater(const NativeBumpWaterSettings& settings)
{
 if(!CanInitializeWater())throw std::runtime_error("Native Water4 original scene consumers are not connected");
 RequireRetiredGPUWork();
 session->waterCapture.Initialize(settings,[](const std::string& path){return session->capture.CaptureTexture(path);});
}
void UpdateWater() { if(session && session->waterCapture.Initialized())session->waterCapture.Update(); }
void ResetWater()
{
 if(!session)return;
 RequireRetiredGPUWork();session->waterCapture.Reset();
 if(session->renderer)session->renderer->ClearWaterResources();
}
bool WaterInitialized() {return session && session->waterCapture.Initialized();}
void InitializeBasicWater()
{
 if(!session || !session->renderer)throw std::runtime_error("Native BasicWater requires the original scene stage");
 RequireRetiredGPUWork();
 session->basicWaterCapture.Initialize([](const std::string& path){return session->capture.CaptureTexture(path);});
}
void ResetBasicWater()
{
 if(!session)return;
 RequireRetiredGPUWork();session->basicWaterCapture.Reset();
 if(session->renderer)session->renderer->ClearBasicWaterResources();
}
bool BasicWaterInitialized() {return session && session->basicWaterCapture.Initialized();}
bool CanInitializeDynamicWater(){return false;}
void InitializeDynamicWater(){
 if(!CanInitializeDynamicWater()||!session||!session->renderer)throw std::runtime_error("Dynamic original source profile/native acceptance is incomplete");
 RequireRetiredGPUWork();session->dynamicWaterCapture.Initialize(DynamicWaterSourceProfile{});
}
void ResetDynamicWater(){if(!session)return;RequireRetiredGPUWork();session->dynamicWaterCapture.Reset();session->dynamicCaptureReceipt.reset();if(session->renderer)session->renderer->ClearDynamicWaterResources();}
void UpdateDynamicWater(){if(!session||!session->dynamicWaterCapture.Initialized())throw std::logic_error("Dynamic Update without its source lifetime");session->dynamicWaterCapture.Update();}
void AddDynamicWaterExplosion(const float3& pos,float strength,float size){if(!session||!session->dynamicWaterCapture.Initialized())throw std::logic_error("Dynamic explosion without source lifetime");session->dynamicWaterCapture.AddExplosion(pos,strength,size);}
bool CanInitializeLegacyWater()
{
 // One admission gate: enable only after source-state ownership and native
 // compilation/device parity gates in INTEGRATION-MANIFEST.json are met.
 return false;
}
void InitializeLegacyWater()
{
 if(!CanInitializeLegacyWater()||!session||!session->renderer)throw std::runtime_error("Native Water1 source/native acceptance gate is not complete");
 // No public availability until native compiler/device acceptance is complete.
 RequireRetiredGPUWork();session->legacyWaterCapture.Initialize();
}
bool CanInitializeLegacyRefractiveWater(){return false;}
void InitializeLegacyRefractiveWater(){
 // No original capability observation exists yet. Never let headless GLAD choose this branch.
 constexpr auto observedTarget=LegacyRefractionTarget::Unobserved;
 if(!CanInitializeLegacyRefractiveWater()||observedTarget==LegacyRefractionTarget::Unobserved||!session||!session->renderer)
  throw std::runtime_error("Refractive source capability/native acceptance remains unobserved");
 RequireRetiredGPUWork();session->legacyWaterCapture.Initialize(observedTarget);
}
void ResetLegacyWater(){if(!session)return;RequireRetiredGPUWork();session->legacyWaterCapture.Reset();if(session->renderer)session->renderer->ClearLegacyWaterResources();}



namespace {
// This scope is the source CBumpWater camera/draw-mode boundary, including
// unwind on an original Lua/C++ capture error. It never changes synced state.
class WaterViewScope {
 CCamera* previous=CCameraHandler::GetActiveCamera();
 CGame::GameDrawMode previousMode=game->GetDrawMode();
 bool reflection=IWater::GetWater()->DrawReflectionPass(),refraction=IWater::GetWater()->DrawRefractionPass();
 float3 diffuse=sunLighting->modelDiffuseColor,ambient=sunLighting->modelAmbientColor;
public:
 WaterViewScope(NativeWaterScenePass pass,uint32_t reflectionSize)
 {
  const bool reflected=pass==NativeWaterScenePass::Reflection;
  if(reflected){
   CCameraHandler::SetActiveCamera(CCamera::CAMTYPE_UWREFL);
   camera->CopyStateReflect(previous);camera->UpdateLoadViewport(0,0,reflectionSize,reflectionSize);
  }else{
   previous->Update();
   sunLighting->modelDiffuseColor*=float3(.5f,.7f,.9f);
   sunLighting->modelAmbientColor*=float3(.6f,.8f,1.f);
  }
  game->SetDrawMode(reflected?CGame::gameReflectionDraw:CGame::gameRefractionDraw);
  IWater::GetWater()->SetNativeDrawPass(reflected,!reflected);
 }
 ~WaterViewScope()
 {
  sunLighting->modelDiffuseColor=diffuse;sunLighting->modelAmbientColor=ambient;
  IWater::GetWater()->SetNativeDrawPass(reflection,refraction);
  game->SetDrawMode(previousMode);
  CCameraHandler::SetActiveCamera(previous->GetCamType());previous->Update();
 }
};

// CBumpWater::UpdateWater saves GL_FOG_BIT around BOTH offscreen views and
// restores the main viewport after them. Each view's source writers otherwise
// remain live across the real refraction -> reflection transition.
class WaterUpdateFogScope {
 NativeEngineModelFogSource fog=OriginalNativeModelContext().fog;
 bool enabled=OriginalNativeModelContext().fogEnabled;
public:
 ~WaterUpdateFogScope(){
  OriginalNativeModelContext().fog=fog;OriginalNativeModelContext().fogEnabled=enabled;
 }
};

std::optional<std::array<int32_t,4>> dynamicWaterViewport;
WorldCamera CurrentWaterSourceCamera()
{
 if(!camera)throw std::logic_error("Original water source has no actual active camera");
 WorldCamera actual;actual.view=std::to_array(camera->GetViewMatrix().m);actual.projection=std::to_array(camera->GetProjectionMatrix().m);
 actual.position={camera->GetPos().x,camera->GetPos().y,camera->GetPos().z};std::copy_n(camera->viewport,4,actual.viewport.begin());if(dynamicWaterViewport)actual.viewport=*dynamicWaterViewport;
 const auto control=std::to_array(camera->GetClipControlMatrix().m);auto zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
 if(control==ui::IdentityMatrix())actual.clipDepth=ClipDepthConvention::MinusOneToOne;
 else if(control==zero)actual.clipDepth=ClipDepthConvention::ZeroToOne;
 else throw std::runtime_error("Original water source clip control has no actual native consumer");
 return actual;
}

std::shared_ptr<const NativeOpaqueModelFrame> CaptureWaterOpaqueModels(NativeWaterScenePass pass,const WorldCamera& actual)
{
 if(!session||!session->cusModelSource||!session->cusModelSource->Ready())
  throw std::logic_error("Original water opaque models lack their actual source owners");
 const bool reflected=pass==NativeWaterScenePass::Reflection;
 auto& recorder=ui::GetRecorder();
 const auto stage=recorder.CurrentWorldStage();
 if(!stage||*stage!=(reflected?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction))
  throw std::logic_error("Original water opaque source lost its enclosing original water scope");
 // IWater::SetModelClippingPlane writes with identity model-view, so this
 // stored legacy eye equation is exactly the original model-space plane.
 auto& clips=OriginalNativeModelContext().clips;
 clips.SetEyeEquation(2,NativeWaterModelClipPlane(pass));clips.SetEnabled(2,true);
 // The enclosing water stage also owns the real ground callback state.
 // Do not reset matrices, viewport or raster state between ground and models.
 NativeOpaqueModelCallbacks callbacks;callbacks.ready=[]{return session&&session->ModelSourceOwnersConnected();};callbacks.engine=session->EngineHooks();
 callbacks.commonTextures=[]{return session->CommonModelTextures();};
 callbacks.legacyMaterials=[](NativeEngineModelPhase& phase,bool deferred){session->legacyModelSource->Capture(phase,deferred,session->legacyServices);};
 callbacks.postDeferred=[](bool,const std::function<void()>& original){original();};
 return session->opaqueModelCapture->Capture(globalRendering->drawFrame,actual,callbacks,reflected,!reflected);
}

std::shared_ptr<const NativeOpaqueModelFrame> CaptureOriginalAlphaModels(NativeWorldDrawStage stage,bool reflection=false,bool refraction=false)
{
	if(!session||!session->opaqueModelCapture||!session->ModelSourceOwnersConnected())
		throw std::logic_error("Original alpha models lack their actual installed source/material/program owners");
	const auto actual=CurrentWaterSourceCamera();auto& recorder=ui::GetRecorder();
	const auto* active=CCameraHandler::GetActiveCamera();
	recorder.BeginWorldStage(stage,actual.view,actual.projection,std::to_array(active->GetBillBoardMatrix().m),false,actual.clipDepth==ClipDepthConvention::ZeroToOne);
	recorder.SetViewport({actual.viewport[0],actual.viewport[1],uint32_t(actual.viewport[2]),uint32_t(actual.viewport[3])});
	NativeOpaqueModelCallbacks callbacks;callbacks.ready=[]{return session&&session->ModelSourceOwnersConnected();};callbacks.engine=session->EngineHooks();
	callbacks.commonTextures=[]{return session->CommonModelTextures();};
	callbacks.legacyMaterials=[](NativeEngineModelPhase& phase,bool deferred){session->legacyModelSource->Capture(phase,deferred,session->legacyServices);};
	try{
		auto source=session->opaqueModelCapture->CaptureAlpha(globalRendering->drawFrame,actual,stage,callbacks,reflection,refraction);
		recorder.EndWorldStage();return source;
	}catch(...){recorder.EndWorldStage();throw;}
}

std::shared_ptr<const NativeWaterLuaFrame> CaptureWaterPreParticles(NativeWorldDrawStage stage,
 bool above,bool below,bool reflection,bool refraction)
{
 auto& recorder=ui::GetRecorder();
 const auto control=std::to_array(camera->GetClipControlMatrix().m);
 auto zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
 if(control!=ui::IdentityMatrix() && control!=zero)throw std::runtime_error("Water callback uses unsupported clip-depth convention");
 recorder.BeginWorldStage(stage,std::to_array(camera->GetViewMatrix().m),std::to_array(camera->GetProjectionMatrix().m),
  std::to_array(camera->GetBillBoardMatrix().m),true,control==zero);
 recorder.SetViewport({camera->viewport[0],camera->viewport[1],uint32_t(camera->viewport[2]),uint32_t(camera->viewport[3])});
 // Original ProjectileDrawer::DrawAlpha's GL::SubState surrounds this
 // callback. Unlike DrawWorld, it has no Lua safe-mode ResetGLState.
 recorder.UpdateWorldState([](NativeWorldDrawState& s){s.depthTest=true;s.depthWrite=false;s.depthCompare=ScreenAlphaCompare::LessEqual;});
 ScreenBlendState blend;blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::One;recorder.SetBlend(blend);
 try {
  auto output=CaptureNativeWaterLuaFrame([&]{eventHandler.DrawWorldPreParticles(above,below,reflection,refraction);});
  recorder.EndWorldStage();return output;
 }catch(...){recorder.EndWorldStage();throw;}
}

std::shared_ptr<const NativeWaterLuaFrame> CaptureWaterPost()
{
 auto& recorder=ui::GetRecorder();
 const auto control=std::to_array(camera->GetClipControlMatrix().m);
 auto zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
 if(control!=ui::IdentityMatrix() && control!=zero)throw std::runtime_error("Water post callback uses unsupported clip-depth convention");
 recorder.BeginWorldStage(NativeWorldDrawStage::WaterPost,std::to_array(camera->GetViewMatrix().m),std::to_array(camera->GetProjectionMatrix().m),std::to_array(camera->GetBillBoardMatrix().m),false,control==zero);
 recorder.SetViewport({camera->viewport[0],camera->viewport[1],uint32_t(camera->viewport[2]),uint32_t(camera->viewport[3])});
 // Original DrawWaterPost uses NOOP_STATE_FUNCS: retain the completed water
 // pass's ordinary depth/blend state, and do not reset between Lua clients.
 recorder.UpdateWorldState([](NativeWorldDrawState& s){s.depthTest=true;s.depthWrite=true;s.depthCompare=ScreenAlphaCompare::LessEqual;});
 try {auto output=CaptureNativeWaterLuaFrame([]{eventHandler.DrawWaterPost();});recorder.EndWorldStage();return output;}
 catch(...){recorder.EndWorldStage();throw;}
}

std::shared_ptr<const NativeWaterSceneView> CaptureWaterView(NativeWaterScenePass pass,
 const NativeBumpWaterFrame* water,uint64_t sourceDrawFrame,bool legacyReflective=false,uint32_t dynamicExtent=0,std::array<float,9>* sourceBasis=nullptr)
{
 WaterViewScope scope(pass,legacyReflective?512:(dynamicExtent?512:water->settings.reflectionSize));
 struct ViewportRestore{std::optional<std::array<int32_t,4>> prior=dynamicWaterViewport;~ViewportRestore(){dynamicWaterViewport=prior;}} viewportRestore;
 if(dynamicExtent&&pass==NativeWaterScenePass::Refraction)dynamicWaterViewport=std::array<int32_t,4>{0,0,int32_t(dynamicExtent),int32_t(dynamicExtent)};
 if(sourceBasis)*sourceBasis={camera->GetRight().x,camera->GetRight().y,camera->GetRight().z,camera->GetUp().x,camera->GetUp().y,camera->GetUp().z,camera->GetDir().x,camera->GetDir().y,camera->GetDir().z};
 auto output=std::make_shared<NativeWaterSceneView>();
 output->legacyReflective=legacyReflective;output->dynamicWater=dynamicExtent!=0;
 output->pass=pass;output->drawGround=dynamicExtent || legacyReflective || pass==NativeWaterScenePass::Refraction || water->settings.reflection>1;
 output->drawFog=dynamicExtent?OriginalNativeModelContext().fogEnabled:pass==NativeWaterScenePass::Reflection;
 output->modelClipPlane=NativeWaterModelClipPlane(pass);output->terrainClipPlane=legacyReflective?std::array<float,4>{0,1,0,0}:NativeWaterTerrainClipPlane(pass);
 const auto actual=CurrentWaterSourceCamera();auto& recorder=ui::GetRecorder();
 const bool reflection=pass==NativeWaterScenePass::Reflection;
 recorder.BeginWorldStage(reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction,
  actual.view,actual.projection,std::to_array(camera->GetBillBoardMatrix().m),false,actual.clipDepth==ClipDepthConvention::ZeroToOne);
 recorder.SetViewport({actual.viewport[0],actual.viewport[1],uint32_t(actual.viewport[2]),uint32_t(actual.viewport[3])});
 std::shared_ptr<const NativeOpaqueModelFrame> actualModels;
 try{
  // CBumpWater clears before IWater draws either view. Preserve its real
  // color writer, clear depth, write masks and scissor in the common stream.
  const auto& fog=ISky::GetSky()->fogColor;recorder.SetOriginalClearColor({fog.x,fog.y,fog.z,(legacyReflective||dynamicExtent)?1.f:0.f});
  output->initialClearSequence=GetWorldDrawRecorder().RecordClear(reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction,
   static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth),recorder.CurrentClearValues(),
   recorder.CurrentWorldState(),recorder.CurrentState().draw.scissor);
  if(!dynamicExtent&&!output->drawFog)OriginalNativeModelContext().fogEnabled=false;
  // glClipPlane stores inverse-transpose(MV)*plane at its actual write.
  // Refraction writes before sky; reflection writes after sky. A Lua/sky
  // matrix change must therefore be visible at the same original boundary.
  const auto writeGroundClip=[&]{
   const auto current=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);
   CMatrix44f modelView;std::copy(current.begin(),current.end(),modelView.m);
   bool invertible=false;auto inverse=modelView.Invert(&invertible);
   if(!invertible)throw std::runtime_error("Original water ground clip plane has a singular actual model-view");
   const auto plane=output->terrainClipPlane;inverse.Transpose();
   const auto eye=inverse*float4(plane[0],plane[1],plane[2],plane[3]);
   auto& clips=OriginalNativeModelContext().clips;
   clips.SetEnabled(2,true);clips.SetEyeEquation(2,{eye.x,eye.y,eye.z,eye.w});
  };
  if(!reflection)writeGroundClip();
  // The real sky and ground source writers share this one scope with models.
  // Their captured state must survive until the model setup changes it.
  ISky::GetSky()->Draw();
  if(reflection)writeGroundClip();
  if(output->drawGround){
   if(!session->waterGroundSource)throw std::logic_error("Original water ground source owner is absent");
   const auto stage=reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction;
   // Complete these captures before model capture starts. All three owners
   // observe the one original Draw in the same live context and ordinal stream.
   output->groundLua=CaptureNativeWaterLuaFrame([&]{
    output->groundCUS=session->cusModelSource->Capture(globalRendering->drawFrame,[&]{
     output->groundSource=session->waterGroundSource->Capture(globalRendering->drawFrame,stage,[&]{
      readMap->GetGroundDrawer()->Draw(reflection?DrawPass::WaterReflection:DrawPass::WaterRefraction);
     });
    });
   });
  }
  actualModels=CaptureWaterOpaqueModels(pass,CurrentWaterSourceCamera());
  recorder.EndWorldStage();
 }catch(...){recorder.EndWorldStage();throw;}
  output->frame=session->capture.Capture(pass,{},actualModels);
 if(dynamicExtent&&pass==NativeWaterScenePass::Refraction)output->frame.viewCamera.viewport={0,0,int32_t(dynamicExtent),int32_t(dynamicExtent)};
 // Captured legacy refraction lighting retains the original temporary tint.
 // The consumer separately keeps pre-world GL4 UBO lighting/player eye.
 const bool reflected=pass==NativeWaterScenePass::Reflection;
 const auto stage=reflected?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction;
	output->frame.alphaModelFrames[0]=CaptureOriginalAlphaModels(stage,reflected,!reflected);
 output->preParticlesLua=CaptureWaterPreParticles(stage,reflected,!reflected,reflected,!reflected);
 output->preParticlesBoundary=NextNativeWorldDrawSequence();
 output->worldLua=CaptureNativeWaterLuaFrame([&]{if(reflected)eventHandler.DrawWorldReflection();else eventHandler.DrawWorldRefraction();});
 OriginalNativeModelContext().clips.SetEnabled(2,false);
 ValidateNativeWaterSceneView(*output,sourceDrawFrame);
 return output;
}
}

void CaptureOriginalTerrainReflectionFace(unsigned face,bool skyOnly,uint64_t initialClearSequence)
{
 if(!session||!session->waterGroundSource||!session->cusModelSource||session->reflectionGroundPending||face>=6||!initialClearSequence)
  throw std::logic_error("Original IBL face source owner is absent or repeated");
 auto out=std::make_shared<NativeTerrainSceneSourceFrame>();out->sourceDrawFrame=globalRendering->drawFrame;
 out->stage=NativeWorldDrawStage::TerrainReflection;out->viewCamera=CurrentWaterSourceCamera();
 out->reflectionFace=face;out->skyOnly=skyOnly;out->groundInvoked=!skyOnly;out->initialClearSequence=initialClearSequence;
 auto& recorder=ui::GetRecorder();const auto& actual=out->viewCamera;
 recorder.BeginWorldStage(out->stage,actual.view,actual.projection,std::to_array(camera->GetBillBoardMatrix().m),false,
  actual.clipDepth==ClipDepthConvention::ZeroToOne);
 recorder.SetViewport({actual.viewport[0],actual.viewport[1],uint32_t(actual.viewport[2]),uint32_t(actual.viewport[3])});
 try {
  if(globalRendering->drawDebugCubeMap){
   out->debugCubeSky=debugCubeMapTexture.GetNativeResource();
   out->debugCubeState=recorder.CurrentWorldState();
   if(!out->debugCubeSky)throw std::runtime_error("Original debug cube native producer is absent");
   out->groundInvoked=false; // The original debug branch skips both sky.Draw and ground.Draw.
  }else ISky::GetSky()->Draw();
  if(out->groundInvoked)out->lua=CaptureNativeWaterLuaFrame([&]{
   out->cus=session->cusModelSource->Capture(globalRendering->drawFrame,[&]{
    out->ground=session->waterGroundSource->Capture(globalRendering->drawFrame,out->stage,[&]{
     readMap->GetGroundDrawer()->Draw(DrawPass::TerrainReflection);
    });
   });
  });
  recorder.EndWorldStage();
 }catch(...){recorder.EndWorldStage();throw;}
 session->reflectionGroundPending=std::move(out);
}
std::shared_ptr<const NativeTerrainSceneSourceFrame> TakeOriginalTerrainReflectionFace()
{
 if(!session||!session->reflectionGroundPending)throw std::logic_error("Original IBL caller did not capture its selected face");
 return std::exchange(session->reflectionGroundPending,{});
}

void PrepareShieldMapBeforePreUnit()
{
 if(!session)return;
 if(!readMap||!readMap->GetGroundDrawer()||!session->mainInitialClear||session->mainGroundPending)
  throw std::logic_error("Original main ground source lacks its actual caller/map/clear");
 auto out=std::make_shared<NativeTerrainSceneSourceFrame>();out->sourceDrawFrame=globalRendering->drawFrame;
 out->stage=NativeWorldDrawStage::Ground;out->viewCamera=CurrentWaterSourceCamera();
 out->initialClearSequence=*session->mainInitialClear;out->groundInvoked=globalRendering->drawGround;
 const auto& actual=out->viewCamera;auto& recorder=ui::GetRecorder();
 recorder.BeginWorldStage(out->stage,actual.view,actual.projection,std::to_array(camera->GetBillBoardMatrix().m),false,
  actual.clipDepth==ClipDepthConvention::ZeroToOne);
 recorder.SetViewport({actual.viewport[0],actual.viewport[1],uint32_t(actual.viewport[2]),uint32_t(actual.viewport[3])});
 try {
  if(out->groundInvoked)out->lua=CaptureNativeWaterLuaFrame([&]{
   out->ground=session->waterGroundSource->Capture(globalRendering->drawFrame,out->stage,[&]{
    out->cus=session->cusModelSource->Capture(globalRendering->drawFrame,[&]{readMap->GetGroundDrawer()->Draw(DrawPass::Normal);});
    session->waterGroundSource->FinishDeferredCallbacks(out->cus);
   },&actual);
  });
  recorder.EndWorldStage();
 }catch(...){recorder.EndWorldStage();throw;}
 auto& context=OriginalNativeTextureContext();const auto& names=context.Geometry(NativeGeometryTextureRole::Map);
 const auto* ground=readMap->GetGroundDrawer();const auto* buffer=ground->GetGeometryBuffer();
 NativeMapDepthSelection selection;selection.drawGround=globalRendering->drawGround;selection.onlyVoidWater=readMap->HasOnlyVoidWater();
 selection.requestedDeferred=ground->DrawDeferred();selection.geometryBufferValid=buffer&&buffer->Valid();
 // The completed original phase records either its actual deferred program
 // or the original standard-state clear-only branch. Never infer validity from a request.
 selection.selectedDeferredShaderValid=out->ground&&out->ground->mapMRT&&!out->ground->deferredClearOnly;selection.advancedShading=ground->UseAdvShading();
 selection.namedDepthStorageExists=names.Name(NativeGeometryTextureNames::DepthAttachment)!=0;
 selection.storageWidth=names.Width();selection.storageHeight=names.Height();
 session->mainGroundPending=out;
 session->renderer->BeginShieldMapPublication(session->shieldCapture,context,globalRendering->drawFrame,actual,selection);
 session->mainGroundPending=std::move(out);session->mainInitialClear.reset();
}

void PrepareGroundDecalsBeforePreUnit()
{
 if (!session) return;
 if (session->preparedDecalDraw) throw std::logic_error("Original decal boundary captured twice without world consumption");
 if (globalRendering->drawGround) {
  auto& recorder=ui::GetRecorder();
  const auto control=std::to_array(camera->GetClipControlMatrix().m);
  auto zero=ui::IdentityMatrix();zero[10]=zero[14]=.5f;
  if(control!=ui::IdentityMatrix() && control!=zero)throw std::runtime_error("Original decal callback clip convention unsupported");
  recorder.BeginWorldStage(NativeWorldDrawStage::PreDecals,std::to_array(camera->GetViewMatrix().m),
   std::to_array(camera->GetProjectionMatrix().m),std::to_array(camera->GetBillBoardMatrix().m),false,control==zero);
  recorder.SetViewport({globalRendering->viewPosX,globalRendering->viewPosY,uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)});
  // DrawPreDecals has no safe-state reset. Preserve the actual Normal
  // Draw/Border and ground callback exit state from this invocation.
  try {
   LuaNativeGameDecal::BeginWorldFrame();
   eventHandler.DrawPreDecals();
   session->preparedGameDecals=LuaNativeGameDecal::EndWorldFrame();
   recorder.EndWorldStage();
  } catch(...) {
   LuaNativeGameDecal::AbortWorldFrame();recorder.EndWorldStage();throw;
  }
  session->preparedGroundDecals=CaptureNativeGroundDecals();
 }
 session->preparedDecalDraw=globalRendering->drawFrame;
}

void PrepareWorld(std::shared_ptr<const NativeUnitPrimitiveFrame> luaUnitPrimitiveFrame,
	std::shared_ptr<const NativeOpaqueModelFrame> actualOpaqueModels)
{
	if (!session)
		return;
	if (session->preparedFrame)
		throw std::logic_error("Native engine world was prepared twice without presentation or abort");
 // Only an exactly unchanged surface permits this value-snapshot capture to
 // overlap N's admitted prepare/encode work. Drain before later alpha/water/Lua work.
 const bool deferredParticles=BeginReadyPilotRequested()||PrepareReadyPilotRequested();
 const bool earlyCPURead220=deferredParticles&&CPUReadPacketMode220()>=2;
 std::unique_ptr<CPUReadReceipt220> cpuReceipt220;
 NativeCUSCPUReadWorker220::Join cpuJoin220; // Join before main-owned receipt destruction on every exit.
 int cpuReadFallback220=session->cpuReadDisabled220?2:0;
 // BEGIN EARLY_CPU_READ_LAUNCH220
 const auto launchCPURead220=[&](const WorldFrame& frame){
 if(CPUReadPacketMode220()&&frame.opaqueModelFrame&&!session->cpuReadDisabled220)try{
  cpuReceipt220=std::make_unique<CPUReadReceipt220>();
  cpuReceipt220->frame=frame.sourceDrawFrame;cpuReceipt220->models=frame.opaqueModelFrame;
  cpuReceipt220->ordinal=session->cpuReadJobs220+1;
  auto& job=cpuReceipt220->job;job.measure=CPUReadPacketMeasure220();
  if(earlyCPURead220)job.ExpectTail();
  for(const auto& pass:frame.opaqueModelFrame->passes){job.sources.push_back(pass.lua);if(pass.mrt)job.sources.push_back(pass.mrt->lua);}
  if(CPUReadPacketMode220()>=2&&!session->cpuReadWorker220){
   const auto start=job.measure?NativeCUSCPUReadWorker220::Clock::now():NativeCUSCPUReadWorker220::Clock::time_point{};
   session->cpuReadWorker220=std::make_unique<NativeCUSCPUReadWorker220>();
   if(job.measure)cpuReceipt220->startupNs=NativeCUSCPUReadWorker220::Elapsed(start);
  }
  if(job.measure)job.submitted=NativeCUSCPUReadWorker220::Clock::now();
  if(CPUReadPacketMode220()>=2){session->cpuReadWorker220->Start(job);cpuJoin220.Arm(*session->cpuReadWorker220,job);}
  else NativeCUSCPUReadWorker220::Build(job);
  ++session->cpuReadJobs220;
 }catch(...){
  // Start cannot throw after publishing the job, and Arm/Build are noexcept.
  // An unpublished setup failure must not advance the original error boundary.
  cpuReceipt220.reset();session->cpuReadDisabled220=true;cpuReadFallback220=1;
 }
 };
 // END EARLY_CPU_READ_LAUNCH220

 CaptureAtNativeSurfaceBoundary(deferredParticles,
  [&]{return session->surface.CaptureRefreshUnchanged();},
  []{RequireRetiredGPUWork();},[&]{session->surface.Refresh();},[&]{
   // Publish only at the actual capture boundary, after any changed-surface drain.
   session->opaqueSourcePending=actualOpaqueModels;
   // Particle source writers run once after the old encoding tail is drained.
   session->preparedFrame=session->capture.Capture(NativeWaterScenePass::Main,session->actualShadowSourcePending,actualOpaqueModels,deferredParticles);
   // No producer callback or GPU access: only retained opaque read rows.
   if(earlyCPURead220)launchCPURead220(*session->preparedFrame);
  });
 if(deferredParticles)session->capture.CompleteDeferredMainParticles(*session->preparedFrame);

 auto& frame=*session->preparedFrame;
 frame.mainSampling=session->renderer->ActualMainSampling();
 frame.minSampleShadingRate=globalRendering->minSampleShadingRate;
 frame.mapTextures=session->mapTextures;
 if(!session->mainGroundPending||session->mainGroundPending->sourceDrawFrame!=frame.sourceDrawFrame)
  throw std::logic_error("Original main ground boundary was not captured before this world draw");
 frame.mainGround=session->mainGroundPending; // Retain for the existing pre-Begin abort path too.
 frame.genesisCUSFrame=std::exchange(session->genesisSourcePending,{});
 // Only opaque CUS frames have sealed at this boundary. The following water,
 // alpha and Lua captures stay on the original main thread in original order.
 // Reset only after the old render job has settled. It may still retain its
 // own immutable packet, which is never replaced by this new local receipt.
 session->cpuReadReceipt220.reset();
 if(!earlyCPURead220)launchCPURead220(frame);
 session->cpuReadFallback220=cpuReadFallback220;
 if(cpuReceipt220&&cpuReceipt220->job.measure)cpuReceipt220->mainBeginNs=NativeCUSCPUReadWorker220::Timestamp(NativeCUSCPUReadWorker220::Clock::now());
	if (frame.opaqueModelFrame) {
		// Source capture already owns every model color/depth publication.
		// Do not create a second standalone depth phase after that capture.
		frame.shieldDepthFrame=session->renderer->BorrowActualMRTModelDepth(session->shieldCapture,frame);
	} else {
	NativeModelDepthSelection modelSelection;
	const auto* modelBuffer=LuaObjectDrawer::GetGeometryBuffer();
	modelSelection.drawDeferredEnabled=LuaObjectDrawer::NativeDrawDeferredEnabled();
	modelSelection.geometryBufferValid=modelBuffer&&modelBuffer->Valid();
	modelSelection.canDrawDeferred=true; // The paired actual native UnitVertexMain/depth-alpha programs initialized before Lua.
	modelSelection.unitsDeferred=CUnitDrawer::DrawDeferred();modelSelection.featuresDeferred=CFeatureDrawer::DrawDeferred();
	modelSelection.allowSecondClear=configHandler->GetBool("AllowDeferredModelBufferClear");
	frame.shieldDepthFrame=session->renderer->PublishShieldModelDepth(session->shieldCapture,frame,modelSelection);
	}

 if (!session->preparedDecalDraw || *session->preparedDecalDraw!=frame.sourceDrawFrame)
  throw std::logic_error("Original decal boundary was not captured before this world draw");
 frame.groundDecals=std::move(session->preparedGroundDecals);
 frame.luaGameDecalFrame=std::move(session->preparedGameDecals);
 frame.luaUnitPrimitiveFrame=std::move(luaUnitPrimitiveFrame);session->preparedDecalDraw.reset();
 // Preserve CWorldDrawer's eligibility; forceRendering belongs to the inner
 // water renderer and cannot bypass the outer voidWater/drawWater boundary.
 if((session->waterCapture.Initialized() || session->basicWaterCapture.Initialized() || session->legacyWaterCapture.Initialized() || session->dynamicWaterCapture.Initialized()) && globalRendering->drawWater && readMap->HasVisibleWater()){
  auto* water=IWater::GetWater().get();
  if(!water)throw std::logic_error("Water CPU producer outlived its original IWater instance");
  if(unsigned(session->waterCapture.Initialized())+unsigned(session->basicWaterCapture.Initialized())+unsigned(session->legacyWaterCapture.Initialized())+unsigned(session->dynamicWaterCapture.Initialized())>1)throw std::logic_error("Two original water renderers active in one IWater lifetime");
  if(session->waterCapture.Initialized())frame.bumpWater=session->waterCapture.Capture(frame,bool(frame.shadow)&&frame.lighting.requested.shadows>0,
   infoTextureHandler && infoTextureHandler->IsEnabled(),water->WireFrameModeRef());
  else if(session->basicWaterCapture.Initialized())frame.basicWater=session->basicWaterCapture.Capture(frame,water->WireFrameModeRef());
  auto scenes=std::make_shared<NativeWaterSceneFrame>();scenes->sourceDrawFrame=frame.sourceDrawFrame;
		frame.alphaModelFrames[0]=CaptureOriginalAlphaModels(NativeWorldDrawStage::AlphaBelowWater);
  scenes->belowWaterLua=CaptureWaterPreParticles(NativeWorldDrawStage::AlphaBelowWater,false,true,false,false);
  if(session->legacyWaterCapture.Initialized())frame.legacyWater=session->legacyWaterCapture.Capture(frame,water->WireFrameModeRef());
  std::array<float,9> dynamicReflectionBasis{},dynamicRefractionBasis{};
  if(session->dynamicWaterCapture.Initialized()){
   frame.dynamicWater=session->dynamicWaterCapture.CaptureDerived(frame.sourceDrawFrame);
   session->dynamicCaptureReceipt=frame.dynamicWater; // Retain before later view/finalization can throw.
  }
  {
   WaterUpdateFogScope originalWaterFog;
   if(frame.dynamicWater){
    auto& recorder=ui::GetRecorder();recorder.PushAttrib(ui::NativeUIAttribute::Enable|ui::NativeUIAttribute::Depth);
    try{
     recorder.UpdateWorldState([](NativeWorldDrawState& w){w.depthTest=true;w.depthWrite=true;},false);
     recorder.UpdateBlend([](ScreenBlendState& b){b.enabled=false;});
     auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);
     scenes->refraction=CaptureWaterView(NativeWaterScenePass::Refraction,nullptr,frame.sourceDrawFrame,false,frame.dynamicWater->material->refractionExtent,&dynamicRefractionBasis);
     scenes->reflection=CaptureWaterView(NativeWaterScenePass::Reflection,nullptr,frame.sourceDrawFrame,false,512,&dynamicReflectionBasis);
     recorder.PopAttrib();
    }catch(...){recorder.PopAttrib();throw;}
   }
   if(frame.bumpWater && frame.bumpWater->settings.refraction>1){
    scenes->refraction=CaptureWaterView(NativeWaterScenePass::Refraction,frame.bumpWater.get(),frame.sourceDrawFrame);
    // CBumpWater::DrawRefraction explicitly re-enables fog after the
    // complete original view, before the next reflection begins.
    OriginalNativeModelContext().fogEnabled=true;
   }
   if(frame.legacyWater){
    auto& recorder=ui::GetRecorder();
    recorder.PushAttrib(ui::NativeUIAttribute::Blend|ui::NativeUIAttribute::Alpha|ui::NativeUIAttribute::ColorMask);
    try{
     recorder.UpdateBlend([](ScreenBlendState& b){b.enabled=true;b.sourceColor=b.destinationColor=b.sourceAlpha=b.destinationAlpha=ScreenBlendFactor::One;});
     recorder.Color({1,1,1,1}); // CAdvWater bump tail's actual current-color writer.
     scenes->reflection=CaptureWaterView(NativeWaterScenePass::Reflection,nullptr,frame.sourceDrawFrame,true);
     recorder.PopAttrib();
    }catch(...){recorder.PopAttrib();throw;}
    // CAdvWater restores color-buffer attribs, then writes the main blend factors.
    recorder.UpdateBlend([](ScreenBlendState& b){b.sourceColor=b.sourceAlpha=ScreenBlendFactor::SourceAlpha;b.destinationColor=b.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;});
   }
   if(frame.bumpWater && frame.bumpWater->settings.reflection>0)scenes->reflection=CaptureWaterView(NativeWaterScenePass::Reflection,frame.bumpWater.get(),frame.sourceDrawFrame);
   ui::GetRecorder().SetViewport({globalRendering->viewPosX,globalRendering->viewPosY,uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)});
  }
  if(frame.legacyWater){
   frame.legacyWater=session->legacyWaterCapture.FinalizeSurface(frame.legacyWater,CurrentWaterSourceCamera(),water->WireFrameModeRef());
   // Original Draw disables alpha test and depth writes, then restores depth writes.
   // WaterPost observes the completed Water1 state, not the reflection scope.
   auto& recorder=ui::GetRecorder();
   // CAdvWater::Draw tail precedes the derived CRefractWater::Draw tail.
   auto& texgen=OriginalNativeLegacyWaterTexGen();
   texgen.Enable(1,0x2000,false);texgen.Enable(1,0x2001,false);
   if(frame.legacyWater->refraction){
    // SetupWaterDepthTex binds the actual source to3. Draw tail disables2D,
    // retains that binding; Copy already bound the exact target-aware source name to2.
    auto& history=OriginalNativeTextureContext().History();
    history.ActiveTexture(3);history.Bind(3,frame.legacyWater->refraction->shading.object);
    history.Enable(3,NativeCUSTextureTarget::Image2D,true);
    history.Enable(3,NativeCUSTextureTarget::Image2D,false);
    texgen.Enable(3,0x2000,false);texgen.Enable(3,0x2001,false);
    session->legacyWaterCapture.RefractionDrawTail();recorder.SyncObservedTextureBinding(2);recorder.SyncObservedTextureBinding(3);
   }
   if(!frame.legacyWater->refraction)OriginalNativeTextureContext().History().ActiveTexture(0);
   // Draw(false) disables blending only for surface raster, then enables it.
   recorder.UpdateBlend([](ScreenBlendState& b){b.enabled=true;});
   auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);
   recorder.UpdateWorldState([](NativeWorldDrawState& w){w.depthWrite=true;w.frontPolygonMode=w.backPolygonMode=NativePolygonMode::Fill;},false);
  }
  if(frame.dynamicWater){
   frame.dynamicWater=session->dynamicWaterCapture.FinalizeSurface(frame.dynamicWater,CurrentWaterSourceCamera(),dynamicReflectionBasis,dynamicRefractionBasis,water->WireFrameModeRef());
   // Dynamic Draw saves ENABLE bits; blend/alpha/fog enable return unchanged.
   ui::GetRecorder().Color({1,1,1,.5f});
   ui::GetRecorder().UpdateWorldState([](NativeWorldDrawState& w){w.frontPolygonMode=w.backPolygonMode=NativePolygonMode::Fill;},false);
  }
  scenes->waterPostLua=CaptureWaterPost();
		frame.alphaModelFrames[1]=CaptureOriginalAlphaModels(NativeWorldDrawStage::AlphaAboveWater);
  scenes->aboveWaterLua=CaptureWaterPreParticles(NativeWorldDrawStage::AlphaAboveWater,true,false,false,false);
  ValidateNativeWaterSceneFrame(*scenes,frame.sourceDrawFrame,(frame.dynamicWater || frame.legacyWater || (frame.bumpWater && frame.bumpWater->settings.reflection>0)),frame.dynamicWater || (frame.bumpWater && frame.bumpWater->settings.refraction>1));
  frame.waterScene=std::move(scenes);
 }else{
  // Original ProjectileDrawer::DrawAlpha invokes this callback once even
  // without a visible water surface. It precedes the engine AlphaAll stream.
		frame.alphaModelFrames[2]=CaptureOriginalAlphaModels(NativeWorldDrawStage::AlphaAll);
  frame.alphaAllLua=CaptureWaterPreParticles(NativeWorldDrawStage::AlphaAll,true,true,false,false);
 }
 // BEGIN CPU_READ_TAIL220
 // Seal only sources already captured at their original main-thread sites.
 // No new callback/order or water-view scan is introduced.
 if(earlyCPURead220&&cpuReceipt220){
  try{
   std::vector<std::shared_ptr<const NativeCUSSourceFrame>> tail;
   for(const auto& models:frame.alphaModelFrames)if(models)for(const auto& pass:models->passes){tail.push_back(pass.lua);if(pass.mrt)tail.push_back(pass.mrt->lua);}
   if(frame.originalShadow)tail.push_back(frame.originalShadow->cus);
   cpuReceipt220->alphaModels=frame.alphaModelFrames;cpuReceipt220->shadow=frame.originalShadow;
   std::vector<std::shared_ptr<const NativeCUSSourceFrame>> optionalWater;
   try{auto water=CPUReadWaterRoster220::Capture(frame.sourceDrawFrame,frame.waterScene);optionalWater=water.sources;cpuReceipt220->waterReads=std::move(water);}catch(...){/* Speculation setup must not move a new consumer error earlier. */}
   cpuReceipt220->job.PublishTail(std::move(tail),std::move(optionalWater));cpuReceipt220->includesTail=true;
  }catch(...){
   cpuJoin220.Finish();cpuReceipt220.reset();session->cpuReadDisabled220=true;session->cpuReadFallback220=1;
  }
 }
 // END CPU_READ_TAIL220
 if(cpuReceipt220&&cpuReceipt220->job.measure)cpuReceipt220->mainEndNs=NativeCUSCPUReadWorker220::Timestamp(NativeCUSCPUReadWorker220::Clock::now());
 // Only successful opt-in early jobs may span subsequent original Lua/UI capture.
 // Exceptions before this transfer retain the local join-before-owner guard.
 if(earlyCPURead220&&cpuReceipt220)cpuJoin220.TransferTo(cpuReceipt220->pending);
 else cpuJoin220.Finish();
 session->cpuReadReceipt220=std::move(cpuReceipt220);
}

void AbortPreparedWorld()
{
 FinishCPUReadPreparation(); // Before source aborts, including quarantine early returns.
 if(session&&!session->dynamicWaterCapture.CanAbortCapture(session->dynamicCaptureReceipt))return;
	if(session&&session->renderer && !session->renderer->AbortShieldPublications(session->preparedFrame?session->preparedFrame->opaqueModelFrame:session->opaqueSourcePending,session->actualShadowSourcePending,session->preparedFrame&&session->preparedFrame->mainGround?session->preparedFrame->mainGround:session->mainGroundPending,session->environmentStage,&session->shieldCapture))return; // Keep failed-Begun source owners intact.

	if(session)session->legacyWaterCapture.AbortUncompletedRefraction();
	if(session && session->renderer && !session->renderer->AbortScreenContinuation())return;
 if(!AbortDynamicCapture())return;
	LuaNativeBlockout::AbortWorldFrame();
	LuaNativeGameDecal::AbortWorldFrame();
	LuaNativeUnitPrimitive::AbortWorldFrame();
	LuaNativeStartBox::AbortWorldFrame();
	LuaNativeWeaponRange::AbortWorldFrame();
	LuaNativeSensor31657Circle::AbortWorldFrame();
	LuaNativeSensorPreview::AbortCapture();
	LuaNativeCommands::AbortWorldFrame();
	if (session) {
		session->cpuReadReceipt220.reset();
		session->preparedFrame.reset();session->opaqueSourcePending.reset();session->actualShadowSourcePending.reset();
		session->preparedGroundDecals.reset();session->preparedGameDecals.reset();session->preparedDecalDraw.reset();
 session->mainGroundPending.reset();session->shieldCapture.Reset();session->reflectionGroundPending.reset();session->mainInitialClear.reset();
	}
}

namespace {
void SetReadbackSubmissionReceipts(Metal4WorldRenderOptions& options)
{
	options.submissionBegan=[](std::uint64_t submission,const NativePostprocessFrame* post){
		if(post)LuaNativePostprocess::GetRecorder().RecordSubmission(*post,submission);
	};
	options.submissionCompleted=[](std::uint64_t submission){
		LuaNativePostprocess::GetRecorder().RetireSubmission(submission);
	};
}
}
std::vector<std::uint8_t> ReadCurrentImage(const ui::NativeUIReadbackPrefix& prefix,
	const NativeImageReadbackRequest& request,const ReadbackWorldInputs& inputs)
{
	if(!session || !session->renderer || !globalRendering)
		throw std::runtime_error("SaveImage has no actual native scene owner");
	// These are the original selectors for the single color attachment. Zero
	// leaves the currently bound read attachment selected, as LuaOpenGL does.
	if(request.readBuffer && request.readBuffer!=(prefix.readColor?GL_COLOR_ATTACHMENT0:GL_BACK))
		throw std::runtime_error("SaveImage readbuffer does not name the actual current color attachment");
	const ScreenScissor rectangle{request.x,request.y,request.width,request.height};
	if(auto pixels=session->renderer->ReadIndependentTargetPrefix(prefix,rectangle,false))return std::move(*pixels);
	if(!inputs.worldCallinsComplete || !prefix.screenStage || !session->preparedFrame ||
		session->preparedFrame->sourceDrawFrame!=std::uint64_t(globalRendering->drawFrame))
		throw std::runtime_error("SaveImage dependency requires its completed current world callins and original screen stage");
	// Keep the captured engine frame in its Game owner until the final suffix.
	// Copying shared handles here never recaptures particles or invokes Lua.
	WorldFrame frame=*session->preparedFrame;
	frame.luaNanoFrame=inputs.luaNanoFrame;
	frame.luaProjectileEffectsFrame=inputs.luaProjectileEffectsFrame;
	frame.luaWorldDrawFrame=inputs.luaWorldDrawFrame;
	frame.luaEnergyExplosionFrame=inputs.luaEnergyExplosionFrame;
	frame.luaVolumetricCloudFrame=inputs.luaVolumetricCloudFrame;
	frame.luaLightningEffectFrame=inputs.luaLightningEffectFrame;
	frame.luaBlueprintFrame=inputs.luaBlueprintFrame;
	frame.placementFrame=inputs.placementFrame;
	frame.luaFireSmokeFrame=inputs.luaFireSmokeFrame;
	frame.luaMissileThrusterFrame=inputs.luaMissileThrusterFrame;
	frame.luaShieldFrame=inputs.luaShieldFrame;
	frame.luaFlamethrowerFrame=inputs.luaFlamethrowerFrame;
	frame.luaNapalmPreviewFrame=inputs.luaNapalmPreviewFrame;
	frame.luaCommandsFrame=inputs.luaCommandsFrame;
	frame.luaFireGL4Frame=inputs.luaFireGL4Frame;
	frame.luaBloomFrame=inputs.luaBloomFrame;
	frame.luaDeferredLightFrame=inputs.luaDeferredLightFrame;
	frame.luaUnitStencilFrame=inputs.luaUnitStencilFrame;
	frame.luaOutlineFrame=inputs.luaOutlineFrame;
	frame.luaSnowFrame=inputs.luaSnowFrame;

	auto screen=prefix.View();
	screen.logicalViewport=ScreenScissor{globalRendering->viewPosX,globalRendering->viewPosY,
		std::uint32_t(globalRendering->viewSizeX),std::uint32_t(globalRendering->viewSizeY)};
	screen.postprocess=LuaNativePostprocess::GetRecorder().SnapshotPrefix(prefix.cutoff);
	Metal4WorldRenderOptions options;
 options.environmentStage=session->environmentStage;
 options.pbrDeclarations=session->renderer->CapturePBRSource();
	options.screenUI=&screen;options.presentWorld=false;options.readback=&prefix;options.readbackRectangle=rectangle;
	options.sourceLogicalExtent=session->lastLogicalExtent;
	SetReadbackSubmissionReceipts(options);
	options.sourceCompleted=[](const Metal4WorldPresentationResult& completed){
		if(completed.completedWorldInfo[0])RetireNativeWorldInfo(completed.completedWorldInfo[0],completed.completedWorldInfo[1]);
		if(completed.completedWaterUpdates[0])session->waterCapture.DidComplete(completed.completedWaterUpdates[0],completed.completedWaterUpdates[1],completed.completedWaterUpdates[2]);
		session->opaqueSourcePending.reset();session->actualShadowSourcePending.reset();session->mainGroundPending.reset();session->shieldCapture.Reset();
	};
	auto result=session->renderer->Draw(frame,options);
 if(result.dynamicWaterGeneration)CompleteDynamicCapture(result.dynamicWaterGeneration,result.dynamicWaterSequence,result.dynamicWaterSourceDrawFrame);
	if(!result.sourceFrameCompleted || result.sourceDrawFrame!=frame.sourceDrawFrame ||
		result.completedPostprocessSerial!=screen.postprocess->serial)
		throw std::runtime_error("SaveImage did not retire its actual current source frame");
	return std::move(result.readbackPixels);
}

DrawResult DrawWorld(const ScreenFrameView* screenUI, bool presentWorld,
	std::shared_ptr<const NativeLuaNanoFrame> luaNanoFrame,
	std::shared_ptr<const NativeProjectileEffectFrame> luaProjectileEffectsFrame,
	std::shared_ptr<const NativeWorldDrawFrame> luaWorldDrawFrame,
	std::shared_ptr<const NativeEnergyExplosionFrame> luaEnergyExplosionFrame,
	std::shared_ptr<const NativeVolumetricCloudFrame> luaVolumetricCloudFrame,
	std::shared_ptr<const NativeLightningEffectFrame> luaLightningEffectFrame,
	std::shared_ptr<const NativeBlueprintFrame> luaBlueprintFrame,
	std::shared_ptr<const NativePlacementFrame> placementFrame,
	std::shared_ptr<const NativeFireSmokeFrame> luaFireSmokeFrame,
	std::shared_ptr<const NativeMissileThrusterFrame> luaMissileThrusterFrame,
	std::shared_ptr<const NativeShieldFrame> luaShieldFrame,
	std::shared_ptr<const NativeFlamethrowerFrame> luaFlamethrowerFrame,
	std::shared_ptr<const NativeNapalmPreviewFrame> luaNapalmPreviewFrame,
	std::shared_ptr<const NativeCommandsFrame> luaCommandsFrame,
	std::shared_ptr<const NativeFireGL4Frame> luaFireGL4Frame,
	std::shared_ptr<const NativeBloomFrame> luaBloomFrame,
	std::shared_ptr<const NativeDeferredLightFrame> luaDeferredLightFrame,
	std::shared_ptr<const NativeUnitStencilFrame> luaUnitStencilFrame,
	std::shared_ptr<const NativeOutlineFrame> luaOutlineFrame,
	std::shared_ptr<const NativeSnowFrame> luaSnowFrame,NativeSourceFrameCredits::Lease sourceCredit)
{
	if (!session)
		return {};
 // The next captured CPU packet already owns immutable CUS inputs. Validate
 // those reads while N may still execute, before the actual retirement below.
 // The receipt contains no GPU resource and lives through the downstream Draw.
 NativeCUSCPUReadScope cpuReadScope;
 static const bool measureCPU=[] {const char* p=std::getenv("BAR_NATIVE_RESOURCE_PROBE_FILE");return p&&p[0]=='/';}();
 const auto cpuBegin=measureCPU?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
 if(presentWorld&&session->preparedFrame){
  const auto& captured=*session->preparedFrame;
  const auto primeModels=[&](const std::shared_ptr<const NativeOpaqueModelFrame>& models){
   if(!models)return;for(const auto& pass:models->passes){cpuReadScope.Prime(pass.lua);if(pass.mrt)cpuReadScope.Prime(pass.mrt->lua);}
  };
   auto receipt=std::move(session->cpuReadReceipt220);
   if(receipt)receipt->pending.Finish(); // Synchronize before Matches, errors, digest or packet reads.
   const bool accept=receipt&&receipt->Matches(captured);
   const uint64_t digest=CPUReadPacketMeasure220()&&receipt?receipt->job.packet.RowDigest():0;
   const auto consumeStart=CPUReadPacketMeasure220()?NativeCUSCPUReadWorker220::Clock::now():NativeCUSCPUReadWorker220::Clock::time_point{};
   const bool tailAccepted=accept&&receipt->includesTail;
   const auto primeTail=[&]{for(const auto& models:captured.alphaModelFrames)primeModels(models);if(captured.originalShadow)cpuReadScope.Prime(captured.originalShadow->cus);};
   bool verified=false;
   if(accept){
    // Errors not swallowed by the original Prime are delivered at its original
    // pre-render consume boundary, including sealed tail errors. Diagnostic
    // trace/mode3 re-priming follows error delivery; no producer callback moves.
    if(receipt->job.error)std::rethrow_exception(receipt->job.error);
    if(CPUReadPacketMode220()==3){
     primeModels(captured.opaqueModelFrame);if(tailAccepted)primeTail();
     bool optionalReference=true;
     try{if(receipt->waterReads)for(const auto& source:receipt->waterReads->sources)cpuReadScope.Prime(source);}catch(...){optionalReference=false;}
     if(optionalReference){if(!cpuReadScope.Matches(receipt->job.packet))throw std::runtime_error("CUS opaque CPU receipt differs from original rows/owners/capacity");verified=true;}

    }else cpuReadScope.Adopt(std::move(receipt->job.packet));
   }else primeModels(captured.opaqueModelFrame);
   const uint64_t consumeNs=CPUReadPacketMeasure220()?NativeCUSCPUReadWorker220::Elapsed(consumeStart):0;
   TraceCPUReadPacket220(captured.sourceDrawFrame,captured.simulationFrame,receipt.get(),accept,verified,session->cpuReadFallback220,cpuReadScope.Count(),cpuReadScope.Bytes(),consumeNs,CPUReadPacketMeasure220()?NativeCUSCPUReadWorker220::Timestamp(consumeStart):0,digest);
   if(!tailAccepted)primeTail();
 }
  session->cpuReadReceipt220.reset(); // Also release a skipped/hidden world's receipt on main.
 if(measureCPU)cpuReadScope.SetPreparationNanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-cpuBegin).count());
 // This owner pins N+1's exact source and immutable shared geometry across
 // retirement of N. No command, residency, producer image or published
 // completion is touched until the existing retirement boundary below.
 static const bool prepareOverlap=[] {const char* value=std::getenv("BAR_NATIVE_PREPARE_OVERLAP");return !value||std::string_view(value)=="1";}();
 const auto independentResources=(prepareOverlap&&presentWorld&&session->preparedFrame&&session->renderer)?
  session->renderer->PrepareIndependentResources(*session->preparedFrame):std::shared_ptr<const metal4::ShadowTerrainPreparation>{};
	// Consume even if the window became hidden or its drawable is unavailable.
 // CPU capture is complete. Retire N before touching shared GPU preparation
 // caches/targets for N+1. Completion identifies only N's producer records.
 static const bool prepare137=[] {const char* value=std::getenv("BAR_NATIVE_PREPARE137");return !value||std::string_view(value)=="1";}();
 const bool overlapPreparation=prepare137&&presentWorld&&session->preparedFrame&&session->renderer&&
  !session->renderer->HasScreenContinuation()&&session->surface.Snapshot().CanPresent();
 if(!overlapPreparation)RequireRetiredGPUWork();
	// A later presentation must use its own engine/Lua world callback ordering.
	struct LegacyCopyExit {~LegacyCopyExit(){
  // A failed actual Begin may precede EncodeCopy, so resource encoding flags
  // alone cannot authorize rollback of this still-quarantined source owner.
  if(session->renderer&&!session->renderer->CanAbortUnsubmittedSource())return;
  session->legacyWaterCapture.AbortUncompletedRefraction();
 }} legacyCopyExit;
	auto prepared = std::move(session->preparedFrame);
	session->preparedFrame.reset();
 // Refresh writes CAMetalLayer properties, not just an immutable observation.
 // The pilot conservatively ends CPU overlap here; no surface revision proof yet.
 if(BeginReadyPilotRequested()&&!PrepareReadyPilotRequested())RequireRetiredGPUWork();
	session->surface.Refresh();
 const auto pbrDeclarations=session->renderer?session->renderer->CapturePBRSource():NativePBRDeclarationReceipt{};
	const auto submitTargets = [&]() -> DrawResult {
		const auto independent=screenUI?std::count_if(screenUI->targetPasses.begin(),screenUI->targetPasses.end(),[](const auto& pass){return !pass.sourceDependent && pass.sourceStage==NativePostprocessStage::Screen;}):0;
		auto worldInfo = CaptureNativeWorldInfo();
		const bool stencilUpdates=luaUnitStencilFrame && !luaUnitStencilFrame->passes.empty();
		if (!independent && !stencilUpdates && !worldInfo && !session->renderer->HasPendingPBRSource(pbrDeclarations)){session->genesisSourcePending.reset();return {};}
		if (!session->renderer) {
			session->renderer = std::make_unique<Metal4WorldRenderer>(session->surface,session->requestedMSAA);
			globalRendering->msaaLevel=int(session->renderer->ActualMainSampling().effective);
		}
		const auto maintenance = session->renderer->SubmitMaintenance(screenUI, std::move(worldInfo), false, luaUnitStencilFrame,pbrDeclarations);
		if (maintenance.completedWorldInfo[0])
			RetireNativeWorldInfo(maintenance.completedWorldInfo[0], maintenance.completedWorldInfo[1]);
		if (maintenance.screenTargetsCompleted) {
			if (session->targetOnlySubmissions++ == 0)
				LOG("[Metal4Scene] First completed offscreen UI submission: %zu independent target passes, no drawable or present", std::size_t(independent));
			session->targetOnlyPasses += independent;
			session->awaitingPresentationAfterTargets = true;
		}
		session->genesisSourcePending.reset();
		return {false, maintenance.screenTargetsCompleted};
	};
	auto sourcePlan=ChooseNativeSourceSubmission(presentWorld,bool(prepared),session->surface.Snapshot().CanPresent(),screenUI);
	if(session->renderer->HasScreenContinuation())
		sourcePlan=presentWorld && session->surface.Snapshot().CanPresent()?NativeSourceSubmission::PresentWorld:NativeSourceSubmission::OffscreenSource;
	if(sourcePlan==NativeSourceSubmission::IndependentTargets && prepared &&
		((prepared->legacyWater&&prepared->legacyWater->refraction) || prepared->originalShadow || (prepared->mainGround&&prepared->mainGround->ground&&prepared->mainGround->ground->mapMRT) || (prepared->opaqueModelFrame&&prepared->opaqueModelFrame->HasWriters()) || (prepared->shieldDepthFrame && prepared->shieldDepthFrame->HasWriters()) || session->renderer->HasPendingEnvironmentSource(session->environmentStage,pbrDeclarations) || HasNativeDeferredPostprocessWrites(*prepared,luaBloomFrame)))
		sourcePlan=NativeSourceSubmission::OffscreenSource;
	if (sourcePlan==NativeSourceSubmission::IndependentTargets){
  if(overlapPreparation)RequireRetiredGPUWork();
  if(!session->dynamicWaterCapture.CanAbortCapture(session->dynamicCaptureReceipt))throw std::logic_error("Cannot discard admitted dynamic water");
		if(!session->renderer->AbortShieldPublications(prepared?prepared->opaqueModelFrame:nullptr,session->actualShadowSourcePending,prepared?prepared->mainGround:session->mainGroundPending,session->environmentStage,&session->shieldCapture))throw std::logic_error("Cannot discard failed-Begun source owners");session->opaqueSourcePending.reset();session->actualShadowSourcePending.reset();session->mainGroundPending.reset();session->shieldCapture.Reset();if(!AbortDynamicCapture())throw std::logic_error("Cannot abort dynamic capture");return submitTargets();
	}

	auto frameOwner=std::make_shared<WorldFrame>(std::move(*prepared));
 auto& capturedFrame=*frameOwner;
	capturedFrame.luaNanoFrame = std::move(luaNanoFrame);
	capturedFrame.luaProjectileEffectsFrame = std::move(luaProjectileEffectsFrame);
	capturedFrame.luaEnergyExplosionFrame = std::move(luaEnergyExplosionFrame);
	capturedFrame.luaWorldDrawFrame = std::move(luaWorldDrawFrame);
	capturedFrame.luaVolumetricCloudFrame = std::move(luaVolumetricCloudFrame);
	capturedFrame.luaLightningEffectFrame = std::move(luaLightningEffectFrame);
	capturedFrame.luaFireSmokeFrame = std::move(luaFireSmokeFrame);
	capturedFrame.luaMissileThrusterFrame = std::move(luaMissileThrusterFrame);
	capturedFrame.luaShieldFrame = std::move(luaShieldFrame);
	capturedFrame.luaFlamethrowerFrame = std::move(luaFlamethrowerFrame);
	capturedFrame.luaFireGL4Frame = std::move(luaFireGL4Frame);
	capturedFrame.luaNapalmPreviewFrame = std::move(luaNapalmPreviewFrame);
	capturedFrame.luaSnowFrame = std::move(luaSnowFrame);
	capturedFrame.luaCommandsFrame = std::move(luaCommandsFrame);
	capturedFrame.luaBloomFrame=std::move(luaBloomFrame);
	capturedFrame.luaDeferredLightFrame=std::move(luaDeferredLightFrame);
	// Keep the Genesis source available to target-only fallback if presentation is unavailable.
	capturedFrame.luaUnitStencilFrame=luaUnitStencilFrame;
	capturedFrame.luaOutlineFrame=std::move(luaOutlineFrame);
	capturedFrame.luaBlueprintFrame = std::move(luaBlueprintFrame);
	capturedFrame.placementFrame = std::move(placementFrame);
	if (!CUnitDrawer::UseScreenIcons())
		capturedFrame.unsupported.push_back("World-space unit icon mode selected; native world-icon parity remains unverified (screen/minimap paths are separate)");
	for (const auto& unsupported: capturedFrame.unsupported) {
		if (session->reportedUnsupported.insert(unsupported).second)
			LOG_L(L_WARNING, "[Metal4Scene] Rendering path needs review: %s", unsupported.c_str());
	}
	if (!session->renderer) {
		session->renderer = std::make_unique<Metal4WorldRenderer>(session->surface,session->requestedMSAA);
		globalRendering->msaaLevel=int(session->renderer->ActualMainSampling().effective);
	}
	Metal4WorldRenderOptions options;
 options.preparedShadowTerrain=independentResources;
 session->sourceCredits.Validate(sourceCredit,capturedFrame.sourceDrawFrame);options.sourceCredit=sourceCredit;
 // Preserve the already-built exact read receipt across the joined worker.
 // Allocate before moving rows; independent main preparation has finished.
 auto cpuReadPacket=std::make_shared<NativeCUSCPUReadScope::Packet>();
 *cpuReadPacket=cpuReadScope.Seal();options.cpuReadPacket=cpuReadPacket;
 options.environmentStage=session->environmentStage;
 options.pbrDeclarations=pbrDeclarations;
	options.screenUI = screenUI;
	options.presentWorld=sourcePlan==NativeSourceSubmission::PresentWorld;
	options.sourceLogicalExtent=session->lastLogicalExtent;
 if(session->renderer->HasScreenContinuation())SetReadbackSubmissionReceipts(options);
 static const bool overlap=[] {const char* value=std::getenv("BAR_NATIVE_FRAME_OVERLAP");return !value||std::string_view(value)=="1";}();
 CloudAdmissionOwner cloudAdmission;
 DynamicWaterAdmissionOwner waterAdmission{session->dynamicWaterCapture,capturedFrame.dynamicWater};
 NativeFrameAdmissionTransaction admission(ui::GetRecorder(),LuaNativePostprocess::GetRecorder(),cloudAdmission,waterAdmission,
  capturedFrame.sourceDrawFrame,screenUI&&screenUI->postprocess?screenUI->postprocess->serial:0);
 std::shared_ptr<NativeFrameCompletionMailbox::Ticket> completionTicket;
 if(overlap&&options.presentWorld&&capturedFrame.simulationFrame>0&&screenUI&&screenUI->postprocess&&!session->renderer->HasScreenContinuation()) {
  const auto expectedDraw=capturedFrame.sourceDrawFrame,expectedPost=screenUI->postprocess->serial;
  const auto mailbox=session->completionMailbox;
  // Preserve the prior renderer boundary's ability to drain full held ledgers.
  // Do not reject a capturedFrame solely because its predecessors have not retired yet.
  if(!ui::GetRecorder().HasSubmittedCapacity(expectedDraw) ||
     !LuaNativePostprocess::GetRecorder().HasSubmittedCapacity(expectedPost) ||
     !LuaNativeVolumetricClouds::HasSubmittedCapacity(expectedDraw) || !mailbox->HasCapacity())RequireRetiredGPUWork();
  // Main remains blocked through Draw: these checks cannot race another capture.
  admission.Validate();
  options.beginReceipt=std::make_shared<NativeFrameBeginReceipt>(expectedDraw,expectedPost);
  const auto ticket=completionTicket=mailbox->Prepare(mailbox->Epoch(),expectedDraw,expectedPost);
  options.deferredCompleted=[mailbox,ticket](const Metal4WorldPresentationResult& completed) {
   mailbox->Publish(ticket,{completed.sourceFrameCompleted,
    completed.sourceDrawFrame,completed.completedPostprocessSerial,completed.dynamicWaterGeneration,
    completed.dynamicWaterSequence,completed.dynamicWaterSourceDrawFrame,
    completed.completedWorldInfo,completed.completedWaterUpdates});
  };
 }
 // All allocations, callback construction and validation precede any move.
 // Capture stays main-blocked; the three no-throw moves are one admission step.
 bool admitted=bool(completionTicket);
 if(admitted)options.heldSourceDraw=capturedFrame.sourceDrawFrame;
 // The prepare-ready experiment supersedes the narrower Begin-ready mode.
 options.allowPrepareReady=PrepareReadyPilotRequested()&&admitted&&!capturedFrame.dynamicWater&&!capturedFrame.legacyWater;
 options.allowBeginReady=BeginReadyPilotRequested()&&!PrepareReadyPilotRequested()&&admitted&&!capturedFrame.dynamicWater&&!capturedFrame.legacyWater;
 // Begin alone does NOT retire N-1. This pilot permits only N plus capture N+1.
 if(options.allowBeginReady)RequireRetiredGPUWork();
 const auto submission=std::make_shared<const Metal4WorldSubmission>(frameOwner,options);
 frameOwner.reset(); // No mutable owning alias remains after sealing.
 const auto& frame=submission->Frame();
 std::shared_ptr<ScenePendingAdmission> pendingAdmission;
 if(admitted){
  if(session->sourceSettlement)throw std::logic_error("An earlier source settlement remains quarantined");
  if(session->opaqueSourcePending!=frame.opaqueModelFrame || session->actualShadowSourcePending!=frame.originalShadow ||
     session->mainGroundPending!=frame.mainGround || session->shieldCapture.Receipt()!=frame.shieldDepthFrame ||
     !session->environmentStage.Matches(options.environmentStage.frame))
   throw std::logic_error("Source settlement differs from its sealed render packet");
  if(bool(session->dynamicCaptureReceipt)!=bool(frame.dynamicWater) ||
     (session->dynamicCaptureReceipt && (session->dynamicCaptureReceipt->sourceDrawFrame!=frame.sourceDrawFrame ||
     frame.dynamicWater->sourceDrawFrame!=frame.sourceDrawFrame || session->dynamicCaptureReceipt->material!=frame.dynamicWater->material ||
     session->dynamicCaptureReceipt->throughUpdate!=frame.dynamicWater->throughUpdate)))
   throw std::logic_error("Dynamic rollback receipt differs from its finalized render frame");
  if(session->admissionSettlement)throw std::logic_error("Earlier exact admission remains unsettled");
  pendingAdmission=std::make_shared<ScenePendingAdmission>(*session,submission,completionTicket);
  session->admissionSettlement=pendingAdmission; // Durable before any transfer or GPU Begin.
  try{pendingAdmission->Transfer();}catch(...){session->admissionSettlement.reset();throw;}
 }
 const auto restoreAdmitted=[pendingAdmission,&admitted]{
  pendingAdmission->Restore();pendingAdmission->ReleaseAnchor();admitted=false;
 };
 const auto quarantineAdmitted=[pendingAdmission]() noexcept {pendingAdmission->Quarantine();};
 const auto failedAdmission=[pendingAdmission]{
  if(pendingAdmission&&!pendingAdmission->failureHandled&&!pendingAdmission->cpuAccepted){pendingAdmission->Fail();if(!pendingAdmission->transferred)pendingAdmission->ReleaseAnchor();}
 };
 const auto cpuSettled=[pendingAdmission](const Metal4WorldPresentationResult* result,std::exception_ptr failure){
  if(pendingAdmission)pendingAdmission->CompleteCPU(result,failure);
 };
 const auto invokeRenderer=[&] {
 try {return (overlapPreparation&&!options.allowBeginReady)?
  session->renderer->DrawOwned(submission,[owner=session.get()]{
   if(session.get()!=owner)throw std::logic_error("Preparation retirement lost its scene owner");
   ApplyDeferredFrameReceipts();
  },cpuSettled):session->renderer->DrawOwned(submission,{},cpuSettled);
 }catch(...){
  const auto failure=std::current_exception();
  try{failedAdmission();}catch(...){/* Keep the durable held record and original renderer failure. */}
  std::rethrow_exception(failure);
 }
 };
 const auto result=invokeRenderer();
 if(result.queued){
  try{
  if(!pendingAdmission||!pendingAdmission->transferred||result.presented||result.sourceFrameCompleted||result.screenTargetsCompleted||result.completedPostprocessSerial||
   result.sourceDrawFrame!=frame.sourceDrawFrame||(!options.allowPrepareReady&&!options.beginReceipt->Submission()))
   throw std::logic_error("Queued renderer return lacks its exact admitted source owner");
  pendingAdmission->MarkQueued();
  // No current pending reset, GPU retirement or presentation report here.
  return {false,false,false,0,frame.sourceDrawFrame,true,screenUI->postprocess->serial};
  }catch(...){const auto failure=std::current_exception();try{failedAdmission();}catch(...){}std::rethrow_exception(failure);}
 }
 if(pendingAdmission&&pendingAdmission->cpuAccepted)admitted=false;
 try {
 // A hidden/unavailable surface may return before opening a runtime lease.
 if(!result.presented&&overlapPreparation)RequireRetiredGPUWork();
 if(admitted && (!result.presented || result.sourceFrameCompleted)){
  // Synchronous completion returns ownership to the historical pending-retire
  // caller. No-Begin fallback returns pending publications for original rollback.
  if(!result.sourceFrameCompleted && options.beginReceipt->Submission()){
   quarantineAdmitted();throw std::logic_error("Admitted frame began without a completed or deferred result");
  }
  restoreAdmitted();
 }
 if(admitted) {
  pendingAdmission->Accept(result);
  pendingAdmission->ReleaseAnchor();
 }
 }catch(...){const auto failure=std::current_exception();try{failedAdmission();}catch(...){}std::rethrow_exception(failure);}
 if(result.dynamicWaterGeneration)CompleteDynamicCapture(result.dynamicWaterGeneration,result.dynamicWaterSequence,result.dynamicWaterSourceDrawFrame);
 if(result.sourceFrameCompleted){session->opaqueSourcePending.reset();session->actualShadowSourcePending.reset();session->mainGroundPending.reset();session->shieldCapture.Reset();}
 if(result.completedWorldInfo[0])RetireNativeWorldInfo(result.completedWorldInfo[0],result.completedWorldInfo[1]);
 if(result.completedWaterUpdates[0])session->waterCapture.DidComplete(result.completedWaterUpdates[0],result.completedWaterUpdates[1],result.completedWaterUpdates[2]);
	if (!result.presented) {
		if(!result.sourceFrameCompleted){if(!session->dynamicWaterCapture.CanAbortCapture(session->dynamicCaptureReceipt))throw std::logic_error("Cannot discard admitted dynamic water");if(!session->renderer->AbortShieldPublications(frame.opaqueModelFrame,session->actualShadowSourcePending,frame.mainGround,session->environmentStage,&session->shieldCapture))throw std::logic_error("Cannot discard failed-Begun source owners");session->opaqueSourcePending.reset();session->actualShadowSourcePending.reset();session->mainGroundPending.reset();session->shieldCapture.Reset();if(!AbortDynamicCapture())throw std::logic_error("Cannot abort dynamic capture");}
		if(result.sourceFrameCompleted)return {false,result.screenTargetsCompleted,true,result.completedPostprocessSerial,result.sourceDrawFrame};
		return submitTargets();
	}
 return (pendingAdmission&&pendingAdmission->cpuAccepted)?
  DrawResult{true,result.screenTargetsCompleted,result.sourceFrameCompleted,result.completedPostprocessSerial,result.sourceDrawFrame}:
  ReportPresentedWorldResult(*session,frame,result,screenUI);
}

void ResetWorld()
{
	// Original Lua Shutdown still needs the shared source services.
	SetNativeSSAOConsumerAvailable(false);
	SetNativeBloomConsumerAvailable(false);
	if (!session)
		return;
	RequireRetiredGPUWork();
 if(session->renderer)session->renderer->AbortShieldPublications(session->opaqueSourcePending,session->actualShadowSourcePending,session->mainGroundPending,session->environmentStage,&session->shieldCapture);
	if (session->presentedFrames > 0)
		LOG("[Metal4Scene] Retired world after %llu presented frames; last simulation frame %llu",
			static_cast<unsigned long long>(session->presentedFrames),
			static_cast<unsigned long long>(session->lastPresentedSimulationFrame));
	if (session->targetOnlySubmissions > 0)
		LOG("[Metal4Scene] Retired offscreen UI: %llu submissions, %llu target passes",
			static_cast<unsigned long long>(session->targetOnlySubmissions),
			static_cast<unsigned long long>(session->targetOnlyPasses));
	// Keep the actually compiled native effect families available before the
	// next game's original Lua Initialize callbacks; clear all per-game caches.
	if (session->renderer) session->renderer->ClearStaticResources(session->environmentStage,&session->shieldCapture);
 session->environmentStage={};
	ResetNativeWorldInfo();
	ResetMetalInfoResources();
	EndLoadingScreen();
	session->preparedFrame.reset();
	session->preparedGroundDecals.reset();session->preparedGameDecals.reset();session->preparedDecalDraw.reset();
 session->mainGroundPending.reset();session->shieldCapture.Reset();session->reflectionGroundPending.reset();session->mainInitialClear.reset();
	session->waterCapture.Reset();
	session->basicWaterCapture.Reset();session->legacyWaterCapture.Reset();
	session->capture.Reset();
	ResetNativeCUSMaterialState();
	LuaNativeNano::Reset();
	LuaNativePostprocess::Reset();
	LuaNativeVolumetricClouds::Reset();
	LuaNativeProjectileEffects::Reset();
	LuaNativeEnergyExplosion::Reset();
	LuaNativeLightning::Reset();
	LuaNativePiP::Reset();
	LuaNativeBlueprint::Reset();
	LuaNativeBlockout::Reset();
	LuaNativeFireSmoke::Reset();
	LuaNativeMissileThruster::Reset();
	LuaNativeFlamethrower::Reset();
	LuaNativeFireGL4::Reset();
	LuaNativeNapalmPreview::Reset();
	LuaNativeSnow::Reset();
	LuaNativeCommands::Reset();
	LuaNativeGameDecal::Reset();
	LuaNativeUnitPrimitive::Reset();
	LuaNativeStartBox::Reset();
	LuaNativeWeaponRange::Reset();
	LuaNativeSensor31657Circle::Reset();
	LuaNativeSensorPreview::Reset();
	session->mapTextures.reset();session->reportedFirstStartBoxWorld=false;session->reportedFirstStartBoxMiniMap=false;
	LuaNativeShield::Reset();
	LuaNativeSSAO::Reset();
	LuaNativeBloom::Reset();
	LuaNativeDeferredLights::Reset();LuaNativeDistortion::Reset();
	LuaNativeUnitStencil::Reset();
	LuaNativeOutline::Reset();
	LuaNativePlacement::Reset();
	GetWorldDrawRecorder().Reset();
	session->reportedUnsupported.clear();
	session->presentedFrames = 0;
	session->lastPresentedSimulationFrame = 0;
	session->reportedCUSRevision = 0;
	session->reportedAdvancedTerrain.reset();
	session->reportedReflectionReady.reset();
	session->reportedSourcePaths.reset();
	session->lastSSAOImageIdentity = 0;
	session->targetOnlySubmissions = 0;
	session->targetOnlyPasses = 0;
	session->awaitingPresentationAfterTargets = false;
	session->reportedFirstUnits = false;
	session->reportedFirstFeatures = false;
	session->reportedFirstParticles = false;
	session->reportedFirstLuaNano = false;
	session->reportedFirstWorldOverlays = false;
	session->reportedFirstProjectileEffects = false;
	session->reportedFirstEnergyExplosion = false;
	session->reportedFirstLightning = false;
	session->reportedFirstDeferredLights = false;
	session->reportedFirstFireSmoke = false;
	session->reportedFirstMissileThruster = false;
	session->reportedFirstFlamethrower = false;
	session->reportedFirstNapalmPreview = false;
	session->reportedFirstSnow = false;
	session->reportedFirstCommands = false;
	session->reportedFirstGameDecal = false;
	session->reportedFirstUnitPrimitive = {};
	session->reportedFirstShield=false;
	session->reportedFirstBlueprint = false;
	session->reportedFirstBuildingSample = false;
	session->reportedFirstBuildingGrid = false;
	session->reportedFirstPiP = false;
	session->reportedFirstUI = false;
	session->reportedFirstPostprocess = false;
}

void DetachOriginalModelSourcesAfterLua()
{
	LuaNativeDeferredLights::Configure({});LuaNativeDistortion::Configure({});
	LuaNativeUnitStencil::Configure({});
	LuaNativeOutline::Configure({});
	if (!session) return;
	RequireRetiredGPUWork();
	SetNativeSSAOConsumerAvailable(false);
	SetNativeBloomConsumerAvailable(false);
	if (session->cusModelSource) session->cusModelSource->Reset();
 session->originalPrograms.Reset();session->originalShadowPrograms.Reset();session->legacyServices={};
 session->opaqueSourcePending.reset();session->genesisSourcePending.reset();session->actualShadowSourcePending.reset();
 if(session->renderer)session->renderer->ClearOriginalModelSources();
}

void Shutdown()
{
	LuaNativeDeferredLights::Configure({});LuaNativeDistortion::Configure({});
	LuaNativeUnitStencil::Configure({});
	LuaNativeOutline::Configure({});
	SetNativeSSAOConsumerAvailable(false);
	SetNativeBloomConsumerAvailable(false);
	RequireRetiredGPUWork();
	SetNativeGroundDecalConsumerAvailable(false);SetNativeGroundDecalImageProvider({});
	SetNativeGameDecalConsumerAvailable(false);
	SetNativeUnitPrimitiveConsumerAvailable(false);
	SetNativeCommandsConsumerAvailable(false);
	if (session) {
		if (session->renderer) session->renderer->ClearStaticResources(session->environmentStage,&session->shieldCapture);
 session->environmentStage={};
		ResetNativeWorldInfo();
		ShutdownMetalInfoMaintenance();
	}
	LuaNativeBlueprint::Reset();
	LuaNativeBlockout::Reset();
	LuaNativeFireSmoke::Reset();
	LuaNativeMissileThruster::Reset();
	LuaNativeFlamethrower::Reset();
	LuaNativeFireGL4::Reset();
	LuaNativeNapalmPreview::Reset();
	LuaNativeSnow::Reset();
	LuaNativeCommands::Reset();
	LuaNativeGameDecal::Reset();
	LuaNativeUnitPrimitive::Reset();
	LuaNativeStartBox::Reset();
	LuaNativeWeaponRange::Reset();
	LuaNativeSensor31657Circle::Reset();
	LuaNativeSensorPreview::Reset();
	if(session){session->mapTextures.reset();session->reportedFirstStartBoxWorld=false;session->reportedFirstStartBoxMiniMap=false;}
	LuaNativeShield::Reset();
	LuaNativeSSAO::Reset();
	LuaNativeBloom::Reset();
	LuaNativeDeferredLights::Reset();LuaNativeDistortion::Reset();
	LuaNativeUnitStencil::Reset();
	LuaNativeOutline::Reset();
	LuaNativePlacement::Reset();
	LuaNativeBlueprint::SetModelProvider({});
	LuaNativeBlockout::Configure({},{});
	LuaNativeCommands::SetModelProvider({});
	NativeTerraformCaptureModels()={};
	LuaNativePlacement::SetModelProvider({});
	LuaNativePiP::Reset();
	LuaNativePiP::SetDrawSink({}, false);
	LuaNativePiP::SetModelProvider({});
	session.reset();
}

bool IsInitialized()
{
	return session != nullptr;
}

} // namespace rendering::scene
