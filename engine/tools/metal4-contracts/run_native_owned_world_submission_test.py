#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-owned-world');out.mkdir(exist_ok=True)
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert bridge.index('std::make_shared<const Metal4WorldSubmission>(frameOwner,options)')<bridge.index('admission.TransferValidated()')
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'if(!owned||!enabled' in renderer
assert 'dispatchReceipt,deferredReceipt=std::move(deferredReceipt)]() mutable' in renderer
assert 'DrawPrepared(owned->Frame(),owned->Options()' in renderer
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeOwnedWorldSubmission.cpp')),str(root/'rts/Rendering/Backend/ScreenDrawList.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
