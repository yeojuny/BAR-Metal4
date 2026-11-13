#include "Rendering/Backend/NativeRendererControl.h"
#include "Rendering/Backend/NativeDeferredLightData.h"
#include "Rendering/Backend/NativeUnitStencilData.h"
#include "Rendering/Backend/NativeOutlineData.h"
#include "Rendering/Backend/NativeDistortionData.h"
#include "Rendering/Backend/NativeSensorPreviewData.h"
#include "Rendering/Backend/NativePBRSourceData.h"
#include "Rendering/EngineBridge/NativeTerrainProgramSource.h"
#include <cassert>
#include <iostream>
using namespace rendering;
template<class T>void Check(NativeRendererAccessGate& gate,std::unique_ptr<NativeSerialRenderWorker>& worker,std::string T::* field){
 static_assert(std::is_copy_constructible_v<T>);T source;source.*field=std::string(512,'x');auto owned=std::make_shared<const T>(source);source.*field="changed";
 auto returned=InvokeNativeRendererControl(worker,gate,owned,[field](const auto& captured,const auto&){assert((captured.get()->*field)==std::string(512,'x'));return captured;});assert(returned==owned);
 bool caught=false;try{InvokeNativeRendererControl(worker,gate,owned,[](const auto&,const auto&)->int{throw std::runtime_error("compile failure");});}catch(const std::runtime_error&){caught=true;}assert(caught);
 assert(InvokeNativeRendererControl(worker,gate,owned,[](const auto&,const auto&){return 7;})==7);
}
int main(){NativeRendererAccessGate gate;std::unique_ptr<NativeSerialRenderWorker> worker;
 for(int i=0;i<100;++i){
 Check(gate,worker,&NativeTerrainProgramRequest::vertexSource);Check(gate,worker,&NativeDeferredLightProgramSource::vertex);Check(gate,worker,&NativeUnitStencilProgramSource::vertex);Check(gate,worker,&NativeOutlineProgramSource::vertex);Check(gate,worker,&NativeDistortionProgramSource::vertex);Check(gate,worker,&NativeSensorPreviewProgramSource::vertex);Check(gate,worker,&NativePBRProgramSource::vertex);
 auto unsupported=InvokeNativeRendererControl(worker,gate,NativeTerrainProgramRequest{},[](const auto&,const auto&){return NativeTerrainProgramLink{nullptr,"unsupported source",{}};});assert(!unsupported.consumer&&unsupported.unsupportedReason=="unsupported source"&&unsupported.linkDefaults.empty());
 }
 // Controlled callback adapter uses the production helper and actual source DTO.
 // Its initializer stack expires before execution, just as persistent Lua services do.
 std::function<std::shared_ptr<const NativePBRProgramSource>(const NativePBRProgramSource&)> persistent;
 {auto& slot=worker;auto& access=gate;
  persistent=[renderWorker=&slot,gate=&access](const NativePBRProgramSource& source){
   auto request=std::make_shared<const NativePBRProgramSource>(source);
   return InvokeNativeRendererControl(*renderWorker,*gate,std::move(request),[](const auto& captured,const auto&){return captured;});
  };
 }
 NativePBRProgramSource pbr;pbr.vertex="vertex";pbr.fragment="fragment";pbr.initialInt["samples"]={1024};
 auto exact=persistent(pbr);pbr.initialInt["samples"][0]=1;pbr.vertex="changed";
 assert(exact->initialInt.at("samples")[0]==1024&&exact->vertex=="vertex"&&exact->fragment=="fragment");
 persistent={};worker.reset();
 std::cout<<"Seven actual source types/control/worker:700 owned-copy handoffs+failure recovery,100 actual terrain unsupported return envelopes PASS (no native compiler execution)\n";
}
