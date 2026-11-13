#include "Rendering/Backend/NativeRendererControl.h"
#include "Rendering/Backend/NativeDeferredLightData.h"
#include "Rendering/Backend/NativeUnitStencilData.h"
#include "Rendering/Backend/NativeOutlineData.h"
#include "Rendering/Backend/NativeDistortionData.h"
#include "Rendering/Backend/NativeSensorPreviewData.h"
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
 Check(gate,worker,&NativeTerrainProgramRequest::vertexSource);Check(gate,worker,&NativeDeferredLightProgramSource::vertex);Check(gate,worker,&NativeUnitStencilProgramSource::vertex);Check(gate,worker,&NativeOutlineProgramSource::vertex);Check(gate,worker,&NativeDistortionProgramSource::vertex);Check(gate,worker,&NativeSensorPreviewProgramSource::vertex);
 auto unsupported=InvokeNativeRendererControl(worker,gate,NativeTerrainProgramRequest{},[](const auto&,const auto&){return NativeTerrainProgramLink{nullptr,"unsupported source",{}};});assert(!unsupported.consumer&&unsupported.unsupportedReason=="unsupported source"&&unsupported.linkDefaults.empty());
 }
 std::cout<<"Six actual source types/control/worker:600 owned-copy handoffs+failure recovery,100 actual terrain unsupported return envelopes PASS (no native compiler execution)\n";
}
