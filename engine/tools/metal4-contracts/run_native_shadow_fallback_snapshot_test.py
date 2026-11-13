#!/usr/bin/env python3
"""Actual snapshot template with controlled adapters; structural native wiring only."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-shadow-fallback-snapshot');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeShadowFallbackSnapshot.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('return DrawPrepared(frame,options,')<dispatch.index('const auto shadowFallback=')
assert 'frame.originalShadow?NativeShadowFallbackSnapshot{}:' in dispatch
assert 'std::nullopt,std::nullopt)' in dispatch
assert 'cpuTraceContext,shadowFallback,' in dispatch
assert 'uiResolver,mrtFrontier,sourceContext,shadowFallback)' in dispatch
prepared=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
lazy=prepared.split('if(!frame.originalShadow&&!shadowFallback)',1)[1].split('if(frame.originalShadow)',1)[0]
assert 'Require([NSThread isMainThread]' in lazy and 'NativeShadowFallbackSnapshot::Capture' in lazy
assert 'initialShadow=shadowFallback->images' in prepared
assert 'OriginalNativeShadowTextures().Image(' not in prepared
assert 'backing->operation==NativeShadowTextureOperation::InitialClear||backing->completed.load()' in prepared
assert prepared.index('originalLifetime->initialShadow=initialShadow')<prepared.index('lease.Retain(originalLifetime)')
assert 'initialShadow=std::move(initialShadow)' in prepared
print('Exact worker receipt/originalShadow bypass/lazy direct/readiness/GPU owner retention structural PASS')
