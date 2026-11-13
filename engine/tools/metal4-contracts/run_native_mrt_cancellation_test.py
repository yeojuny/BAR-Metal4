#!/usr/bin/env python3
"""Actual source/registry CPU test. No GPU execution."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-mrt-cancellation');out.mkdir(exist_ok=True)
backend=root/'rts/Rendering/Backend'
for file in ['Metal4DeferredColorNative.mm','Metal4ShieldNative.mm']:
 text=(root/'rts/Rendering/Metal4'/file).read_text()
 assert 'CanCancelUnsubmitted()const noexcept override{return image.producer&&!image.producer->encoded&&!image.producer->completed;}' in text
phase=(backend/'NativeCUSDeferredData.cpp').read_text().split('bool CanCancelNativeCUSDeferredPhase',1)[1].split('void Validate',1)[0]
assert phase.index('for(const auto& color:phase.colors)if(blocked(color))return false;')<phase.index('registry.CancelContentPublication')

renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text().split('bool Metal4WorldRenderer::AbortShieldPublications',1)[1].split('bool Metal4WorldRenderer::HasScreenContinuation',1)[0]
assert renderer.index('!CanCancelNativeShieldDepth(*shield))return false;')<renderer.index('CancelNativeOpaqueModels(')
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeMRTCancellation.cpp')),*[str(backend/(name+'.cpp')) for name in ['NativeCUSDeferredData','NativeDeferredColorData','NativeShieldDepthPublication','NativeShieldDepthBacking','NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
