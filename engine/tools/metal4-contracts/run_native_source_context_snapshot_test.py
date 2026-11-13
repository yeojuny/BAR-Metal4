#!/usr/bin/env python3
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-source-context-snapshot');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeSourceContextSnapshot.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('return DrawPrepared')<dispatch.index('NativeSourceContextSnapshot::Capture')<dispatch.index('dispatchReceipt->Reserve')
assert 'std::move(mrtFrontier),std::nullopt,std::nullopt)' in dispatch
assert 'snapshot,sourceContext,' in dispatch
prepared=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
helpers=prepared.split('NativeCPUPhaseScope cpuPhase',1)[0]
assert 'if(sourceContext)return sourceContext->ContextGeneration()' in helpers
assert 'if(sourceContext)return sourceContext->Models()' in helpers
assert helpers.count('Require([NSThread isMainThread]')==2
body=prepared.split('NativeCPUPhaseScope cpuPhase',1)[1]
assert 'OriginalNativeTextureContext().Registry().ContextGeneration()' not in body
assert 'OriginalNativeTextureContext().Geometry(' not in body
assert body.count('==contextGeneration()')==2 and 'const auto modelNames=modelGeometry();' in body
assert body.index('RuntimeLease lease')<body.index('const auto modelNames=modelGeometry();')
print('Worker-only capture,direct lazy post-retirement live reads,and3 production consumer sites structural PASS')
