#!/usr/bin/env python3
"""Value policy plus actual dispatch-site guard; not an Apple runtime test."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-presentation-policy-test');out.mkdir(exist_ok=True)
source=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
continuation=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::ContinueScreen(',1)[1].split('bool Metal4WorldRenderer::HasScreenContinuation',1)[0]
# Limit by the next concrete method, independent of definition order.
continuation=continuation.split('std::shared_ptr<const void> Metal4WorldRenderer::PrepareIndependentResources',1)[0]
prepared=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert 'globalRendering->fullScreen' not in continuation
assert 'globalRendering->fullScreen' not in prepared
assert 'globalRendering->viewPosX' not in continuation
assert 'globalRendering->viewPosX' not in prepared
dispatch=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('MainThread();Require(impl!=nullptr')<dispatch.index('CaptureNativePresentationPolicy(')<dispatch.index('worker.Invoke(')
assert 'DrawPrepared(frame,options,presentation,std::move(previousRetired),' in dispatch
assert 'DrawPrepared(owned->Frame(),owned->Options(),presentation,std::move(deferredReceipt),' in dispatch
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativePresentationPolicy.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
