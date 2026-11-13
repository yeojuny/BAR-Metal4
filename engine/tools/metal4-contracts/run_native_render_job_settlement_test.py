#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-render-job-settlement');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
a=s.split('Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldRenderer::DrawPrepared(',1)[0]
assert a.index('FinishCPUWork();')<a.index('cacheAccess.emplace')
assert 'impl->cpuJob->Submit(worker,' in a
assert a.index('FinishCPUWork();',a.index('impl->cpuJob->Submit'))<a.index('impl->cpuJob->Consume()')
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text().split('void RequireRetiredGPUWork()',1)[1]
assert scene.index('FinishCPUWork()')<scene.index('Metal4RendererCanSafelyDetachSurface()')
pbr=(root/'rts/Rendering/Metal4/Metal4PBRSourceService.mm').read_text().split('services.compile=',1)[1]
assert pbr.index('finishCPU();')<pbr.index('NativeRendererAccessGate::Scope')
assert '[owner=impl.get()]{owner->FinishCPUWork();}' in s
for name in ['CompileOriginalCUSProgram','CompileOriginalTerrainProgram','CompileOriginalSensorPreviewProgram','CompileOriginalDeferredLightProgram','CompileOriginalUnitStencilProgram','CompileOriginalOutlineProgram','CompileOriginalDistortionProgram','CheckShieldGeometryAllocation','AllocateCoverageTarget','PrepareIndependentResources','HasScreenContinuation','ClearWaterFamily','ClearStaticResources','ReadIndependentTargetPrefix','SubmitMaintenance','ClearOriginalModelSources']:
 body=s.split('Metal4WorldRenderer::'+name+'(',1)[1].split('{',1)[1]
 assert body.lstrip().startswith('FinishCPUWork();'),name
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeRenderJobSettlement.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Production CPU settlement precedes explicit source control/GPU drain boundaries; dispatch remains joined')
