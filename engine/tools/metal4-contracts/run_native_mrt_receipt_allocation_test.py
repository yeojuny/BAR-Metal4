#!/usr/bin/env python3
"""Actual Begin/helper/registry fault injection; old-expression negative control."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-mrt-receipt-allocation');out.mkdir(exist_ok=True)
b=root/'rts/Rendering/Backend'
files=['NativeCUSDeferredData','NativeDeferredColorData','NativeShieldDepthPublication','NativeShieldDepthBacking','NativeTextureRegistry','NativeCUSTextureHistory']
base=['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
source=Path(__file__).with_name('TestNativeMRTReceiptAllocation.cpp')
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:alloc_dealloc_mismatch=0'}
def build(path,extra=[]):
 subprocess.run(base+extra+['-I'+str(root/'rts'),str(source)]+[str(b/(f+'.cpp')) for f in files]+['-o',str(path)],check=True)
build(out/'test');subprocess.run([str(out/'test')],env=env,check=True)
override=out/'old/Rendering/Backend';override.mkdir(parents=True,exist_ok=True)
text=(b/'NativeCUSDeferredData.h').read_text();start=text.index(' auto owner=std::make_shared<NativeCUSDeferredPhase>();');end=text.index('\n}',start)
text=text[:start]+' return std::make_shared<NativeCUSDeferredPhase>(std::forward<Begin>(begin)());'+text[end:]
(override/'NativeCUSDeferredData.h').write_text(text)
build(out/'negative',['-I'+str(out/'old'),'-I'+str(b)])
r=subprocess.run([str(out/'negative')],env=env,capture_output=True,text=True)
assert r.returncode!=0 and 'Assertion' in r.stderr,(r.returncode,r.stdout,r.stderr)
print('Negative control old make_shared(Begin()) failed expected receipt/rollback assertion')
for name in ['NativeOpaqueModelCapture.cpp','NativeWaterGroundSourceCapture.cpp']:
 text=(root/'rts/Rendering/EngineBridge'/name).read_text()
 assert 'OwnNativeCUSDeferredPhase([&]' in text
 assert 'make_shared<NativeCUSDeferredPhase>(Begin' not in text
print('Both actual capture call sites use production allocation-before-publication helper')
