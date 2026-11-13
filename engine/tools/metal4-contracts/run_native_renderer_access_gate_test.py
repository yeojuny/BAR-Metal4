#!/usr/bin/env python3
"""Actual CPU gate/worker tests and structural ObjC++ call-site coverage."""
import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-renderer-gate');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
expected='ActualMainSampling ClearWaterResources ClearBasicWaterResources ClearDynamicWaterResources ClearLegacyWaterResources CanInitializeWater Sensor31657CircleConsumerLifetime SensorPreviewMaximumDimension CompileOriginalSensorPreviewProgram ClearStaticResources ReadIndependentTargetPrefix SubmitMaintenance CheckShieldGeometryAllocation CapturePBRSource AllocateCoverageTarget AllocateBlockoutTarget ConnectPBRSourceContext ConnectShieldTextureContext ConstructOriginalModelPrograms SetOriginalShadowCapture ConstructOriginalShadowPrograms ConstructOriginalShadowParticlePrograms CompileOriginalCUSProgram CompileOriginalTerrainProgram CompileOriginalDeferredLightProgram CompileOriginalUnitStencilProgramConsumer CompileOriginalOutlineProgram CompileOriginalDistortionProgram ClearOriginalModelSources ConnectBloomDeferredSources ConnectSSAODeferredSources BeginShieldMapPublication PublishShieldModelDepth BorrowActualMRTModelDepth HasScreenContinuation ContinueScreen PrepareIndependentResources DrawPrepared'.replace('CompileOriginalUnitStencilProgramConsumer','CompileOriginalUnitStencilProgram').split()
for name in expected:
 m=re.search(r'Metal4WorldRenderer::'+name+r'\([^;]*?\)(?: const)?\s*\{',s);assert m,name
 assert s[m.end():].lstrip().startswith('NativeRendererAccessGate::Scope cacheAccess'),name
part=s.split('Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldRenderer::DrawPrepared(',1)[0]
assert part.index('cacheHandoff.emplace(impl->access)')<part.index('cacheAccess.reset(); // Reservation')<part.index('worker.Invoke(')
assert part.index('admittedAccess(*cacheHandoff)')<part.index('Metal4SurfaceWorkerScope scope(snapshot)')
assert part.count('cacheHandoff.reset();')==2
assert 'cacheHandoff.reset();\n  deliverTargetNotice();' in part
assert 'catch(...){cacheHandoff.reset();deliverTargetNotice();' in part
assert 'NativeRendererAccessGate::Scope completionAccess(&owner->access);' in s
assert s.count('NativeRendererAccessGate::Scope providerAccess')==11
runtime=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text().split('void Runtime::DrainOldestLocked(',1)[1].split('FrameMetrics RuntimeLease::SubmitAndWait',1)[0]
assert runtime.index('pending->retire(')<runtime.index('pendingSubmissions.pop_front()')
assert 'poisoned=true;failedFrame=frame;' in runtime
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert '!session->renderer->AbortScreenContinuation())return;' in bridge
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeRendererAccessGate.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
print('Structural entry/provider/retirement checks passed; no Objective-C++/Metal execution')
