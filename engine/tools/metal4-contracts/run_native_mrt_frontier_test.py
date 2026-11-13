#!/usr/bin/env python3
"""Actual production frontier capture and registry; no Metal runtime."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-mrt-frontier');out.mkdir(exist_ok=True)
b=root/'rts/Rendering/Backend'
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeMRTFrontier.cpp')),str(b/'NativeTextureRegistry.cpp'),str(b/'NativeCUSTextureHistory.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=renderer.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('MRTFrontier209::Capture')<dispatch.index('cacheAccess.reset()')
direct=dispatch.split('if(!owned||!enabled',1)[1].split('NativeTargetSubmissionNotice',1)[0]
assert direct.index('if(previousRetired)mrtFrontier.reset()')<direct.index('return DrawPrepared')
proof=(root/'rts/Rendering/Metal4/Metal4MRTWriteReuse209.h').read_text()
assert 'registry.Find' not in proof and 'NativeTextureRegistry*' not in proof
assert 'u.use_count()!=structuralObjectMembers' in proof and 'ub.use_count()!=structuralBackingMembers' in proof
print('Main-before-handoff capture,direct callback opt-out,no worker registry lookup,exclusive U proof preserved structural PASS')
