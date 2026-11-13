#!/usr/bin/env python3
"""Actual gate/executor protocol + structural native cleanup control routing."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-renderer-control-order');out.mkdir(exist_ok=True)
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'static NativeSerialRenderWorker worker;' not in renderer
assert 'auto& worker=*impl->renderWorker;' in renderer
control=renderer.split('void Metal4WorldRenderer::ClearWaterFamily(',1)[1].split('bool Metal4WorldRenderer::DeferredPostprocessConsumersInitialized',1)[0]
for method,family in [('ClearWaterResources','Bump'),('ClearBasicWaterResources','Basic'),('ClearDynamicWaterResources','Dynamic'),('ClearLegacyWaterResources','Legacy')]:
 assert 'void Metal4WorldRenderer::'+method+'(){ClearWaterFamily(WaterReset::'+family+');}' in renderer
dynamic=control.split('case WaterReset::Dynamic:',1)[1].split('case WaterReset::Legacy:',1)[0]
assert dynamic.index('dynamicWaterGPU.ClearResources()')<dynamic.index('owner->waterReflection=nil')
assert control.index('SharedRuntime().CanSafelyDetach()')<control.index('Handoff handoff')<control.index('cacheAccess.reset()')<control.index('renderWorker->Invoke')
assert 'PublishWorkerSnapshot' not in control and 'Metal4SurfaceWorkerScope' not in control
worker=control.split('renderWorker->Invoke',1)[1]
assert worker.index('Retirement::RequireAlreadyRetired')<worker.index('clear();')
runtime=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text().split('RuntimeCleanupLease::RuntimeCleanupLease',1)[1].split('void RuntimeLease::SetMinSampleShadingRate',1)[0]
assert runtime.index('Retirement::RequireAlreadyRetired')<runtime.index('runtime.pendingSubmissions.empty()')<runtime.index('else if (!runtime.poisoned) runtime.DrainPendingLocked()')<runtime.index('runtime.persistentResidency.reset()')
destructor=renderer.split('~Impl()',1)[1].split('void RetainTextureAllocations',1)[0]
assert destructor.index('renderWorker.reset()')<destructor.index('SetNativeCUSMaterialConsumerReady(false)')
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeRendererControlOrder.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)

service=(root/'rts/Rendering/Metal4/Metal4PBRSourceService.mm').read_text()
compile_callback=service.split('services.compile=',1)[1].split('services.allocate=',1)[0]
assert compile_callback.index('Require([NSThread isMainThread]')<compile_callback.index('NativeRendererAccessGate::Scope owner(gate)')<compile_callback.index('runtime.CanSafelyDetach()')<compile_callback.index('InvokeNativeRendererControl')<compile_callback.index('RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime)')<compile_callback.index('gpu.Compile')
assert '[this,gate=&access,renderWorker=&worker,finishCPU=std::move(finishCPU)]' in compile_callback
assert compile_callback.index('finishCPU();')<compile_callback.index('NativeRendererAccessGate::Scope owner(gate)')
assert 'make_shared<const NativePBRProgramSource>(source)' in compile_callback
assert 'gpu.Compile(lease,*captured,validation,compileMilliseconds)' in compile_callback
assert compile_callback.count('RuntimeLease lease')==1 and 'Metal4WindowSurfaceNativeAccess' not in compile_callback
assert 'impl->pbrSource.Initialize(context,impl->access,impl->renderWorker,' in renderer
shutdown=service.split('void PBRSourceService::Shutdown()',1)[1].split('bool PBRSourceService::Ready()',1)[0]
assert shutdown.index('LuaNativePBRSource::Configure({},nullptr,nullptr)')<shutdown.index('context=nullptr')
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
detach=bridge.split('if(pbrTextureSourceAttached){renderer->ConnectPBRSourceContext(false)',1)[1]
assert detach.index('pbrTextureSourceAttached=false')<detach.index('context.Destroy()')
print('Persistent PBR stable slot/main-drain/worker-only lease and source shutdown structural PASS; callback adapter is not native Lua execution')
