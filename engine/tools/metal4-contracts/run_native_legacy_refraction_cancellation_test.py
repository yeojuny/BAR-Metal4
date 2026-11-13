#!/usr/bin/env python3
"""Actual publication/registry CPU tests; structural actual-Begin exit guard."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-refraction-cancellation');out.mkdir(exist_ok=True)
backend=root/'rts/Rendering/Backend'
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text().split('struct LegacyCopyExit',1)[1].split('legacyCopyExit;',1)[0]
assert bridge.index('!session->renderer->CanAbortUnsubmittedSource())return;')<bridge.index('AbortUncompletedRefraction()')
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
query=renderer.split('bool Metal4WorldRenderer::CanAbortUnsubmittedSource()',1)[1].split('bool Metal4WorldRenderer::AbortScreenContinuation',1)[0]
assert 'access.Acquired()&&(!impl||!impl->activeSourceSubmission)' in query
begin=renderer.split('id<MTL4CommandBuffer> command = lease.Begin("Actual engine terrain',1)[1]
assert begin.index('refractionOwner->MarkBegun(sourceSubmission)')<begin.index('impl->activeSourceSubmission=sourceSubmission;')
assert begin.index('impl->activeSourceSubmission=sourceSubmission;')<begin.index('targetSubmissionBegan(sourceSubmission)')
legacy=(root/'rts/Rendering/Metal4/Metal4LegacyWater.mm').read_text()
assert 'CanCancelUnsubmitted()const noexcept override{return !encodedSubmission&&!completed;}' in legacy
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeLegacyRefractionCancellation.cpp')),*[str(backend/(name+'.cpp')) for name in ['NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
