#!/usr/bin/env python3
"""Actual cube source + registry/history CPU test; not GPU/runtime validation."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-cube-cancellation');out.mkdir(exist_ok=True)
backend=root/'rts/Rendering/Backend'
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
abort=renderer.split('bool Metal4WorldRenderer::AbortShieldPublications',1)[1].split('bool Metal4WorldRenderer::HasScreenContinuation',1)[0]
assert 'CurrentReflectionUpdate' not in abort
assert 'CancelUnsubmittedReflection(*environment.frame->sourceCubeUpdate)' in abort
capture=(root/'rts/Rendering/WorldFrameCapture.cpp').read_text().split('WorldFrameCapture::PrepareEnvironmentBeforePreUnit()',1)[1].split('WorldFrame WorldFrameCapture::Capture',1)[0]
assert capture.index('cubeRollback.Capture')<capture.index('sky->GetLight()->Update')<capture.index('cubeRollback.Release')
assert capture.index('state->environmentFrame=std::move(out)')<capture.index('cubeRollback.Release')

handler=(root/'rts/Rendering/Env/CubeMapHandler.cpp').read_text().split('void CubeMapHandler::UpdateReflectionTexture()',1)[1].split('// NOTE:',1)[0]
assert handler.index('rollback.Capture(update)')<handler.index('CreateReflectionFace')<handler.index('source.Bind')<handler.index('currReflectionFace = nextFace')<handler.index('rollback.Release')
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
prepare=bridge.split('void PrepareEnvironmentBeforePreUnit()',1)[1].split('bool CanInitializeWater',1)[0]
assert prepare.index('environmentStage={}')<prepare.index('shadowHandler.CreateShadows')
assert bridge.count('ClearStaticResources(session->environmentStage,session->shieldCapture)')==2
assert 'AbortShieldPublications({}, {}, {}, environment, shield)' in renderer
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeCubeCancellation.cpp')),*[str(backend/(name+'.cpp')) for name in ['NativeCubeTextureSource','NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
