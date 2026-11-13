#!/usr/bin/env python3
"""Actual inline CPU selection policy plus source wiring; no Metal execution."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-dynamic-water-derived-policy');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeDynamicWaterDerivedPolicy.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
capture=(root/'rts/Rendering/Backend/NativeDynamicWaterCapture.cpp').read_text()
assert 'frame->initializesDetail=true' in capture
assert 'impl->firstDraw=false;impl->receipts.Complete(sourceDrawFrame)' in capture
assert 'if(impl->receipts.Any())throw' in capture
renderer=(root/'rts/Rendering/Metal4/Metal4DynamicWater.mm').read_text()
assert 'NativeDynamicWaterDerivedCount' in renderer
assert 'bool detailReady=bool(p->previous)' in renderer
assert 'copy(impl->previous->detail,impl->staging->detail)' in renderer
assert 'impl->owner->active.expired()' in renderer
assert 'Dynamic-water first draw omitted original detail initialization' in renderer
print('Bootstrap marker / retained committed detail / unchanged serial gates structural PASS')
assert renderer.index('NativeDynamicWaterDerivedCount(*frame,false)') < renderer.index('auto p=std::make_unique<PreparedDynamicWater::Impl>()')
assert 'Require(frame->material==committed->material->source' in renderer
assert 'NativeDynamicWaterDerivedCount(*frame,bool(p->previous))' in renderer
assert 'for(size_t i=0;i<derivedCount;++i)' in renderer
updates=renderer.split('for(const auto& update:frame->updates){',2)[2].split('const auto derivedCount=',1)[0]
assert 'p->updates.push_back(prepareDraw(d,' in updates
assert 'NativeDynamicWaterDerivedCount' not in updates
print('Early validation / exact predecessor / update loop separation structural PASS')
