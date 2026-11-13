#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-native-shield-cancellation');out.mkdir(exist_ok=True)
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
abort=renderer.split('bool Metal4WorldRenderer::AbortShieldPublications',1)[1].split('bool Metal4WorldRenderer::HasScreenContinuation',1)[0]
assert abort.index('activeSourceSubmission)return false')<abort.index('CancelNativeOpaqueModels')<abort.index('shield->Cancel()')
assert 'CancelNativeShieldDepth(*impl->shieldRegistry,*impl->shieldPending)' not in abort
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert 'session->renderer->BeginShieldMapPublication(session->shieldCapture,' in bridge
abortBridge=bridge.split('void AbortPreparedWorld()',1)[1].split('namespace {',1)[0]
assert abortBridge.index('AbortShieldPublications')<abortBridge.index('AbortUncompletedRefraction')<abortBridge.index('shieldCapture.Reset')
assert '))return; // Keep failed-Begun source owners intact.' in abortBridge
backend=root/'rts/Rendering/Backend'
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeShieldCancellation.cpp')),*[str(backend/(n+'.cpp')) for n in ['NativeShieldDepthPublication','NativeShieldDepthBacking','NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
